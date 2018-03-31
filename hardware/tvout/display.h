/*
* Copyright 2013,2016,2018 Jacques Deschênes
* This file is part of VPC-32VGA.
*
*     VPC-32VGA is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     VPC-3VGA is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with VPC-32.  If not, see <http://www.gnu.org/licenses/>.
*/

/* 
 * File:   display.h
 * Author: jacques
 *
 * Created on 19 février 2018, 21:12
 */

#ifndef DISPLAY_H
#define	DISPLAY_H

#ifdef	__cplusplus
extern "C" {
#endif
#include "../../font.h"


#define TAB_WIDTH (4)
#define FRAME_LINES (525) // lines per VGA frame
#define VRES (240)  // vertical pixels
#define HRES (480)  // horizontal pixels
#define IPL  (480/32) // nombre d'entiers par ligne vidéo.    
#define BPL  (480/8) // nombre d'octets par ligne vidéo
#define PPB (8)     // pixels per byte
#define BMP_SIZE (VRES*HRES/PPB) // video buffer size
#define LINE_PER_SCREEN ((int)VRES/CHAR_HEIGHT)
#define CHAR_PER_LINE ((int)(HRES/CHAR_WIDTH))

typedef union{
    unsigned xy;
    struct{
        unsigned x : 16;
        unsigned y : 16;
    };
} text_coord_t;

typedef enum _CURSOR_SHAPE {CR_UNDER=0,CR_BLOCK} cursor_t;

    

#ifdef	__cplusplus
}
#endif

#endif	/* DISPLAY_H */

