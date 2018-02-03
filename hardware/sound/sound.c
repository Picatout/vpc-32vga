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
volatile unsigned int *tones_list;


void tone(unsigned int freq, // frequency hertz
          unsigned int msec){ // duration  milliseconds
        //config OC3 for tone generation
        OC3CONbits.OCM = 5; // PWM mode
        OC3CONbits.OCTSEL=1; // use TIMER3
        OC3RS=0;
        T3CON=0;
        T3CONbits.TCKPS=3;
        PR3=(SYSCLK/8/freq)-1; // 50% duty cycle
        OC3R=SYSCLK/16/freq;
        duration=msec;
        fSound |=TONE_ON;
        mTone_on();
        T3CONbits.ON=1;

} //tone();

// play a sequence of tones
void tune(const unsigned int *buffer){
    tones_list=(unsigned int *)buffer;
    if (*tones_list && *(tones_list+1)){
        fSound |= PLAY_TUNE;
        IPC3bits.T3IP=2;
        IPC3bits.T3IS=3;
        IFS0bits.T3IF=0;
        IEC0bits.T3IE=1;
        tone(*tones_list++,*tones_list++);
    }
}//tune()


// TIMER3 interrupt service routine
// select next note to play
void __ISR(_TIMER_3_VECTOR, IPL2SOFT)  T3Handler(void){
    unsigned int f,d;
       mT3ClearIntFlag();
       if (fSound==PLAY_TUNE){
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
