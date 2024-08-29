#include "tee_client_api.h"

#ifndef TA_ZBOUNCER_H
#define TA_ZBOUNCER_H

#define ZBOUNCER_TA_UUID \
	{ 0x9e9e7bb0, 0x02b3, 0x4169, \
		{ 0x8e, 0x57, 0x3d, 0x1f, 0x88, 0x1e, 0x28, 0xe8} }

#define TA_ZBOUNCER_CMD_ENTRY		0
#define TA_ZBOUNCER_CMD_EXIT		1
#define TA_ZBOUNCER_CMD_DUMP		2

#define TA_ZBOUNCER_USE				5

void zbouncer_destroy();
void zbouncer_violation();
void zbouncer_entry(char* v1);
void zbouncer_exit(uint32_t* v1);
void zbouncer_init(char* v1);

#endif


