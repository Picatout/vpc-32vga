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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>
#include <p32xxxx.h>
#include "HardwareProfile.h"
#include "serial_comm/serial_comm.h"


void _general_exception_handler(){
#define _prt(s) ser_print(s);vga_print(s)

    unsigned cause,epc;
    char fmt[64];


    asm volatile("mfc0 %0,$13":"=r"(cause));
    cause>>=2;
    cause&=0x1f;
    epc= __builtin_mfc0(_CP0_EPC, _CP0_EPC_SELECT);
    asm volatile("mfc0 $k0, $12");
    asm volatile("andi $k0,0xfd");
    asm volatile("mtc0 $k0,$12");
    asm volatile("ei");
    asm volatile("ehb");
    sprintf(fmt,"Fatal error at address: %0x\r",epc);
    _prt(fmt);
    switch (cause){
        case SYS:
            _prt("opcode syscall invoked\r");
            break;
        case INTR:
            _prt("unmanaged interrupt\r");
            break;
        case ADEL:
            _prt("Address error exception (load or instruction fetch)\r");
            break;
        case ADES:
            _prt("Address error exception (store)\r");
            break;
        case IBE:
            _prt("Bus error exception (instruction fetch)\r");
            break;
        case DBE:
            _prt("Bus error exception (data reference: load or store)\r");
            break;
        case BP:
            _prt("Breakpoint exception\r");
            break;
        case RI:
            _prt("Reserved instruction exception\r");
            break;
        case CPU:
            _prt("Coprocessor Unusable exception\r");
            break;
        case OVF:
            _prt("Arithmetic Overflow exception\r");
            break;
        case TRAP:
            _prt("Trap exception\r");
            break;
        default:
            _prt("unknown exception\r");
    }//switch
    _prt("<CTRL>-<ALT>-<DEL> to reboot.\r");
    while (1){
        power_led(PLED_OFF);
        delay_ms(500);
        power_led(PLED_ON);
        delay_ms(500);
    }//while
}


