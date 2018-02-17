/*
* Copyright 2013,2017,2018 Jacques Deschênes
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "vt100.h"


void EscSeq(){
    UartPutch(SERIO,ESC);
    UartPutch(SERIO,LBRACKET);
}

void vt_clear(){
    UartPutch(SERIO, FF);
}

void vt_clear_eol(){
    UartPutch(SERIO,ESC);
    UartPutch(SERIO,LBRACKET);
    UartPutch(SERIO,'K');
}

void vt_clear_line(){
    UartPutch(SERIO,ESC);
    UartPutch(SERIO,LBRACKET);
    UartPutch(SERIO,'2');
    UartPutch(SERIO,'K');
    
}


