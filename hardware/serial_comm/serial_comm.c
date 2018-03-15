/*
* Copyright 2013,2017 Jacques Deschênes
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
 * Author: Jacques Deschênes
 * Description: fonctions de bas niveau pour communication UART avec le PC
 * Created on 17 avril 2013, 14:48
 * rev: 2017-07-321
 */

#include <stdbool.h>
#include <plib.h>
#include "serial_comm.h"
#include "../HardwareProfile.h"

#define QUEUE_SIZE (32)

volatile static int head,tail,count;
volatile static char rx_queue[QUEUE_SIZE];
volatile static bool rx_off;

// ajuste la vitesse de transmission du port sériel.
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
   // activation interruption sur réception ou erreur réception.
   IPC9bits.U2IP=3;
   IPC9bits.U2IS=0;
   IFS1bits.U2EIF=0;
   IFS1bits.U2RXIF=0;
   IEC1bits.U2EIE=1;
   IEC1bits.U2RXIE=1;
   U2MODEbits.ON=1;
   UARTSendDataByte(SERIO,FF);
   return 0;
};

// get character from serial port
// return 0 if none available
char ser_get_char(){
    char c=0;
    if (count){
        c=rx_queue[head++];
        head%=QUEUE_SIZE;
        count--;
    }else{
        IEC1bits.U2RXIE=0;
        if (rx_off){
            UARTSendDataByte(SERIO, XON);
            rx_off=false;
        }
        IEC1bits.U2RXIE=1;
    }
    return c;
};

// Attend un caractère du port sériel
char ser_wait_char(){
    char c;
    while (!(c=ser_get_char()));
    return c;
}

// send a character to serial port
void ser_put_char(char c){
    while(!UARTTransmitterIsReady(SERIO));
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
    char c;
    BOOL loop=TRUE;
    
    *buffer=(char)0;
    while (loop && (count < (buff_len-1))){
        switch((c=ser_wait_char())){
            case CR:
                ser_put_char('\r'); 
                loop=FALSE;
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

#define ERROR_BITS (7<<1)
// interruption sur réception ou erreur réception port sériel.
void __ISR(_UART_2_VECTOR,IPL3SOFT) serial_rx_isr(void){
    if (U2STA&ERROR_BITS){
        U2MODEbits.ON=0;
        U2MODEbits.ON=1;
        IFS1bits.U2EIF=0;
    }else{
        rx_queue[tail++]=U2RXREG;
        tail%=QUEUE_SIZE;
        count++;
        if (count>(QUEUE_SIZE/2)){
            UARTSendDataByte(SERIO,XOFF);
            rx_off=true;
        }
        IFS1bits.U2RXIF=0;
    }
} // serial_rx_isr()

