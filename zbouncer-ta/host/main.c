#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* For the UUID (found in the TA's h-file(s)) */
#include <zbouncer_ta.h>

TEEC_Result res;
TEEC_Context ctx;
TEEC_Session sess;
TEEC_Operation op;
TEEC_UUID uuid = ZBOUNCER_TA_UUID;
uint32_t err_origin;

// void zbouncer_init(){
// 	res = TEEC_InitializeContext(NULL, &ctx);
// 	if (res != TEEC_SUCCESS)
// 		errx(1, "TEEC_InitializeContext failed with code 0x%x\n", res);

// 	res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
// 	if (res != TEEC_SUCCESS)
// 		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x\n", res, err_origin);

// 	memset(&op, 0, sizeof(op));
// }

// void zbouncer_destroy(){
// 	TEEC_CloseSession(&sess);
// 	TEEC_FinalizeContext(&ctx);
// 	printf("finish\n");
// 	exit(1);
// }

// void zbouncer_violation(){
// 	zbouncer_destroy(); // only for now
// }

// void zbouncer_entry(uint32_t value){
// 	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
// 	op.params[0].value.a = value;

// 	res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_CMD_ENTRY, &op, &err_origin);
// 	if (res != TEEC_SUCCESS)
// 		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
// 	printf("Entry executed from SW\n");
// }

// void zbouncer_exit(uint32_t value){
// 	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
// 	op.params[0].value.a = value;

// 	res = TEEC_InvokeCommand(&sess, TA_ZBOUNCER_CMD_EXIT, &op, &err_origin);
// 	printf("Exit executed from SW\n");
// 	if(res != TEEC_SUCCESS){	
// 		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n", res, err_origin);
// 	}else{
// 		if(op.params[0].value.b == 0xFFFFFFFF){
// 			printf("[Violation NW] halt program b=%x\n", op.params[0].value.b);
// 			zbouncer_violation();
// 		}else{
// 			printf("[OK] b=%x\n", op.params[0].value.b);
// 		}
// 	}
// }

int main(void){

	printf("##### MY ZBOUNCER #####\n");

	// zbouncer_init();

	// zbouncer_entry(3);
	// zbouncer_exit(3);

	// zbouncer_entry(5);

	// 	zbouncer_entry(7);
	// 	zbouncer_exit(7);

	// 	zbouncer_entry(10);

	// 		zbouncer_entry(2);
	// 			zbouncer_entry(9);
	// 			zbouncer_exit(9);
	// 		zbouncer_exit(2);

	// 		zbouncer_entry(11);
	// 			zbouncer_entry(12);
	// 			zbouncer_exit(12);
	// 		zbouncer_exit(11);

	// 	zbouncer_exit(9); // violation is 9 but should be 10

	// zbouncer_exit(5);

	// zbouncer_destroy();

	return 0;
}
