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
 * File:   spiram.h
 * Author: jacques Deschênes
 * Description: Low level SPI RAM interface
 * Created on 17 novembre 2014, 20:42
 * rev: 2017-07-31
 */

#ifndef SPIRAM_H
#define	SPIRAM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../store/store_spi.h"

    //sram commands
#define SRAM_READ (3)
#define SRAM_WRITE (2)
#define SRAM_EDIO (0x3b)
#define SRAM_EQIO (0x38)
#define SRAM_RSTIO (0xff)
#define SRAM_RDMR (5)
#define SRAM_WRMR (1)
#define SRAM_BTMOD (0x00)
#define SRAM_PGMD (0x80)
#define SRAM_SQMD (0x40)
    //#define BIG_SRAM
#if defined BIG_SRAM
    // 23LC1024 size in bytes    
#define SRAM_SIZE 131072
#else    
    // 23LC512 size in bytes
#define SRAM_SIZE (65536)
#endif    
    // SPI RAM API
    // initialize SPI RAM operation mode.
    int sram_init();
    // read a byte from SPI RAM
    unsigned char sram_read_byte(unsigned addr);
    // write a byte from SPI RAM
    void sram_write_byte(unsigned addr, unsigned char byte);
    // read a block of bytes from SPI RAM
    void sram_read_block(unsigned addr, unsigned char buffer[], unsigned count);
    // write a block or bytes to SPI RAM
    void sram_write_block(unsigned addr, const char buffer[], unsigned count);
    // select SPI RAM write mode. i.e. byte,page,sequenetial 
    void sram_write_mode(unsigned char mode);
    // Read SPI RAM operation mode.  i.e. byte,page,sequenetial 
    unsigned char sram_read_mode();
    // clear all SPI RAM to 0.
    void sram_clear();
    // clear a block to 0.
    void sram_clear_block(unsigned addr, unsigned size);
    // write a string to SPI RAM
    void sram_write_string(unsigned addr, const char *str);
    // read a string from SPI RAM
    int sram_read_string(unsigned addr, char *buffer, unsigned size);
    // move a block of size bytes from src to dest
    void sram_move(unsigned dest, unsigned src, unsigned size);
    //charge un fichier dans la mémoire SPI
    int sram_load(unsigned dest, const char *file_name);
    //sauvegarde un partie de la SPI RAM dans un fichier
    int sram_save(unsigned src, const char *file_name, unsigned size);

#ifdef	__cplusplus
}
#endif

#endif	/* SPIRAM_H */

