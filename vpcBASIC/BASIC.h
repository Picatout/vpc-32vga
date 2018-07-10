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
#ifndef BASIC_H_
#define BASIC_H_

#define DEFAULT_HEAP 4096

/////////////////////////////////
//options de compilation de vm.S
/////////////////////////////////
// sortie contrôlée de la VM sur mauvais code opérationnel
#define _CHECK_OPCODE
// sortie contrôlée de la VM sur débordement de dstack ou rstack
#define _CHECK_STACKS
// sortie contrôlée de la VM sur requête utiliseur CTRL_C
//#define _CHECK_USER_ABORT

#ifndef _ASM_CODE_


enum BASIC_OPTIONS{
    BASIC_PROMPT,
    EXEC_FILE,
    EXEC_STRING,
    EXEC_STAY,
};

enum ERROR_CODES{
    eERR_NONE,
    eERR_MISSING_ARG,
    eERR_EXTRA_ARG,
    eERR_BAD_ARG,
    eERR_BAD_VAR,
    eERR_SYNTAX,
    eERR_ALLOC,
    eERR_REDEF,
    eERR_NOT_ARRAY,
    eERR_BOUND,
    eERR_PROGSPACE,
    eERR_JUMP,
    eERR_CSTACK_UNDER,
    eERR_CSTACK_OVER,
    eERR_STR_ALLOC,
    eERR_DIM_FORBID,
    eERR_CONST_FORBID,
    eERR_DUPLICATE,
    eERR_NOT_DEFINED,
    eERR_BAD_ARG_COUNT,
    eERR_UNKNOWN,
    eERR_CMD_ONLY,
    eERR_BAD_FILE,
    eERR_PROG_MOVED,
    eERR_FILE_IO,
    eERR_FILE_ALREADY_OPEN,
    eERR_FILE_NOT_OPENED,
    eERR_NO_FILE_HANDLE,
    eERR_BADOP,
    eERR_DSTACK_NOT_EMPTY,
    eERR_DSTACK_OVF,
    eERR_RSTACK_OVF,
    eERR_USER_ABORT,
    eERR_PLAY,
    };

#define FIRST_VM_ERROR 28

    
#else
    FIRST_VM_ERROR=28
    ERR_NONE=0
    ERR_ALLOC=6
    ERR_BAD_OPCODE=FIRST_VM_ERROR
    ERR_DSTACK_NOT_EMPTY=FIRST_VM_ERROR+1
    ERR_DSTACK_OVF=FIRST_VM_ERROR+2
    ERR_RSTACK_OVF=FIRST_VM_ERROR+3
    ERR_USER_ABORT=FIRST_VM_ERROR+4    
    ERR_PLAY=FIRST_VM_ERROR+5
            
#endif //#ifndef _ASM_CODE_










#endif // BASIC_H_