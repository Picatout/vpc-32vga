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
 * File:   sound.c
 * Author: Jacques Deschênes
 *
 * Created on 13 septembre 2013, 20:42
 * rev: 2017-07-31
 */


#include <plib.h>
#include "../HardwareProfile.h"
#include "sound.h"


volatile unsigned char fSound=0; // flags
volatile unsigned int duration;
volatile static unsigned int *tones_list;

int sound_init(){
    OC3CONbits.OCM = 5; // PWM mode
    OC3CONbits.OCTSEL=1; // use TIMER3
    T3CON=(3<<4); // timer 3 prescale 1/8.
    IPC3bits.T3IP=2; // timer interrupt priority
    IPC3bits.T3IS=0; // sub-priority
    return 0;
}

void tone(unsigned int freq, // frequency hertz
          unsigned int msec){ // duration  milliseconds
        //config OC3 for tone generation
        OC3RS=0;
        T3CONbits.ON=0;
        PR3=(SYSCLK/8/freq)-1; 
        OC3R=SYSCLK/16/freq; // 50% duty cycle
        duration=msec;
        fSound |=TONE_ON;
        mTone_on();
        T3CONbits.ON=1;
} //tone();

// play a sequence of tones
void tune(const unsigned int *buffer){
    if (*buffer && *(buffer+1)){
        tones_list=(unsigned int *)buffer;
        IFS0bits.T3IF=0;
        IEC0bits.T3IE=1;
        fSound |= PLAY_TUNE;
        tone(*tones_list++,*tones_list++);
    }
}//tune()


// TIMER3 interrupt service routine
// select next note to play
void __ISR(_TIMER_3_VECTOR, IPL2SOFT)  T3Handler(void){
    unsigned int f,d;
       mT3ClearIntFlag();
       if ((fSound&(TONE_ON|PLAY_TUNE))==PLAY_TUNE){
           f=*tones_list++;
           d=*tones_list++;
           if (d){
                if (f){
                    tone(f,d);
                }else{
                    duration=(*tones_list);
                    fSound |= TONE_ON;
                }
            }else{
               fSound&=~PLAY_TUNE;
               IEC0bits.T3IE=0;
               T3CONbits.ON=0;
           } // if 
       }//if 
}// T3Handler
