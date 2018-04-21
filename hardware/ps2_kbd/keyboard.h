/*
* Copyright 2013,2017 Jacques Deschênes
* This file is part of VPC-32.
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
 * File:   keyboard.h
 * Author: Jacques Deschênes
 *
 * Created on 26 août 2013, 10:44
 * ver: 2017-07-30
 */

#ifndef KEYBOARD_H
#define	KEYBOARD_H

#ifdef	__cplusplus
extern "C" {
#endif
#include "../tvout/vga.h"
 
#define CTRL_C 3
    
    enum VIRTUAL_KEY{
    VK_BACK =	8,
    VK_TAB =	9,
    VK_ESC =	27,
    VK_ENTER =	'\r',
    VK_SPACE =	' ', 
    VK_DELETE =	127, 
    VK_F1 =	128,
    VK_F2 =	129,
    VK_F3 =	130,
    VK_F4 =	131,
    VK_F5 =	132,
    VK_F6 =	133,
    VK_F7 =	134,
    VK_F8 =	135,
    VK_F9 =	136,
    VK_F10 =	138,
    VK_F11 =	139,
    VK_F12 =	140,
    VK_UP =	141,
    VK_DOWN =	142,
    VK_LEFT =	143,
    VK_RIGHT =	144,
    VK_HOME =	145,
    VK_END =	146,
    VK_PGUP =	147,
    VK_PGDN =	148,
    VK_INSERT =	149,
    VK_APPS =	151,
    VK_PRN	=	152,
    VK_PAUSE =	153,
    VK_NLOCK =  154, // numlock
    VK_CLOCK =	155, // capslock
    VK_LSHIFT =	156,
    VK_LCTRL =	157,
    VK_LALT =	158,
    VK_RSHIFT =	159,
    VK_LGUI =	160,
    VK_RCTRL =	161,
    VK_RGUI =	162,
    VK_RALT =	163,
    VK_SCROLL =	164,
    VK_NUM	=	165, 
    VK_CAPS =	168,
    //<SHIFT>-<KEY> 
    VK_SUP	=	169,
    VK_SDOWN =	170,
    VK_SLEFT =	171,
    VK_SRIGHT =	172,
    VK_SHOME =	173,
    VK_SEND	=	174,
    VK_SPGUP =	175,
    VK_SPGDN =	176,
    //<CTRL>-<KEY>
    VK_CA =     1,
    VK_CB =     2,
    VK_CC =     3,
    VK_CD =     4,
    VK_CE =     5,
    VK_CF =     6,
    VK_CG =     7,
    VK_CH =     8,
    VK_CI =     9,
    VK_CJ =     10,
    VK_CK =     11,
    VK_CL =     12,
    VK_CM =     13,
    VK_CN =     14,
    VK_CO =     15,
    VK_CP =     16,
    VK_CQ =     17,
    VK_CR =     18,
    VK_CS =     19,
    VK_CT =     20,
    VK_CU =     21,
    VK_CV =     22,
    VK_CW =     23,
    VK_CX =     24,
    VK_CY =     25,
    VK_CZ =     26,
    VK_CUP	=	177,
    VK_CDOWN =	178,	
    VK_CLEFT =	179,
    VK_CRIGHT =	180,
    VK_CHOME =	181,
    VK_CEND =	182,
    VK_CPGUP =	183,
    VK_CPGDN =	184,
    VK_CDEL =   185,
    VK_CBACK =  186,
    VK_LWINDOW= 187,
    VK_RWINDOW= 188,
    VK_MENU	=   189,
    VK_SLEEP =	190,
    VK_SDEL  =  191,
    };

// keyboard API
// initialise interface clavier ps/2
int kbd_init();
// retourne une touche du clavier si disponible
//autrement retourne 0.
unsigned char kbd_get_key();
// attend une touche du clavier
unsigned char kbd_wait_key();
//lecture d'une ligne de texte du clavier
unsigned char kbd_read_line(unsigned char *ibuff,unsigned char max_char);


#ifdef	__cplusplus
}
#endif

#endif	/* KEYBOARD_H */

