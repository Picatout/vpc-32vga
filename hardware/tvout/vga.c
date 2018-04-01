/*
* Copyright 2013,2017,2018 Jacques Deschênes
* This file is part of VPC-32vga.
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
*     along with VPC-32vga.  If not, see <http://www.gnu.org/licenses/>.
*/
/* 
 * File:   vga.c
 * Author: Jacques Deschênes
 * Description: VGA video signal generator and display fonctions.
 * Created on 20 août 2013, 08:48
 * rev: 2018-02-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/attribs.h>
#include <plib.h>
#include "../HardwareProfile.h"
#include "vga.h"
#include "../serial_comm/serial_comm.h"

/*
 * The generator use an output compare to generate a regular train of pulses
 * for HSync signal. The vertical VSync signal in generated in software inside
 * the TIMER2 interrupt. TIMER2 is used as the horizontal period timer and as
 * the output compare reference clock for HSync output compare.
 * The video pixel are serialized using an SPI channel and DMA channel. This
 * way the total time used by the MCU to generate the video signal is a small
 * fraction of total MCU time.
 */
/* USING DMA channel 0 and SPI1
 *  The SPI is configured in frame mode with is SS line connected to a second
 *  Output compare that generate the frame signal.
 *  The SPI interrupt trigger the DMA channel which send video data to SPI transmit
 *  buffer.
 */

#define PWM_PERIOD (SYSCLK/31469)-1 // horizontal line duration.
#define HSYNC  (3813/PBCLK_PER)-1  // 3,813µSec  horizontal sync. pulse duration.
#define FIRST_LINE (34)   //First video output line
#define LAST_LINE (FIRST_LINE+2*VRES+1)  // Disable video output for this frame.
#define BITCLK ((int)(HRES * 1000000L/25)) // bit clock for SPI serializer.
#define SPI_DLY 95 // video outpout delay after end of HSync pulse.
#define _enable_video_out()  PPSOutput(3,RPB6,SDO1);SPI1CONbits.ON=1;
#define _disable_video_out() PPSOutput(3,RPB6,NULL);SPI1CONbits.ON=0;

unsigned int video_bmp[VRES][HRES/32]; // video bitmap buffer
volatile static int *dma_source; // pointer for DMA source.

// indicateurs booléens
#define CUR_SHOW 1  // curseur actif
#define CUR_VIS  2  // curseur visible
#define INV_VID  4  // inverse vidéo

static unsigned short cx=0, cy=0;  // coordonnée courante du curseur texte en pixels.
static unsigned char tab_width=TAB_WIDTH;
static cursor_t cur_shape=CR_UNDER;
static volatile unsigned short flags=0;


#define BLINK_DELAY (40) // 40/60 secondes

volatile static cursor_timer_t cursor_timer={FALSE,0,NULL};



// configure video generator.
// use TIMER2 as horizontal period timer.
int vga_init(void){
    T2CON = 0;
    PR2=PWM_PERIOD;
    OC2CONbits.OCM = 5; // pulse train mode (HSync)
    OC2RS = 0; 
    OC2R = HSYNC;
    IFS0bits.T2IF=0;
    IEC0bits.T2IE=1;
    IPC2bits.T2IP=7;
    IPC2bits.T2IS=3;
    OC2CONbits.ON =1;  // enable output compare
    T2CONbits.ON=1;  // enable TIMER2
    // using OC4 as frame trigger for SPI1
    OC4CONbits.OCM = 5; //pulse train mode.
    OC4RS=0;
    OC4R=HSYNC+SPI_DLY;
    OC4CONbits.ON=1;
    // configure  DMA channel 0.
    // triggered by SPI1 TX interrupt.
    DmaChnOpen(0,0,DMA_OPEN_DEFAULT);
    DmaChnSetEventControl(0,DMA_EV_START_IRQ_EN|
                          DMA_EV_START_IRQ(_SPI1_TX_IRQ));
    DmaChnSetTxfer(0,(void *)dma_source,(void *)&SPI1BUF,HRES/8,4,4);
    // configuredu SPI1 
    SPI1CONbits.DISSDI=1; // SDI not used
    SPI1CONbits.FRMEN=1; // frame mode
    SPI1CONbits.FRMCNT=5; // 32 bytes per frame.
    SPI1CONbits.FRMPOL=1; // sync on rising edge
    SPI1CONbits.FRMSYNC=1; // slave synchronization
    SPI1CONbits.MSTEN=1; // SPI as master
    SPI1CONbits.MODE32=1; // 32 bits mode
    SPI1CONbits.STXISEL=1; // interrupt on TBE
    SpiChnSetBitRate(SPI_CHANNEL1, PBCLK, BITCLK); // bit rate
    return 0;
}//init_video()

void vga_clear_screen(){
    if (flags&INV_VID){
        memset(video_bmp,255,HRES/8*VRES);
    }else{
        memset(video_bmp,0,HRES/8*VRES);
    }
    cx=0;
    cy=0;
} // vga_clear_screen()


//efface ligne, laisse le curseur au début.
void vga_clear_line(unsigned line){
    uint8_t* addr;
    
    addr=(uint8_t*)video_bmp;
    addr+=line*CHAR_HEIGHT*BPL;
    if (flags&INV_VID){
        memset(addr,255,BPL*CHAR_HEIGHT);
    }else{
        memset(addr,0,BPL*CHAR_HEIGHT);
    }
    vga_set_curpos(0,line);
}


void vga_scroll_up(){
    char *src, *dst;
    dst = (char*)video_bmp;
    src = (char*)video_bmp +(CHAR_HEIGHT)*HRES/8;
    memmove(dst,src,(LINE_PER_SCREEN-1)*CHAR_HEIGHT*HRES/8);
    dst= (char*)video_bmp+(CHAR_HEIGHT*(LINE_PER_SCREEN-1))*HRES/8;
    memset(dst,0,HRES/8*CHAR_HEIGHT);
}//vga_scroll_up();

void vga_scroll_down(){
    cx=0;
    cy=0;
    char *src, *dst;
    src = (char*)video_bmp;
    dst = (char*)video_bmp+(CHAR_HEIGHT)*HRES/8;
    memmove(dst,src,(LINE_PER_SCREEN-1)*CHAR_HEIGHT*HRES/8);
    dst=(char*)video_bmp;
    memset(dst,0,HRES/8*CHAR_HEIGHT);
}//vga_scroll_down()


void vga_cursor_right(){
    cx += CHAR_WIDTH;
    if (cx>=(CHAR_PER_LINE*CHAR_WIDTH)){
        cx = 0;
        if (cy>=((LINE_PER_SCREEN-1)*CHAR_HEIGHT)){
            vga_scroll_up();
        }else{
            cy += CHAR_HEIGHT;
        }
    }
} // vga_cursor_right()

void vga_cursor_left(){
    if (cx>=(CHAR_WIDTH)){
        cx -= CHAR_WIDTH;
    }else{
        cx = (CHAR_PER_LINE-1);
        if (cy>=CHAR_HEIGHT){
            cy -= CHAR_HEIGHT;
        }else{
            vga_scroll_down();
        }
    }
}// vga_cursor_left()

void vga_cursor_up(){
    if (cy>=CHAR_HEIGHT){
        cy -= CHAR_HEIGHT;
    }else{
        vga_scroll_down();
    }
}// vga_cursor_up()

void vga_cursor_down(){
    if (cy<=((CHAR_HEIGHT*(LINE_PER_SCREEN-2)))){
        cy += CHAR_HEIGHT;
    }else{
        vga_scroll_up();
    }
}//vga_cursor_down()

void vga_crlf(){
    cx=0;
    if (cy==((LINE_PER_SCREEN-1)*CHAR_HEIGHT)){
        vga_scroll_up();
    }else{
        cy += CHAR_HEIGHT;
    }
}//vga_crlf()

void vga_put_char(char c){
    register int i,l,r,b,x,y;
    x=cx;
    y=cy;
    switch (c){
        case CR:
        case NL:
            vga_crlf();
            break;
        case TAB:
            i=cx%(CHAR_WIDTH*tab_width);
            cx += i?i:tab_width*CHAR_WIDTH;
            if (cx>=(CHAR_PER_LINE*CHAR_WIDTH)){
                cx = 0;
                if (cy==((LINE_PER_SCREEN-1)*CHAR_HEIGHT)){
                    vga_scroll_up();
                }else{
                    cy += CHAR_HEIGHT;
                }
            }
            break;
        case FF:
            vga_clear_screen();
            break;
        case '\b':
            vga_cursor_left();
            break;
        default:
            if ((c<32) || (c>(FONT_SIZE+32))) break;
            c -=32;
            b=x>>5; // position index ligne video_bmp
            r=0;
            l=(32-CHAR_WIDTH)-(x&0x1f); // décalage  à l'intérieur de l'entier
            if (l<0){
                r=-l;
            }
            for (i=0;i<8;i++){
                if (r){
                    if (flags & INV_VID){
                        video_bmp[y][b] |= (0x3f>>r);
                        video_bmp[y][b] &=~(font6x8[c][i]>>r);
                        video_bmp[y][b+1] |= (0x3f<<32-r);
                        video_bmp[y][b+1] &= ~(font6x8[c][i]<<(32-r));
                    }else{
                        video_bmp[y][b] &= ~(0x3f>>r);
                        video_bmp[y][b] |= font6x8[c][i]>>r;
                        video_bmp[y][b+1] &= ~(0x3f<<32-r);
                        video_bmp[y][b+1] |= font6x8[c][i]<<(32-r);
                    }
                    y++;
                } else{
                    if (flags & INV_VID){
                        video_bmp[y][b] |= (0x3f<<l);
                        video_bmp[y++][b] &=~(font6x8[c][i]<<l);
                    }else{
                        video_bmp[y][b] &= ~(0x3f<<l);
                        video_bmp[y++][b] |= font6x8[c][i]<<l;
                    }
                }
            }
            vga_cursor_right();
    }//switch(c)
}//vga_put_char()

void vga_print(const char *text){
    while (*text){
        vga_put_char(*text++);
    }
}// vga_print()

void vga_println( const char *str){
    vga_print(str);
    vga_crlf();
}// vga_println

void vga_set_tab_width(unsigned char width){
    tab_width=width;
}// vga_set_tab_width()

int vga_get_tab_width(){
    return tab_width;
}

void vga_clear_eol(void){
    int x,y;
    
    y=cy;
    while (y<(cy+CHAR_HEIGHT)){
        x=cx;
        while (x<HRES){
            if (flags & INV_VID)
                setPixel(x++,y);
            else
                clearPixel(x++,y);
        }
        y++;
    }
}// vga_clear_eol()

unsigned vga_get_curpos(){
    text_coord_t cpos;
    cpos.x = cx/CHAR_WIDTH;
    cpos.y = cy/CHAR_HEIGHT;
    return cpos.xy;
} // vga_get_cursor_pos()

void vga_set_curpos(unsigned short x, unsigned short y){// {x,y} coordonnée caractère
    BOOL active;
    if (x>(CHAR_PER_LINE-1) || y>(LINE_PER_SCREEN-1)){
        return;
    }
    if ((active=vga_is_cursor_active())){
        vga_show_cursor(FALSE);
    }
    cx=x*CHAR_WIDTH;
    cy=y*CHAR_HEIGHT;
    if (active){
        vga_show_cursor(TRUE);
    }
}//vga_set_curpos()

// inverse vidéo du caractère à la position courante
void vga_invert_char(){
    register int i,l,r,b,x,y;
    x=cx;
    y=cy;
    b=x>>5;
    r=0;
    l=(32-CHAR_WIDTH)-(x&0x1f);
    if (l<0){
        r=-l;
    }
    for (i=9;i;i--){
        if (r){
            video_bmp[y][b] ^= (0x3f>>r);
            video_bmp[y][b+1] ^= (0x3f<<32-r);
            y++;
        } else{
            video_bmp[y++][b] ^= (0x3f<<l);
        }
    }
}//vga_invert_char()

static void vga_toggle_underscore(){
    register int l,r,b,x;
    x=cx;
    b=x>>5;
    r=0;
    l=(32-CHAR_WIDTH)-(x&0x1f);
    if (l<0){
        r=-l;
    }
    if (r){
        video_bmp[cy+CHAR_HEIGHT-1][b] ^= (0x3f>>r);
        video_bmp[cy+CHAR_HEIGHT-1][b+1] ^= (0x3f<<32-r);
    } else{
        video_bmp[cy+CHAR_HEIGHT-1][b] ^= (0x3f<<l);
    }
}//vga_toggle_underscore()

// cette fonction ne doit-être appellée
// que par l'interruption du TIMER2 lorsque le cursor est actif.
void vga_toggle_cursor(){
    if (cur_shape==CR_BLOCK){
        vga_invert_char();
    }else{
        vga_toggle_underscore();
    }
    flags ^=CUR_VIS;
}// vga_toggle_cursor()


void vga_show_cursor(BOOL show){
    if (show){
        flags |= CUR_SHOW;
        flags &= ~CUR_VIS;
        enable_cursor_timer(TRUE,(cursor_tmr_callback_f)vga_toggle_cursor);
    }else{
        enable_cursor_timer(FALSE,NULL);
        flags &= ~CUR_SHOW;
        if (flags & CUR_VIS){
            vga_toggle_cursor();
            flags &= ~CUR_VIS;
        }
    }
}// vga_show_cursor()


BOOL vga_is_cursor_active(){
    return flags&CUR_SHOW;
}// vga_is_cursor_active()

void vga_set_cursor(cursor_t shape){
    if (flags & CUR_VIS){
        vga_show_cursor(FALSE);
        cur_shape=shape;
        vga_show_cursor(TRUE);
    }else{
        cur_shape=shape;
    }
}// vga_set_cursor()

void vga_invert_video(unsigned char invert){
    if (invert){
        flags |= INV_VID;
    }else{
        flags &= ~INV_VID;
    }
}//vga_invert_video()

// renvoie le mode video
BOOL vga_is_invert_video(){
    return flags&INV_VID;
}//vga_is_invert_video()


void vga_spaces(unsigned char count){
    while (count){
        vga_put_char(' ');
        count--;
    }
}//vga_spaces()

void vga_insert_line(){
    char *src, *dst;
    int count;
    
    if (cy>=(CHAR_HEIGHT*(LINE_PER_SCREEN-1))){
        vga_clear_line(cy/CHAR_HEIGHT);
    }else{
        cx=0;
        count=BMP_SIZE-(cy*BPL)-CHAR_HEIGHT*BPL;
        src = cy*BPL+(char*)video_bmp;
        dst = cy*BPL+CHAR_HEIGHT*BPL+(char*)video_bmp;
        memmove(dst,src,count);
        memset(src,0,CHAR_HEIGHT*BPL);
    }
}


void enable_cursor_timer(BOOL enable, cursor_tmr_callback_f cb){
    if (enable && cb){
        if (!cursor_timer.active){
            cursor_timer.period=BLINK_DELAY;
            cursor_timer.cb=cb;
            cursor_timer.active=TRUE;
        }
    }else{
        cursor_timer.active=FALSE;
    }
}

// interruption qui assure le fonctionnement du 
// générateur vidéo.
void __ISR(_TIMER_2_VECTOR,IPL7AUTO) tmr2_isr(void){
    _disable_video_out();
    static int ln_cnt=0;
    static char video=0;
    static char even=1;
    
    ln_cnt++;
    switch (ln_cnt){
        case 1:
            PORTBCLR =VSYNC_OUT;
            break;
        case 3:
            PORTBSET=VSYNC_OUT;
            break;
        case 4:
            if (cursor_timer.active){
                cursor_timer.period--;
                if (!cursor_timer.period){
                    cursor_timer.cb();
                    cursor_timer.period=BLINK_DELAY;
                }
            }
            break;
        case FIRST_LINE:
            video=1;
            dma_source=(void*)&video_bmp[0];
            break;
        case LAST_LINE:
            video=0;
            break;
        case FRAME_LINES:
            ln_cnt=0;
            break;
        default:
            if (video){
                asm volatile(// jitter cancel code.
                "la $v0,%0\n"
                "lhu $a0, 0($v0)\n"
                "andi $a0,$a0,7\n"
                "sll $a0,$a0,2\n"
                "la $v0, jit\n"
                "addu $a0,$v0\n"
                "jr $a0\n"
                "jit:\n"
                "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
                ::"i"(&TMR2)
                );
                _enable_video_out();
                IFS1bits.SPI1TXIF=1;
                DCH0SSA=KVA_TO_PA((void *)dma_source);
                if ((ln_cnt+1)&1) dma_source +=HRES/32;
                DCH0CON |=128; // Enable DMA channel 0.
            }
    }//switch (ln_cnt)
    mT2ClearIntFlag();
}//tmr2_isr()

