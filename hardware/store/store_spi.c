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
 * File:   store_spi.c
 * Author: jacques Deschênes
 * Description: shared SPI interface used by SDcard and SPIRAM
 *
 * Created on 25 novembre 2014, 15:40
 * rev: 2017-07-31
 */

#include <peripheral/ports.h>
#include "store_spi.h"

unsigned char store_initialized=0; // set to 1 by store_spi_init

// set SPI interface clock frequency
// input:
//      freq  frequency Hertz
void spi_clock_freq(int freq){
    STORE_SPICON &= ~BIT_15;
    STORE_SPIBRG = (mGetPeripheralClock() / (freq<<1)) - 1;
    STORE_SPICON |= BIT_15;
}

// configure SPI port
void store_spi_init(){
    _sdc_deselect();
    _sram_deselect();
    STORE_SPICON = 0x8120;   // ON (0x8000), CKE=1 (0x100), CKP=0, Master mode (0x20)
    spi_clock_freq(FAST_CLOCK);
    store_initialized=1;
}


// send one byte of data and receive one back at the same time
// input:
//      b  char to send
// output:
//      char reveived from interface
unsigned char writeSPI(unsigned char b)
{
	STORE_SPIBUF = b;
	while(!STORE_SPISTATbits.SPIRBF); // wait transfer complete
	return STORE_SPIBUF; // read the received value
}// writeSPI

