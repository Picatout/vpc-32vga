
#include "syscall.h"
#include "../graphics.h"


int __attribute__((address(0x9d000000))) syscall (service_t service, arg_t *arg_list){
    unsigned int val=-1;
    switch (service){
        case PUTCHAR:
            put_char(arg_list[0].dev,arg_list[1].c);
            break;
        case GETCHAR:
            val=get_key(arg_list[0].dev);
            break;
        case WAITCHAR:
            val=wait_key(arg_list[0].dev);
            break;
        case PRINT:
            print(arg_list[0].dev,arg_list[1].cstr);
            break;
        case PSET:
            putPixel(arg_list[0].i,arg_list[1].i,arg_list[2].u);
            break;
    }
    return val;
}


//void systest(){
//    arg_t arg[4];
//    arg[0].dev=0;
//    arg[1].c='A';
//    syscall(PUTCHAR,arg);
//    arg[0].i=240;
//    arg[1].i=120;
//    arg[2].i=15;
//    syscall(PSET,arg);
//}
