/*
* Copyright 2013,2017 Jacques Deschênes
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
 * File:   vga.c
 * Author: Jacques Deschênes
 * Description: VGA video signal generator.
 * Created on 20 août 2013, 08:48
 * rev: 2017-07-31
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/attribs.h>
#include <plib.h>
#include "../HardwareProfile.h"
#include "vga.h"

/*
 * The generator use an output comapre to generate a regalar train of impulsion
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

#define PWM_PERIOD (SYSCLK/31469)-1
#define HSYNC  (3813/PBCLK_PER)-1  // 3,813µSec
#define FIRST_LINE (34)   //First video output line
#define LAST_LINE (FIRST_LINE+2*VRES+1)  // Disable video output for this frame.
#define BITCLK ((int)(HRES * 1000000L/25)) // 25µSec one horizontal line duration.
#define SPI_DLY 95 // video outpout delay after end of HSync pulse.
#define _enable_video_out()  PPSOutput(3,RPB6,SDO1);SPI1CONbits.ON=1;
#define _disable_video_out() PPSOutput(3,RPB6,NULL);SPI1CONbits.ON=0;

unsigned int video_bmp[VRES][HRES/32]; // video bitmap buffer
volatile static int *DmaSrc; // pointer for DMA source.

// configure video generator.
// use TIMER2 as horizontal period timer.
void VideoInit(void){
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
    DmaChnSetTxfer(0,(void *)DmaSrc,(void *)&SPI1BUF,HRES/8,4,4);
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
}//init_video()



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
        case FIRST_LINE:
            video=1;
            DmaSrc=(void*)&video_bmp[0];
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
                DCH0SSA=KVA_TO_PA((void *)DmaSrc);
                if ((ln_cnt+1)&1) DmaSrc +=HRES/32;
                DCH0CON |=128; // Enable DMA channel 0.
            }
    }//switch (ln_cnt)
    mT2ClearIntFlag();
}//tmr2_isr()
