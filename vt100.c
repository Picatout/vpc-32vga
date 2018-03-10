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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "vt100.h"

static BOOL auto_scroll=TRUE;

static void send_esc_seq(){
    ser_put_char(ESC);
    ser_put_char(LBRACKET);
}

static BOOL wait_esc(){
    char c;
    c=ser_wait_char();
    if (c==ESC){
        c=ser_wait_char();
        if (c==LBRACKET){
            return TRUE;
        }else{
            return FALSE;
        }
    }else{
        return FALSE;
    }
}

// attend un paramètre numérique
// terminé par la caractère c.
static int get_param(char c){
    int n=0;
    char rx;
    rx=ser_wait_char();
    while ((rx!=c) && (rx>='0') && (rx<='9')){
        n*=10;
        n+=rx-'0';
        rx=ser_wait_char();
    }
    return n;
}


void vt_clear_screen(){
    ser_put_char( FF);
}

void vt_clear_eol(){
    send_esc_seq();
    ser_put_char('K');
}

void vt_clear_line(){
    send_esc_seq();
    ser_put_char('2');
    ser_put_char('K');
    
}

// envoie une séquence ESC [ 6 n
// attend la réponse
text_coord_t vt_get_curpos(){
    char c;
    text_coord_t coord;
    coord.x=0;
    coord.y=0;
    send_esc_seq();
    ser_print("6n");
    if (wait_esc()){
        coord.y=get_param(';')-1;
        coord.x=get_param('R')-1;
    }
    return coord;
}

void vt_set_curpos(int x, int y){
    char fmt[32];
    sprintf(fmt,"\033[%d;%df",y+1,x+1);
    ser_print(fmt);
}


void vt_print(const char *str){
    ser_print(str);
}

void vt_spaces(unsigned char count){
    while (count){
        ser_put_char(A_SPACE);
        count--;
    }
}

void vt_invert_video(BOOL yes){
    send_esc_seq();
    if (yes){
        ser_print("7m");
    }else{
        ser_print("0m");
    }
}

void vt_crlf(){
    ser_put_char('\r');
}

void vt_scroll_up(){
    
}

void vt_scroll_down(){
    
}

void vt_println(const char *str){
    vt_print(str);
    vt_crlf();
}

void vt_set_tab_width(int width){
    
}

int vt_get_tab_width(){
    return 0;
}