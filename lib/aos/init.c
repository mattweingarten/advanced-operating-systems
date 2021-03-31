/**
 * \file
 * \brief Barrelfish library initialization.
 */

/*
 * Copyright (c) 2007-2019, ETH Zurich.
 * Copyright (c) 2014, HP Labs.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <stdio.h>

#include <aos/aos.h>
#include <aos/dispatch.h>
#include <aos/curdispatcher_arch.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/systime.h>
#include <barrelfish_kpi/domain_params.h>

#include "threads_priv.h"
#include "init.h"

// added
#include <aos/aos_rpc.h>

/// Are we the init domain (and thus need to take some special paths)?
static bool init_domain;

extern size_t (*_libc_terminal_read_func)(char *, size_t);
extern size_t (*_libc_terminal_write_func)(const char *, size_t);
extern void (*_libc_exit_func)(int);
extern void (*_libc_assert_func)(const char *, const char *, const char *, int);

void libc_exit(int);

__weak_reference(libc_exit, _exit);
void libc_exit(int status)
{
    debug_printf("libc exit NYI!\n");
    thread_exit(status);
    // If we're not dead by now, we wait
    while (1) {}
}

static void libc_assert(const char *expression, const char *file,
                        const char *function, int line)
{
    char buf[512];
    size_t len;

    /* Formatting as per suggestion in C99 spec 7.2.1.1 */
    len = snprintf(buf, sizeof(buf), "Assertion failed on core %d in %.*s: %s,"
                   " function %s, file %s, line %d.\n",
                   disp_get_core_id(), DISP_NAME_LEN,
                   disp_name(), expression, function, file, line);
    sys_print(buf, len < sizeof(buf) ? len : sizeof(buf));
}

__attribute__((__used__))
static size_t syscall_terminal_write(const char *buf, size_t len)
{
    if(len) {
        return sys_print(buf, len);
    }
    return 0;
}

__attribute__((__used__))
static size_t syscall_terminal_read(char * buf,size_t len){

  sys_getchar(buf);
  return 1;
}

__attribute__((__used__))
static size_t aos_terminal_write(const char * buf,size_t len){
  // debug_printf("Terminal write: in aos_terminal_write\n");

  struct aos_rpc * rpc = get_init_rpc();
  if(len){
    for(size_t i = 0;i < len;++i){
      aos_rpc_serial_putchar(rpc,buf[i]);
    }
  }
  return 0;
}

__attribute__((__used__))
static size_t aos_terminal_read(char* buf,size_t len){
    debug_printf("Got to aos_terminal_read\n");
    struct aos_rpc * rpc = get_init_rpc();
    errval_t err;
    err = aos_rpc_serial_getchar(rpc, buf);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"Failed get char in aos_terminal_read\n");
    }
    return 1;
}





/* Set libc function pointers */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and
    // what we need for that
    // TODO: change these to use the user-space serial driver if possible
    // TODO: set these functions
    if(init_domain){
      _libc_terminal_read_func = syscall_terminal_read;
      _libc_terminal_write_func = syscall_terminal_write;
      _libc_exit_func = libc_exit;
      _libc_assert_func = libc_assert;
    } else {
      _libc_terminal_read_func = aos_terminal_read;
      _libc_terminal_write_func = aos_terminal_write;
      _libc_exit_func = libc_exit;
      _libc_assert_func = libc_assert;
    }

    /* morecore func is setup by morecore_init() */

    // XXX: set a static buffer for stdout
    // this avoids an implicit call to malloc() on the first printf
    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IOLBF, sizeof(buf));
}


/** \brief Initialise libbarrelfish.
 *
 * This runs on a thread in every domain, after the dispatcher is setup but
 * before main() runs.
 */
errval_t barrelfish_init_onthread(struct spawn_domain_params *params)
{
    errval_t err;

    // do we have an environment?
    if (params != NULL && params->envp[0] != NULL) {
        extern char **environ;
        environ = params->envp;
    }

    // Init default waitset for this dispatcher
    struct waitset *default_ws = get_default_waitset();
    waitset_init(default_ws);

    // Initialize ram_alloc state
    ram_alloc_init();
    /* All domains use smallcn to initialize */
    err = ram_alloc_set(ram_alloc_fixed);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }

    err = paging_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_VSPACE_INIT);
    }

    err = slot_alloc_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_SLOT_ALLOC_INIT);
    }

    err = morecore_init(BASE_PAGE_SIZE);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_MORECORE_INIT);
    }

    lmp_endpoint_init();

    // HINT: Use init_domain to check if we are the init domain.
    if (init_domain) { // init does not need a channel to itself
        err = cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPointLMP, 0, 1);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);
        return SYS_ERR_OK;
    }


    err = ram_alloc_set(NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "unable to set ram_alloc func");
        return err;
    }

    // for other domains: allocate and set init_rpc
    static struct aos_rpc init_rpc;
    struct capref self_ep_cap = (struct capref) {
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_SELFEP
    };

    struct capref init_ep_cap = (struct capref) {
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_INITEP
    };

    // create and initialize rpc
    aos_rpc_init(&init_rpc, self_ep_cap, init_ep_cap, NULL);
    set_init_rpc(&init_rpc);

    // set ram_alloc function for non-init threads
    /*errval_t rpc_ram_alloc(struct capref *ret, size_t size, size_t alignment) {
        return aos_rpc_get_ram_cap(get_init_rpc(), size, alignment, ret, NULL);
    }*/


    // TODO MILESTONE 3: register ourselves with init
    /* allocate lmp channel structure */
    /* create local endpoint */
    /* set remote endpoint to init's endpoint */
    /* set receive handler */
    /* send local ep to init */
    /* wait for init to acknowledge receiving the endpoint */
    /* initialize init RPC client with lmp channel */
    /* set init RPC client in our program state */

    /* TODO MILESTONE 3: now we should have a channel with init set up and can
     * use it for the ram allocator */

    // right now we don't have the nameservice & don't need the terminal
    // and domain spanning, so we return here
    return SYS_ERR_OK;
}


/**
 *  \brief Initialise libbarrelfish, while disabled.
 *
 * This runs on the dispatcher's stack, while disabled, before the dispatcher is
 * setup. We can't call anything that needs to be enabled (ie. cap invocations)
 * or uses threads. This is called from crt0.
 */
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg);
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg)
{
    init_domain = init_dom_arg;
    disp_init_disabled(handle);
    thread_init_disabled(handle, init_dom_arg);
}
