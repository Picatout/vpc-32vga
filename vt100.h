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
 * File:   vt100.h
 * Author: jacques
 *
 * Created on 13 février 2018, 13:12
 */

#ifndef VT100_H
#define	VT100_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <GenericTypeDefs.h>
    
#include "hardware/HardwareProfile.h"
#include "hardware/serial_comm/serial_comm.h"    
#include "ascii.h"
#include "hardware/tvout/display.h"
    
#define ESC 27
#define CR  13
#define LF  10
#define TAB 9
#define SPACE 32
#define FF 12
#define LBRACKET 91
    
    int vt_init();
    unsigned char vt_get_char();
    unsigned char vt_wait_char();
    void vt_clear_screen();
    void vt_clear_eol();
    void vt_clear_line(unsigned line);
    unsigned vt_get_curpos();
    void vt_set_curpos(int x, int y);
    void vt_put_char(char c);
    void vt_print(const char *str);
    void vt_println(const char *str);
    void vt_spaces(unsigned char count);
    void vt_invert_video(BOOL yes);
    void vt_crlf();
    void vt_scroll_up();
    void vt_scroll_down();
    void vt_set_tab_width(int width);
    int  vt_get_tab_width();
    void vt_insert_line();
    
#ifdef	__cplusplus
}
#endif

#endif	/* VT100_H */

