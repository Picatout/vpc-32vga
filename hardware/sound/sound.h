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
 * File:   sound.h
 * Author: Jacques Deschênes
 *
 * Created on 13 septembre 2013, 20:42
 * rev: 2017-07-31
 */

#ifndef SOUND_H
#define	SOUND_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>
    
    
// flags in fSound variable
#define TONE_ON  (1)  // tone active
#define PLAY_TUNE (2) // play tune active
    
#define mTone_off() OC3CONbits.ON=0
#define mTone_on()  OC3CONbits.ON=1

extern volatile unsigned char tone_play;
extern volatile unsigned char tune_play;
//extern volatile unsigned char fSound; // flags variable
extern volatile unsigned int duration; // durée totale de la note
extern volatile unsigned int audible; // durée audible de la note.

typedef enum TONE_FRACTION{
    eTONE_PAUSE,      // silence
    eTONE_2ON8,       // 2/8
    eTONE_3ON8,       // 3/8        
    eTONE_4ON8,       // 4/8
    eTONE_5ON8,       // 5/8        
    eTONE_STACCATO,   // 6/8
    eTONE_NORMAL,     // 7/8
    eTONE_LEGATO,     // 8/8
}tone_fraction_t;

typedef struct note{
    float freq; // fréquence
    uint16_t  duration; // durée totale en milisecondes
    uint16_t  fraction; // durée audible de la note. 
}note_t;

// initialisation son
int sound_init();
// fait entendre un son. Le son est joué en arrière-plan.
void tone(float freq, uint16_t duration);
// play a sequence of tones.
void tune(const note_t *buffer);
// fait entendre un son cours de 1000 hertz
void beep();
//fait jouer une mélodie
int play(const char *melody);

#ifdef	__cplusplus
}
#endif

#endif	/* SOUND_H */

