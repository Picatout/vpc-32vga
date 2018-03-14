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
 * REF: https://vt100.net/docs/vt102-ug/contents.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "vt100.h"
#include "hardware/ps2_kbd/keyboard.h"

// longueur du tampon pour r�ception d'une s�quence de contr�le
#define CS_SIZE  (32)

// s�quence pour s�lectionn� le mode 80 colonnes  par lignes
static const char cols_80[]="\0x1b[?3l";
// s�quence pour le mode auto-wrap
static const char auto_wrap[]="\0x1b[?7h";
// s�quence pour mode lnm, i.e. '\n' produit un lf et un cr.
static const char ln_mode[]="\0x1b[20h";

static char vt_col;
static char vt_line;
static char tab_width=4;

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

// attend un param�tre num�rique
// termin� par la caract�re c.
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

// envoie une s�quence ESC [ 6 n
// attend la r�ponse
text_coord_t vt_get_curpos(){
    char c;
    text_coord_t coord;
    coord.x=0;
    coord.y=0;
    send_esc_seq();
    ser_print("6n");
    if (wait_esc()){
        vt_col=get_param(';');
        vt_line=get_param('R');
        coord.y=vt_col-1;
        coord.x=vt_line-1;
    }
    return coord;
}

void vt_set_curpos(int x, int y){
    char fmt[32];
    vt_col=x+1;
    vt_line=y+1;
    sprintf(fmt,"\033[%d;%df",vt_line,vt_col);
    ser_print(fmt);
}

typedef struct ansi_to_vk{
    unsigned char vk;
    unsigned char *ansi;
}ansi_to_vk_t;

static const ansi_to_vk_t ansi_table[]={
    {VK_UP,"[A"},
    {VK_DOWN,"[B"},
    {VK_RIGHT,"[C"},
    {VK_LEFT,"[D"},
    {VK_HOME,"[1~"},
    {VK_END,"OF"},
    {VK_PGUP,"[5~"},
    {VK_PGDN,"[6~"},
    {VK_INSERT,"[2~"},
    {VK_F2,"OQ"},
    {VK_F3,"OR"},
    {VK_F4,"OS"},
    {VK_F5,"[16~"},
    {VK_F6,"[17~"},
    {VK_F7,"[18~"},
    {VK_F8,"[19~"},
    {VK_F9,"[20~"},
    {VK_F10,"[21~"},
    {VK_F12,"[24~"},
    {VK_CUP,"[1;5A"}, 
    {VK_CDOWN,"[1;5B"},
    {VK_CRIGHT,"[1;5C"},
    {VK_CLEFT,"[1;5D"},
    {VK_CHOME,"[1;5H"},
    {VK_CEND,"[1;5F"},
    {VK_CPGUP,"[5;5~"},
    {VK_CPGDN,"[6;5~"},
    {VK_CDEL,"[3;5~"},
    {VK_SUP,"[1;2A"},
    {VK_SDOWN,"[1;2B"},
    {VK_SLEFT,"[1;2D"},
    {VK_SRIGHT,"[1;2C"},
    {0,""}
};


static unsigned char get_vk(char *ansi){
    int i=0;

    while (ansi_table[i].vk && strcmp(ansi_table[i].ansi,ansi)){
        i++; 
    }
    return ansi_table[i].vk;
}// get_l1_vk()

static int get_control_sequence(char *buf,int size){
    int len=0;
    unsigned char c;
    size--;
    buf[len++]=ser_wait_char();
    while ((len<size) && ((buf[len++]=ser_wait_char())<'@'));
    buf[len]=0;
    return len;    
}// get_control_sequence()

unsigned char vt_get_char(){
    unsigned char c;
    unsigned char buf[CS_SIZE];
    int len;

    c=(unsigned char)ser_get_char();
    switch(c){
        case A_ESC:
            len=get_control_sequence(buf,CS_SIZE);
            if (len) c=get_vk(buf); else c=0;
            break;
        default:;
    }//switch
    return c;
}//vt_get_char()

unsigned char vt_wait_char(){
    unsigned char c;
    while (!(c=vt_get_char()));
    return c;
}//vt_wait_char()

void vt_put_char(char c){
    unsigned char spaces;
    
    switch(c){
        case '\b':
            vt_col--;
            ser_put_char(c);
            break;
        case '\t':
            spaces=tab_width+vt_col%tab_width-1;
            vt_spaces(spaces);
            vt_col+=spaces;
            break;
        default:
            ser_put_char(c);
            break;
    }//switch
}

void vt_print(const char *str){
    while(*str){
        vt_put_char(*str++);
    } 
}

void vt_println(const char *str){
    vt_print(str);
    vt_crlf();
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
    ser_put_char('\n');
}

void vt_scroll_up(){
    vt_set_curpos(79,29);
    vt_put_char('\n');
    vt_set_curpos(vt_col+1,vt_line+1);
}

void vt_scroll_down(){
    
}

void vt_set_tab_width(int width){
    tab_width=width;
}

int vt_get_tab_width(){
    return tab_width;
}

static bool terminal_ready(){
    bool result=false;
    unsigned char buf[CS_SIZE];
    send_esc_seq();
    ser_print("5n");
    if (wait_esc()){
        receive_control_sequence(buf,CS_SIZE);
        if (!strcmp(buf,"0n")){result=true;}
    }
    return result;
}//terminal_ready()

//initialisation du terminal vt100.
// 80 colonnes
// retour � la ligne automatique
// mode LNM  i.e. '\n' produit aussi un '\r'
// 30 lignes
int vt_init(){
    int code=0;
    if (terminal_ready){
        vt_print(cols_80);
        vt_print(auto_wrap);
        vt_clear_screen();
        vt_col=1;
        vt_line=1;
        code=1;
    }
    return code;
}


