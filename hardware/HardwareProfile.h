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
 * File:   HardwareProfile.h
 * Author: Jacques Deschênes
 * Description: configuration hardware spécifique
 * Created on 17 avril 2013, 14:41
 * rev: 2017-07-31
 */

#ifndef HARDWAREPROFILE_H
#define	HARDWAREPROFILE_H

#include <p32xxxx.h>
#include <plib.h>

#define VPC_32

#define STDIO   UART2
#define STDOUT  STDIO
#define STDIN   STDIO
#define STDERR  STDIO

#define RAM_SIZE (65536)

#define LOCAL_CON 0  // local console
#define SERIAL_CON STDIO // remote console via RS-232 port

// RS-232 port 
#define TX BIT_10 // UART2 TX on PB10
#define RX BIT_11 // UART2 RX on PB11
#define SER_PORT PORTB // serial port is PORTB
#define SER_LATSET LATBSET
#define SER_LATCLR LATBCLR
#define SER_TRISSET TRISBSET
#define SER_TRISCLR TRISBCLR

// MCU clock
#define SYSCLK 40000000L
// peripheral clock
#define PBCLK   SYSCLK
#if SYSCLK==40000000L  // up to 50Mhz
#define PBCLK_PER 25 // nanoseconds
#else
#define PBCLK_PER 20 // nanoseconds for SYSCLK=50Mhz
#endif
#define mGetSystemClock() (SYSCLK)
#define mGetInstructionClock()	(mGetSystemClock()/1)	//
#define mGetPeripheralClock() (mGetSystemClock())
#define CORE_TICK_RATE (mGetSystemClock()/2/1000) // system tick 1msec
#define CLK_PER_USEC (SYSCLK/1000000L) // system clock ticks per microsecond.
// memory regions
#define FLASH_BEG			 	0x9D000000
#define FLASH_END			 	0x9D000000+BMXPFMSZ-1
#define RAM_BEG				 	0xA0000000
#define RAM_END				 	0xA0000000+BMXDRMSZ-1

#define CS_RAM

#define USE_CORE_TIMER  // comment out if not using MCU core timer.

//SDcard and SPIRAM share same SPI channel
//SPI2 used by storage devices 
#define STORE_SPICON SPI2CON
#define STORE_SPIBRG SPI2BRG
#define STORE_SPISTATbits SPI2STATbits
#define STORE_SPIBUF SPI2BUF
#define STORE_PORT PORTB
#define STORE_PORTCLR PORTBCLR
#define STORE_PORTSET PORTBSET
#define STORE_TRIS TRISB
#define STORE_TRISCLR TRISBCLR
#define STORE_TRISSET TRISBSET
#define STORE_CLK BIT_15 //RB15  SPI2 clock
#define STORE_MISO BIT_4 //RA4   SPI2 SDI
#define STORE_MOSI BIT_8 //RB8   SPI2 SDO

//SPI RAM select line
#define SRAM_PORTCLR PORTACLR
#define SRAM_PORTSET PORTASET
#define SRAM_TRISCLR TRISACLR
#define SRAM_TRISSET TRISASET
#define SRAM_SEL  BIT_0  // RA0

// SD card select line
#define SDC_SEL BIT_3  //  RB3
// SD card detectio line
#define SDC_DET BIT_12 //  RB12


// video output
#define VGA_TRISSET TRISBSET
#define VGA_TRISCLR TRISBCLR
#define VGA_LATSET TRISBSET
#define VGA_LATCLR TRISBCLR
#define VSYNC_OUT BIT_4  // VGA vertical sync pulse
#define HSYNC_OUT BIT_5  // VGA horizontal sync pulse
#define VIDEO_OUT BIT_6  // VGA video output
#define SS1_IN BIT_7     // SPI1 sync select line
#define SPI_TRIG_OUT BIT_13  // output pulse to SS1 (trigger SPI1 frame)

//keyboard interface
#define KBD_RX BIT_2  // RB2  keyboard uart1 rx
// keyboard interface on PORTB
#define KBD_PORT PORTB
#define KBD_LATCLR LATBCLR
#define KBD_LATSET LATBSET
#define KBD_TRISCLR TRISBCLR
#define KBD_TRISSET TRISBSET
#define KBD_RP_FN  (4)

typedef struct time_struct{
    unsigned s:6;
    unsigned m:6;
    unsigned h:5;
} stime_t;

typedef struct date_struct{
    unsigned y:16;
    unsigned m:4;
    unsigned d:5;
} sdate_t;

// user RAM pool size.
unsigned int heap_size;


// initialize hardware at boot time.
void HardwareInit();
// return size of available user RAM.
unsigned free_heap();

// initialize a timer and return a pointer to it.
volatile unsigned int* get_timer(unsigned int msec);

#ifdef USE_CORE_TIMER
// MCU core timer ticks count.
unsigned int ticks(void);
#endif
// pause in microseconds
void delay_us(unsigned int usec);
// pause milliseconds
void delay_ms(unsigned int msec);

#endif	/* HARDWAREPROFILE_H */

