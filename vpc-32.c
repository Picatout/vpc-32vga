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
 * File:   vpc-32.c
 * Author: Jacques Deschênes
 *
 * Created on 26 août 2013, 07:38
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
#pragma config PMDL1WAY = OFF           // permet plusieurs configurations des périphériques.
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

const unsigned int e3k[]={ // rencontre du 3ième type
784,500, // sol4
880,500, // la4
698,500, // fa4
349,500, // fa3
523,500, // do4
0,0
};

// affiche la date et l'heure
void display_date_time(){
    char fmt[32];
    rtcc_get_date_str(fmt);
    print(con,fmt);
    rtcc_get_time_str(fmt);
    println(con,fmt);
}


enum PRT_DEV {VGA,SERIAL,BOTH};

void init_msg(int output, int code, const char *msg){
    char fmt[CHAR_PER_LINE];

    if (!code){
        sprintf(fmt,"%s initialization completed.\r",msg);
    }else{
        sprintf(fmt,"%s failed to initialize, error code %d\r",msg,code);
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

//__attribute__((mips16))
void main(void) {
#if defined _DEBUG_
    debug=-1;
#endif  
    cold_start_init();
    init_msg(SERIAL,ser_init(115200,DEFAULT_LINE_CTRL),"Serial port");
    vga_init();
    init_msg(SERIAL,vga_init(),"video");
    vga_clear_screen();
    init_msg(SERIAL,rtcc_init(),"RTCC");
    heap_size=free_heap();
#if defined _DEBUG_
    test_pattern();
#endif
    init_msg(SERIAL,sound_init(),"Sound");
    tune((unsigned int*)&e3k[0]);
    init_msg(SERIAL,kbd_init(),"keyboard");
    text_coord_t cpos;
    init_msg(SERIAL,!mount(0),"SD card");
    init_msg(SERIAL,sram_init(),"SPI RAM");
    //test_vm();
#if defined _DEBUG_    
    // sram test
    circle(HRES/2,VRES/2,100);
    delay_ms(1000);
    sram_write_block(0,&video_bmp,HRES*VRES/8);
    clear_screen();
    delay_ms(1000);
    sram_read_block(0,&video_bmp,HRES*VRES/8);
    delay_ms(1000);
    UartPrint("sound test.\r");
#endif    
//    set_cursor(CR_BLOCK); // sauvegare video_buffer dans SRAM
//    clear_screen();
//    unsigned char c;
//    while (1){
//        c=wait_key(VGA_CONSOLE);
//        put_char(VGA_CONSOLE,c);
//    }
#if defined _DEBUG_
    graphics_test();
    set_curpos(0,LINE_PER_SCREEN-1);
    print(comm_channel,"test");
    sram_write_block(100000,video_bmp,BMP_SIZE);
    delay_ms(1000);
    clear_screen();
    delay_ms(1000);
    sram_read_block(100000,video_bmp,BMP_SIZE);
    delay_ms(1000);
    clear_screen();
//    print(comm_channel,"heap_size: ");
//    print_int(comm_channel,heap_size,0);
//    crlf();
#endif
//    char fmt[40];
//    text_coord_t curpos;
//    vga_crlf();
//    curpos=vga_get_curpos();
//    while(1){
//        vga_set_curpos(curpos.x,curpos.y);
//        rtcc_get_date_str(fmt);
//        vga_print(fmt);
//        rtcc_get_time_str(fmt);
//        vga_print(fmt);
//        vga_put_char(CR);
//        sprintf(fmt,"system ticks %d\r",ticks());
//        vga_print(fmt);
//        _usec_delay(3000);
//    }
    print(con,"free RAM (bytes): ");
    print_int(con,free_heap(),0);
    crlf(con);
    last_shutdown();
    display_date_time();
    shell();
} // main()


