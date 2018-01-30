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


typedef enum SERVICES{
        PUTCHAR=1,
        GETCHAR,
        WAITCHAR,
        PRINT,
    }service_t;

    
typedef union argument{
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
#ifdef	__cplusplus
}
#endif

#endif	/* EXCEPTION_H */

