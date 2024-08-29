#ifndef TA_ZBOUNCER_H
#define TA_ZBOUNCER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define ZBOUNCER_TA_UUID \
	{ 0x9e9e7bb0, 0x02b3, 0x4169, \
		{ 0x8e, 0x57, 0x3d, 0x1f, 0x88, 0x1e, 0x28, 0xe8} }

#define TA_ZBOUNCER_DEF				4
#define TA_ZBOUNCER_USE				5
#define TA_ZBOUNCER_CMD_GOTVALUE	6
#define TA_ZBOUNCER_IUSE			7
#define TA_ZBOUNCER_DDEF			8
#define TA_ZBOUNCER_FUNC_CTR		9
#define TA_ZBOUNCER_TIME_GLOBAL		10
#define TA_ZBOUNCER_FLUSH_CACHE		11

#endif /*TA_ZBOUNCER_H*/
