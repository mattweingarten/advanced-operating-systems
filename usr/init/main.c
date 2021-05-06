    /**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/waitset.h>
#include <aos/aos_rpc.h>
#include <mm/mm.h>
#include <grading.h>
#include <aos/core_state.h>
#include <aos/systime.h>
#include <aos/threads.h>

#include <aos/dispatch.h>
#include <aos/dispatcher_arch.h>
#include <aos/curdispatcher_arch.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <barrelfish_kpi/startup_arm.h>


#include <spawn/spawn.h>
#include <spawn/process_manager.h>
#include "spawn_server.h"
#include "mem_alloc.h"
#include "rpc_server.h"
#include "test.h"

#include <aos/kernel_cap_invocations.h>


// #include <aos/curdispatcher_arch.h>


coreid_t my_core_id;

__unused
static errval_t init_process_manager(void){
    errval_t err;
    struct waitset *default_ws = get_default_waitset();
    struct spawninfo *pm_si = spawn_create_spawninfo();
    domainid_t *pm_pid = &pm_si -> pid;
    err = spawn_load_by_name("process_manager",pm_si,pm_pid);
    ON_ERR_RETURN(err);
    struct aos_rpc *pm_rpc = &pm_si -> rpc;
    aos_rpc_init(pm_rpc);
    initialize_rpc_handlers(pm_rpc);
    debug_printf("waiting for process manager to come online...\n");
    while(!get_pm_online()){

        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    debug_printf("Process manager is online!\n");

    err = aos_rpc_call(pm_rpc,AOS_RPC_REGISTER_PROCESS,disp_get_domain_id(),disp_get_core_id(),"init");
    ON_ERR_RETURN(err);
    err = aos_rpc_call(pm_rpc,AOS_RPC_REGISTER_PROCESS,*pm_pid,disp_get_core_id(),"process_manager");
    ON_ERR_RETURN(err);
    set_pm_rpc(pm_rpc);
    debug_printf("all finished!\n");

    return SYS_ERR_OK;
}


__attribute__((unused)) static errval_t init_memory_server(domainid_t *mem_pid){
    errval_t err;
    struct spawninfo *mem_si = spawn_create_spawninfo();
    domainid_t *m_pid = &mem_si -> pid;
    err = spawn_load_by_name("memory_server",mem_si,m_pid);
    ON_ERR_RETURN(err);
    struct aos_rpc *mem_rpc = &mem_si -> rpc;
    aos_rpc_init(mem_rpc);
    initialize_rpc_handlers(mem_rpc);
    struct waitset *default_ws = get_default_waitset();
    debug_printf("waiting for memory server to come online ... \n");
    while(!get_mem_online()){
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    // err = spawn_new_domain("memory_server",&pid)
    set_mem_rpc(mem_rpc);
    debug_printf("Memory server online!\n");

    *mem_pid = mem_si -> pid;
    return SYS_ERR_OK;
}


__unused
static void hey(void* arg) {
    debug_printf("we were pinged!\n");
    struct lmp_recv_buf msg;
    lmp_endpoint_recv(arg, &msg, NULL);
    lmp_endpoint_register(arg, get_default_waitset(), MKCLOSURE(hey, arg));
}


static errval_t init_foreign_core(void){

    struct capref epcap;
    slot_alloc(&epcap);

    struct capability ipi_ep = {
        .type = ObjType_EndPointIPI,
        .rights = CAPRIGHTS_READ_WRITE,
        .u.endpointipi = {
            .channel_id = 1,
            .notifier = (void*) 0xffff000008000000,
            .listener_core = 0
        }
    };

    invoke_monitor_create_cap((uint64_t *)&ipi_ep,
                                     get_cnode_addr(epcap),
                                     get_cnode_level(epcap),
                                     epcap.slot, 1);

    invoke_ipi_notify(epcap);
    debug_printf("notifying\n");
    invoke_ipi_notify(epcap);
    //invoke_ipi_register(epcap, 4);

    //lmp_endpoint_register(ep, get_default_waitset(), MKCLOSURE(hey, ep));

    return SYS_ERR_OK;


    errval_t err;
    uint64_t *urpc_init = (uint64_t*) MON_URPC_VBASE;
        debug_printf("Bootinfo base: %lx, bootinfo size: %lx\n",urpc_init[0],urpc_init[1]);
    debug_printf("Ram base: %lx, Ram size: %lx\n",urpc_init[2],urpc_init[3]);
    struct capref bootinfo_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_BOOTINFO,
    };
    struct capref core_ram = {
        .cnode = cnode_super,
        .slot = 0,
    };

    err = frame_forge(bootinfo_cap, urpc_init[0], urpc_init[1], 0);
    ON_ERR_RETURN(err);
    
    err = paging_map_frame_complete(get_current_paging_state(),(void **) &bi,bootinfo_cap,NULL,NULL);
    ON_ERR_RETURN(err);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to map bootinfo struct");
    // }

    assert(bi && "Boot info in appmain is NULL");


    err = ram_forge(core_ram, urpc_init[2], urpc_init[3], disp_get_current_core_id());
    ON_ERR_RETURN(err);
    err = initialize_ram_alloc_foreign(core_ram);
    
    ON_ERR_RETURN(err);

    const int nBenches = 100;

    for (int i = 0; i < 3; i++) {
        uint64_t before = systime_now();
        for (int j = 0; j < nBenches; j++) {
            struct capref thrown_away;
            err  = ram_alloc(&thrown_away, BASE_PAGE_SIZE);
            // debug_printf("Allocated ram!\n");
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to allocate ram in benchmarkmm\n");
            }
        }
        uint64_t end = systime_now();

        debug_printf("measurement %d took: %ld\n", i, systime_to_ns(end - before));
    }

    struct capref mc = {
        .cnode = cnode_root,
        .slot = ROOTCN_SLOT_MODULECN
    };
    err = cnode_create_raw(mc, NULL, ObjType_L2CNode, L2_CNODE_SLOTS, NULL);
    
    err = frame_forge(cap_mmstrings, urpc_init[4], urpc_init[5], 0);
    ON_ERR_RETURN(err);
    
    for(int i = 0; i < bi -> regions_length;++i) {
        
        if(bi -> regions[i].mr_type == RegionType_Module){
            struct capref module_cap = {
                .cnode = cnode_module,
                .slot = bi -> regions[i].mrmod_slot
            };
            size_t size = bi->regions[i].mrmod_size;
            // debug_printf("Trying to forge cap: %ld bytes\n", size);
            err = frame_forge(module_cap, bi->regions[i].mr_base, ROUND_UP(size, BASE_PAGE_SIZE), disp_get_current_core_id()); 
            ON_ERR_RETURN(err);
        }
    }

    init_core_channel(0, (lvaddr_t) urpc_init);
    err = aos_rpc_call(core_channels[0],AOS_RPC_REGISTER_PROCESS,disp_get_domain_id(),disp_get_current_core_id(),"init");
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to register new init to pm\n");
    }
    return SYS_ERR_OK;
}








static int bsp_main(int argc, char *argv[])
{
    errval_t err;

    // Grading
    grading_setup_bsp_init(argc, argv);

    // First argument contains the bootinfo location, if it's not set
    bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
    assert(bi);

    // TODO: initialize mem allocator, vspace management here
    err = initialize_ram_alloc();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "/>/> Error: Initialize_ram_alloc");
    }

    err = init_terminal_state();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init terminal state\n");
    }

    struct capref lmp_ep;
    struct lmp_endpoint *le;

    struct capref ump_ep;

    endpoint_create(64, &lmp_ep, &le);
    ipi_endpoint_create(&ump_ep);
    invoke_ipi_register(lmp_ep, ump_ep);


    lmp_endpoint_register(le, get_default_waitset(), MKCLOSURE(hey, le));


/*
    domainid_t mem_pid;
    err = init_memory_server(&mem_pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init memory server\n");
    }

    err  = init_process_manager();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init process manager!\n");
    }*/

    spawn_new_core(my_core_id + 1);
    //spawn_new_core(my_core_id + 2);

    /*for (int i = 0; i < 100000000; i++) {
        __asm volatile("mov x7, x7\n");
    }
    for (int i = 0; i < 100000000; i++) {
        __asm volatile("mov x7, x7\n");
    }

    for (int i = 0; i < 10; i++) {
        debug_printf("pinging core 1 on channel 4\n");
        invoke_monitor_send_ping(1, 4);
        for (int j = 0; j < 100000000; j++) {
            __asm volatile("mov x7, x7\n");
        }
    }*/

    // char buffer[512];
    // err = aos_rpc_call(get_pm_rpc(),AOS_RPC_GET_PROC_NAME,0,buffer);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to resolve name 0\n");
    // }

    // debug_printf("Got string %s\n",buffer);

    // 
    
    //run_init_tests(my_core_id);

    

    // struct spawninfo *pm_si = spawn_create_spawninfo();
    // domainid_t *pm_pid = &pm_si -> pid;
    // err = spawn_load_by_name("process_manager",pm_si,pm_pid);
    // struct aos_rpc *pm_rpc = &pm_si -> rpc;
    // aos_rpc_init(pm_rpc);
    // initialize_rpc_handlers(pm_rpc);

    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to spawn pm!\n");
    // }


    // debug_printf("waiting for process manager to come online...\n");
    // while(!get_pm_online()){
    //     err = event_dispatch(default_ws);
    //     if (err_is_fail(err)) {
    //         DEBUG_ERR(err, "in event_dispatch");
    //         abort();
    //     }
    // }
    // debug_printf("Process manager is online!\n");

    // err = aos_rpc_call(pm_rpc,AOS_RPC_REGISTER_PROCESS,disp_get_domain_id(),disp_get_core_id(),"init");
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to send to pm\n");
    // }
    // event_dispatch(default_ws);




    // err = aos_rpc_send_number(pm_rpc,1);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to send number to pm!\n");
    // }

    // Grading
    grading_test_early();

    // TODO: Spawn system processes, boot second core etc. here
    
    
    //spawn_new_domain("performance_tester", NULL);


    // size_t counter = 0;
    // while(1) {
    //     if (counter % (1 << 28) == 0){
    //         aos_rpc_call(core_channels[1], AOS_RPC_SEND_NUMBER, counter);
    //     }
    //     counter++;
    // }


    // Grading
    grading_test_late();

    debug_printf("Message handler loop\n");
    // Hang around
    
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}










static int app_main(int argc, char *argv[])
{
    // Implement me in Milestone 5
    // Remember to call
    // - grading_setup_app_init(..);
    // - grading_test_early();
    // - grading_test_late();
    
    errval_t err;

    debug_printf("Hello from core: %d!\n",disp_get_current_core_id());
    err = init_foreign_core();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to initialize ram and bootinfo for new core core\n");
    }
    
    //run_init_tests(my_core_id);

    grading_setup_app_init(bi);

    grading_test_early();

    grading_test_late();

    debug_printf("Message handler loop\n");
    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    thread_exit(0);
    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
    errval_t err;


    /* Set the core id in the disp_priv struct */
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID ", invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");
    fflush(stdout);


    if (my_core_id == 0)
        return bsp_main(argc, argv);
    else
        return app_main(argc, argv);
}
