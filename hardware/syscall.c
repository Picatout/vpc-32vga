
#include "syscall.h"



int __attribute__((address(0x9d000000))) syscall (service_t service, arg_t *arg_list){
    unsigned int val=-1;
    switch (service){
        case PUTCHAR:
            put_char(arg_list[0].i,arg_list[1].c);
            break;
        case GETCHAR:
            val=get_key(arg_list[0].i);
            break;
        case WAITCHAR:
            val=wait_key(arg_list[0].i);
            break;
        case PRINT:
            print(arg_list[0].i,arg_list[1].cstr);
            break;
    }
    return val;
}



