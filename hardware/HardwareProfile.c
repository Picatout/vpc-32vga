/*
* Copyright 2013,2018 Jacques Deschênes
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
#include "rtcc/rtcc.h"
#include <plib.h>
#include "sound/sound.h"

volatile unsigned int  sys_ticks; // milliseconds counter.
volatile unsigned int timer; // count down timer

// initialisation matérielle au démarrage.
void cold_start_init(){
   SYSTEMConfig(mGetSystemClock(), SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
   INTEnableSystemMultiVectoredInt();
    // MCU core timer configuration
   OpenCoreTimer(CORE_TICK_RATE); // 1 msec interrupt rate.
   mConfigIntCoreTimer((CT_INT_ON | CT_INT_PRIOR_2 | CT_INT_SUB_PRIOR_2));
   // disable all analogs inputs, not used by vpc32-v.
   ANSELBCLR=0xFFFFFFFF;
   ANSELACLR=0xFFFFFFFF;
   //power LED config
   PLED_TRISCLR=PLED_PIN; // output mode
   PLED_ODCSET=PLED_PIN; // open drain
   PLED_RPR=PLED_FN;  // PPS analog comparator output Cxout
   // configuration du comparateur analogique
   // ON=1,OE=1,CPOL=1,CREF=1,CCH=3
   PLED_CMCON=PLED_CMENBL|PLED_OE|PLED_CPOL|PLED_CREF|PLED_CM_CCH;
   // configuration CVref ON=1,CVRR=1,CVR=9
   CVRCON=BIT_15|BIT_5|9;
    // serial port config
   SER_LATSET=TX; // Set TX to high (idle state).
   SER_TRISCLR=TX;  // set serial output pin.
   //keyboard
   I2C1CONbits.DISSLW=1; // see pic32mx1xxx/2xxx-errata.pdf rev. E, point 9
   // Peripheral Pin Select
   // see pps.h
   PPSUnLock;  // unlock PPS to enable configuration
   PPSOutput(4, RPB10, U2TX);  // U2TX on PB10
   PPSInput (2, U2RX, RPB11);  // U2RX on PB11
   PPSOutput(2,RPB5,OC2); // OC2  on PB5, VGA HSync signal.
   PPSOutput(3,RPB13,OC4); // video SPI1 frame sync pulse ouput.
   PPSOutput(3,RPB6,SDO1); // SDO1 VGA video output on RB6
   PPSInput(1,SS1,RPB7); // SPI1 SS1 input on RPB7
   PPSInput(3,SDI2,RPA4); // SD card and SPI RAM SDI (MISO) on RA4
   PPSOutput(2,RPB8,SDO2); // SD card and SPI RAM SDO  (MOSI) on RB8
   PPSOutput(4,RPB9,OC3); //  OC3 audio output on PB9
   PPSLock; // lock PPS to avoid accidental modification.
   // all these pins are VGA output pins
   VGA_TRISCLR=(SPI_TRIG_OUT|VSYNC_OUT|HSYNC_OUT|VIDEO_OUT);
   VGA_LATCLR=VIDEO_OUT;
   // store interface output pins
   STORE_TRISCLR=STORE_MOSI|STORE_CLK|SDC_SEL;
   SRAM_TRISCLR=SRAM_SEL;
}

// contrôle de la LED power
void power_led(pled_state_t state){
    if (state){
        PLED_CMCONSET=PLED_CPOL;
    }else{
        PLED_CMCONCLR=PLED_CPOL;
    }
}

// return the value of systicks counter
// millisecond since bootup
// rollover after ~ 50 days.
inline unsigned int ticks(void){
    return sys_ticks;
} //ticks()



// suspend exécution pour une durée en microsecondes.
inline void delay_us(unsigned int usec){
    T1CON=0;
    IFS0bits.T1IF=0;
    PR1=40*usec;
    T1CONbits.ON=1;
    while (!IFS0bits.T1IF);
    T1CONbits.ON=0;
}//delay_us()

// pause execution for duration in millisecond.
// idle loop.
void delay_ms(unsigned int msec){
    unsigned int t0;
    t0=sys_ticks+msec;
    while (sys_ticks!=t0);
} // delay_ms()

// set_timer(msec)
//  initialize la minuterie
// input:
//      msec  durée milliseconds
void set_timer(unsigned int msec){
    timer=msec;
}

void reset_timer(){
    timer=0;
}
// retourne vrai si timer=0
bool timeout(){
    return !timer;
}

// retourne la grandeur du plus gros morceau de RAM allouable sur le heap.
unsigned biggest_chunk(){
    unsigned size, last_failed=RAM_SIZE, last_succeed=0;
    void *ptr=NULL;

    size=RAM_SIZE;
    while ((last_failed-last_succeed)>=8){
        ptr=malloc(size);
        if (!ptr){
            last_failed=size;
            size-=(last_failed-last_succeed)>1;
        }else{
            free(ptr);
            last_succeed=size;
            size+=(last_failed-last_succeed)>1;
        }
    }
    return last_succeed;
}


// retourne le total de la mémoire RAM disponible.
// en additionnant la taille des morceaux allouables.
// Si le heap est très fragmenté et contient plus de 32
// morceaux allouables la valeur retournée sera incorrecte.
unsigned free_heap(){
    unsigned count=0,size, total=0;
    void* list[32];
    
    do{
        size=biggest_chunk();
        total+=size;
        if (size) list[count++]=malloc(size);
    }while (size && (count<32));
    while (count){
        free(list[--count]);
    }
    return total;
}


//MCU core timer interrupt
// period 1msec
// also control TONE timer
void __ISR(_CORE_TIMER_VECTOR, IPL2SOFT) CoreTimerHandler(void){
     sys_ticks++;
     unsigned int ctimer;
     
     ctimer=_CP0_GET_COMPARE();
     ctimer+=CORE_TICK_RATE;
     _CP0_SET_COMPARE(ctimer);
     mCTClearIntFlag();
     if (tone_play){
         duration--;
         if (duration<=audible){
                mTone_off();
        }
        if (duration==0){
            tone_play=0;
        }
     }
     if (timer){ timer--;}
}


