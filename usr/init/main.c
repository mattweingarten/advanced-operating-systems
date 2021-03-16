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

#include "mem_alloc.h"




struct bootinfo *bi;

coreid_t my_core_id;


static void test_free_coalesce(void)
{
    struct capref caps[1000];
    for (int i = 0; i < 1000; i++) {
        ram_alloc_aligned(&caps[i], 4096, 1);
    }
    for (int i = 0; i < 1000; i++) {
        if ((i / 10) % 2) {
            aos_ram_free(caps[i]);
        }
    }

    print_mm_state(&aos_mm);
}


static void test_map_frame_8192(void)
{
    struct capref my_frame;
    size_t f_size;
    frame_alloc(&my_frame, 8192, &f_size);

    lvaddr_t addr = VADDR_OFFSET + 0x125000;
    paging_map_fixed_attr(get_current_paging_state(), addr, my_frame, 8192, 0);

    long* pointer = (long*) addr;
    for (int i = 0; i < 1024; i++) {
        pointer[i] = i;
    }
    for (int i = 0; i < 512; i++) {
        pointer[0] += pointer[i];
    }
    printf("value in memory at v-address %p: %d\n", pointer, pointer[0]);
}


static void test_align(void)
{
    struct capref cap;
    ram_alloc_aligned(&cap, 4096, 1024 * 1024 * 1024);
    print_mm_state(&aos_mm);
}


static void test(void)
{
    // begin experiment
    printf("start experiment!\n");
    test_align();

    if(0) test_free_coalesce();

    test_map_frame_8192();

    struct capref my_frame;
    size_t f_size;
    frame_alloc(&my_frame, 4096, &f_size);

    lvaddr_t addr = VADDR_OFFSET + 0x123000;
    paging_map_fixed_attr(get_current_paging_state(), addr, my_frame, 4096, 0);

    int* pointer = (int*) addr;
    for (int i = 0; i < 100; i++) {
        pointer[i] = i;
    }
    for (int i = 0; i < 100; i++) {
        pointer[0] += pointer[i];
    }
    printf("value in memory at v-address %p: %d\n", pointer, pointer[0]);
    /*ram_alloc_aligned(&a_page, 4096, 4096);
    ram_alloc_aligned(&a_page, 4096, 4096);
    ram_alloc_aligned(&a_page, 4096, 4096);
    ram_alloc_aligned(&a_page, 4096, 4096);*/
    printf("end experiment!\n");
    // end experiment
}


static int
bsp_main(int argc, char *argv[]) {
    errval_t err;

    // Grading 
    grading_setup_bsp_init(argc, argv);

    // First argument contains the bootinfo location, if it's not set
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    assert(bi);

    err = initialize_ram_alloc();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    // TODO: initialize mem allocator, vspace management here

    test();
    
    // Grading 
    grading_test_early();

    // TODO: Spawn system processes, boot second core etc. here
    
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

static int
app_main(int argc, char *argv[]) {
    // Implement me in Milestone 5
    // Remember to call
    // - grading_setup_app_init(..);
    // - grading_test_early();
    // - grading_test_late();
    return LIB_ERR_NOT_IMPLEMENTED;
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


    if(my_core_id == 0) return bsp_main(argc, argv);
    else                return app_main(argc, argv);
}