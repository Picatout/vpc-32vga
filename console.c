/*
* Copyright 2013,2017,2018 Jacques Desch�nes
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
 *  Name: console.c
 *  Description: text console for NTSC video output
 *  Author: Jacques Desch�nes
 *  Date Created: 2013-09-06
 *  rev: 2017-08-02
 */

#include <string.h>
#include "console.h"
#include "hardware/HardwareProfile.h"
#include "hardware/serial_comm/serial_comm.h"
#include "hardware/ps2_kbd/keyboard.h"
#include "hardware/tvout/vga.h"
#include "vt100.h"


static int tab_width=4;

void uppercase(char *str){// in situ uppercase
    while (*str){
        if (*str>='a' && *str<='z') *str-=32;
        str++;
    }
}

 // efface l'�cran et positionne le curseur � {0,0}
void clear_screen(dev_t dev){
    if (dev==VGA_CONSOLE){
        vga_clear_screen();
    }else{
        vt_clear_screen();
    }
}

// efface la fin de la ligne � partir du curseur.
void clear_eol(dev_t dev){
    if (dev==VGA_CONSOLE){
        vga_clear_eol();
    }else{
        vt_clear_eol();
    }
}

// retourne une touche du clavier si disponible
// sinon retourne 0.
unsigned char get_key(dev_t dev){
    if (dev==VGA_CONSOLE){
        return kbd_get_key();
    }else{
        return vt_get_char();
    }
}
 // attend r�ception d'un caract�re
unsigned char wait_key(dev_t dev){
    if (dev==VGA_CONSOLE){
        return kbd_wait_key();
    }else{
        return vt_wait_char();
    }
}

unsigned char read_line(dev_t dev, unsigned char *ibuff,unsigned char max_char){ // lit une ligne au clavier, retourne la longueur de texte.
    if (dev==VGA_CONSOLE){
        return kbd_read_line(ibuff,max_char);
    }else{
        return vt_read_line(ibuff,max_char);
    }
}

// retourne les coordonn�es du curseur texte.
text_coord_t get_curpos(dev_t dev){
    if (dev==VGA_CONSOLE){
        return vga_get_curpos();
    }else{
        return vt_get_curpos();
    }
}

// positionnne le curseur texte
void set_curpos(dev_t dev,int x, int y){
    if (dev==VGA_CONSOLE){
        vga_set_curpos(x,y);
    }else{
        vt_set_curpos(x,y);
    }
}

void put_char(dev_t dev, char c){
    if (dev==VGA_CONSOLE){
        vga_put_char(c);
    }else{
        vt_put_char(c);
    }
}

void print(dev_t dev, const char *str){
    if (!str) return;
    if (dev==VGA_CONSOLE){
        vga_print(str);
    }else{
        vt_print(str);
    }
}

void spaces(dev_t dev, unsigned char count){
    if (dev==VGA_CONSOLE){
        vga_spaces(count);
    }else{
        vt_spaces(count);
    }
}

void invert_video(dev_t dev, BOOL yes){
    if (dev==VGA_CONSOLE){
        vga_invert_video(yes);
    }else{
        vt_invert_video(yes);
    }
}

void crlf(dev_t dev){
    if (dev==VGA_CONSOLE){
        vga_crlf();
    }else{
        vt_crlf();
    }
}

void print_int(dev_t dev, int number, int width){
    int sign=0;
    char str[18], *d;
    str[17]=0;
    str[16]=' ';
    d=&str[15];
    if (width>16){width=16;}
    if (number<0){
        sign=1;
        number = -number;
        width--;
    }
    if (!number){
        *d--='0';
        width--;
    }
    while (number>0){
       *d--=(number%10)+'0';
        number /= 10;
        width--;
    }
    if (sign){*d--='-';}
    while (width>0){
        *d--=' ';
        width--;
    }
    print(dev,++d);
}

void print_hex(dev_t dev, unsigned hex, int width){
    char c[18], *d;
    int i;
    c[17]=0;
    c[16]=' ';
    d= &c[15];
    if (width>16){width=16;}
    if (!hex){*d--='0'; width--;}
    while (hex){
        *d=hex%16+'0';
        if (*d>'9'){
            *d+=7;
        }
        hex>>=4;
        d--;
        width--;
    }
    while(width>0){
        *d--=' ';
        width--;
    }
    print(dev,++d);
}

void println(dev_t dev,const char *str){
    if (!str){
        crlf(dev);
    }else if (dev==VGA_CONSOLE){
        vga_println(str);
    }else{
        vt_println(str);
    }
}

void scroll_down(dev_t dev){
    if (dev==VGA_CONSOLE){
        vga_scroll_down();
    }else{
        vt_scroll_down();
    }
}

void scroll_up(dev_t dev){
    if (dev==VGA_CONSOLE){
        vga_scroll_up();
    }else{
        vt_scroll_up();
    }
}


void set_tab_witdh(dev_t dev, int width){
    if (dev==VGA_CONSOLE){
        vga_set_tab_width(width);
    }else{
        vt_set_tab_width(width);
    }
}

int get_tab_width(dev_t dev){
    if (dev==VGA_CONSOLE){
        return vga_get_tab_width();
    }else{
        return vt_get_tab_width();
    }
    
}

 // efface la ligne d�sign�e au complet. et laisse le curseur au d�but
void clear_line(dev_t dev, unsigned line){
    if (dev==VGA_CONSOLE){
        vga_clear_line(line);
    }else{
        vt_clear_line(line);
    }
}