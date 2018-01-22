/*  StackVM test
 */

#if defined _VM_TEST_

#include <string.h>
#include <plib.h>

#include "vm.h"
#include "../console.h"

extern int stackVM(const char* code, int* variables);



// variables système Forth
#define BASE (0)
#define DP (BASE+CELL_SIZE)
#define TOIN (DP+CELL_SIZE)
#define TSOURCE (TOIN+CELL_SIZE) // adresse et longueur
#define LATEST (TSOURCE+2*CELL_SIZE) // lien dernière entrée dictionnaire
#define TIB (LATEST+CELL_SIZE)
#define PAD (TIB+CELL_SIZE)
#define STATE (PAD+CELL_SIZE)
#define HP (STATE+CELL_SIZE)
#define FIRST_USER (HP+CELL_SIZE)

//const char* vm_words[]={
//    "BYE","LITERAL","DOCOL","RET","BRA","?BRA","EXEC","!","@","C!","C@",
//};

const unsigned char test_code[]={
    ICLIT,-1,ICLIT,2,IDO,IIFETCH,IDOT,ICLIT,-1,IPLOOP,5,ICR,
    ICLIT,8,ICLIT,0,IDO,IIFETCH,IDOT,ICLIT,1,IPLOOP,5,ICR,
    IDOTQ,5,'H','E','L','L','O',ICR,
    IWLIT,24,0,ICLIT,4,ISTAR,ILIT,6,0,0,0,ISLASH,IBYE};

//void compile(char *line,int size,int dp){
//    
//}

void test_vm(){
    int* variables;
    int dp=0,size;
//    unsigned char* code;
//    char line[80];
//    code=malloc(8192);
#define VAR_SIZE (128)    
    variables=malloc(VAR_SIZE);
    print_int(LOCAL_CON,StackVM(test_code,strlen(test_code),variables,VAR_SIZE),0);
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
    free(variables);
//    free(code);
}

#endif
