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