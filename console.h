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
#include "vt100.h"
    
#define LINE_PER_SCREEN ((int)VRES/CHAR_HEIGHT)
#define CHAR_PER_LINE ((int)(HRES/CHAR_WIDTH))
#define TAB_WIDTH 4

//typedef  unsigned char dev_t;

    typedef struct{
        unsigned short x;
        unsigned short y;
    } text_coord_t;
    
    typedef enum _CURSOR_SHAPE {CR_UNDER=0,CR_BLOCK} cursor_t;

extern dev_t comm_channel;

// fonctions de l'interface
void clear_screen(dev_t dev); // efface l'écran et positionne le curseur à {0,0}
void uppercase(char *str);// in situ uppercase
void clear_eol(dev_t dev); // efface la fin de la ligne à partir du curseur.
unsigned char get_key(dev_t dev); // lecture touches clavier
unsigned char wait_key(dev_t dev); // attend qu'une touche soit enfoncée.
unsigned char readline(dev_t dev, unsigned char *ibuff,unsigned char max_char); // lit une ligne au clavier, retourne la longueur de texte.

#ifdef	__cplusplus
}
#endif

#endif	/* CONSOLE_H */

