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

#include <plib.h>
#include "serial_comm.h"
#include "../HardwareProfile.h"

static char unget;

// ajuste la vitesse de transmission du port sériel.
void ser_set_baud(int baudrate){
   UARTSetDataRate(SERIO, mGetPeripheralClock(), baudrate);
}


// serial port config
int ser_init(int baudrate, UART_LINE_CONTROL_MODE LineCtrl){
   UARTConfigure(SERIO, UART_ENABLE_PINS_TX_RX_ONLY); // no hardware control.
   UARTSetLineControl(SERIO, LineCtrl);
   UARTSetDataRate(SERIO, mGetPeripheralClock(), baudrate);
   // enable peripheral
   UARTEnable(SERIO, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
   unget=-1;
   UARTSendDataByte(SERIO,FF);
   return 0;
};

// get character from serial port
// return 0 if none available
char ser_get_char(){
    char ch;
    if (!unget==-1) {
        ch=unget;
        unget=-1;
        return ch;
    }else{
        if (UARTReceivedDataIsAvailable (SERIO)){
               return UARTGetDataByte(SERIO);
        }else{
            return 0;
        }
    }
};

// send a character to serial port
void ser_put_char(char c){
    while(!UARTTransmitterIsReady(SERIO));
      UARTSendDataByte(SERIO, c);
};

// Attend un caractère du port sériel avec délais d'expiration.
char ser_wait_char(){
    int t;
    char ch;
    if (unget!=-1){
        ch=unget;
        unget=-1;
        return unget;
    }
    while (1){
        if (UARTReceivedDataIsAvailable(SERIO)){
                return UARTGetDataByte(SERIO);
        }//if
    }//while
}

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
    if (!unget==-1){
        c=unget;
        unget=-1;
        *buffer++=c;
        if (c==CR) return;
    }
    while (count < (buff_len-1)){
        if (UARTReceivedDataIsAvailable(SERIO)){
            c = UARTGetDataByte(SERIO);
            if (c==CR){ser_put_char('\r'); break;}
            if (c==BS){
                if (count){
                    buffer--;
                    count--;
                    ser_print("\b \b");
                }
            }else{
                *buffer++=c;
                count++;
                ser_put_char(c);
            }
            
        }
    }
    if (count) *buffer = (char)0;
    ser_put_char('\r');
    return count;
}


