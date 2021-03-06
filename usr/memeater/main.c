/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>

static struct aos_rpc *init_rpc, *mem_rpc;

const char *str = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                  "sed do eiusmod tempor incididunt ut labore et dolore magna "
                  "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                  "ullamco laboris nisi ut aliquip ex ea commodo consequat. "
                  "Duis aute irure dolor in reprehenderit in voluptate velit "
                  "esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                  "occaecat cupidatat non proident, sunt in culpa qui officia "
                  "deserunt mollit anim id est laborum.";

__attribute__((unused))  static errval_t request_and_map_memory(void)
{
    errval_t err;

    size_t bytes;
    struct frame_identity id;
    debug_printf("testing memory server...\n");

    struct paging_state *pstate = get_current_paging_state();

    debug_printf("obtaining cap of %" PRIu32 " bytes...\n", BASE_PAGE_SIZE);

    struct capref cap1;
    err = aos_rpc_get_ram_cap(mem_rpc, BASE_PAGE_SIZE, BASE_PAGE_SIZE,
                              &cap1, &bytes);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not get BASE_PAGE_SIZE cap\n");
        return err;
    }
    char buf[128];
    debug_print_cap_at_capref(buf, 128, cap1);
    debug_printf("obtained cap: %s", buf);

    struct capref cap1_frame;
    err = slot_alloc(&cap1_frame);
    assert(err_is_ok(err));

    debug_printf("Retype to frame \n");

    err = cap_retype(cap1_frame, cap1, 0, ObjType_Frame, BASE_PAGE_SIZE, 1);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not retype RAM cap to frame cap\n");
        return err;
    }

    err = frame_identify(cap1_frame, &id);
    assert(err_is_ok(err));

    debug_printf("Mapping frame \n");
    void *buf1;
    err = paging_map_frame(pstate, &buf1, BASE_PAGE_SIZE, cap1_frame, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not get BASE_PAGE_SIZE cap\n");
        return err;
    }

    debug_printf("got frame: 0x%" PRIxGENPADDR " mapped at %p\n", id.base, buf1);

    debug_printf("performing memset.\n");
    memset(buf1, 0x00, BASE_PAGE_SIZE);



    debug_printf("obtaining cap of %" PRIu32 " bytes using frame alloc...\n",
                 LARGE_PAGE_SIZE);

    struct capref cap2;
    err = frame_alloc(&cap2, LARGE_PAGE_SIZE, &bytes);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not get BASE_PAGE_SIZE cap\n");
        return err;
    }

    err = frame_identify(cap2, &id);
    assert(err_is_ok(err));

    void *buf2;
    err = paging_map_frame(pstate, &buf2, LARGE_PAGE_SIZE, cap2, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not get BASE_PAGE_SIZE cap\n");
        return err;
    }

    debug_printf("got frame: 0x%" PRIxGENPADDR " mapped at %p\n", id.base, buf1);

    debug_printf("performing memset.\n");
    memset(buf2, 0x00, LARGE_PAGE_SIZE);

    return SYS_ERR_OK;

}

__attribute__((unused))  static errval_t test_basic_rpc(void)
{
    errval_t err;


    domainid_t new_pid;
    err = aos_rpc_process_spawn(get_init_rpc(),"hello",0,&new_pid);
    debug_printf("RPC: testing basic RPCs...\n");

    debug_printf("RPC: sending number...\n");
    // print(init_rpc);
    // debug_printf("Initrpc address = %lx\n",init_rpc);
    err =  aos_rpc_send_number(init_rpc, 42);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not send a number\n");
        return err;
    }
    uint64_t cnt;
    MEASURE_START(cnt, "Sending a Number");
    err =  aos_rpc_send_number(init_rpc, 102);
    MEASURE_END(cnt);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not send a number\n");
        return err;
    }

    err =  aos_rpc_send_number(init_rpc, 13);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not send a number\n");
        return err;
    }
    debug_printf("RPC: sending small string...\n");
    err =  aos_rpc_send_string(init_rpc, "Hello init");
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not send a string\n");
        return err;
    }

    debug_printf("RPC: sending large string...\n");
    err =  aos_rpc_send_string(init_rpc, str);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not send a string\n");
        return err;
    }

    debug_printf("RPC: testing basic RPCs. SUCCESS\n");

    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
    errval_t err = SYS_ERR_OK;
    debug_printf("memeater started....\n");

    init_rpc = get_init_rpc();
    if (!init_rpc) {
        USER_PANIC_ERR(err, "init RPC channel NULL?\n");
    }

    //mem_rpc = aos_rpc_get_memory_channel();
    mem_rpc = get_mm_rpc();
    if (!mem_rpc) {
        USER_PANIC_ERR(err, "memory RPC channel NULL?\n");
    }
    //
    err = test_basic_rpc();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "failure in testing basic RPC\n");
    }
    //
    /*errval_t get_sum_ram(struct capref *ret, size_t size, size_t alignment) {
        return aos_rpc_get_ram_cap(init_rpc, size, alignment, ret, NULL);
    }

    ram_alloc_set(&get_sum_ram);*/
    err = request_and_map_memory();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "could not request and map memory\n");
    }



    /* test printf functionality */
    debug_printf("testing terminal printf function...\n");

    printf("Hello world using terminal service\n");


    char command[1024];
    debug_printf("Testing terminal read:\n");
    aos_rpc_get_terminal_input(init_rpc, command, 1024);




    debug_printf("Got terminal command : %s\n",command);

    char c = 'A';
    while(c != 13 && c != '\n'){
      c = getchar();
      printf("Char c = %c\n",c);
      //debug_printf("c = %d\n", c);
    }


    char * test = (char * ) malloc(sizeof(char));
    test[0] = 'A';


    // aos_rpc_call(init_rpc,AOS_RPC_MEM_SERVER_REQ,)

    // domainid_t pid = 0;
    // debug_printf("starting hello world process\n");
    // //aos_rpc_process_spawn(init_rpc, "hello", disp_get_current_core_id(), &pid);
    // aos_rpc_process_spawn(init_rpc, "hello", 1, &pid);
    // debug_printf("hello started with pid %d\n", pid);

    // domainid_t perftest;
    // aos_rpc_process_spawn(init_rpc, "performance_tester", disp_get_current_core_id(), &perftest);
    // debug_printf("performance tester started with pid %d\n", perftest);

    debug_printf("memeater terminated....\n");
    return EXIT_SUCCESS;
}
