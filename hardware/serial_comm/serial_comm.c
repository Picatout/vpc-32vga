/*
* Copyright 2013,2018 Jacques Desch�nes
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
/*
 * File:   uart.h
 * Author: Jacques Desch�nes
 * Description: fonctions de bas niveau pour communication UART avec le PC
 * Created on 17 avril 2013, 14:48
 * rev: 2017-07-321
 */

#include <stdbool.h>
#include <plib.h>
#include "serial_comm.h"
#include "../HardwareProfile.h"
#include "../ps2_kbd//keyboard.h"

#define QUEUE_SIZE (128)

volatile static int head,tail,count;
volatile static char rx_queue[QUEUE_SIZE];
volatile static bool rx_off;
volatile static bool host_xoff;
volatile static bool flow_ctrl_enabled;

// ajuste la vitesse de transmission du port s�riel.
void ser_set_baud(int baudrate){
   UARTSetDataRate(SERIO, mGetPeripheralClock(), baudrate);
}


// serial port config
int ser_init(int baudrate, UART_LINE_CONTROL_MODE LineCtrl){
//   UARTConfigure(SERIO, UART_ENABLE_PINS_TX_RX_ONLY); // no hardware control.
//   UARTSetLineControl(SERIO, LineCtrl);
   UARTSetDataRate(SERIO, mGetPeripheralClock(), baudrate);
   // enable peripheral
//   UARTEnable(SERIO, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
   U2RXR=3; // RX on PB11
   RPB10R=2; // TX on PB10
   U2STA=(1<<10)|(1<<12); //RXEN|TXEN
   head=0;
   tail=0;
   count=0;
   // activation interruption sur r�ception ou erreur r�ception.
   IPC9bits.U2IP=3;
   IPC9bits.U2IS=0;
   IFS1bits.U2EIF=0;
   IFS1bits.U2RXIF=0;
   IEC1bits.U2EIE=1;
   IEC1bits.U2RXIE=1;
   U2MODEbits.ON=1;
   UARTSendDataByte(SERIO,FF);
   rx_off=false;
   host_xoff=false;
   flow_ctrl_enabled=true;
   return 0;
};

void flow_control(bool on){
    flow_ctrl_enabled=on;
    if (!flow_ctrl_enabled && rx_off){
        UARTSendDataByte(SERIO,XON);
        rx_off=false;
    }
}


// get character from serial port
// return 0 if none available
char ser_get_char(){
    char c=0;
    if (count){
        c=rx_queue[head++];
        head&=(QUEUE_SIZE-1);
        count--;
    }else if (flow_ctrl_enabled){
        IEC1bits.U2RXIE=0;
        if (rx_off){
            UARTSendDataByte(SERIO, XON);
            rx_off=false;
        }
        IEC1bits.U2RXIE=1;
    }
    return c;
};

// s'il y a un caract�re en attente dans la file
// le retourne sans l'extraire.
static char ser_pending_char(){
    if (!count) return 0;
    else{
        return rx_queue[head];
    }
}

// attend pour un caract�re 
// expire et retourne 0 apr�s delay msec.
char ser_wait_char_dly(unsigned delay){
    char c;
    
    set_timer(delay);
    while (!timeout() && !(c=ser_get_char()));
    return c;
}


// Attend un caract�re du port s�riel
char ser_wait_char(){
    char c;
    while (!(c=ser_get_char()));
    return c;
}


// send a character to serial port
void ser_put_char(char c){
    while(host_xoff || !UARTTransmitterIsReady(SERIO));
      UARTSendDataByte(SERIO, c);
};

// send a string to serial port.
void ser_print(const char* str){
   while(*str != (char)0)
   {
      while(!UARTTransmitterIsReady(SERIO));
      UARTSendDataByte(SERIO, *str++);
   }
   while(!UARTTransmissionHasCompleted(SERIO));
};

// read a line from serial port
int ser_read_line(char *buffer, int buff_len){
    int count=0;
    char c=0;
    
    *buffer=(char)0;
    while ((c!=CR) && (count < (buff_len-1))){
        switch((c=ser_wait_char())){
            case CR:
                ser_put_char('\r'); 
                break;
            case BS:
                if (count){
                    buffer--;
                    count--;
                    ser_print("\b \b");
                }
                break;
            default:
                *buffer++=c;
                count++;
                ser_put_char(c);
        }//swtich
    }//while
    *buffer = (char)0;
    return count;
}

void ser_flush_queue(){
    IEC1bits.U2RXIE=0;
    head=0;
    tail=0;
    count=0;
    IEC1bits.U2RXIE=1;
}

extern volatile bool abort_signal;

#define ERROR_BITS (7<<1)
// interruption sur r�ception ou erreur r�ception port s�riel.
void __ISR(_UART_2_VECTOR,IPL3SOFT) serial_rx_isr(void){
    char c;
    
    if (U2STA&ERROR_BITS){
        U2MODEbits.ON=0;
        U2MODEbits.ON=1;
        IFS1bits.U2EIF=0;
    }else{
        c=U2RXREG;
        if (flow_ctrl_enabled && (c==XOFF)){
            host_xoff=true;
        }else if (flow_ctrl_enabled && (c==XON)){
            host_xoff=false;
        }else{
            rx_queue[tail++]=c;
            tail&=(QUEUE_SIZE-1);
            count++;
            if (flow_ctrl_enabled && (count>(QUEUE_SIZE/3))){
                UARTSendDataByte(SERIO,XOFF);
                rx_off=true;
            }
        }
        IFS1bits.U2RXIF=0;
    }
} // serial_rx_isr()

