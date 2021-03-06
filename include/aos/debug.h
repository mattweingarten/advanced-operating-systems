/**
 * \file
 * \brief Debugging functions
 */

/*
 * Copyright (c) 2008, 2010, 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef BARRELFISH_DEBUG_H
#define BARRELFISH_DEBUG_H

#include <sys/cdefs.h>

#include <errors/errno.h>
#include <aos/caddr.h>
#include <stddef.h>
#include <stdlib.h>
#include <barrelfish_kpi/registers_arch.h>
// #include <aos/core_state.h>
__BEGIN_DECLS

struct capability;
struct paging_region;
struct paging_state;
struct morecore_state;

errval_t debug_cap_identify(struct capref cap, struct capability *ret);
errval_t debug_dump_hw_ptables(void *);
errval_t debug_cap_trace_ctrl(uintptr_t types, genpaddr_t start_addr, gensize_t size);
void debug_cspace(struct capref root);
void debug_my_cspace(void);
void debug_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));


int debug_print_cap(char *buf, size_t len, struct capability *cap);
int debug_print_cap_at_capref(char *buf, size_t len, struct capref cap);
int debug_print_capref(char *buf, size_t len, struct capref cap);
int debug_print_cnoderef(char *buf, size_t len, struct cnoderef cnode);

void debug_print_paging_state(struct paging_state );
void debug_print_paging_region(struct paging_region );
void debug_print_morecore_state(struct morecore_state );

void debug_print_save_area(arch_registers_state_t *state);
void debug_print_fpu_state(arch_registers_state_t *state);
void debug_dump(arch_registers_state_t *state);
void debug_call_chain(arch_registers_state_t *state);
void debug_return_addresses(void);
void debug_dump_mem_around_addr(lvaddr_t addr);
void debug_dump_mem(lvaddr_t base, lvaddr_t limit, lvaddr_t point);




void debug_err(const char *file, const char *func, int line,
               errval_t err, const char *msg, ...);
void user_panic_fn(const char *file, const char *func, int line,
                   const char *msg, ...)
    __attribute__((noreturn));

#ifdef NDEBUG
# define DEBUG_PRINTF(fmt...) ((void)0)
# define DEBUG_ERR(err, msg...) ((void)0)
# define HERE ((void)0)
#else
# define DEBUG_PRINTF(fmt...) debug_printf(fmt);
# define DEBUG_ERR(err, msg...) debug_err(__FILE__, __func__, __LINE__, err, msg)
# include <aos/dispatch.h>
# define HERE fprintf(stderr, "Disp %.*s.%u: %s, %s, %u\n",             \
                        DISP_NAME_LEN, disp_name(), disp_get_core_id(), \
                      __FILE__, __func__, __LINE__)
#endif

/**
 * \brief Prints out a string, errval and then aborts the domain
 */
#define USER_PANIC_ERR(err, msg...) do {               \
    debug_err(__FILE__, __func__, __LINE__, err, msg); \
    abort();                                           \
} while (0)

/**
 * \brief Start cycle measurement
 */
#define MEASURE_START(cnt, str...) do {                        \
    debug_printf("==> Start cycle measurement for *%s*\n", str);    \
    cnt = rdtsc();                                          \
} while (0)

/**
 * \brief End cycle measurement
 */
#define MEASURE_END(cnt) do {\
    cnt = rdtsc() - cnt;\
    if (cnt/1200000000.0 > 0.5) {\
        debug_printf("==> End cycle measurement. Duration: %lu cycles and %.2f s\n", cnt, (cnt/1200000000.0));\
    } else if (cnt/1200000.0 > 0.5){\
        debug_printf("==> End cycle measurement. Duration: %lu cycles and %.2f ms\n", cnt, (cnt/1200000.0));\
    } else if (cnt/1200.0 > 0.5) {\
        debug_printf("==> End cycle measurement. Duration: %lu cycles and %.2f us\n", cnt, (cnt/1200.0));\
    } else {\
        debug_printf("==> End cycle measurement. Duration: %lu cycles and %.2f ns\n", cnt, (cnt/1.2));\
    }\
} while (0)

/**
 * \brief Null ptr check with err return
 */
#define NULLPTR_CHECK(ptr, err) do {                    \
    if (ptr == NULL) {                                  \
        HERE;                                           \
        return err;                                     \
    }                                                   \
} while (0)

/**
 * \brief Prints out a string and abort the domain
 */
#define USER_PANIC(msg...)                                 \
    user_panic_fn(__FILE__, __func__, __LINE__, msg);      \

/**
 * \brief Returns an error on fail
 */
#define ON_ERR_RETURN(err) do {                        \
    if (err_is_fail(err)) {                            \
        HERE;                                          \
        return err;                                    \
    }                                                  \
} while (0)

/**
 * \brief Pushes an error-code into the error and returns it on fail
 */
#define ON_ERR_PUSH_RETURN(err, code) do {             \
    if (err_is_fail(err)) {                            \
        HERE;                                          \
        return err_push(err, code);                    \
    }                                                  \
} while (0)

/**
 * \brief Returns an error on fail
 */
#define ON_ERR_NO_RETURN(err) do {                      \
    if (err_is_fail(err)) {                             \
        HERE;                                           \
        DEBUG_ERR(err, "");                             \
        return;                                         \
    }                                                   \
} while (0)

__END_DECLS

#endif //BARRELFISH_DEBUG_H
