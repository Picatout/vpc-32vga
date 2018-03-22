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
    sprintf(fmt,"Fatal error at address: %0x\n",epc);
    _prt(fmt);
    switch (cause){
        case SYS:
            _prt("opcode syscall invoked\n");
            break;
        case INTR:
            _prt("unmanaged interrupt\n");
            break;
        case ADEL:
            _prt("Address error exception (load or instruction fetch)\n");
            break;
        case ADES:
            _prt("Address error exception (store)\n");
            break;
        case IBE:
            _prt("Bus error exception (instruction fetch)\n");
            break;
        case DBE:
            _prt("Bus error exception (data reference: load or store)\n");
            break;
        case BP:
            _prt("Breakpoint exception\n");
            break;
        case RI:
            _prt("Reserved instruction exception\n");
            break;
        case CPU:
            _prt("Coprocessor Unusable exception\n");
            break;
        case OVF:
            _prt("Arithmetic Overflow exception\n");
            break;
        case TRAP:
            _prt("Trap exception\n");
            break;
        default:
            _prt("unknown exception\n");
    }//switch
    _prt("<CTRL>-<ALT>-<DEL> to reboot.\n");
    while (1){
        power_led(PLED_OFF);
        delay_ms(500);
        power_led(PLED_ON);
        delay_ms(500);
    }//while
}


