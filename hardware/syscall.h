/* 
 * File:   exception.h
 * Author: jacques
 *
 * Created on 27 janvier 2018, 22:08
 */

#ifndef EXCEPTION_H
#define	EXCEPTION_H

#ifdef	__cplusplus
extern "C" {
#endif
//#include "../console.h"
//#include "../graphics.h"
//#include "../reader.h"
//#include "rtcc/rtcc.h"
#include "serial_comm/serial_comm.h"
#include "sound/sound.h"
#include "spiram/spiram.h"
#include "store/store_spi.h"
#include "tvout/vga.h"
#include "../console.h"    

typedef enum SERVICES{
        PUTCHAR=1,
        GETCHAR,
        WAITCHAR,
        PRINT,
        PRINTINT,
        PRINTHEX,
        PSET,
                
}service_t;

    
typedef union argument{
    int16_t i16;
    uint16_t u16;
    console_t dev;
    int i;
    unsigned int u;
    long l;
    unsigned long ul;
    char c;
    unsigned char uc;
    char *str;
    const char *cstr;
}arg_t;    

extern int __attribute__((address(0x9d000000))) syscall (service_t service, arg_t *arg_list); 
extern void systest();
#ifdef	__cplusplus
}
#endif

#endif	/* EXCEPTION_H */

