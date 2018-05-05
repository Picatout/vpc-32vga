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

#include "../HardwareProfile.h"
#include "spiram.h"
#include "../Pinguino/sdmmc.h"
#include "../Pinguino/ff.h"

/* File:   spiram.c
 * Author: jacques Deschênes
 * description: Low level SPI RAM interface
 * rev: 2017-07-31
*/

// send command to SPI RAM
// input:
//      cmd  commmand code
//      addr address
static void sram_cmd(unsigned char cmd, unsigned addr){
    writeSPI(cmd);
#if defined BIG_SRAM    
    writeSPI(addr>>16);
#endif    
    writeSPI(addr>>8);
    writeSPI(addr&0xff);
}

// sram_write_mode
//   select write operating mode {byte,page,sequential}
// input:
//   mode operating mode {SRAM_BTMOD,SRAM_PGMOD,SRAM_SQMOD}
// output:
//   none
void sram_write_mode(unsigned char mode){
    _sram_select();
    writeSPI(SRAM_WRMR);
    writeSPI(mode);
    _sram_deselect();
}

// sram_read_mode()
//      Read operation mode {byte,page,sequential}
// input:
//   none
// output:
//   char  Operating mode {SRAM_BTMOD,SRAM_PGMOD,SRAM_SQMOD}
unsigned char sram_read_mode(){
    unsigned char mode;
    _sram_select();
    writeSPI(SRAM_RDMR);
    mode=writeSPI(0);
    _sram_deselect();
    return mode;
}

// initialize SPI RAM operating mode
// use sequencial mode, SPI interface.
int sram_init(){
    if (!store_initialized) store_spi_init();
    sram_write_mode(SRAM_SQMD);
    return 0;
}

// sram_clear()
//      Clear RAM to zero.
// input:
//      none
// output:
//      none
void sram_clear(){
    unsigned i;
    _sram_select();
    sram_cmd(SRAM_WRITE,0);
    for (i=0;i<SRAM_SIZE;i++) writeSPI(0);
    _sram_deselect();
}

// sram_clear_block()
//      Clear to zero a block in SPI RAM
// input:
//      addr  Start address
//      size  Size in byte to zeroes.
// output:
//      none
void sram_clear_block(unsigned addr, unsigned size){
    int i;
    _sram_select();
    sram_cmd(SRAM_WRITE,addr);
    for(i=0;i<size;i++)writeSPI(0);
    _sram_deselect();
}

// sram_read_byte()
//      Read a single byte from SPI RAM
// input:
//      addr  Adress to read.
// output:
//      char  byte read.
unsigned char sram_read_byte(unsigned addr){
    unsigned char b;

    _sram_select();
    sram_cmd(SRAM_READ,addr);
    b=writeSPI(0);
    _sram_deselect();
    return b;
}

// sram_write_byte()
//      Write a byte to SPI RAM
// input:
//      addr  Adresse to write.
//      byte  Value to write
// ouput:
//      none    
void sram_write_byte(unsigned addr, unsigned char byte){
    _sram_select();
    sram_cmd(SRAM_WRITE,addr);
    writeSPI(byte);
    _sram_deselect();
}

// sram_read_block()
//      Read a range of adress in SPI RAM
// input:
//      addr  Adressse first byte.
//      buffer Buffer to store data.
//      count  Number of bytes to read.
// output:
//      none  Except for buffer[] content.
void sram_read_block(unsigned addr, unsigned char buffer[], unsigned count){
    unsigned i;
    _sram_select();
    sram_cmd(SRAM_READ,addr);
    for (i=0;i<count;i++) buffer[i]=writeSPI(0);
    _sram_deselect();
}

// sram_write_block()
//      Write a range of byte to SPI RAM
// input:
//      addr Adresse of first byte location.
//      buffer  Buffer that contain data to write.
//      count  Number of bytes to write.
// output:
//      none
void sram_write_block(unsigned addr, const char buffer[],unsigned count){
    unsigned i;
    _sram_select();
    sram_cmd(SRAM_WRITE,addr);
    for (i=0;i<count;i++) writeSPI(buffer[i]);
    _sram_deselect();
}

// sram_write_string()
//      Write a zero terminated string to SPI RAM
// input:
//      addr  Start adress in SPI RAM
//      *str  Pointer to asciiz string
// output:
//      none
void sram_write_string(unsigned addr, const char *str){
    _sram_select();
    sram_cmd(SRAM_WRITE,addr);
    while (*str) writeSPI(*str++);
    writeSPI(0);
    _sram_deselect();
}

// sram_read_string()
//      Read a asciiz str from SPI RAM
// input:
//      addr Adresse to read from.
//      *buffer Buffer to receive str.
//      size  Maximum length to read.
// output:
//      len  Length of string read
int sram_read_string(unsigned addr, char *buffer,unsigned size){
    int i=0;

    _sram_select();
    sram_cmd(SRAM_READ,addr);
    while (i<size && (buffer[i]=writeSPI(0xff))) i++;
    _sram_deselect();
    if (i==size)buffer[--i]=0;
    return --i;
}

static void move_up(unsigned dest,unsigned src, unsigned size){
#define BUF_SIZE (64)
    
    uint8_t buf[BUF_SIZE];
    unsigned count;
    while (size){
        count=min(BUF_SIZE,size);
        sram_read_block(src,buf,count);
        sram_write_block(dest,buf,count);
        size-=count;
        src+=count;
        dest+=count;
    }
}

static void move_down(unsigned dest,unsigned src, unsigned size){
#define BUF_SIZE (64)
    uint8_t buf[BUF_SIZE];
    unsigned count;
    
    dest+=size;
    src+=size;
    while (size){
        count=min(BUF_SIZE,size);
        sram_read_block(src-count,buf,count);
        sram_write_block(dest-count,buf,count);
        size-=count;
        src-=count;
        dest-=count;
    }
}

// sram_move()
//  move a block from src to dest.
// input:
//  dest    address destination
//  src     address source   
//  size    count of bytes to move
void sram_move(unsigned dest, unsigned src, unsigned size){
    if (dest<src){
        move_up(dest,src,size);
    }else if (dest>src){
        move_down(dest,src,size);
    }
}

// sram_load()
//charge un fichier dans la mémoire SPI
// arguments:
//  dest  addresse spi ram destination
//  file_name  nom du fichier à charger
// retourne:
//   nombre d'octets chargés, ou -1 si erreur i/o
int sram_load(unsigned dest,const char *file_name){
   FIL *fh;
   FRESULT error=FR_OK;
   char *buffer;
   DWORD buf_size,size;
   int total,count;
   
   fh=malloc(sizeof(FIL));
   if (!fh){
       return -1;
   }
   error=f_open(fh,file_name,FA_READ);
   if (error!=FR_OK) return -1;
   size=fh->fsize;
   size=min(SRAM_SIZE-dest,size);
   buf_size=min(size,512);
   buffer=malloc(buf_size);
   if (!buffer){
       f_close(fh);
       free(fh);
       return -1;
   }
   total=0;
   while ((error==FR_OK) && (size>0)){
       count=min(buf_size,size);
       error=f_read(fh,buffer,count,&count);
       sram_write_block(dest,buffer,count);
       dest+=count;
       total+=count;
       size-=count;
   }
   f_close(fh);
   free(fh);
   free(buffer);
   return total;
}

// sram_save()
//sauvegarde un partie de la SPI RAM dans un fichier
// arguments:
//     src  adresse SPI RAM début bloc à sauvegarder
//     file_name nom du fichier destination
//     size  nombre d'octets à sauvegarder.
// reotourne:
//      nombre d'octets écris dans le fichier ou -1 si erreur.
int sram_save(unsigned src,const char *file_name,unsigned size){
    FIL *fh;
    FRESULT error=FR_OK;
    char *buffer;
    int buf_size,total,count;
    
    fh=malloc(sizeof(FIL));
    if (!fh) return -1;
    error=f_open(fh,file_name,FA_WRITE|FA_CREATE_ALWAYS);
    if (error!=FR_OK){
        free(fh);
        return -1;
    }
    buf_size=min(512,size);
    buffer=malloc(buf_size);
    if (!buffer){
        f_close(fh);
        free(fh);
        return -1;
    }
    total=0;
    while (size && (error==FR_OK)){
        count=min(size,buf_size);
        sram_read_block(src,buffer,count);
        error=f_write(fh,buffer,count,&count);
        src+=count;
        total+=count;
        size-=count;
    }
    f_close(fh);
    free(fh);
    free(buffer);
    return error==FR_OK?total:-1; 
}
