#include <stdlib.h>
#include <stdio.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <zbouncer_ta.h>

#define DYNAMIC_TRACE_SIZE 2048

typedef struct use{
	uint32_t def_addr;
	uint32_t def;
	uint32_t def_size;

    uint32_t use_addr;
	uint32_t use_id;

	uint32_t i_def;
	uint32_t i_use;
	uint32_t i_addr;
}use_t;

typedef struct timetrace{
	uint8_t func;
	uint32_t start;
	uint32_t stop;
} timetrace_t;


use_t dynamic_trace[DYNAMIC_TRACE_SIZE]={0};
int timetrace_count = 0;
int trace_id;
int random = 0;
static use_t *mshared = NULL;

uint8_t enable_time_tracing = 0x0;
uint32_t global_start;
uint32_t global_end;

uint32_t func_ctr[15] = {0};

// Get millis
uint32_t read_time(){
	TEE_Time t;
	TEE_GetSystemTime(&t);
	uint32_t ret = t.millis + t.seconds * 1000;
	return ret;
}

TEE_Result TA_CreateEntryPoint(void){
	//IMSG("TA_CreateEntryPoint has been called started");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void){
	//IMSG("TA_DestroyEntryPoint has been called");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx){
	
	global_start = read_time();

	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	//IMSG("TA_OpenSessionEntryPoint has been called");

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	(void)&sess_ctx;

	if(params[0].value.a == 0x00ff00ff){
		enable_time_tracing = 0xff;
	}

	trace_id = 0;
	//IMSG("[SW] init, enable_time_tracing=%x", enable_time_tracing);
	
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx){
	//IMSG("TA_CloseSessionEntryPoint hash been called");
	
	(void)&sess_ctx;
}

static TEE_Result zbouncer_def(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def = params[0].value.a;
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_addr = params[0].value.b;
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_size = -1;
	trace_id++;

	IMSG("DEF | %05d | %08x\n", params[0].value.a, params[0].value.b);

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_ddef(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	
	uint32_t *data = params[0].memref.buffer;
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def = (uint32_t)data[0];
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_addr = (uint32_t)data[1];
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_size = data[2];
	trace_id++;
	
	IMSG("DDEF | %05d | %08x | %05d\n", data[0], data[1], data[2]);

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_use(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS; 

	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].use_id = params[0].value.a;
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].use_addr = params[0].value.b;
	trace_id++;

	IMSG("USE | %05d | %08x\n", params[0].value.a, params[0].value.b);

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_iuse(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	
	uint32_t *data = params[0].memref.buffer;
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].i_use = (uint32_t)data[0];
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].i_def = (uint32_t)data[1];
	dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].i_addr = data[2];
	
	IMSG("IUSE | %05d | %05d | %08x\n", dynamic_trace[trace_id].i_def, dynamic_trace[trace_id].i_use, dynamic_trace[trace_id].i_addr);
	trace_id++;

	return TEE_SUCCESS;
}


static TEE_Result copy_trace(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	TEE_MemMove(params[0].memref.buffer, dynamic_trace, 1024*sizeof(use_t));
	for(int k = 0; k < 1024; k++)
		//IMSG("DEF | %05d | %05d \n", dynamic_trace[k].def_addr, dynamic_trace[k].def);

	return TEE_SUCCESS;
}


static TEE_Result zbouncer_time_global(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						    TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types){
		IMSG("Parameeters not correct");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	global_end = read_time();

	params[0].value.a = global_start;
	params[0].value.b = global_end;

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_func_ctr(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						    TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types){
		IMSG("Parameeters not correct");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	params[0].value.a = (func_ctr[TA_ZBOUNCER_USE] << 16) | (func_ctr[TA_ZBOUNCER_DEF] & 0x0000ffff);  // use, def
	params[0].value.b = (func_ctr[TA_ZBOUNCER_IUSE] << 16) | (func_ctr[TA_ZBOUNCER_DDEF] & 0x0000ffff); // iuse, ddef

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_flush_cache(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	
	use_t *cache = params[0].memref.buffer;

	IMSG("Copy cache_shared OK addr: 0x%x\n", cache);
	for( int i = 0; i <= 128; i++){
		use_t use_entry = cache[i];
		IMSG("[%d] use_t >>> def_addr=%u, def=%u, def_size=%u, use_addr=%u, use_id=%u, i_use_def=%u, i_use_use=%u, i_use_addr=%u\n",
			i, use_entry.def_addr, use_entry.def, use_entry.def_size, use_entry.use_addr, use_entry.use_id,
			use_entry.i_def, use_entry.i_use, use_entry.i_addr);
	}
	IMSG("End cache\n");

	for(int i = 0; ; i++){
		use_t use_entry = cache[i];
		IMSG("cache i: %d", i);

		if(use_entry.def != 0 && use_entry.def_size == 0){
			dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def = use_entry.def;
			dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_addr = use_entry.def_addr;
			dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_size = -1;
			trace_id++;
			//func_ctr[TA_ZBOUNCER_DEF] = func_ctr[TA_ZBOUNCER_DEF] + 1;
			//IMSG("flushed def");
			
		}else{
			if(use_entry.use_id != 0){
				dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].use_id = use_entry.use_id;
				dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].use_addr = use_entry.use_addr;
				trace_id++;
				//func_ctr[TA_ZBOUNCER_USE] = func_ctr[TA_ZBOUNCER_USE] + 1;
				//IMSG("flushed use");

			}else{
				if(use_entry.i_use != 0){
					dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].i_use = use_entry.i_use;
					dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].i_def = use_entry.i_def;
					dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].i_addr = use_entry.i_addr;
					trace_id++;
					//func_ctr[TA_ZBOUNCER_IUSE] = func_ctr[TA_ZBOUNCER_IUSE] + 1;
					//IMSG("flushed iuse");

				}else{
					if(use_entry.def_size != 0){
						dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def = use_entry.def;
						dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_addr = use_entry.def_addr;
						dynamic_trace[trace_id % DYNAMIC_TRACE_SIZE].def_size = use_entry.def_size;
						trace_id++;
						//func_ctr[TA_ZBOUNCER_DDEF] = func_ctr[TA_ZBOUNCER_DDEF] + 1;
						//IMSG("flushed ddef");
					}else{
						//IMSG("flushed exit");
						break;
					}
				}
			}
		}
	}

	return TEE_SUCCESS;
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id, uint32_t param_types, TEE_Param params[4]){
	(void)&sess_ctx;
	TEE_Result error;

	//IMSG("switch param: %d", cmd_id);
	switch (cmd_id) {
		case TA_ZBOUNCER_USE:
			error = zbouncer_use(param_types, params);
			//IMSG("[NON CACHE] use");
			break;
		case TA_ZBOUNCER_DEF:
			error = zbouncer_def(param_types, params);
			//IMSG("[NON CACHE] def");
			break;
		case TA_ZBOUNCER_CMD_GOTVALUE:
			error = copy_trace(param_types, params);
			break;
		case TA_ZBOUNCER_IUSE:
			error = zbouncer_iuse(param_types, params);
			//IMSG("[NON CACHE] iuse");
			break;
		case TA_ZBOUNCER_DDEF:
			error = zbouncer_ddef(param_types,params);
			//IMSG("[NON CACHE] ddef");
			break;
		case TA_ZBOUNCER_TIME_GLOBAL:
			error = zbouncer_time_global(param_types,params);
			break;
		case TA_ZBOUNCER_FUNC_CTR:
			error = zbouncer_func_ctr(param_types,params);
			break;
		case TA_ZBOUNCER_FLUSH_CACHE:
			error = zbouncer_flush_cache(param_types,params);
			func_ctr[TA_ZBOUNCER_DEF] = func_ctr[TA_ZBOUNCER_DEF] + 1;
			break;
		default:
			error = TEE_ERROR_BAD_PARAMETERS;
			break;
	}

	//func_ctr[cmd_id] = func_ctr[cmd_id] + 1;
	//IMSG("func_ctr[%d]: %u", cmd_id, func_ctr[cmd_id]);

	return error;
}
