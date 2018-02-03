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

// flags in fSound variable
#define TONE_ON  (1)  // tone active
#define PLAY_TUNE (2) // play tune active
    
#define mTone_off() (OC3CONbits.ON=0)
#define mTone_on()  (OC3CONbits.ON=1)

extern volatile unsigned char fSound; // flags variable
extern volatile unsigned int duration; // sound duration

// generate a tone
void tone(unsigned int freq, unsigned int duration);
// play a sequence of tones.
void tune(const unsigned int *buffer);

#ifdef	__cplusplus
}
#endif

#endif	/* SOUND_H */

