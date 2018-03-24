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
    IDOTQ,5,'H','E','L','L','O',ICR,
    IWLIT,24,0,IDUP,IDOT,ICLIT,4,IDUP,IDOT,ISTAR,IDUP,IDOT,ILIT,6,0,0,0,ISLASH,
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
