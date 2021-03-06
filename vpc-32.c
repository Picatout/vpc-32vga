/*
* Copyright 2013,2018 Jacques Desch�nes
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
 * File:   vpc-32.c
 * Author: Jacques Desch�nes
 *
 * Created on 26 ao�t 2013, 07:38
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plib.h>
#include "graphics.h"

#include "hardware/HardwareProfile.h"
#include "hardware/tvout/vga.h"
#include "hardware/serial_comm/serial_comm.h"
#include "hardware/ps2_kbd/keyboard.h"
#include "hardware/Pinguino/diskio.h"
#include "hardware/Pinguino/fileio.h"
#include "console.h"
#include "hardware/Pinguino/ff.h"
#include "vpcBASIC/vm.h"
//#include "vpcBASIC/vpcBASIC.h"
#include "hardware/sound/sound.h"
#include "hardware/syscall.h"
#include "hardware/rtcc/rtcc.h"

// PIC32MX150F128B Configuration Bit Settings
#include <xc.h>

// DEVCFG3
// USERID = No Setting
#pragma config PMDL1WAY = OFF           // permet plusieurs configurations des p�riph�riques.
#pragma config IOL1WAY = OFF            // permet plusieurs configuration des broches.

// DEVCFG2
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider (2x Divider)
#if SYSCLK==50000000L
#pragma config FPLLMUL = MUL_20
#elif SYSCLK==40000000L
#pragma config FPLLMUL = MUL_16         // SYSCLK=40Mhz
#else
#pragma config FPLLMUL = MUL_15          // PLL Multiplier (15x Multiplier) SYSCLK=37,5Mhz
#endif
#pragma config FPLLODIV = DIV_2         // System PLL Output Clock Divider (PLL Divide by 2)

// DEVCFG1
#pragma config FNOSC = PRIPLL           // Oscillator Selection Bits (Primary Osc w/PLL (XT+,HS+,EC+PLL))
#pragma config FSOSCEN = OFF            // Secondary Oscillator Enable (Disabled)
#pragma config IESO = OFF               // Internal/External Switch Over (Disabled)
#pragma config POSCMOD = HS             // Primary Oscillator Configuration (XT osc mode)
#pragma config OSCIOFNC = OFF           // CLKO Output Signal Active on the OSCO Pin (Disabled)
#pragma config FPBDIV = DIV_1           // Peripheral Clock Divisor (Pb_Clk is Sys_Clk/1)
#pragma config FCKSM = CSDCMD           // Clock Switching and Monitor Selection (Clock Switch Disable, FSCM Disabled)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable (WDT Disabled (SWDTEN Bit Controls))

// DEVCFG0
#pragma config JTAGEN = OFF             // JTAG Enable (JTAG Disabled)
#pragma config ICESEL = RESERVED        // ICE/ICD Comm Channel Select (Communicate on PGEC1/PGED1)
#pragma config PWP = OFF                // Program Flash Write Protect (Disable)
#pragma config BWP = OFF                // Boot Flash Write Protect bit (Protection Disabled)
#pragma config CP = OFF                 // Code Protect (Protection Disabled)



#if defined _DEBUG_
const char *msg1="video target\r";
const char *msg2="0123456789";



void test_pattern(void){
    int i,j;
    for (i=0;i<VRES;i++){
        video_bmp[i][0]=0x80000000;
        video_bmp[i][HRES/32-1]=1;
    }
    for (i=0;i<HRES/32;i++){
        video_bmp[0][i]=0xffffffff;
        video_bmp[VRES-1][i]=0xffffffff;
    }
    for (i=VRES/4;i<VRES/2+VRES/4;i++){
        video_bmp[i][2]=0xFF00FF00;
        video_bmp[i][3]=0xF0F0F0F0;
        video_bmp[i][4]=0xcccccccc;
        video_bmp[i][5]=0xaaaaaaaa;
    }//i
    print(LOCAL_CON,msg1);
    for (i=0;i<8;i++) print(LOCAL_CON,msg2);
    delay_ms(1000);
}//test_pattern()

const int pts[6]={HRES/2,VRES/2,HRES/2+HRES/3,VRES/2+VRES/3,HRES/2-HRES/3,VRES/2+VRES/3};

void graphics_test(){ // test des fonctions graphiques
    int i;

    rectangle(0,0,HRES-1,VRES-1);
    polygon(pts,3);
    circle(HRES/2,VRES/2,100);
    for (i=0;i<100;i++){
        ellipse(HRES/3+i,VRES/3+i,50,30);
    }
    bezier(20,200,20,40,300,40);
    delay_ms(500);
}//graphics_test

#endif

static const char e3k[]="o4g4a4f4o3f4o4c4";

// affiche la date et l'heure
static void display_date_time(){
    char fmt[32];
    rtcc_get_date_str(fmt);
    print(con,fmt);
    rtcc_get_time_str(fmt);
    println(con,fmt);
}

//Affichage de la date et heure du dernier shutdown
//enerigistr� dans le RTCC.
static void last_shutdown(){
    alm_state_t shutdown;
    
    rtcc_power_down_stamp(&shutdown);
    if (shutdown.day){
        printf("Last power down: %s %02d/%02d %02d:%02d\r",weekdays[shutdown.wkday],
                shutdown.month,shutdown.day,shutdown.hour,shutdown.min);
    }
}


enum PRT_DEV {VGA,SERIAL,BOTH};

static void init_msg(int output, int code, const char *msg){
    char fmt[CHAR_PER_LINE];

    if (!code){
        sprintf(fmt,"%s initialization completed.\n",msg);
    }else{
        sprintf(fmt,"%s failed to initialize, error code %d\n",msg,code);
    }
    switch (output){
        case VGA:
            vga_print(fmt);
            break;
        case SERIAL:
            ser_print(fmt);
            break;
        case BOTH:
            vga_print(fmt);
            ser_print(fmt);
    }//switch
}


#include <math.h>
//__attribute__((mips16))
void main(void) {
    cold_start_init();
    //ref: http://microchipdeveloper.com/xc32:redirect-stdout
    setbuf(stdout,NULL); 
    setbuf(stdin,NULL);
    init_msg(SERIAL,ser_init(115200,DEFAULT_LINE_CTRL),"Serial port");
    vga_init();
    init_msg(SERIAL,vga_init(),"video");
    vga_clear_screen();
    init_msg(SERIAL,rtcc_init(),"RTCC");
    heap_size=free_heap();
    init_msg(SERIAL,sound_init(),"Sound");
    play(e3k);
    init_msg(SERIAL,kbd_init(),"keyboard");
    init_msg(SERIAL,!mount(0),"SD card");
    init_msg(SERIAL,sram_init(),"SPI RAM");
    printf("free RAM (bytes): %d\r",heap_size);// il faut utilis� '\r' avec printf()
    last_shutdown();
    display_date_time();
    
//    text_coord_t cpos;
//    while (1){
//        put_char(con,ser_wait_char());
//        cpos.xy=get_curpos(con);
//        if (cpos.y==29){ wait_key(con);clear_screen(con);}
//    }

    shell();
} // main()


