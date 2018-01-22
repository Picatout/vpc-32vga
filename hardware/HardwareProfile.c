/*
* Copyright 2013,2014,2017 Jacques Deschênes
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
 * File:   HardwareProfile.h
 * Author: Jacques Deschênes
 * Description: MCU hardware configuration
 * Created on 17 avril 2013, 14:41
 * rev: 2017-07-31
 */

#include "HardwareProfile.h"
#include <plib.h>
#include "sound/sound.h"

#define TMR_COUNT 4
volatile unsigned int  sys_ticks; // milliseconds counter.
static volatile unsigned int timers[TMR_COUNT]; // count down timer


static volatile stime_t  stime;
static volatile sdate_t sdate;

// boot time hardware initialization
void HardwareInit(){
   SYSTEMConfig(mGetSystemClock(), SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
   INTEnableSystemMultiVectoredInt();
    // MCU core timer configuration
#ifdef USE_CORE_TIMER
   OpenCoreTimer(CORE_TICK_RATE); // 1 msec interrupt rate.
   mConfigIntCoreTimer((CT_INT_ON | CT_INT_PRIOR_1 | CT_INT_SUB_PRIOR_0));
#endif
   // disable all analogs inputs, not used by vpc32-v.
   ANSELBCLR=0xFFFFFFFF;
   ANSELACLR=0xFFFFFFFF;
    // serial port config
   SER_LATSET=TX; // Set TX to high (idle state).
   SER_TRISCLR=TX;  // set serial output pin.
   // Peripheral Pin Select
   // see pps.h
   PPSUnLock;  // unlock PPS to enable configuration
   //keyboard
   I2C1CONbits.DISSLW=1; // see pic32mx1xxx/2xxx-errata.pdf rev. E, point 9
   PPSInput(1,INT4,RPA0); // keyboard clock input use INT4
   RPA1R=0; // no peripheral on RA1 (keyboard data) 
   PPSOutput(4, RPB10, U2TX);  // U2TX on PB10
   PPSInput (2, U2RX, RPB11);  // U2RX on PB11
   PPSOutput(2,RPB5,OC2); // OC2  on PB5, VGA HSync signal.
   OUT_PIN_PPS3_RPB13=OUT_FN_PPS3_OC4; // video SPI1 frame sync pulse ouput.
   // all these pins are VGA output pins
   VGA_TRISCLR=(SPI_TRIG_OUT|VSYNC_OUT|HSYNC_OUT|VIDEO_OUT);
   VGA_LATCLR=VIDEO_OUT;
   PPSOutput(3,RPB13,OC4);  // OC4 generate frame signal for SPI1 (video) on RB13
   PPSOutput(3,RPB6,SDO1); // SDO1 VGA video output on RB6
   PPSOutput(4,RPB9,OC3); // OC3 audio output on RB9.
   PPSInput(1,SS1,RPB7); // SPI1 SS1 input on RPB7
   PPSInput(3,SDI2,RPA4); // SD card and SPI RAM SDI (MISO) on RA4
   PPSOutput(2,RPB8,SDO2); // SD card and SPIRAM SDO  (MOSI) on RB8
   // store interface output pins
   STORE_TRISCLR=STORE_MOSI|STORE_CLK|SRAM_SEL|SDC_SEL;
   PPSLock; // lock PPS to avoid accidental modification.
}

// return the value of systicks counter
// millisecond since bootup
// rollover after ~ 50 days.
inline unsigned int ticks(void){
    return sys_ticks;
} //ticks()


void set_time(unsigned short hr, unsigned short min, unsigned short sec){
    stime.h=hr;
    stime.m=min;
    stime.s=sec;
}

void get_time(stime_t *t){
    t->h=stime.h;
    t->m=stime.m;
    t->s=stime.s;
}

void set_date(unsigned year,unsigned month, unsigned day){
    sdate.y=year&0xffff;
    sdate.m=month&0xf;
    sdate.d=day&0x1f;
}

void get_date(sdate_t *d){
    d->y=sdate.y;
    d->m=sdate.m;
    d->d=sdate.d;
}

const unsigned day_in_month[12]={31,28,31,30,31,30,31,31,30,31,30,31};

BOOL leap_year(unsigned short year){
    return (!(year/4) && (year/100)) || !(year/400); 
}

static void next_day(){
    sdate.d++;
    if (sdate.d>day_in_month[sdate.m-1]){
        if(sdate.m==2){
            if (!leap_year(sdate.y)|| (sdate.d==30)){sdate.m++;}
            sdate.d=1;
        }else{
            sdate.m++;
            sdate.d=1;
        }
        if (sdate.m>12){
            sdate.y++;
            sdate.m=1;
        }
    }
}

static void update_rtcc(){
    stime.s++;
    if (!(stime.s%60)){
        stime.s=0;
        stime.m++;
        if (!(stime.m%60)){
            stime.m=0;
            stime.h++;
            if (stime.h==24){
                stime.h=0;
                next_day();
            }
        }
    }
    
}

// pause execution for duration in microsecond.
// idle loop.
inline void delay_us(unsigned int usec){
    for (usec=usec*(CLK_PER_USEC/3);usec;usec--);
}//delay_us()

// pause execution for duration in millisecond.
// idle loop.
void delay_ms(unsigned int msec){
#ifdef USE_CORE_TIMER
    unsigned int t0;
    t0=sys_ticks+msec;
    while (sys_ticks!=t0);
#else
    while (msec--){
        delay_us(1000);
    }
#endif
} // delay_ms()

// get_timer())
//  initialize a timer and return a pointer to it.
// input:
//      msec  duration in milliseconds
// output:
//      tmr*   Pointer to countdown variable, return NULL if none available
volatile unsigned int* get_timer(unsigned int msec){
    int i;
    for (i=0;i<TMR_COUNT;i++){
        if (!timers[i]){
            timers[i]=msec;
            return &timers[i];
        }
    }
    return NULL;
}
// compute free bytes available on heap.
// successive allocation trial until sucess
// binary division algorithm
unsigned free_heap(){
    unsigned top=RAM_SIZE,size,bottom=0;
    void *ptr=NULL;

    size=RAM_SIZE/2;
    while ((top-bottom)>16){
        ptr=malloc(size);
        if (!ptr){
            top=size;
            size-=(top-bottom)>>1;
        }else{
            free(ptr);
            bottom=size;
            size+=(top-bottom)>>1;
        }
    }
    if (ptr) free(ptr);
    return size;
}

#ifdef USE_CORE_TIMER
//MCU core timer interrupt
// period 1msec
// also control TONE timer
void __ISR(_CORE_TIMER_VECTOR, IPL1SOFT) CoreTimerHandler(void){
     sys_ticks++;
//     __asm__("addiu $sp,$sp,-4");
//     __asm__("sw $v0,0($sp)");
     __asm__("mfc0 $v0, $11");
     __asm__("addiu $v0,$v0,%0"::"I"(CORE_TICK_RATE));
     __asm__("mtc0 $v0, $11");
//     __asm__("lw $v0,0($sp)");
//     __asm__("addiu $sp,$sp,4");
     mCTClearIntFlag();
     if ((fSound & TONE_ON) && !(--duration)){
         fSound &= ~TONE_ON;
         mTone_off();
     }
     int i;
     for (i=0;i<TMR_COUNT;i++){
        if (timers[i]) timers[i]--;
     }
     if (!(sys_ticks%1000)){
         update_rtcc();
     }
}
#endif

