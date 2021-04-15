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


#include <spawn/spawn.h>
#include <spawn/process_manager.h>
#include "mem_alloc.h"

#include "test.h"

int test_stack_overflow(void);
/**
 * \brief causes a stack-overflow by continuously allocating stack-space, then
 * calling itself again
 * \return does not return
 */
__attribute__((noreturn,unused))
int test_stack_overflow(void) {
    char c[1024];
    c[0] = 'a';

    /* debug_printf("Stack address: %ld\n",c); */
    test_stack_overflow();
}

int test_infinite_loop(void);
/**
 * \brief tests the limits memory allocation:
 * indefenitely allocates memory (and maybe frees it)
 * \return does not return
 */
__attribute__((noreturn,unused))
int test_infinite_loop(void){
    
    uint64_t count = 0;
    while(true){
        size_t size = 1L << 15;

        char* p = malloc(size * sizeof(char));
        for(int i = 0; i < size;++i){
            p[i] = i % 255;
        }
        uint64_t random[5] = { 512,300,144,235,10};
        for(int i = 0; i < 5; ++i){
            assert(p[random[i]] == random[i]%255);
        }
        // free(p);
        if(count % 1 == 0){
            debug_printf("P: %ld",p);

            debug_printf("Ran %ld times\n",count);
        }  
        count++;
    }
}

int test_printf(void);
/**
 * \brief Tests the printf function by trying to print the target string
 */
int test_printf(void) {
    TEST_START;
    char* tgt = "Test_String";
    debug_printf("trying to print: \"%s\"\n", tgt);
    printf(tgt);
    printf("\n");
    return 0;
}

int test_getchar(void);
/**
 * \brief Tests the getchar function: prompts user to enter various
 * characters, fails if the result is not as requested.
 */
int test_getchar(void) {
    TEST_START;
    for (char t = 'a'; t < 'h'; t++) {
        debug_printf("please enter the char '%c'", t);
        char entered = getchar();

        if (entered == t) {
            debug_printf(" - k thx\n");
            continue;
        }

        debug_printf("ERROR: requested was '%c',"
                     "but getchar() returned '%c'\n", t, entered);
        return 1;
    }
    return 0;
}

int test_malloc(void);
/**
 * \brief Tests malloc functionality: allocates memory up to target size,
 * uses it, then frees it.
 */
int test_malloc(void) {
    TEST_START;
    for (int i = 1024; i <= (1024 * 1024); i *= 2) {
        debug_printf("trying with %d bytes\n", i);
        char* cur = malloc(i * sizeof(char));

        // write to memory
        for (int j = 0; j < i; j++) {
            cur[j] = j % 255;
        }

        // verify results
        for (int j = 0; j < i; j++) {
            if (cur[j] != j % 255) {
                debug_printf("ERROR: index %d should be %d but is %d",
                             j, j % 255, cur[j]);
                free(cur);
                return 1;
            }
        }

        // free memory
        free(cur);
    }
    return 0;
}

static int test_count = 3;
int (*tests[])(void) = {
    &test_printf,
    &test_getchar,
    &test_malloc
};

/**
 * \brief main testing function: call your tests from this function
 * \return 0 if all tests pass, non-zero int otherwise
 */
int run_init_tests(void) {
    debug_printf("=======================\n");
    debug_printf("BEGINNING TESTS IN INIT\n");

    for (int i = 0; i < test_count; i++) {
        debug_printf("running test %d\n", i + 1);
        int out = (*tests[i])();

        if (out) {
            debug_printf("function %d returned error, abort\n", i);
            return out;
        }
    }
    
    return 0;
}
