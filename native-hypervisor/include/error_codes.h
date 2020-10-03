#ifndef __ERROR_CODES_H_
#define __ERROR_CODES_H_

#include <types.h>


#define STATUS QWORD
#define STATUS_SUCCESS               0
#define STATUS_FAILURE               1
#define STATUS_RSDP_NOT_FOUND        2
#define STATUS_RSDP_INVALID_CHECKSUM 3
#define STATUS_NO_MEM_AVAILABLE      4
#define STATUS_RSDT_INVALID_CHECKSUM 5
#define STATUS_APIC_NOT_FOUND        6
#define STATUS_NO_CORES_FOUND        7
#define STATUS_VMLAUNCH_FAILED       8
#define STATUS_MEMORY_NOT_ALIGNED    9
#define STATUS_ACCESS_TO_HIDDEN_BASE 10
#define STATUS_INVALID_ACCESS_RIGHTS 11

#endif