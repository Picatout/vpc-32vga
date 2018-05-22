/*
* Copyright 2013,2014,2015 Jacques Deschênes
* This file is part of VPC-32.
*
*     VPC-32 is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     VPC-32 is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with VPC-32.  If not, see <http://www.gnu.org/licenses/>.
*/

/* 
 * File:   console.h
 * Description: text console for ntsc video output
 * Author: Jacques Descênes
 *
 * Created on 6 septembre 2013, 16:28
 */

#ifndef CONSOLE_H
#define	CONSOLE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <GenericTypeDefs.h>
#include <sys/types.h>    
#include "font.h"
#include "hardware/HardwareProfile.h"
#include "hardware/tvout/vga.h"
#include "vt100.h"
#include "hardware/ps2_kbd/keyboard.h"
    

//typedef  unsigned char console_t;

typedef enum{
    VGA_CONSOLE,
    SERIAL_CONSOLE            
}console_e;
 
typedef uint8_t console_t;

extern console_t con;
extern volatile unsigned abort_signal;

// fonctions de l'interface
void uppercase(char *str);// in situ uppercase
void clear_screen(console_t dev); // efface l'écran et positionne le curseur à {0,0}
void clear_line(console_t dev, unsigned line); // efface la ligne désignée au complet. et laisse le curseur au début
void clear_eol(console_t dev); // efface la fin de la ligne à partir du curseur.
unsigned char get_key(console_t dev); // lecture touches clavier
unsigned char wait_key(console_t dev); // attend qu'une touche soit enfoncée.
unsigned char read_line(console_t dev, unsigned char *ibuff,unsigned char max_char); // lit une ligne au clavier, retourne la longueur de texte.
unsigned get_curpos(console_t dev);
void set_curpos(console_t dev,int x, int y);
void put_char(console_t dev, char c);
void print(console_t dev, const char *str);
void spaces(console_t dev, unsigned char count);
void invert_video(console_t dev, BOOL yes);
void crlf(console_t dev);
void print_int(console_t dev, int number, int width);
void print_hex(console_t dev, unsigned hex, int width);
void print_float(console_t dev, float f);
void println(console_t dev,const char *str);
void scroll_down(console_t dev);
void scroll_up(console_t dev);
void set_tab_witdh(console_t dev, int width);
int get_tab_width(console_t dev);
void insert_line(console_t dev);


#ifdef	__cplusplus
}
#endif

#endif	/* CONSOLE_H */

