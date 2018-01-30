
#include <setjmp.h>
#include <p32xxxx.h>
#include "HardwareProfile.h"
#include "../console.h"

void _general_exception_handler(){
    unsigned cause,epc;
#define _nl()  put_char(comm_channel,'\r')    
#define _prt(n) print(comm_channel,n)
    asm volatile("mfc0 %0,$13":"=r"(cause));
    cause>>=2;
    cause&=0x1f;
    epc= __builtin_mfc0(_CP0_EPC, _CP0_EPC_SELECT);
    asm volatile("mfc0 $k0, $12");
    asm volatile("andi $k0,0xfd");
    asm volatile("mtc0 $k0,$12");
    asm volatile("ei");
    asm volatile("ehb");

    _prt("Fatal error at address: 0x");
    print_hex(comm_channel,epc,0);
    _nl();
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
    _prt("<CTRL>-<ALT>-<DEL> to reboot.");
    while (1){
        power_led(PLED_OFF);
        delay_ms(500);
        power_led(PLED_ON);
        delay_ms(500);
    }//while
}


