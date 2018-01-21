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
int debug=0;

// serial port config
void UartInit(UART_MODULE channel, int baudrate, UART_LINE_CONTROL_MODE LineCtrl){
   UARTConfigure(channel, UART_ENABLE_PINS_TX_RX_ONLY); // no hardware control.
   UARTSetLineControl(channel, LineCtrl);
   UARTSetDataRate(channel, mGetPeripheralClock(), baudrate);
   // enable peripheral
   UARTEnable(channel, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
   unget=-1;
   UartPutch(STDOUT,12);
};

// get character from serial port
// return 0 if none available
char UartGetch(UART_MODULE channel){
    char ch;
    if (!unget==-1) {
        ch=unget;
        unget=-1;
        return ch;
    }else{
        if (UARTReceivedDataIsAvailable (channel)){
               return UARTGetDataByte(channel);
        }else{
            return 0;
        }
    }
};

// send a character to serial port
void UartPutch(UART_MODULE channel, char c){
    while(!UARTTransmitterIsReady(channel));
      UARTSendDataByte(channel, c);
};

// wait for a character from serial port with expiration delay
char UartWaitch(UART_MODULE channel, int delay){
    int t;
    char ch;
    if (!unget==-1){
        ch=unget;
        unget=-1;
        return unget;
    }
    if (!delay) while (1) if (UARTReceivedDataIsAvailable(channel)) return UARTGetDataByte(channel);
    t=ticks()+delay;
    while (ticks()<delay){
       if (UARTReceivedDataIsAvailable(channel)) return UARTGetDataByte(channel);
    }
    return 0;
}

// send a string to serial port.
void UartPrint(UART_MODULE channel, const char* str){
   while(*str != (char)0)
   {
      while(!UARTTransmitterIsReady(channel));
      UARTSendDataByte(channel, *str++);
   }
   while(!UARTTransmissionHasCompleted(channel));
};

// read a line from serial port
int UartReadln(UART_MODULE channel, char *buffer, int buff_len){
    int count=0;
    char c;
    if (!unget==-1){
        c=unget;
        unget=-1;
        *buffer++=c;
        if (c==CR) return;
    }
    while (count < (buff_len-1)){
        if (UARTReceivedDataIsAvailable(channel)){
            c = UARTGetDataByte(channel);
            if (c==CR){UartPutch(channel,'\r'); break;}
            if (c==BS){
                if (count){
                    buffer--;
                    count--;
                    UartPrint(channel,"\b \b");
                }
            }else{
                *buffer++=c;
                count++;
                UartPutch(channel,c);
            }
            
        }
    }
    if (count) *buffer = (char)0;
    UartPutch(channel,'\r');
    return count;
}

// return TRUE if received an ESC character
int UartReceivedESC(UART_MODULE channel){
   char ch;
   if (unget==-1 && UARTReceivedDataIsAvailable(channel)) {
       ch=UARTGetDataByte(channel);
       if (ch==ESC) return 1; else unget=ch;
   }
   return 0;
}

// print debug message on remote terminal
void DebugPrint(const char* str){
    if (debug){
        UartPrint(STDOUT,str); 
    }
}

