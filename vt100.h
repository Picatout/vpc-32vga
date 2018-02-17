/*
* Copyright 2013,2017,2018 Jacques Desch�nes
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
 * File:   vt100.h
 * Author: jacques
 *
 * Created on 13 f�vrier 2018, 13:12
 */

#ifndef VT100_H
#define	VT100_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "hardware/HardwareProfile.h"
#include "hardware/serial_comm/serial_comm.h"    
  
#define ESC 27
#define CR  13
#define LF  10
#define TAB 9
#define SPACE 32
#define FF 12
#define LBRACKET 91
    
    
    void vt_clear();
    void vt_clear_eol();
    void vt_clear_line();
    

#ifdef	__cplusplus
}
#endif

#endif	/* VT100_H */

