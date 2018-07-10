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
/*  StackVM test
 */

#if defined _VM_TEST_

#include <string.h>
#include <plib.h>

#include "vm.h"
#include "../console.h"






//const char* vm_words[]={
//    "BYE","LITERAL","DOCOL","RET","BRA","?BRA","EXEC","!","@","C!","C@",
//};

const unsigned char test_code[]={
    ICLIT,0,IDUP,ICLIT,5,INEQUAL,IQBRAZ,7,IDUP,IDOT,ICLIT,1,IPLUS,IBRA,-13,IBYE,
    IPSTR,'H','E','L','L','O',0,ICR,
    ILIT,24,0,0,0,IDUP,IDOT,ICLIT,4,IDUP,IDOT,ISTAR,IDUP,IDOT,ILIT,6,0,0,0,ISLASH,
    IDUP,IDOT,IBYE};

//void compile(char *line,int size,int dp){
//    
//}

int test_vm(){
//    int dp=0,size;
//    unsigned char* code;
//    char line[80];
//    code=malloc(8192);
    return StackVM(test_code);
//    while (1){
//        if (!(size=readline(LOCAL_CON,line,80))) break;
//        upper(line);
//        if (!strcmp(line,"RUN")){
//            print_hex(LOCAL_CON,StackVM(code,variables),0);
//            print(LOCAL_CON," ok\r");
//            dp=0;
//            memset(code,0,8192);
//        }else{
//            compile(line,size,dp);
//        }
//    }
}

#endif
