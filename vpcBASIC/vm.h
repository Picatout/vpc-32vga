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
 * File:   vm.h
 * Author: Jacques Deschênes
 *
 * Created on 31 mai 2013, 16:40
 * rev: 2017-08-01
 */

#ifndef OPCODES_H
#define	OPCODES_H

#include <stdint.h>

// op codes
enum OP_CODES{
 IBYE,
 IABORT,
 ILIT,
 IWLIT,
 ICLIT,
 IBRA , 
 IQBRA ,
 ISTORE , 
 IFETCH , 
 ICSTORE ,
 ICFETCH ,
 IDROP ,  
 IDUP , 
 ISWAP ,
 IOVER , 
 IPICK ,
 INEG,
 IPLUS ,
 ISUB  ,
 ISTAR  , 
 ISLASH , 
 IMOD , 
 ILTZ , 
 IZEQUAL ,
 IAND ,   
 IOR  ,  
 IXOR  , 
 INOT  , 
 IBOOL_NOT,
 IBOOL_OR,
 IBOOL_AND,
 ITICKS ,
 ISLEEP, 
 IQDUP  , 
 IPLUS1 ,
 IMINUS1 ,
 IPLUSSTR ,
 IROT  ,  
 INROT,
 IMIN ,
 IMAX , 
 IABS ,
 ILSHIFT,
 IRSHIFT, 
 IQBRAZ, 
 IDCNT,
 IEQUAL, 
 INEQUAL, 
 ILESS,  
 IGREATER ,
 ILTEQ , 
 IGTEQ ,  
 IFORSAVE,  
 IFORREST , 
 IFOR, 
 IFORTEST ,
 IFORNEXT,   
 IRANDOMIZE, 
 IRND , 
 IKEY , 
 IQRX ,  
 IEMIT , 
 IPSTR ,
 ICR , 
 IDOT,
 ITYPE, 
 ISPACES,
 ISOUND,
 IBEEP,
 IPLAY,
 ICLS, 
 IUBOUND, 
 ILCSTORE,
 ILCFETCH,
 ILCADR,
 IFRAME, 
 ICALL, 
 ILEAVE,
 ILOCAL,
 ITRACE,
 ISTRADR, 
 ILEN,    
 IREADLN,
 I2INT,
 IALLOC,  
 ISTRALLOC, 
 ISTRFREE,
 IFREENOTREF,
 ISTRCPY,
 ICURLINE,
 ICURCOL,
 ILOCATE, 
 IBTEST,   
 ISETTMR,
 ITIMEOUT,
 IINV_VID,
 ISCRLDN, 
 ISCRLUP,  
 IINSERTLN, 
 IPGET,
 IPSET,
 IPXOR,
 ILINE,
 IRECT,
 IBOX,
 ICIRCLE,
 IELLIPSE,
 IPOLYGON,
 IFILL,
 ISPRITE,
 IVGACLS,
 ISRCLEAR,
 ISRREAD,
 ISRWRITE,
 ISAVESCR,
 IRESTSCR,
 ISRLOAD,
 ISRSAVE,
 IMDIV,
 I2STR,
 IASC,
 ICHR,
 IDATE,
 ITIME,
 IAPPEND,
 IINSERT,
 ILEFT,
 IMID,
 IPREPEND,
 IRIGHT,
 ISUBST,
 IINSTR,
 ISTRCMP,
 IUCASE,
 ILCASE,
 ISTRSTORE,
 ISTRHEX,
 IDP0,
 IFCLOSE,
 IFCLOSEALL,
 IEOF,
 IFOPEN,
 IEXIST,
 ISEEK,
 IGETC,
 IPUTC,
 IWRITEFIELD,
 IREADFIELD,
 IF2STR,
 ISTR2F,
 IFNEG,
 IF2INT,
 II2FLOAT,
 IFADD,
 IFSUB,
 IFMUL,
 IFDIV,
 ISINE , 
 ICOS ,  
 ITAN , 
 IATAN ,
 IACOS ,
 IASINE ,
 ISQRT,
 IEXP,
 IPOWER,
 ILOG,
 ILOG10,
 IFABS,
 IFLOOR,
 ICEIL,
 IFMOD,
 IFDOT,
 IFMIN,
 IFMAX,
 ICON,        
 IENV,
};





//#define CELL_SIZE (4)

//#define DSTACK_SIZE (32*CELL_SIZE)
//#define RSTACK_SIZE (32*CELL_SIZE)

#ifndef _ASM_CODE_
extern int StackVM(const uint8_t* prog_space);
#endif

#endif	/* OPCODES_H */

