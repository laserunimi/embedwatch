#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <libzbouncer_ta.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct Node{
    uint32_t lbr;
    struct Node *next;
}node_t;

typedef struct Stack{
    node_t *top;
    int size; 
}stack_t;

stack_t *calls = NULL;

void stack_init(stack_t *stack);
void stack_push(stack_t *stack, uint32_t data);
node_t *stack_pop(stack_t *stack);
node_t *stack_head(stack_t *stack);
void print_stack(stack_t *stack);
void violation(uint32_t ra1, uint32_t ra2);

void stack_init(stack_t *stack){
	stack->top = NULL;
	stack->size = 0;
}

void stack_push(stack_t *stack, uint32_t data){
	node_t *node = (node_t *)malloc(sizeof(node_t));

	if(stack->top == NULL)
    	node->next = NULL;
	else
		node->next = stack->top;
    
	node->lbr = data;
	stack->top = node;
	stack->size++;
}

node_t *stack_pop(stack_t *stack){

	 if(stack->size != 0){
        node_t *tmp = stack->top;
        stack->top = stack->top->next;
        tmp->next = NULL;
        tmp = NULL;
        free(tmp);
        stack->size--;
    }
}

node_t *stack_head(stack_t *stack){
	return stack->top;
}

void print_stack(stack_t *stack){
	IMSG("--- call chain: %d ---", stack->size);
	if(stack != NULL){
		if(stack->top != NULL){
			node_t *n = stack->top;
			while(n != NULL){
				IMSG("0x%08x", n->lbr);
				n = n->next;
			}
		}else{
			IMSG("Stack empty");
		}
	}else{
		IMSG("Stack NULL");
	}
	IMSG("----------------------");
}

void violation(uint32_t right, uint32_t wrong){
	IMSG("***********************");
	IMSG("****** VIOLATION ******");
	IMSG("***********************");
	IMSG("RA mismatch, the function is trying to return to 0x%x instead of 0x%x", wrong, right);
	print_stack(calls);
	// dump host memory
	// send payload outside
	// halt the program

	IMSG("Halt");
}

TEE_Result TA_CreateEntryPoint(void){
	DMSG("TA_CreateEntryPoint has been called");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void){
	DMSG("TA_DestroyEntryPoint has been called");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx){

	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("TA_OpenSessionEntryPoint has been called");

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	(void)&params;
	(void)&sess_ctx;

	calls = (stack_t *)malloc(sizeof(stack_t));
	stack_init(calls);

	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx){
	(void)&sess_ctx;
	IMSG("TA_CloseSessionEntryPoint hash been called\n");
}

static TEE_Result zbouncer_entry(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	stack_push(calls, params[0].value.a);
	IMSG("[Entry] Get entry point: %x from NW", params[0].value.a);

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_exit(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	node_t *head = stack_head(calls);
	if(head == NULL){
		IMSG("head is NULL");
		return TEE_ERROR_SECURITY;
	}

	uint32_t ra = head->lbr;

	if(params[0].value.a == head->lbr){
		if(stack_pop(calls) != NULL){
			IMSG("[Exit] Call-returned securely | on-stack-ra: 0x%x | stored-ra: 0x%x", params[0].value.a, ra);
		}else{
			IMSG("[Violation] Has been triggered a return but the stack is empty");
			violation(head->lbr, params[0].value.a);
			params[0].value.b = 0xffffffff;
		}
	}else{
		IMSG("[Violation] An invalid exit path has been triggered");
		violation(head->lbr, params[0].value.a);
		params[0].value.b = 0xffffffff;
	}

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_dump(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("[Dump] reading maps of pid: %d", params[0].value.b);

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_loop_exit(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("[LOOP] i=0x%x i=%d", params[0].value.a, params[0].value.a);

	return TEE_SUCCESS;
}

static TEE_Result zbouncer_wmemcpy(uint32_t param_types, TEE_Param params[4]){
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("[wmemcpy]");

	return TEE_SUCCESS;
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id, uint32_t param_types, TEE_Param params[4]){
	(void)&sess_ctx;

	IMSG("strange switch: %d", cmd_id);

	switch (cmd_id) {
	case TA_ZBOUNCER_CMD_ENTRY:
		return zbouncer_entry(param_types, params);
	case TA_ZBOUNCER_CMD_EXIT:
		return zbouncer_exit(param_types, params);
	case TA_ZBOUNCER_CMD_DUMP:
		return zbouncer_dump(param_types, params);
	case TA_ZBOUNCER_CMD_LOOP_EXIT:
		return zbouncer_loop_exit(param_types, params);
	case TA_ZBOUNCER_CMD_WMEMCPY:
		return zbouncer_wmemcpy(param_types, params);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
