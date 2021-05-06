
#ifndef _INIT_PROCESS_MANAGER_H_
#define _INIT_PROCESS_MANAGER_H_

#include <aos/aos.h>
#include <spawn/spawn.h>


struct spawninfo *spawn_create_spawninfo(void);
struct aos_rpc *get_rpc_from_spawn_info(domainid_t pid);
// domainid_t spawn_get_new_domainid(void);

#endif /* _INIT_PROCESS_MANAGER_H_ */
