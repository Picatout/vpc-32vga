/*
* Copyright 2013,2017,2018 Jacques Deschênes
* This file is part of VPC-32vga.
*
*     VPC-32vga is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     VPC-32vga is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with VPC-32vga.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
 * Name: keyboard.c
 * Author: Jacques Deschênes
 * Description:  PS/2 keyboard low level interface
 * REF: http://www.computer-engineering.org/ps2protocol/
 * Date: 2013-08-26
 * rev: 2017-07-30
 */
#include <p32xxxx.h>
#include <plib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../HardwareProfile.h"
#include "keyboard.h"
#include "../serial_comm/serial_comm.h"

// using circular queues for received scan codes and translated codes.
#define KBD_QUEUE_SIZE (32)
static  unsigned char kbd_queue[KBD_QUEUE_SIZE]; // keyboard translated codes queue
volatile static unsigned char kbd_head=0, kbd_tail=0; // kbd_queue head and tail pointer

// initialize UART1 keyboard receive character.
int kbd_init(){
    U1BRG=PBCLK/16/9600-1;
    U1RXR=KBD_RP_FN;
    U1STA=(1<<12); //RXEN
    IPC8bits.U1IP=3;
    IPC8bits.U1IS=0;
    IFS1bits.U1EIF=0;
    IFS1bits.U1RXIF=0;
    IEC1bits.U1EIE=1;
    IEC1bits.U1RXIE=1;
    U1MODE=(1<<15);
    return 0;
} //keyboard_init()



// return translated key code.
// from kbd_queue.
unsigned char kbd_get_key(){
    unsigned char key;
    
    if (kbd_head==kbd_tail) return 0;
    key=kbd_queue[kbd_head++];
    kbd_head&=KBD_QUEUE_SIZE-1;
    return key;
} // GetKey()

unsigned char kbd_wait_key(){ // attend qu'une touche soit enfoncée et retourne sa valeur.
    unsigned char key;
    
    vga_show_cursor(TRUE);
    while (!(key=kbd_get_key())){
    }//while
    vga_show_cursor(FALSE);
    return key;
}//kbd_wait_key()

 // lit une ligne au clavier, retourne la longueur du texte.
unsigned char kbd_read_line(unsigned char *ibuff,unsigned char buf_len){
    unsigned char c=0, count=0;
    while ((c!='\r') && (count<(buf_len-1))){
        c=kbd_wait_key();
        if (c==CR){
            vga_put_char('\n');
            break;
        }else if (c==BS){
            if (count){
                ibuff--;
                count--;
                vga_print("\b \b");
            }
        }else if (c >=32 && c<=127){
            *ibuff++=c;
            count++;
            vga_put_char(c);
        }
    }// while
    *ibuff=(char)0;
    return count;
} // readline()


#define ERROR_BITS (7<<1)

extern bool abort_signal;

// keyboard character reception
void __ISR(_UART_1_VECTOR,IPL3SOFT) kbd_rx_isr(void){
    char c;
    if (U1STA&ERROR_BITS){
        U1MODEbits.ON=0;
        U1MODEbits.ON=1;
        IFS1bits.U1EIF=0;
    }else{
        c=U1RXREG;
        if (c==CTRL_C){
            abort_signal=true;
        }else{
            kbd_queue[kbd_tail++]=c;
            kbd_tail&=KBD_QUEUE_SIZE-1;
        }
        IFS1bits.U1RXIF=0;
    }
} // kbd_rx_isr()

