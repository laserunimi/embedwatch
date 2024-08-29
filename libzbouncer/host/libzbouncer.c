//#include <err.h>
//#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <sys/types.h>
#include <unistd.h>
//#include <stdarg.h>
//#include <errno.h>

//#include <sys/socket.h>
#include <arpa/inet.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* For the UUID (found in the TA's h-file(s)) */
#include "libzbouncer_ta.h"

#include "libzbouncer.h"

#define TRACE_SIZE 2048

extern int errno ;
int cache_enabled = 1;

static TEEC_SharedMemory sharedExtractor = {0};
static TEEC_SharedMemory sharedTmp = {0};

pid_t parent_pid; 
pid_t child;

typedef struct use{
	uint32_t def_addr;
	uint32_t def;
    uint32_t def_size;

    uint32_t use_addr;
	uint32_t use_id;

	uint32_t i_use_def;
	uint32_t i_use_use;
	uint32_t i_use_addr;
}use_t;

enum ENTRY_TYPE {
    def = 0,
    use,
    libuse,
    iuse,
    ilibuse,
    dalloca,
    novalue
};

int res_open;
char *line = NULL;
size_t len = 0, rd;
FILE *fp = NULL;
use_t *cache;
int cache_last_index = -1;

void contextsw_test(){
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = 0;
    op.params[0].value.b = 0;

    res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_DEF, &op, &err_origin);
    //if (res != TEEC_SUCCESS)
    //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
}

/*
    ****************** Handle Caching *************************
*/

void zbouncer_update_entry(enum ENTRY_TYPE type, int index, int id, char *addr, int def_id, int def_size, int size_of_write){
    use_t *use_entry = &cache[index];
    switch (type) {
        case def:
            use_entry->def = id;
            use_entry->def_addr = addr;
            break;
        
        case use:
            use_entry->use_id = id;
            use_entry->use_addr = addr;
            break;

        case libuse:
            use_entry->use_id = id;
            int size;
            if(size_of_write == 0xdeadbeef){
                size = getEndOfObject(addr);
            }else{
                size = addr + size_of_write;
            }
            use_entry->use_addr = size;
            break;

        case iuse:
            use_entry->i_use_use = id;
            use_entry->i_use_def = def_id;
            use_entry->i_use_addr = addr;
            break;

        case ilibuse:
            use_entry->i_use_use = id;
            use_entry->i_use_def = def_id;
            if(size_of_write == 0xdeadbeef){
                size = getEndOfObject(addr);
            }else{
                size = addr + size_of_write;
            }
            use_entry->i_use_addr = size;
            break;

        case dalloca:
            use_entry->def = id;
            use_entry->def_addr = addr;
            use_entry->def_size = def_size;
            break;
    }
}

void zbouncer_collect_entry(enum ENTRY_TYPE type, int id, char *addr, int def_id, int def_size, int size_of_write) {

    if(cache_enabled == 1 ){
        for(int i = 0; i <= cache_last_index; i++){
            use_t use_entry = cache[i];
            
            if(use_entry.def == id && (!(type == iuse || type == ilibuse) || use_entry.def == def_id)){
                zbouncer_update_entry(type,i,id,addr,def_id,def_size,size_of_write);
                return;
            }
            
        }

        zbouncer_update_entry(type, ++cache_last_index, id, addr, def_id, def_size, size_of_write);
        
        if(cache_last_index > 120){
            zbouncer_flush_cache();
        }

    }else{

        switch (type){
            case use:
                zbouncer_use(id, addr);
                break;
            case libuse:
                zbouncer_luse(id, addr, size_of_write);
                break;
            case iuse:
                zbouncer_iuse(id, def_id, addr);
                break;
            case ilibuse:
                zbouncer_iluse(id, def_id, addr, size_of_write);
                break;
            case dalloca:
                zbouncer_collect_alloca_dyn(id, addr, def_size);
                break;
            case def:
                zbouncer_collect_alloca(id, addr);
                break;
            
            default:
                break;
        }
    }

}

void zbouncer_flush_cache(){

    if(cache_enabled == 1){

        use_t *cache_shared = (use_t *)malloc(128 * sizeof(use_t));
        memcpy(cache_shared, cache, 128 * sizeof(use_t));

        sharedTmp.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
        sharedTmp.buffer = cache_shared;
        sharedTmp.size = 128 * sizeof(use_t);

        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
        op.params[0].memref.parent = &sharedTmp;
        op.params[0].memref.size = sharedTmp.size;

        
        if (TEEC_RegisterSharedMemory(&ctx, &sharedTmp) != TEEC_SUCCESS){
            free(sharedTmp.buffer);
            //printf("[HOST] Error RegisterSharedMemory\n");
        }

        //printf("Flushing the cache with last_index=%d\n", cache_last_index);
        res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_FLUSH_CACHE, &op, &err_origin);
        //if (res != TEEC_SUCCESS)
        //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
        
        TEEC_ReleaseSharedMemory(&sharedTmp);

    }else{
        for( int i = 0; i <= cache_last_index; i++){
            use_t use_entry = cache[i];

            if(use_entry.def != 0 && use_entry.def_size == 0)
                zbouncer_collect_alloca(use_entry.def,use_entry.def_addr);
                
            else if(use_entry.use_id != 0)
                zbouncer_use(use_entry.use_id,use_entry.use_addr);

            else if(use_entry.i_use_use != 0)
                zbouncer_iuse(use_entry.i_use_use, use_entry.i_use_def, use_entry.i_use_addr);

            else if(use_entry.def_size != 0)
                zbouncer_collect_alloca_dyn(use_entry.def,use_entry.def_addr,use_entry.def_size);
            
        }
    }
    
    cache_last_index = -1;
    memset(cache, 0, 128 * sizeof(use));
}

/*

void collect_entry(ENTRY_TYPE type, int id, char *addr, int def_id, int size){
*/


/*
    ****************** START ZBouncer Trace sending Functions *************************
*/



/*
    * Open a socket to send zb trace out
*/

int open_socket(char *addr, uint16_t port){
    //printf("[!] Open Socket\n");
    //printf("[*] Address: %s\n", addr);
    //printf("[*] Port: %u\n", port);
    int sock = 0;
    struct sockaddr_in serv_addr;
    //char buffer[1024] = {0};
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        //printf("\n Socket creation error \n");
        return -1;
    }
   
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_port = htons(port);
       
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(PF_INET, addr, &serv_addr.sin_addr)<=0){
        //printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        //printf("\nConnection Failed \n");
        return -1;
    }
    //send(sock,trace,1024,0);
    //printf("Hello message sent\n");
    //valread = read(sock,buffer,1024);
    //printf("%s\n",buffer );

    //close(sock);
    return sock;
}


/*
    * Function to send trace to verifier via socket 
*/

void send_trace_to_verifier(){

    char infoline[] = "INFO\n";

    if((res_open =  open_socket("172.17.0.1", 8080)) < 0){
            //printf("[*] Error in opening Socket\n");
            exit(0);
    }
    if ( (fp = fopen("trace.txt", "r")) == NULL){
        //perror ("The following error occurred");
        //printf("Value of errno: %d\n", errno);
    }else{
        while((rd = getline(&line, &len, fp)) != -1){
            send(res_open, line, strlen(line) ,0);
        }
        free(line);
        fclose(fp); 
        send(res_open, infoline, strlen(infoline) ,0);  
    }
    close(res_open);
}


/*
    * Function to write ztrace from shared mem to a file
*/


void  write_trace_to_file(){
    if ( (fp = fopen("trace.txt", "w")) == NULL){
        //perror ("The following error occurred");
        //printf("Value of errno: %d\n", errno);
    }else{
        for(int i = 0; i < TRACE_SIZE ; i++){
            if(((use_t*)sharedExtractor.buffer)[i].def != 0 && ((use_t*)sharedExtractor.buffer)[i].def_size == -1){
                //printf("DEF | %05u | %08x\n",  ((use_t*)sharedExtractor.buffer)[i].def, ((use_t*)sharedExtractor.buffer)[i].def_addr);
                fprintf(fp,"DEF | %05u | %08x\n",  ((use_t*)sharedExtractor.buffer)[i].def, ((use_t*)sharedExtractor.buffer)[i].def_addr);
            }
            if(((use_t*)sharedExtractor.buffer)[i].def != 0 && ((use_t*)sharedExtractor.buffer)[i].def_size != -1)
                fprintf(fp, "DDEF | %05u | %08x | %05u\n",  ((use_t*)sharedExtractor.buffer)[i].def, ((use_t*)sharedExtractor.buffer)[i].def_addr, ((use_t*)sharedExtractor.buffer)[i].def_size);

            if(((use_t*)sharedExtractor.buffer)[i].use_id != 0)
                fprintf(fp, "USE | %05u | %08x\n", ((use_t*)sharedExtractor.buffer)[i].use_id, ((use_t*)sharedExtractor.buffer)[i].use_addr);
            
            if(((use_t*)sharedExtractor.buffer)[i].i_use_def != 0 )
                fprintf(fp, "I-USE | %05d | %05d | %08x\n", ((use_t*)sharedExtractor.buffer)[i].i_use_def, ((use_t*)sharedExtractor.buffer)[i].i_use_use, ((use_t*)sharedExtractor.buffer)[i].i_use_addr);
        }
        fclose(fp);
        send_trace_to_verifier();
    } 
}

void z_extractor(pid_t cpid){
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    sharedExtractor.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    sharedExtractor.buffer = calloc(TRACE_SIZE, sizeof(use_t));
    sharedExtractor.size = TRACE_SIZE * sizeof(use_t);

    op.params[0].memref.parent = &sharedExtractor;
    op.params[0].memref.size = sharedExtractor.size;

    //register a memory through the GlobalPlatfor
    //Client API function TEEC_RegisterSharedMemory
    
    if (TEEC_RegisterSharedMemory(&ctx, &sharedExtractor) != TEEC_SUCCESS){
        free(sharedExtractor.buffer);
        //printf("[HOST] Error RegisterSharedMemory\n");
    }
        
    //printf("memory allocated : %p\n", sharedExtractor);
    while(1){
        //printf("I am in the loop, my father is %d ;=)\n", cpid);
        sleep(5);
        cpid = getppid(); 
        if(cpid != parent_pid){
            res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_CMD_GOTVALUE, &op, &err_origin);
            //if (res != TEEC_SUCCESS)
            //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
            // call ta side to get trace
            write_trace_to_file();
            //printf("[EXTRACTOR] I'm gonna die\n");
            TEEC_CloseSession(&sess);
            TEEC_FinalizeContext(&ctx);
            exit(0);
        }
        res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_CMD_GOTVALUE, &op, &err_origin);
        //if (res != TEEC_SUCCESS)
        //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
        // call ta side to get trace
        write_trace_to_file();
    }

    TEEC_ReleaseSharedMemory(&sharedExtractor);
}


/*

    * END ZBouncer Trace sending Functions

*/

void zexit(){

    //printf("[NW] zexit\n");
    zbouncer_flush_cache();

    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_TIME_GLOBAL, &op, &err_origin);
    //if (res != TEEC_SUCCESS)
    //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);

    //printf("[HOST] start: %u, end: %u\n", op.params[0].value.a, op.params[0].value.b);
    //printf("New elapsed: %um\n", op.params[0].value.b - op.params[0].value.a);

    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_FUNC_CTR, &op, &err_origin);
    //if (res != TEEC_SUCCESS)
    //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);

    //uint32_t use_def = op.params[0].value.a;
    //uint32_t iuse_ddef = op.params[0].value.b;
    //printf("total_counter:%u;%u;%u;%u\n", (use_def >> 16), (use_def & 0x0000ffff), (iuse_ddef >> 16), (iuse_ddef & 0x0000ffff));

}

void zinit(){

    res = TEEC_InitializeContext(NULL, &ctx);
    //if (res != TEEC_SUCCESS)
    //    errx(1, "TEEC_InitializeContext failed with code 0x%x\n", res);
    
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, &op, &err_origin);
    //if (res != TEEC_SUCCESS)
    //    errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x\n", res, err_origin);

    //printf("ZINIT\n\n\n");

    cache = calloc(sizeof(use_t), 128);

    parent_pid = getpid();
    child = fork();
    if (child == 0){
        //printf("I am the Child: %d with father %d\n", getpid(), getppid());
        z_extractor(parent_pid);
    }
    
}

/*
    * Function to collect Def 
*/

void zbouncer_collect_alloca(uint32_t def, char* addr){

    //printf("DEF %d\n\t- Address:%x\n",def,(int) addr);
    if(cache_enabled == 1){
        zbouncer_collect_entry(0, def, addr, 0, 0, 0); // 0 = def
    
    }else{
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
        op.params[0].value.a = def;
        op.params[0].value.b = (uint32_t)addr;

        res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_DEF, &op, &err_origin);
        //if (res != TEEC_SUCCESS)
        //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
    }
}

void zbouncer_collect_alloca_dyn(uint32_t def, char* addr, int size){

    //printf("DDEF %d\n\t- Address:%x\n\t- Size:%d",def,(int) addr,size);
    if(cache_enabled == 1){
        zbouncer_collect_entry(5, def, addr, 0, size, 0); // 0 = ddalloca >>> ddef

    }else{
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

        uint32_t *data = calloc(3, sizeof(uint32_t));
        data[0] = def;
        data[1] = (uint32_t) addr;
        data[2] = size;

        sharedTmp.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
        sharedTmp.buffer = data;
        sharedTmp.size = 3*sizeof(uint32_t);

        op.params[0].memref.parent = &sharedTmp;
        op.params[0].memref.size = sharedTmp.size;

        
        if (TEEC_RegisterSharedMemory(&ctx, &sharedTmp) != TEEC_SUCCESS){
            free(sharedTmp.buffer);
            //printf("[HOST] Error RegisterSharedMemory\n");
        }

        res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_DDEF, &op, &err_origin);
        //if (res != TEEC_SUCCESS)
        //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
        
        TEEC_ReleaseSharedMemory(&sharedTmp);
    }
}


/*
    * Function to collect Use 
*/
void zbouncer_use(uint32_t use, char* addr){
    //printf("USE %d\n\t- Address:%x\n",use,(int) addr);

    if(cache_enabled == 1){
        zbouncer_collect_entry(1, use, addr, 0, 0, 0); // 1 = use

    }else{
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
        op.params[0].value.a = use;
        op.params[0].value.b = (uint32_t) addr;

        res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_USE, &op, &err_origin);
        //if (res != TEEC_SUCCESS)
        //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
    }

}

uint32_t getEndOfObject(char *ptr){
  
  char c = *ptr;
  while(c != '\0'){
    ++ptr;
    c = *ptr;
  }

  return (uint32_t)ptr;
}

/*
    * Functions to collect Idirect use 
*/
void zbouncer_luse(uint32_t use, char* addr, uint32_t size_of_write){

    if(size_of_write == 0xdeadbeef){
        zbouncer_use(use, getEndOfObject(addr));
    }else{
        zbouncer_use(use, addr + size_of_write);
    }
}

void zbouncer_iuse(uint32_t use, uint32_t def, char* addr){

    if(cache_enabled == 1){
        zbouncer_collect_entry(3, use, addr, def, 0, 0); // 3 = iuse

    }else{
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

        uint32_t *data = calloc(3, sizeof(uint32_t));
        data[0] = use;
        data[1] = def;
        data[2] = (uint32_t) addr;

        sharedTmp.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
        sharedTmp.buffer = data;
        sharedTmp.size = 3*sizeof(uint32_t);

        op.params[0].memref.parent = &sharedTmp;
        op.params[0].memref.size = sharedTmp.size;

        
        if (TEEC_RegisterSharedMemory(&ctx, &sharedTmp) != TEEC_SUCCESS){
            free(sharedTmp.buffer);
            //printf("[HOST] Error RegisterSharedMemory\n");
        }

        //printf("SENDING IUSE: %05d | %05d | %08x\n", ((uint32_t *) shared.buffer)[0], ((uint32_t *) shared.buffer)[1], ((uint32_t *) shared.buffer)[2]);
        res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_IUSE, &op, &err_origin);
        //if (res != TEEC_SUCCESS)
        //    errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
        
        TEEC_ReleaseSharedMemory(&sharedTmp);
    }
}

void zbouncer_iluse(uint32_t use, uint32_t def, char* addr, uint32_t size_of_write){
     if(size_of_write == 0xdeadbeef){
        zbouncer_iuse(use, def, getEndOfObject(addr));
    }else{
        zbouncer_iuse(use, def, addr + size_of_write);
    }
}
