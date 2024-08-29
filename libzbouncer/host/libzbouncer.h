TEEC_Result res;
TEEC_Context ctx;
TEEC_Session sess;
TEEC_Operation op;
TEEC_SharedMemory shm = {0};
TEEC_UUID uuid = ZBOUNCER_TA_UUID;
uint32_t err_origin;

void zinit();
void zexit();
void zbouncer_collect_alloca(uint32_t def, char* addr);
void zbouncer_collect_alloca_dyn(uint32_t def, char* addr, int size);
void zbouncer_use(uint32_t use, char* addr);
void zbouncer_iuse(uint32_t use, uint32_t def, char* addr);
void zbouncer_luse(uint32_t use, char* addr, uint32_t size_of_write);
void zbouncer_iluse(uint32_t use, uint32_t def, char* addr, uint32_t size_of_write);
uint32_t getEndOfObject(char *ptr);
void zbouncer_flush_cache();

typedef struct map_proc {
    uint32_t start;
    uint32_t end;
    unsigned char r;
    unsigned char w;
    unsigned char e;
    unsigned char s;
    uint32_t offset;
    uint32_t dev0;
    uint32_t dev1;
    uint32_t inode;
    char name[256];
} map_proc_t;
