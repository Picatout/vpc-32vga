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

#include <string.h>
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

const float tone_fraction[]={
    0.0, // eTONE_PAUSE
    0.25,
    0.375,
    0.5, 
    0.625,
    0.75, // eTONE_STACCATO
    0.875, // eTONE_NORMAL
    1.0  // eTONE_LEGATO
};


volatile unsigned int duration;
volatile unsigned int audible;


volatile unsigned char tone_play;
volatile unsigned char tune_play;

static unsigned time_signature=4;

static tone_fraction_t fraction=eTONE_NORMAL;
static uint8_t tempo=120;
static uint8_t octave=4; // {0..6}
volatile static note_t *tones_list;

int sound_init(){
    OC3CONbits.OCM = 5; // PWM mode
    OC3CONbits.OCTSEL=1; // use TIMER3
    T3CON=(3<<4); // timer 3 prescale 1/8.
    IPC3bits.T3IP=2; // timer interrupt priority
    IPC3bits.T3IS=0; // sub-priority
    tone_play=0;
    tune_play=0;
    duration=0;
    audible=0;
    fraction=eTONE_NORMAL;
    return 0;
}

void tone(float freq, // fréquence hertz
          uint16_t msec) // durée  millisecondes
{       float count;
      
    tone_play=0;
    //configuration OC3 pour génération tonalité.
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
    audible=duration-msec*tone_fraction[fraction];
    mTone_on();
    T3CONbits.ON=1;
    tone_play=1;
} //tone();

// play a sequence of tones
void tune(const note_t *buffer){
    if (buffer && ((int)buffer->freq!=ePLAY_STOP)){
        tones_list=(note_t*)buffer;
        IFS0bits.T3IF=0;
        IEC0bits.T3IE=1;
        //fSound |= PLAY_TUNE;
        tone(tones_list->freq,tones_list->duration);
        tune_play=255;
        tones_list++;
    }
}//tune()

#define FRQ_BEEP 1000.0
#define BEEP_MSEC 40
void beep(){
    set_tone_fraction(eTONE_LEGATO);
    tone(FRQ_BEEP,BEEP_MSEC);
}

/*********************************
 * fonction en support à play()
 *********************************/
void set_tempo(uint8_t t){
   tempo=t; 
}

void set_tone_fraction(tone_fraction_t f){
    fraction=f&7;
}

// définie l'octave actif {0..6}    
void set_octave(unsigned o){
    octave=o%7;
}

const char note_name[]="CDEFGAB";
static int note_order(char c){
    char *adr;
    adr=strchr("CDEFGAB",c);
    if (!adr){
        return -1;
    }else{
        return adr-note_name;
    }
}

// calcule la durée de la note en fonction
// de n et des altération.
static unsigned calc_duration(int n,int alteration){
    
}

// retourne la fréquence en fonction de la note 
// et de l'octave actif.
static float frequency(unsigned note){
    return (1<<octave)*octave0[note];
}

static int number(char *nstr, int *i){
    int n=0,inc=0;
    while (isdigit(*nstr)){
        n=n*10+(*nstr)-'0';
        nstr++;
        inc++;
    }
    *i=inc;
    return n;
}

static bool free_play_list=false;
static bool play_background=false;

// joue la mélodie représentée par la chaîne de caractère
// ref: https://en.wikibooks.org/wiki/QBasic/Full_Book_View#PLAY
void play(const char *melody){
#define MAX_NOTES 32
    int i=0,n,inc,o,alteration;
    char c, *play_str;
    note_t *play_list,*first;
    
    play_list=malloc(MAX_NOTES*sizeof(note_t));
    first=play_list;
    play_str=malloc(strlen(melody)+1);
    strcpy(play_str,melody);
    uppercase(play_str);
    free_play_list=true;
    play_background=false;
    //compile la mélodie dans play_list
    while ((i<MAX_NOTES)&&(c=*play_str++)){
        switch(c){
            case 'L':
                n=number(play_str,&inc);
                play_str+=inc;
                if (n){
                    play_list->freq=eNOTE_LENGTH;
                    play_list->duration=n&255;
                    i++;
                    play_list++;
                }
                break;
            case 'M':
                c=*play_str++;
                switch (c){
                    case 'B':
                        play_background=true;
                        break;
                    case 'F':
                        play_background=false;
                        break;
                    case 'S':
                        play_list->freq=eNOTE_MODE;
                        play_list->duration=eTONE_STACCATO;
                        play_list++;
                        i++;
                        break;
                    case 'L':
                        play_list->freq=eNOTE_MODE;
                        play_list->duration=eTONE_LEGATO;
                        play_list++;
                        i++;
                        break;
                    case 'N':
                        play_list->freq=eNOTE_MODE;
                        play_list->duration=eTONE_NORMAL;
                        play_list++;
                        i++;
                        break;
                    default:;
                }//switch
                break;
            case 'N':
                n=number(play_str,&inc);
                play_str+=inc;
                if (n){
                    o=octave;
                    octave=n/12;
                    play_list->freq=frequency(n%12);
                    octave=o;
                }else{
                    play_list->freq=ePLAY_PAUSE;
                }
                play_list->duration=calc_duration(time_signature,0);
                play_list++;
                i++;
                break;
            case 'O':
                n=number(play_str,&inc);
                play_str+=inc;
                play_list->freq=eOCTAVE;
                play_list->duration=n;
                play_list++;
                i++;
                break;
            case 'P':
                n=number(play_str,&inc);
                play_str+=inc;
                play_list->freq=ePLAY_PAUSE;
                play_list->duration=calc_duration(n,0);
                play_list++;
                i++;
                break;
            case 'T':
                n=number(play_str,&inc);
                play_str+=inc;
                if (n>=32 && n<256){
                    play_list->freq=eTEMPO;
                    play_list->duration=n;
                    play_list++;
                    i++;
                }
                break;
            case '<':
                play_list->freq=eOCTAVE_DOWN;
                play_list++;
                i++;
                break;
            case '>':
                play_list->freq=eOCTAVE_UP;
                play_list++;
                i++;
                break;
            case 'X': // joue une sous-chaîne
                break;
            default:
                n=note_order(c);
                if (n>-1){
                    alteration=0;
                    c=*play_str;
                    switch(c){
                        case '+':
                        case '#':
                            n++;
                            play_str++;
                            break;
                        case '-':
                            n--;
                            play_str++;
                            break;
                        case '.':
                            alteration=1;
                            play_str++;
                            break;
                        default:
                            if (isdigit(c)){
                                n=number(play_str,&inc);
                            }else{
                                
                            }
                    }//swtich
                }
        }//switch
    }
    free(play_str);
    tune(first);
    if (!play_background){
        while (tune_play);
    }
}



// TIMER3 interrupt service routine
// select next note to play
void __ISR(_TIMER_3_VECTOR, IPL2SOFT)  T3Handler(void){
    float f;
    uint16_t d;
    
       mT3ClearIntFlag();
       if (tune_play && !tone_play){
           f=tones_list->freq;
           d=tones_list->duration;
           if (f>0.0){
               set_tone_fraction(tones_list->fraction);
               tone(f,d);
           }else if (d>0){
               duration=d;
               audible=duration;
               tone_play=1;
           }else{
               tune_play=0;
           }
           tones_list++;
       }// if
}// T3Handler

