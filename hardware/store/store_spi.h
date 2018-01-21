/*
* Copyright 2013,2014,2017 Jacques Deschênes
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
 * File:   store_spi.h
 * Author: jacques Deschênes
 * Description: shared SPI interface used by SDcard and SPIRAM
 *
 * Created on 25 novembre 2014, 15:40
 * rev: 2017-07-31
 */

#ifndef STORE_SPI_H
#define	STORE_SPI_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../HardwareProfile.h"

// SFRs
#define SDC_SPICON STORE_SPICON
#define SRAM_SPICON STORE_SPICON
#define SDC_SPIBRG STORE_SPIBRG
#define SRAM_SPIBRG STORE_SPIBRG
#define SDC_SPISTATbits STORE_SPISTATbits
#define SRAM_SPISTATbits STORE_SPISTATbits
#define SDC_SPIBUF  STORE_SPIBUF
#define SRAM_SPIBUF STORE_SPIBUF

// select deselect device macros
#define _sram_select()  SRAM_PORTCLR = SRAM_SEL

#define _sram_deselect() SRAM_PORTSET = SRAM_SEL

#define _sdc_select()   STORE_PORTCLR = SDC_SEL

#define _sdc_deselect()  STORE_PORTSET = SDC_SEL

#define readSPI()   writeSPI(0xFF)

#define clockSPI()  writeSPI(0xFF)

    
#define SLOW_CLOCK 100000  // 100Khz
#define FAST_CLOCK 20000000L // 20Mhz

    extern unsigned char store_initialized;
    void spi_clock_freq(int freq); // set SPI clock frequency
    void store_spi_init(); // initialize SPI interface.
    unsigned char writeSPI(unsigned char b); // send a byte to SPI interface
    
#ifdef	__cplusplus
}
#endif

#endif	/* STORE_SPI_H */

