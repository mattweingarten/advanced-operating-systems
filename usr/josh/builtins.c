#include "builtins.h"
#include "format.h"
#include "variables.h"

#include <aos/nameserver.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
struct builtin
{
    const char *cmd;
    int(*handler)(size_t argc, const char **argv, struct aos_datachan *out);
};

int handle_echo(size_t argc, const char **argv, struct aos_datachan *out);
int handle_clear(size_t argc, const char **argv, struct aos_datachan *out);
int handle_env(size_t argc, const char **argv, struct aos_datachan *out);
int handle_time(size_t argc, const char **argv, struct aos_datachan *out);
int handle_pmlist(size_t argc, const char **argv, struct aos_datachan *out);
int handle_nslist(size_t argc, const char **argv, struct aos_datachan *out);
int handle_nslookup(size_t argc, const char **argv, struct aos_datachan *out);


const struct builtin builtins[] = {
    { "echo", &handle_echo },
    { "clear", &handle_clear },
    { "env", &handle_env },
    { "time", &handle_time },
    { "pmlist", &handle_pmlist },
    { "nslist", &handle_nslist},
    { "nslookup", &handle_nslookup}
};



bool check_for_option(size_t argc,const char **argv, const char* option){
    for(size_t i = 0; i < argc;++i){
        if(!strcmp(argv[i],option)){
            return true;
        }
    }
    return false;
}

bool is_flag(const char* string){
    if(*string == '-'){return true;}else{return false;}
}

bool find_query(size_t argc, const char ** argv,  char* query){
    for(size_t i = 0; i < argc;++i){
        if(query_check(argv[i])){
            strcpy(query,argv[i]);
            return true;
        }
    }
    strcpy(query,"/");
    return false;
}


bool find_property(size_t argc, const char ** argv,  char* properties){
    for(size_t i = 0; i < argc;++i){
        if(property_check_terminal(argv[i])){
            strcpy(properties,argv[i]);
            while(*properties!='\0'){
                if(*properties == '%'){
                    *properties='=';
                }
                properties++;
            }
            return true;
        }
    }
    return false;
} 

int is_builtin(const char* cmd)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(cmd, builtins[i].cmd) == 0) {
            return 1;
        }
    }
    return 0;
}


int run_builtin(const char *cmd, size_t argc, const char **argv, struct aos_datachan *out)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(cmd, builtins[i].cmd) == 0) {
            return builtins[i].handler(argc, argv, out);
        }
    }
    return 1;
}


int handle_echo(size_t argc, const char **argv, struct aos_datachan *out)
{
    for (size_t i = 0; i < argc; i++) {
        const char *arg = argv[i];
        aos_dc_send(out, strlen(arg), arg);
        if (i < argc - 1) {
            aos_dc_send(out, 1, " ");
        }
    }
    return 0;
}


int handle_clear(size_t argc, const char **argv, struct aos_datachan *out)
{
    const char code[] = "\033[H\033[2J\033[3J";
    aos_dc_send(out, sizeof code - 1, code);
    return 0;
}

extern char **environ;
int handle_env(size_t argc, const char **argv, struct aos_datachan *out)
{
    for (char **ev = environ; *ev != NULL; ev++) {
        for (char *var = *ev; *var != '\0'; var++) {
            char c = *var;
            if (c == '\033') {
                printf("^[");
            }
            else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    return 0;
}


int handle_time(size_t argc, const char **argv, struct aos_datachan *out)
{
    return 0;
}


int handle_pmlist(size_t argc, const char **argv, struct aos_datachan *out)
{
    errval_t err;
    size_t pid_count;
    domainid_t *pids;
    err = aos_rpc_process_get_all_pids(get_ns_rpc(), &pids, &pid_count);

    if (err_is_fail(err)) {
        printf("error querying nameserver\n");
        return 1;
    }

    printf(JF_BOLD "%-9s %-32s\n" JF_RESET, "PID", "Name");
    for (size_t i = 0; i < pid_count; i++) {
        char *pname;
        err = aos_rpc_process_get_name(get_ns_rpc(), pids[i], &pname);
        if (err_is_ok(err)) {
            printf("%-9"PRIuDOMAINID" %-32s\n", pids[i], pname);
        }
        else {
            printf("error querying process name\n");
        }

        free(pname);
    }
    free(pids);
    return 0;
}



int handle_nslist(size_t argc, const char **argv, struct aos_datachan *out){
    errval_t err;

    char  query[SERVER_NAME_SIZE];
    char  properties[2 * N_PROPERTIES * PROPERTY_MAX_SIZE];
    bool verbose = check_for_option(argc,argv,"-v");
    bool has_query = find_query(argc,argv,query);
    bool has_prop = find_property(argc,argv,properties);
    
    if((int) verbose + (int) has_query + (int) has_prop != argc){
        printf("\nInvalid nslist parameters, try: " JF_BOLD "nslist " JF_RESET "[-v] [prefix] [properties]\n" JF_RESET);
        return 1;
    }
    
    char* ret_string[N_PROPERTIES];
    size_t ret_size;
    debug_printf("query:%s\n",query);
    if(has_prop){
        err = nameservice_enumerate_with_props((char* ) query,properties,&ret_size,(char**) ret_string);
    }else{

        err = nameservice_enumerate((char*)query,&ret_size,(char**)ret_string);
    }

    if(err_is_fail(err) ){
        printf("error querying nameserver\n");
        DEBUG_ERR(err,"");
        return 1;
    }

    printf(JF_BOLD YEL " %-32s" RESET "     # %-9"PRIuDOMAINID"\n\n"  JF_RESET, "Servers",ret_size);

    for(int i = 0;i < ret_size;++i){
        printf("\n  * %-32s\n",ret_string[i]);
        if(verbose){
            char * props;
            err = nameservice_get_props(ret_string[i],&props);
            printf(CYN "     %-32s\n"RESET,props);
            free(props);
        }        
    }



    return 0;

}




int handle_nslookup(size_t argc, const char **argv, struct aos_datachan *out){
    errval_t err;
    domainid_t server_pid;
    if(argc != 1 || !name_check(argv[0])){
        printf("Invalid parameters for nslookup, try: " JF_BOLD "nslookup" JF_RESET " [name]");
        return 1;
    }
    err = nameservice_get_pid(argv[0],&server_pid);
    if(err_is_fail(err)){
        printf("Server name " JF_BOLD "%s" JF_RESET " not online.",argv[0]);
        return 1;
    }
    printf(JF_BOLD "\nPID: %d\n" JF_RESET);
    return 0;
}