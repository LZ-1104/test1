#ifdef __STATUS_C__
#error "This is a private header file, it cannot be included by user files."
#endif


#ifndef __STATUS_H
#define __STATUS_H


#include "stdint.h"

#define STATUS_OK 0
#define STATUS_ERROR 1


typedef enum {
    DEVICE_STATUS_OK = 0,
    DEVICE_STATUS_ERROR = 1,
    DEVICE_STATUS_BUSY = 2,
    DEVICE_STATUS_TIMEOUT = 3
} status_t;


#endif /* __STATUS_H */