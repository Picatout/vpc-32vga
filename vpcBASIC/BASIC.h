#ifndef BASIC_H_
#define BASIC_H_

#define DEFAULT_HEAP 4096

//options de compilation de vm.S
#define _CHECK_OPCODE
#define _CHECK_STACKS

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
    eERR_UNKNOWN,
    eERR_FILE_IO,
    eERR_BADOP=18,
    eERR_DSTACK_NOT_EMPTY,
    eERR_DSTACK_OVF,
    eERR_RSTACK_OVF
    };


#else
    ERR_NONE=0
    ERR_ALLOC=5
    ERR_BAD_OPCODE=18
    ERR_DSTACK_NOT_EMPTY=19
    ERR_DSTACK_OVF=20
    ERR_RSTACK_OVF=21
        
        
#endif //#ifndef _ASM_CODE_










#endif // BASIC_H_