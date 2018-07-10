/*
* Copyright 2013,2018 Jacques Deschênes
* This file is part of VPC-32v.
*
*     VPC-32v is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     VPC-32v is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with VPC-32v.  If not, see <http://www.gnu.org/licenses/>.
*/

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
