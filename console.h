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
#include "hardware/tvout/vga.h"
#include "hardware/HardwareProfile.h"
#include "hardware/tvout/vga.h"
#include "vt100.h"
    

//typedef  unsigned char dev_t;

typedef enum{
    VGA_CONSOLE,
    SERIAL_CONSOLE            
}console_t;
    
extern dev_t con;

// fonctions de l'interface
void uppercase(char *str);// in situ uppercase
void clear_screen(dev_t dev); // efface l'écran et positionne le curseur à {0,0}
void clear_line(dev_t dev, unsigned line); // efface la ligne désignée au complet. et laisse le curseur au début
void clear_eol(dev_t dev); // efface la fin de la ligne à partir du curseur.
unsigned char get_key(dev_t dev); // lecture touches clavier
unsigned char wait_key(dev_t dev); // attend qu'une touche soit enfoncée.
unsigned char read_line(dev_t dev, unsigned char *ibuff,unsigned char max_char); // lit une ligne au clavier, retourne la longueur de texte.
text_coord_t get_curpos(dev_t dev);
void set_curpos(dev_t dev,int x, int y);
void put_char(dev_t dev, char c);
void print(dev_t dev, const char *str);
void spaces(dev_t dev, unsigned char count);
void invert_video(dev_t dev, BOOL yes);
void crlf(dev_t dev);
void print_int(dev_t dev, int number, int width);
void print_hex(dev_t dev, unsigned hex, int width);
void println(dev_t dev,const char *str);
void scroll_down(dev_t dev);
void scroll_up(dev_t dev);
void set_tab_witdh(dev_t dev, int width);
int get_tab_width(dev_t dev);

#ifdef	__cplusplus
}
#endif

#endif	/* CONSOLE_H */

