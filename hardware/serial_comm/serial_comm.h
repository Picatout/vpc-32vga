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
/* 
 * File:   serial_comm.h
 * Author: Jacques Deschênes
 * Description: low level interface to serial port
 * Created on 17 avril 2013, 14:48
 * rev: 2017-07-31
 */

#ifndef UART_H
#define	UART_H
#include <stdbool.h>
#include <p32xxxx.h>
#include <plib.h>

#define NL  10  // newline
#define FF 12  // form feed
#define CR  13  // carriage return
#define ESC 27  // ESC
#define BS 8   // back space
#define SPC 32  // space
#define TAB 9   // horizontal TAB
#define XON (17)  
#define XOFF (19)
#define DEFAULT_LINE_CTRL   UART_DATA_SIZE_8_BITS|UART_PARITY_NONE|UART_STOP_BITS_1


// initialise le port sériel.
int ser_init(int baudrate, UART_LINE_CONTROL_MODE LineCtrl);
// lecture d'un caractère du port sériel
// retourne 
char ser_get_char();
// wait a character from serial port
char ser_wait_char();
// send a character to serial port
void ser_put_char(char c);
// send a string to serial port
void ser_print(const char* str);
// read characters from serial port up to CR
int ser_read_line(char * buffer, int buff_len);
// ajuste la vitesse du port sériel
void ser_set_baud(int baudrate);
// vide la file de réception.
void ser_flush_queue();
// attend pour un caractère 
// expire et retourne 0 après delay msec.
char ser_wait_char_dly(unsigned delay);
// active le contrôle de flux logiciel
void flow_control(bool on);

#endif	/* UART_H */

