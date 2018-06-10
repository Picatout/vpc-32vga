/*
* Copyright 2013,2017,2018 Jacques Deschênes
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
 *  Author: Jacques Deschênes
 *  Date Created: 2013-09-06
 *  rev: 2017-08-02
 */

#include <string.h>
#include <stdio.h>
#include "hardware/HardwareProfile.h"
#include "console.h"


static int tab_width=4;

//console_t con=SERIAL_CONSOLE;
console_t con=VGA_CONSOLE;

volatile unsigned abort_signal=false;


void uppercase(char *str){// in situ uppercase
    while (*str){
        if (*str>='a' && *str<='z') *str-=32;
        str++;
    }
}

 // efface l'écran et positionne le curseur à {0,0}
void clear_screen(console_t dev){
    if (dev==VGA_CONSOLE){
        vga_clear_screen();
    }else{
        vt_clear_screen();
    }
}

// efface la fin de la ligne à partir du curseur.
void clear_eol(console_t dev){
    if (dev==VGA_CONSOLE){
        vga_clear_eol();
    }else{
        vt_clear_eol();
    }
}

// retourne une touche du clavier si disponible
// sinon retourne 0.
unsigned char get_key(console_t dev){
    if (dev==VGA_CONSOLE){
        return kbd_get_key();
    }else{
        return vt_get_char();
    }
}

 // attend réception d'un caractère
unsigned char wait_key(console_t dev){
    if (dev==VGA_CONSOLE){
        return kbd_wait_key();
    }else{
        return vt_wait_char();
    }
}

// renvoie le curseur texte au début de la ligne
void cursor_home(console_t dev){
    text_coord_t coord;
    
    coord.xy=get_curpos(dev);
    set_curpos(dev,0,coord.y);
}

static char last_line[CHAR_PER_LINE]="";

// lit une ligne au clavier, retourne la longueur de texte.
unsigned char read_line(console_t dev, unsigned char *ibuff,unsigned char buff_size){
    text_coord_t ocoord;
    unsigned cpos=0,len=0;
    unsigned char c=0;
    static unsigned overwrite=false;
    ibuff[0]=0;
    ocoord.xy=get_curpos(dev);
    buff_size=min(--buff_size,CHAR_PER_LINE-ocoord.x-1);
    while (!((c==A_CR) || (c==A_LF))){
        c=wait_key(dev);
        switch (c){
            case A_LF:
            case A_CR:
                break;
            case A_BKSP:
                if (cpos){
                    if (cpos<len){
                        memmove(ibuff+cpos-1,ibuff+cpos,len-cpos);
                        clear_eol(dev);
                        print(dev,ibuff+cpos-1);
                        set_curpos(dev,ocoord.x+cpos,ocoord.y);
                    }else{
                        print(dev,"\b \b");
                    }
                    cpos--;
                    len--;
                }else{
                    beep();
                }
                break;
            case VK_INSERT:
                overwrite=!overwrite;
                break;
            case VK_UP:
                len=strlen(last_line);
                strcpy(ibuff,last_line);
                set_curpos(dev,ocoord.x,ocoord.y);
                clear_eol(dev);
                print(dev,ibuff);
                cpos=len;
                set_curpos(dev,ocoord.x+cpos,ocoord.y);
                break;
            case VK_HOME:
                set_curpos(dev,ocoord.x,ocoord.y);
                cpos=0;
                break;
            case VK_END:
                set_curpos(dev,ocoord.x+len,ocoord.y);
                cpos=len;
                break;
            case A_DEL:
                if (cpos==len){
                    beep();
                }else{
                    memmove(ibuff+cpos-1,ibuff+cpos,len-cpos);
                    len--;
                    ibuff[len]=0;
                    print(dev,ibuff+cpos-1);
                    set_curpos(dev,ocoord.x+cpos,ocoord.y);
                }
                break;
            case VK_CX:
                len=0;
                cpos=0;
                ibuff[0]=0;
                set_curpos(dev,ocoord.x,ocoord.y);
                clear_eol(dev);
                break;
            case VK_LEFT:
                if (cpos){
                   cpos--;
                   set_curpos(dev,ocoord.x+cpos,ocoord.y);
                }
                break;
            case VK_RIGHT:
                if (cpos<len){
                    cpos++;
                    set_curpos(dev,ocoord.x+cpos,ocoord.y);
                }
                break;
            default:
                if ((c<A_SPACE)||(c>127)||((len==buff_size) && !overwrite)){
                    beep();
                    break;
                }
                if (cpos<len){
                    if (overwrite){
                        ibuff[cpos++]=c;
                        put_char(dev,c);
                    }else{
                        memmove(ibuff+cpos+1,ibuff+cpos,len-cpos);
                        ibuff[cpos]=c;
                        len++;
                        ibuff[len]=0;
                        print(dev,ibuff+cpos);
                        cpos++;
                        set_curpos(dev,ocoord.x+cpos,ocoord.y);
                    }
                }else{
                    put_char(dev,c);
                    ibuff[cpos++]=c;
                    len++;
                }
        }//switch
    }//while
    ibuff[len]=0;
    if (len){strcpy(last_line,ibuff);}
    put_char(dev,A_CR);
    return len;
}

// retourne les coordonnées du curseur texte.
unsigned get_curpos(console_t dev){
    if (dev==VGA_CONSOLE){
        return vga_get_curpos();
    }else{
        return vt_get_curpos();
    }
}

// positionnne le curseur texte
void set_curpos(console_t dev,int x, int y){
    if (dev==VGA_CONSOLE){
        vga_set_curpos(x,y);
    }else{
        vt_set_curpos(x,y);
    }
}

void put_char(console_t dev, char c){
    if (dev==VGA_CONSOLE){
        vga_put_char(c);
    }else{
        vt_put_char(c);
    }
}

void print(console_t dev, const char *str){
    if (!str) return;
    if (dev==VGA_CONSOLE){
        vga_print(str);
    }else{
        vt_print(str);
    }
}

void spaces(console_t dev, unsigned char count){
    if (dev==VGA_CONSOLE){
        vga_spaces(count);
    }else{
        vt_spaces(count);
    }
}

void invert_video(console_t dev, BOOL yes){
    if (dev==VGA_CONSOLE){
        vga_invert_video(yes);
    }else{
        vt_invert_video(yes);
    }
}

void crlf(console_t dev){
    if (dev==VGA_CONSOLE){
        vga_crlf();
    }else{
        vt_crlf();
    }
}

void print_int(console_t dev, int number, int width){
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

void print_hex(console_t dev, unsigned hex, int width){
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

void print_float(console_t dev, float f){
    char buffer[16];
    sprintf(buffer,"%G",f);
    print(dev,buffer);
}


void println(console_t dev,const char *str){
    if (!str){
        crlf(dev);
    }else if (dev==VGA_CONSOLE){
        vga_println(str);
    }else{
        vt_println(str);
    }
}

void scroll_down(console_t dev){
    if (dev==VGA_CONSOLE){
        vga_scroll_down();
    }else{
        vt_scroll_down();
    }
}

void scroll_up(console_t dev){
    if (dev==VGA_CONSOLE){
        vga_scroll_up();
    }else{
        vt_scroll_up();
    }
}


void set_tab_witdh(console_t dev, int width){
    if (dev==VGA_CONSOLE){
        vga_set_tab_width(width);
    }else{
        vt_set_tab_width(width);
    }
}

int get_tab_width(console_t dev){
    if (dev==VGA_CONSOLE){
        return vga_get_tab_width();
    }else{
        return vt_get_tab_width();
    }
    
}

 // efface la ligne désignée au complet. et laisse le curseur au début
void clear_line(console_t dev, unsigned line){
    if (dev==VGA_CONSOLE){
        vga_clear_line(line);
    }else{
        vt_clear_line(line);
    }
}

// insère une ligne vide à la position du curseur
void insert_line(console_t dev){
    if (dev==VGA_CONSOLE){
        vga_insert_line();
    }else{
        vt_insert_line();
    }
    
}

/********************************
 * les fonctions suivantes sont
 * utilisées par les fonctions
 * stdio définies dans stdlib.h
 * pour déterminer le périphérique
 * d'entrée ou de sortie.
 * ref: http://microchipdeveloper.com/faq:81
 ********************************/

// utilisé par gets(), fgets()
int _mon_getc (int canblock){
    int c;
    if (canblock){
        c=wait_key(con);
        switch (c){
            case A_CR:
                put_char(con,c);
                c=A_LF;
                break;
            case A_BKSP:
                put_char(con,c);
                put_char(con,A_SPACE);
                break;
        }
        put_char(con,c);
        return c;
    }else{
        return get_key(con);
    }
}

// utilisé par printf() et puts()
void _mon_putc(char c){
    put_char(con,c);
}




