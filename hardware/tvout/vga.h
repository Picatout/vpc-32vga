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
 * File:   vga.h
 * Author: Jacques Deschênes
 *
 * Created on 26 août 2013, 07:48
 * rev: 2017-07-31
 */

#ifndef VGA_H
#define	VGA_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <GenericTypeDefs.h>
    
#define FRAME_LINES 525 // lines per VGA frame
#define VRES 240  // vertical pixels
#define HRES 480  // horizontal pixels
#define PPB 8     // pixels per byte
#define BMP_SIZE (VRES*HRES/PPB) // video buffer size

extern unsigned int video_bmp[VRES][HRES/32];

typedef void (*cursor_tmr_callback_f)(void);

extern void enable_cursor_timer(BOOL enable, cursor_tmr_callback_f cb);


// Hardware initialization
    void VideoInit(void);

#ifdef	__cplusplus
}
#endif

#endif	/* VGA_H */

