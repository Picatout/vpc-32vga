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

const float octave0[12]={
    32.70, // DO
    34.65, // DO#,RÉb
    36.71, // RÉ
    38.89, // RÉ#,MIb
    41.20, // MI
    43.65, // FA
    46.25, // FA#,SOLb
    49.0,  // SOL
    51.91, // SOL#,LAb
    55.0,  // LA
    58.27, // LA#,SIb
    61.74, // SI
};

const float tone_mode[]={
    0.0, // eTONE_PAUSE
    0.5, // eTONE_SHORT
    0.75, // eTONE_STACCATO
    0.875, // eTONE_NORMAL
    1.0  // eTONE_LEGATO
};

static tone_mode_t mode=eTONE_NORMAL;
static uint8_t tempo=120;
static uint8_t octave=4; // {0..6}

volatile unsigned char fSound=0; // flags
volatile unsigned int duration;
volatile unsigned int audible;
static unsigned note_length=4;

volatile static note_t *tones_list;

int sound_init(){
    OC3CONbits.OCM = 5; // PWM mode
    OC3CONbits.OCTSEL=1; // use TIMER3
    T3CON=(3<<4); // timer 3 prescale 1/8.
    IPC3bits.T3IP=2; // timer interrupt priority
    IPC3bits.T3IS=0; // sub-priority
    return 0;
}

// retourne la fréquence en fonction de la note 
// et de l'octave actif.
static float frequency(unsigned note){
    return (1<<octave)*octave0[note];
}

void tone(float freq, // frequency hertz
          unsigned int msec) // duration  milliseconds
{       float count;
        //config OC3 for tone generation
        OC3RS=0;
        T3CONbits.ON=0;
        if (freq<100.0){
            count=(SYSCLK>>5)/freq;
            T3CONbits.TCKPS=5;
        }else{
            count=(SYSCLK>>3)/freq;
            T3CONbits.TCKPS=3;
        }
        PR3=(uint16_t)count-1;
        OC3R=(uint16_t)(count/2);
        duration=msec;
        audible=duration*tone_mode[mode];
        fSound |=TONE_ON;
        mTone_on();
        T3CONbits.ON=1;
} //tone();

// play a sequence of tones
void tune(const note_t *buffer){
    if (buffer && ((int)buffer->freq!=ePLAY_STOP)){
        tones_list=(note_t*)buffer;
        IFS0bits.T3IF=0;
        IEC0bits.T3IE=1;
        fSound |= PLAY_TUNE;
        tone(tones_list->freq,tones_list->duration);
        tones_list++;
    }
}//tune()

#define FRQ_BEEP 1000
#define BEEP_MSEC 40
void beep(){
    tone(FRQ_BEEP,BEEP_MSEC);
}

// joue jusqu'à 32 notes en arrière plan.
static unsigned int backplay[64];


void set_tempo(uint8_t t){
   tempo=t; 
}

void set_tone_mode(tone_mode_t m){
    mode=m;
}

// définie l'octave actif {0..6}    
void set_octave(unsigned o){
    octave=o;
}

static bool free_play_list=false;

// joue la mélodie représentée par la chaîne de caractère
// ref: https://en.wikibooks.org/wiki/QBasic/Full_Book_View#PLAY
void play(const char *melody, bool background){
#define MAX_NOTES 32
    int i=0;
    char c;
    note_t *play_list;
    play_list=malloc(MAX_NOTES*sizeof(note_t));
    free_play_list=true;
    //compile la mélodie dans play_list
    while ((i<MAX_NOTES)&&(c=*melody++)){
        switch(c){
            case 'L':
            case 'l':
                break;
            case 'M':
            case 'm':
                break;
            case 'N':
            case 'n':
                break;
            case 'O':
            case 'o':
                break;
            case 'P':
            case 'p':
                break;
            case 'T':
            case 't':
                break;
            case '<':
                break;
            case '>':
                break;
            default:;
                
        }//switch
    }
    tune(play_list);
    if (!background){
        while (fSound&PLAY_TUNE);
    }
}



// TIMER3 interrupt service routine
// select next note to play
void __ISR(_TIMER_3_VECTOR, IPL2SOFT)  T3Handler(void){
    float f;
    unsigned d;
    bool next=true;
    
       mT3ClearIntFlag();
       if (fSound&PLAY_TUNE){
           f=tones_list->freq;
           d=tones_list->duration;
           switch ((int)f){
               case eOCTAVE_UP:
                    if (octave<6) {octave++;}
                    break;
               case eOCTAVE_DOWN:
                    if (octave){octave--;}
                    break;
               case eOCTAVE:
                    octave=d;
                    break;
               case eNOTE_LENGTH:
                    note_length=d;
                    break;
               case eTEMPO:
                    tempo=d;
                    break;
               case ePLAY_PAUSE:
                    if (!duration){
                        duration=d;
                        fSound|=TONE_ON;
                    }else{
                        next=false;
                    }
                    break;
               case ePLAY_STOP:
                    fSound&=~PLAY_TUNE;
                    IEC0bits.T3IE=0;
                    T3CONbits.ON=0;
                    next=false;
                    if (free_play_list){
                        free((void*)tones_list);
                        free_play_list=false;
                    }
                    break;
               default:
                   if (!duration){
                        tone(f,d);
                   }else{
                       next=false;
                   }
           }//switch
       }// if
       if (next){
           tones_list++;
       }
}// T3Handler

