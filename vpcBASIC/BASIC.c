/*
* Copyright 2013,2014,2017,2018 Jacques Desch�nes
* This file is part of VPC-32VGA.
*
*     VPC-32VGA is free software: you can redistribute it and/or modify
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
*     along with VPC-32VGA.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Interpr�teur BASIC compilant du bytecode pour la machine � piles d�finie dans vm.S 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>

#include "../hardware/Pinguino/fileio.h"
#include "../hardware/HardwareProfile.h"
#include "../hardware/tvout/vga.h"
#include "../hardware/serial_comm/serial_comm.h"
#include "../hardware/ps2_kbd/keyboard.h"
#include "../hardware/Pinguino/diskio.h"
#include "../hardware/Pinguino/fileio.h"
#include "../console.h"
#include "../hardware/Pinguino/ff.h"
#include "../hardware/sound/sound.h"
#include "../hardware/syscall.h"
#include "../hardware/rtcc/rtcc.h"
#include "../reader.h"
#include "vm.h"
#include "BASIC.h"

enum frame_type_e {FRAME_SUB,FRAME_FUNC};

/* macros utilis�es par le compilateur BASIC*/


#define _byte0(n) ((uint32_t)(n)&0xff)
#define _byte1(n)((uint32_t)(n>>8)&0xff)
#define _byte2(n)(((uint32_t)(n)>>16)&0xff)
#define _byte3(n)(((uint32_t)(n)>>24)&0xff)


#define _arg_frame(n) bytecode(IFRAME);\
                      bytecode((uint8_t)n)

#define _litc(c)  bytecode(ICLIT);\
                  bytecode(_byte0(c))

#define _litw(w)  bytecode(IWLIT);\
                  bytecode(_byte0(w));\
                  bytecode(_byte1(w))

#define _prt_varname(v) lit((uint32_t)v->name);\
                      bytecode(ITYPE)

// pile de contr�le utilis�e par le compilateur
#define CTRL_STACK_SIZE 32
//#define cdrop() csp--;ctrl_stack_check()
#define _ctop() ctrl_stack[csp-1]
#define _cnext() ctrl_stack[csp-2]
#define _cpick(n) ctrl_stack[csp-n-1]


static bool exit_basic;


#define PAD_SIZE CHAR_PER_LINE
static char *pad; // tampon de travail

extern int complevel;

// utilis� comme code par d�faut pour les DECLARE SUB|FUNC
const uint8_t undefined_sub[3]={ICLIT,eERR_NOT_DEFINED,IABORT};

static uint8_t *progspace; // espace programme.
static unsigned dptr; //data pointer
static void* endmark; // pointeur derni�re variable globale
extern volatile uint32_t pause_timer;

static uint32_t ctrl_stack[CTRL_STACK_SIZE];
static int8_t csp;

static jmp_buf failed; // utilis� par setjmp() et lngjmp()

static reader_t std_reader; // source clavier
static reader_t file_reader; // source fichier
static reader_t *activ_reader=NULL; // source active
static uint32_t prog_size;
static uint32_t line_count;
static uint32_t token_count;
static bool program_loaded;
static bool run_it=false;
static uint32_t program_end;

#define NAME_MAX 32 //longueur maximale nom incluant z�ro terminal
#define LEN_MASK 0x1F   // masque longueur nom.
#define ADR  (void *)



typedef void (*fnptr)();

enum FN_TYPE{
    eFN_NOT, // ce n'est pas une fonction
    eFN_INT, // la fonction retourne un entier 32 bits ou un octet non sign�
    eFN_STR, // la fonction retourne une cha�ne
    eFN_FPT  // la fonction retourne un nombre virgule flottante, float32
};

//entr�e de commande dans liste
typedef struct{
    fnptr cfn; // pointeur fonction commande
    uint8_t len:5;//longueur du nom.
    uint8_t fntype:3; // type de donn�e retourn� par la fonction. 
    char *name; // nom de la commande
} dict_entry_t;


//types de donn�es
typedef enum {
    eVAR_INT, // INTEGER
    eVAR_BYTE, // BYTE
    eVAR_FLOAT, // FLOAT
    eVAR_STR, // STRING
    eVAR_SUB,   // SUB
    eVAR_FUNC   // FUNC
}var_type_e;

// nom des types de donn�es
const char *TYPE_NAME[]={
    "INTEGER",
    "BYTE",
    "FLOAT",
    "STRING",
    0
};

typedef struct _var{
    struct _var* next; // prochaine variable dans la liste
    uint16_t len:5; // longueur du nom
    uint16_t ro:1; // bool�en, c'est une constante
    uint16_t array:1; // bool�en, c'est un tableau
    uint16_t local:1; // book�en, c'est une variable locale.
    uint16_t vtype:3; // var_type_e
    uint16_t dim:5; // nombre de dimension du tableau ou nombre arguments FUNC|SUB
    char *name;  // nom de la variable
    union{
        uint8_t byte; // variable octet
        int32_t n;    // entier ou nombre d'arguments et de variables locales pour func et sub
        char *str;    // adresse cha�ne asciiz
        void *adr;    // addresse tableau,sub ou func
    };
}var_t;

// type d'unit� lexicales
typedef enum {eNONE,eSTOP,eCOLON,eIDENT,eNUMBER,eSTRING,ePLUS,eMINUS,eMUL,eDIV,
              eMOD,eCOMMA,eLPAREN,eRPAREN,eSEMICOL,eEQUAL,eNOTEQUAL,eGT,eGE,eLT,eLE,
              eEND, eELSE,eCMD,eKWORD,eCHAR} tok_id_t;

// longueur maximale d'une ligne de texte source.
#define MAX_LINE_LEN 128

// unit� lexicale.              
typedef struct _token{
   tok_id_t id;
   union{
        int n; //  entier
        float f; // nombre virgule flottante 32bits
        int kw; // indice dans le dictionnaire KEYWORD
   };
   char str[MAX_LINE_LEN]; // cha�ne repr�sentant le jeton.
}token_t;



static token_t token;
static bool unget_token=false;
// prototypes des fonctions
static void clear();
static void kw_abs();
static void kw_append();
static void kw_asc();
static void bad_syntax();
static void kw_beep();
static void kw_box();
static void kw_btest();
static void kw_bye();
static void kw_case();
static void kw_chr();
static void kw_circle();
static void kw_cls();
static void kw_const();
static void kw_curcol();
static void kw_curline();
static void kw_date();
static void kw_declare();
static void kw_dim();
static void kw_do();
static void kw_ellipse();
static void kw_else();
static void kw_end();
static void kw_exit();
static void kw_fill();
static void kw_for();
static void kw_free();
static void kw_func();
static void kw_getpixel();
static void kw_if();
static void kw_input();
static void kw_insert();
static void kw_insert_line();
static void kw_instr();
static void kw_invert_video();
static void kw_key();
static void kw_left();
static void kw_len();
static void kw_let();
static void kw_line();
static void kw_local();
static void kw_locate();
static void kw_loop();
static void kw_max();
static void kw_mdiv();
static void kw_mid();
static void kw_min();
static void kw_next();
static void kw_play();
static void kw_polygon();
static void kw_prepend();
static void kw_print();
static void kw_putc();
static void kw_randomize();
static void kw_rect();
static void kw_ref();
static void kw_rem();
static void kw_restore_screen();
static void kw_return();
static void kw_right();
static void kw_rnd();
static void kw_run();
static void kw_save_screen();
static void kw_scrlup();
static void kw_scrldown();
static void kw_select();
static void kw_setpixel();
static void kw_set_timer();
static void kw_shl();
static void kw_shr();
static void kw_sleep();
static void kw_sound();
static void kw_sprite();
static void kw_srclear();
static void kw_srload();
static void kw_srread();
static void kw_srsave();
static void kw_srwrite();
static void kw_string();
static void kw_sub();
static void kw_subst();
static void kw_then();
static void kw_ticks();
static void kw_time();
static void kw_timeout();
static void kw_tkey();
static void kw_trace();
static void kw_tune();
static void kw_ubound();
static void kw_use();
static void kw_val();
static void kw_vgacls();
static void kw_waitkey();
static void kw_wend();
static void kw_while();
static void kw_xorpixel();

static void patch_fore_jump(int target_addr);
static void patch_back_jump(int target_addr);
static void ctrl_stack_nrot();
static void ctrl_stack_rot();
static void ctrl_stack_check();
static void cpush(uint32_t);
static uint32_t cpop();
static void cdrop();
//static unsigned get_arg_count(void* fn_code);
static void expression();
static void factor();
char* string_alloc(unsigned size);
void string_free(char *);
static void compile();
static void bool_expression();
static void compile_file(const char *file_name);
static var_t *var_search(const char* name);
static void literal_string(char *lit_str);
static bool string_func(var_t *var);
static void string_expression();
static int search_type();
#ifdef DEBUG
static void print_prog(int start);
static void print_cstack();
#endif

//identifiant KEYWORD doit-�tre dans le m�me ordre que
//dans la liste KEYWORD
enum {eKW_ABS,eKW_AND,eKW_APPEND,eKW_ASC,eKW_BEEP,eKW_BOX,eKW_BTEST,eKW_BYE,eKW_CASE,
      eKW_CHR,eKW_CIRCLE,
      eKW_CLEAR,eKW_CLS,
      eKW_CONST,eKW_CURCOL,eKW_CURLINE,eKW_DATE,eKW_DECLARE,eKW_DIM,eKW_DO,
      eKW_ELLIPSE,eKW_ELSE,
      eKW_END,eKW_EXIT,eKW_FILL,
      eKW_FOR,eKW_FREE,eKW_FUNC,eKW_GETPIXEL,eKW_IF,eKW_INPUT,eKW_INSERT,eKW_INSTR,
      eKW_INSERTLN,
      eKW_INVVID,eKW_KEY,eKW_LEFT,eKW_LEN,
      eKW_LET,eKW_LINE,eKW_LOCAL,eKW_LOCATE,eKW_LOOP,eKW_MAX,eKW_MDIV,eKW_MID,eKW_MIN,eKW_NEXT,
      eKW_NOT,eKW_OR,eKW_PLAY,eKW_POLYGON,eKW_PREPEND,
      eKW_PRINT,eKW_PSET,eKW_PUTC,eKW_RANDOMISIZE,eKW_RECT,eKW_REF,eKW_REM,eKW_RESTSCR,
      eKW_RETURN,eKW_RIGHT,eKW_RND,eKW_RUN,eKW_SAVESCR,eKW_SCRLUP,eKW_SCRLDN,
      eKW_SELECT,eKW_SETTMR,eKW_SHL,eKW_SHR,eKW_SLEEP,eKW_SOUND,
      eKW_SPRITE,eKW_SRCLEAR,eKW_SRLOAD,eKW_SRREAD,eKW_SRSSAVE,eKW_SRWRITE,eKW_STR,
      eKW_SUB,eKW_SUBST,eKW_TIME,eKW_THEN,eKW_TICKS,
      eKW_TIMEOUT,eKW_TKEY,eKW_TRACE,eKW_TUNE,eKW_UBOUND,eKW_UNTIL,eKW_USE,eKW_VAL,
      eKW_VGACLS,
      eKW_WAITKEY,eKW_WEND,eKW_WHILE,eKW_XOR,eKW_XORPIXEL
};

//mots r�serv�s BASIC
static const dict_entry_t KEYWORD[]={
    {kw_abs,3,eFN_INT,"ABS"},
    {bad_syntax,3,eFN_NOT,"AND"},
    {kw_asc,3,eFN_INT,"ASC"},
    {kw_append,7,eFN_STR,"APPEND$"},
    {kw_beep,4,eFN_NOT,"BEEP"},
    {kw_box,3,eFN_NOT,"BOX"},
    {kw_btest,5,eFN_INT,"BTEST"},
    {kw_bye,3,eFN_NOT,"BYE"},
    {kw_case,4,eFN_NOT,"CASE"},
    {kw_chr,4,eFN_STR,"CHR$"},
    {kw_circle,6,eFN_NOT,"CIRCLE"},
    {clear,5,eFN_NOT,"CLEAR"},
    {kw_cls,3,eFN_NOT,"CLS"},
    {kw_const,5,eFN_NOT,"CONST"},
    {kw_curcol,6,eFN_INT,"CURCOL"},
    {kw_curline,7,eFN_INT,"CURLINE"},
    {kw_date,5,eFN_STR,"DATE$"},
    {kw_declare,7,eFN_NOT,"DECLARE"},
    {kw_dim,3,eFN_NOT,"DIM"},
    {kw_do,2,eFN_NOT,"DO"},
    {kw_ellipse,7,eFN_NOT,"ELLIPSE"},
    {kw_else,4,eFN_NOT,"ELSE"},
    {kw_end,3,eFN_NOT,"END"},
    {kw_exit,4,eFN_NOT,"EXIT"},
    {kw_fill,4,eFN_NOT,"FILL"},
    {kw_for,3,eFN_NOT,"FOR"},
    {kw_free,4,eFN_NOT,"FREE"},
    {kw_func,4,eFN_NOT,"FUNC"},
    {kw_getpixel,4,eFN_INT,"PGET"},
    {kw_if,2,eFN_NOT,"IF"},
    {kw_input,5,eFN_NOT,"INPUT"},
    {kw_instr,5,eFN_INT,"INSTR"},
    {kw_insert,7,eFN_STR,"INSERT$"},
    {kw_insert_line,8,eFN_NOT,"INSERTLN"},
    {kw_invert_video,9,eFN_NOT,"INVERTVID"},
    {kw_key,3,eFN_INT,"KEY"},
    {kw_left,5,eFN_STR,"LEFT$"},
    {kw_len,3,eFN_INT,"LEN"},
    {kw_let,3,eFN_NOT,"LET"},
    {kw_line,4,eFN_NOT,"LINE"},
    {kw_local,5,eFN_NOT,"LOCAL"},
    {kw_locate,6,eFN_NOT,"LOCATE"},
    {kw_loop,4,eFN_NOT,"LOOP"},
    {kw_max,3,eFN_INT,"MAX"},
    {kw_mdiv,4,eFN_INT,"MDIV"},
    {kw_mid,4,eFN_STR,"MID$"},
    {kw_min,3,eFN_INT,"MIN"},
    {kw_next,4,eFN_NOT,"NEXT"},
    {bad_syntax,3,eFN_NOT,"NOT"},
    {bad_syntax,2,eFN_NOT,"OR"},
    {kw_play,4,eFN_NOT,"PLAY"},
    {kw_polygon,7,eFN_NOT,"POLYGON"},
    {kw_prepend,8,eFN_STR,"PREPEND$"},
    {kw_print,5,eFN_NOT,"PRINT"},
    {kw_setpixel,4,eFN_NOT,"PSET"},
    {kw_putc,4,eFN_NOT,"PUTC"},
    {kw_randomize,9,eFN_NOT,"RANDOMIZE"},
    {kw_rect,4,eFN_NOT,"RECT"},
    {kw_ref,1,eFN_INT,"@"},
    {kw_rem,3,eFN_NOT,"REM"},
    {kw_restore_screen,7,eFN_NOT,"RESTSCR"},
    {kw_return,6,eFN_NOT,"RETURN"},
    {kw_right,6,eFN_STR,"RIGHT$"},
    {kw_rnd,3,eFN_INT,"RND"},
    {kw_run,3,eFN_NOT,"RUN"},
    {kw_save_screen,7,eFN_NOT,"SAVESCR"},
    {kw_scrlup,6,eFN_NOT,"SCRLUP"},
    {kw_scrldown,6,eFN_NOT,"SCRLDN"},
    {kw_select,6,eFN_NOT,"SELECT"},
    {kw_set_timer,6,eFN_NOT,"SETTMR"},
    {kw_shl,3,eFN_INT,"SHL"},
    {kw_shr,3,eFN_INT,"SHR"},
    {kw_sleep,5,eFN_NOT,"SLEEP"},
    {kw_sound,5,eFN_NOT,"SOUND"},
    {kw_sprite,6,eFN_NOT,"SPRITE"},
    {kw_srclear,7,eFN_NOT,"SRCLEAR"},
    {kw_srload,6,eFN_INT,"SRLOAD"},
    {kw_srread,6,eFN_NOT,"SRREAD"},
    {kw_srsave,6,eFN_INT,"SRSAVE"},
    {kw_srwrite,7,eFN_NOT,"SRWRITE"},
    {kw_string,4,eFN_STR,"STR$"},
    {kw_sub,3,eFN_NOT,"SUB"},
    {kw_subst,6,eFN_STR,"SUBST$"},
    {kw_then,4,eFN_NOT,"THEN"},
    {kw_time,5,eFN_STR,"TIME$"},
    {kw_ticks,5,eFN_INT,"TICKS"},
    {kw_timeout,7,eFN_INT,"TIMEOUT"},
    {kw_tkey,4,eFN_INT,"TKEY"},
    {kw_trace,5,eFN_NOT,"TRACE"},
    {kw_tune,4,eFN_NOT,"TUNE"},
    {kw_ubound,6,eFN_INT,"UBOUND"},
    {bad_syntax,5,eFN_NOT,"UNTIL"},
    {kw_use,3,eFN_NOT,"USE"},
    {kw_val,3,eFN_INT,"VAL"},
    {kw_vgacls,6,eFN_NOT,"VGACLS"},
    {kw_waitkey,7,eFN_INT,"WAITKEY"},
    {kw_wend,4,eFN_NOT,"WEND"},
    {kw_while,5,eFN_NOT,"WHILE"},
    {kw_xorpixel,4,eFN_NOT,"PXOR"},
    {NULL,0,eFN_NOT,""}
};


//recherche la commande dans
//le dictionnaire syst�me
// ( uaddr -- n)
static int dict_search(const  dict_entry_t *dict){
    int i=0, len;
    
    len=strlen(token.str);
    while (dict[i].len){
        if ((dict[i].len==len) && 
                !strcmp((const unsigned char*)token.str,dict[i].name)){
            break;
        }
        i++;
    }
    return dict[i].len?i:-1;
}//cmd_search()


static void print_token_info(){
    print(con,"\ntok_id: ");
    print_int(con,token.id,0);
    print(con,"\ntok_value: ");
    print_int(con,token.n,0);
    print(con,"token string: ");
    if (token.str){println(con,token.str);}
}

// messages d'erreurs correspondants � l'enum�ration BASIC_ERROR_CODES dans BASIC.h
static  const char* error_msg[]={
    "Existing BASIC ",  
    "missing argument\n",
    "too much arguments\n",
    "bad argument\n",
    "syntax error\n",
    "memory allocation error\n",
    "Constant can't be redefined\n",
    "undefined array variable\n",
    "array index out of range\n",
    "program space full\n",
    "jump out of range\n",
    "control stack underflow\n",
    "control stack overflow\n",
    "no more string space available\n",
    "Can't use DIM inside sub-routine\n",
    "Can't use CONST inside sub-routine\n",
    "Duplicate identifier\n",
    "Call to an undefined sub-routine\n",
    "Parameters count disagree with DECLARE\n",
    "Unknow variable or constant\n",
    "File open error\n",
    "VM bad operating code\n",
    "VM exited with data stack not empty\n",
    "VM parameters stack overflow\n",
    "VM call stack overflow\n",
    "Program aborted by user\n",
    "Erreur dans la commande PLAY",
 };
 
 
static void throw(int error){
    char message[64];
    
        crlf(con);
        if (error<FIRST_VM_ERROR){
            if (activ_reader->device==eDEV_SDCARD){
                print(con,"line: ");
                print_int(con,line_count,0);
                crlf(con);
            }else{
                print(con,"token#: ");
                print_int(con,token_count,0);
                crlf(con);
            }
        }
        strcpy(message,error_msg[error]);
        print(con,message);
        if (error<FIRST_VM_ERROR){
            print_token_info();
        }
#ifdef DEBUG    
        print_prog(program_end);
#endif    
        activ_reader->eof=true;
        longjmp(failed,error);
}

// name est-til un type de donn�e valide?
// retourne le id du type ou -1
int search_type(char *name){
    int i=0;
    while (TYPE_NAME[i]){
        if (!strcmp(TYPE_NAME[i],name)){
            return i;
        }else{
            i++;
        }
    }//while
    return -1;
}

static var_t *varlist=NULL;
static var_t *globals=NULL;
static bool var_local=false;

// allocation de l'espace pour une cha�ne asciiz sur le heap;
// length est la longueur de la chaine.
char* string_alloc(unsigned length){
    char *str;
    str=malloc(++length);
    if (!str){
        throw(eERR_STR_ALLOC);
    }
    return str;
}

// lib�ration de l'espace r�serv�e pour une cha�ne asciiz sur le heap.
void string_free(char *str){
    if ((uint32_t)str>=RAM_BEGIN && (uint32_t)str<RAM_END){
        free(str);
    }
}

void free_string_vars(){
    var_t *var;
    void *limit;
    char **str_array;
    int i,count;
    
    
    limit=progspace+prog_size;
    var=varlist;
    while (var){
        if ((var->vtype==eVAR_STR) && ! (var->local)){
            if (!var->array){
                string_free(var->str);
            }else{
                str_array=(char**)var->adr;
                count=*(unsigned*)str_array;
                for (i=1;i<=count;i++){
                    string_free(str_array[i]);
                }
            }
        }
        var=var->next;
    }
}

// d�clenche une exeption en cas d'�chec.
void *alloc_var_space(int size){
    void *newmark;
    
    newmark=endmark-size;
    if (((unsigned)newmark)&3){//alignement
        newmark=(void*)((unsigned)newmark&0xfffffff8);
    }
    if (newmark<=(void*)&progspace[dptr]){
        throw(eERR_ALLOC);
    }
    endmark=newmark;
    return endmark;
}//f

// les variables sont allou�es � la fin
// de progspace en allant vers le bas.
// lorsque endmark<=&progrspace[dptr] la m�moire est pleine
static var_t *var_create(char *name, void *value){
    int len;
    void *newend;
    var_t *newvar=NULL;
    void *varname=NULL;
    
    len=strlen(name);
    newvar=var_search(name);
    if ((!var_local && newvar)||(var_local && newvar && newvar->local)){
        throw(eERR_DUPLICATE);
    }
    newend=endmark;
    newvar=alloc_var_space(sizeof(var_t));
    newend=newvar;
    varname=alloc_var_space(len+1);
    newend=varname;
    strcpy(varname,name);
    newvar->ro=false;
    newvar->array=false;
    newvar->dim=0;
    newvar->local=false;
    if (var_local){
        newvar->local=true;
        newvar->n=*(int*)value;
        switch(name[len-1]){
            case '$':
                newvar->vtype=eVAR_STR;
                break;
            case '#':
                newvar->vtype=eVAR_BYTE;
                break;
            default:
                newvar->vtype=eVAR_INT;
        }
    }else{
        if (name[len-1]=='$'){ // variable chaine
            if (value){
                newvar->str=alloc_var_space(strlen(value)+1);
                newend=newvar->adr;
                strcpy(newvar->str,value);
            }
            else{
                newvar->str=NULL;
            }
            newvar->vtype=eVAR_STR;
        }else if (name[len-1]=='#'){ // variable octet
            newvar->vtype=eVAR_BYTE;
            if (value)
                newvar->byte=*((uint8_t*)value);
            else
                newvar->byte=0;
        }else{// entier 32 bits
            newvar->vtype=eVAR_INT;
            if (value){
                newvar->n=*((int*)value);
            }else{
                newvar->n=0;
            }
        }
    }
    endmark=newend;
    newvar->len=len;
    newvar->name=varname;
    newvar->next=varlist;
    varlist=newvar;
    return varlist;
}//f()

static var_t *var_search(const char* name){
    var_t* link;
    
    link=varlist;
    while (link){
        if (!strcmp(link->name,name)) break;
        link=link->next;
    }//while
    return link;
}//f()


// retourne vrai s'il s'agit d'une variable de type fonction
// qui retourne l'adresse d'une cha�ne.
static bool string_func(var_t *var){
    return (var->vtype==eVAR_FUNC) && (var->name[strlen(var->name)-1]=='$');
}

static void bytecode(uint8_t bc){
    if ((void*)&progspace[dptr]>=endmark) throw(eERR_PROGSPACE);
    progspace[dptr++]=bc;
}//f

// compile un entier lit�ral
static void lit(unsigned int n){
    int i;
    
    bytecode(ILIT);
    for (i=0;i<4;i++){
        bytecode(n&0xff);
        n>>=8;
    }
}

//calcul la distance entre slot_address+1 et dptr.
// le d�placement maximal authoriz� est de 32767 octets.
// la valeur du d�placement est d�pos�e dans la fente 'slot_address'.
static void patch_fore_jump(int slot_addr){
    int displacement;
    
    displacement=dptr-slot_addr-2;
    if (displacement>32767){throw(eERR_JUMP);}
    progspace[slot_addr++]=_byte0(displacement);
    progspace[slot_addr]=_byte1(displacement);
}//f

//calcul la distance entre dptr+1 et target_address
//le d�placement maximal authoriz� est de -32768 octets.
// la valeur du d�placement est d�pos� � la position 'dptr'
static void patch_back_jump(int target_addr){
    int displacement;
    
    displacement=target_addr-dptr-2;
    if (displacement<-32768){throw(eERR_JUMP);}
    progspace[dptr++]=_byte0(displacement);
    progspace[dptr++]=_byte1(displacement);
}

void cpush(uint32_t n){
    if (csp==CTRL_STACK_SIZE){throw(eERR_CSTACK_OVER);}
    ctrl_stack[csp++]=n;
}

uint32_t cpop(){
    if (!csp) {throw(eERR_CSTACK_UNDER);}
    return ctrl_stack[--csp];
}

static void cdrop(){
    if (!csp){throw(eERR_CSTACK_UNDER);}
    csp--;
}

// v�rifie les limites de ctrl_stack
static void ctrl_stack_check(){
    if (csp<0){
        throw(eERR_CSTACK_UNDER);
    }else if (csp==CTRL_STACK_SIZE){
        throw(eERR_CSTACK_OVER);
    }
}
// rotation des 3 �l�ments sup�rieur de la pile de contr�le
// de sorte que le sommet se retrouve en 3i�me position
static void ctrl_stack_nrot(){
    int temp;
    
    temp=_cpick(2);
    _cpick(2)=_ctop();
    _ctop()=_cnext();
    _cnext()=temp;
}

//rotation des 3 �l�ments sup�rieur de la pile de contr�le
// de sorte que le 3i�me �l�ment se retrouve au sommet
static void ctrl_stack_rot(){
    int temp;
    
    temp=_cpick(2);
    _cpick(2)=_cnext();
    _cnext()=_ctop();
    _ctop()=temp;
}

//caract�res qui s�parent les unit�es lexicale
//static bool separator(char c){
//	return strchr("()+-*/%,<>=':\"",c);
//}

//saute les espaces
static void skip_space(){
    char c=0;
    while ((c=reader_getc(activ_reader)==' ') || (c=='\t'));
    if (!activ_reader->eof) reader_ungetc(activ_reader);
}//f()

static void parse_string(){
	bool quote=false;
	bool escape=false;
	unsigned i=0;
	char c;
    
    c=reader_getc(activ_reader);
	while (!activ_reader->eof && !quote && (c>0)){
        if (!escape){
            switch (c){
            case '\\':
                escape=true;
                break;
            case '"':
                quote=true;
                break;
            default:
                token.str[i++]=c;
            }
        }else{
            switch (c){
                case '"':
                    token.str[i++]=c;
                    break;
                case 't':
                    token.str[i++]='\t';
                    break;
                case 'n':
                case 'r':
                    token.str[i++]='\n';
                    break;
                default:
                    throw(eERR_SYNTAX);
                    //return i;
            }
            escape=false;
        }
		c=reader_getc(activ_reader);
	}//while
    token.str[i++]=0;
	if (quote){
        reader_ungetc(activ_reader);
    } else{
        throw(eERR_SYNTAX);
    }
	token.id=eSTRING;
}//f()


static void parse_identifier(){
    char c;
    int i=1;

    while (!activ_reader->eof){
        c=reader_getc(activ_reader);
        if (!(isalnum(c) || c=='_'  || c=='$' || c=='#')){
            reader_ungetc(activ_reader);
            break;
        }
        token.str[i++]=toupper(c);
        if (c=='$' || c=='#') break;
    }
    if (i>LEN_MASK) i=LEN_MASK;
    token.str[i]=0;                  
    if ((i=dict_search(KEYWORD))>-1){
        token.id=eKWORD;
        token.kw=i;
    }else{
        token.id=eIDENT;
    }
}//f()

static void parse_integer(int base){
    char c;
    int n=0,d;
    
    c=reader_getc(activ_reader);
    while (!activ_reader->eof){
        if (isdigit(c)){
            d=c-'0';
            if (d>=base) throw(eERR_BAD_ARG);
        }else if (base==16 && isxdigit(c)){
            d=toupper(c)-'A'+10;
        }else{
            reader_ungetc(activ_reader);
            break;
        }
        n= n*base+d;
        c=reader_getc(activ_reader);
    }//while
    token.id=eNUMBER;
    token.n=n;
}//f()


static void next_token(){
    char c;
    
    if (unget_token){
        unget_token=false;
        return;
    }
    token.id=eNONE;
    token.n=0;
    token.str[0]=0;
    if (activ_reader->eof){
        return;
    }
    skip_space();
    c=reader_getc(activ_reader);
    if (c==-1){
        token.id=eNONE;
        token_count=0;
    }else{
        token_count++;
    }
    if (isalpha(c)){
        token.str[0]=toupper(c);
        parse_identifier();
    }else if (isdigit(c)){
        reader_ungetc(activ_reader);
        parse_integer(10);
    }else{
        switch(c){
            case '$': // nombre hexadecimal
                parse_integer(16);
                break;
            case '#': // nombre binaire
                parse_integer(2);
                break;
            case '\'': // commentaire
                token.id=eKWORD;
                token.kw=eKW_REM;
                token.str[0]='\'';
                token.str[1]=0;
                break;
            case '\\':
                token.id=eCHAR;
                token.n=reader_getc(activ_reader);
                token.str[0]=token.n;
                token.str[1]=0;
                break;
            case '"': // cha�ne de caract�res
                parse_string();
                break;
            case '+':
                token.id=ePLUS;
                token.str[0]='+';
                token.str[1]=0;
                break;
            case '-':
                token.id=eMINUS;
                token.str[0]='-';
                token.str[1]=0;
                break;
            case '*':
                token.id=eMUL;
                token.str[0]='*';
                token.str[1]=0;
                break;
            case '/':
                token.id=eDIV;
                token.str[0]='/';
                token.str[1]=0;
                break;
            case '%':
                token.id=eMOD;
                token.str[0]='%';
                token.str[1]=0;
                break;
            case '=':
                token.id=eEQUAL;
                token.str[0]='=';
                token.str[1]=0;
                break;
            case '>':
                token.str[0]='>';
                c=reader_getc(activ_reader);
                if (c=='='){
                    token.id=eGE;
                token.str[1]='=';
                token.str[2]=0;
                }else if (c=='<'){
                    token.id=eNOTEQUAL;
                    token.str[1]='<';
                    token.str[2]=0;
                }else{
                    token.id=eGT;
                    token.str[1]=0;
                    reader_ungetc(activ_reader);
                }
                break;
            case '<':
                token.str[0]='<';
                c=reader_getc(activ_reader);
                if (c=='='){
                    token.id=eLE;
                token.str[1]='=';
                token.str[2]=0;
                }else if (c=='>'){
                    token.id=eNOTEQUAL;
                    token.str[1]='<';
                    token.str[2]=0;
                }else{
                    token.id=eLT;
                    token.str[1]=0;
                    reader_ungetc(activ_reader);
                }
                break;
            case '(':
                token.id=eLPAREN;
                token.str[0]='(';
                token.str[1]=0;
                break;
            case ')':
                token.id=eRPAREN;
                token.str[0]=')';
                token.str[1]=0;
                break;
            case ',':
                token.id=eCOMMA;
                token.str[0]=',';
                token.str[1]=0;
                break;
            case ';':
                token.id=eSEMICOL;
                token.str[0]=';';
                token.str[1]=0;
                break;
            case '?': // alias pour PRINT
                token.id=eKWORD;
                token.kw=eKW_PRINT;
                token.str[0]='?';
                token.str[1]=0;
                break;
            case '@': // r�f�rence
                token.id=eKWORD;
                token.kw=eKW_REF;
                token.str[0]='@';
                token.str[1]=0;
                break;
            case '\n':
            case '\r':
                line_count++;
                if (activ_reader->device==eDEV_SDCARD){ // consid�r� comme un espace
                    next_token();
                    break;
                }
            case ':':    
                token.id=eSTOP;
                token.str[0]='\n';
                token.str[1]=0;
                token_count=0;
                break;
        }//switch(c)
    }//if
}//next_token()

//devrait-�tre � la fin de la commande
static void expect_end(){
    next_token();
    if (!((token.id>=eNONE) && (token.id<=eCOLON))) throw(eERR_EXTRA_ARG);
}//f()


static bool try_relation(){
    next_token();
    if (!((token.id>=eEQUAL) && (token.id<=eLE))){
        unget_token=true;
        return false;
    } 
    return true;
}//f

static bool try_boolop(){
    next_token();
    if ((token.id==eKWORD) && ((token.n==eKW_NOT) || (token.n==eKW_AND)||(token.n==eKW_OR))){
        return true;
    }else{
        unget_token=true;
        return false;
    }
}//f

static void expect(tok_id_t t){
    next_token();
    if (token.id!=t) throw(eERR_SYNTAX);
}

static bool try_addop(){
    next_token();
    if ((token.id==ePLUS) || (token.id==eMINUS)) return true;
    unget_token=true;
    return false;
}//f

static bool try_mulop(){
    next_token();
    if ((token.id==eMUL)||(token.id==eDIV)||token.id==eMOD) return true;
    unget_token=true;
    return false;
}//f


//typedef enum {eNO_FILTER,eACCEPT_ALL,eACCEPT_END_WITH,eACCEPT_ANY_POS,
//        eACCEPT_SAME,eACCEPT_START_WITH} filter_enum;
//
//typedef struct{
//    char *subs; // sous-cha�ne filtre.
//    filter_enum criteria; // crit�re
//}filter_t;
//
//static void parse_filter(){
//    char c;
//    int i=0;
//    skip_space();
//    while (!activ_reader->eof && 
//            (isalnum((c=reader_getc(activ_reader))) || c=='*' || c=='_' || c=='.')){
//        token.str[i++]=toupper(c);
//    }
//    reader_ungetc(activ_reader);
//    token.str[i]=0;
//    if (!i) token.id=eNONE; else token.id=eSTRING;
//}//f
//
//static void set_filter(filter_t *filter){
//    filter->criteria=eACCEPT_ALL;
//    parse_filter();
//    if (token.id==eNONE){
//        filter->criteria=eNO_FILTER;
//        return;
//    }
//    if (token.str[0]=='*' && token.str[1]==0){
//        return;
//    }
//    if (token.str[0]=='*'){
//        filter->criteria++;
//    }else{
//        filter->criteria=eACCEPT_SAME;
//    }
//    if (token.str[strlen(token.str)-1]=='*'){
//        token.str[strlen(token.str)-1]=0;
//        filter->criteria++;
//    }
//    switch(filter->criteria){
//        case eACCEPT_START_WITH:
//        case eACCEPT_SAME:
//            strcpy(filter->subs,token.str);
//            break;
//        case eACCEPT_END_WITH:
//        case eACCEPT_ANY_POS:
//            strcpy(filter->subs,&token.str[1]);
//            break;
//        case eNO_FILTER:
//        case eACCEPT_ALL:
//            break;
//    }//switch
//}//f()
//
//
//static bool filter_accept(filter_t *filter, const char* text){
//    bool result=false;
//    char temp[32];
//    
//    strcpy(temp,text);
//    uppercase(temp);
//    switch (filter->criteria){
//        case eACCEPT_SAME:
//            if (!strcmp(filter->subs,temp)) result=true;
//            break;
//        case eACCEPT_START_WITH:
//            if (strstr(temp,filter->subs)==temp) result=true;
//            break;
//        case eACCEPT_END_WITH:
//            if (strstr(temp,filter->subs)==temp+strlen(temp)-strlen(filter->subs)) result=true;
//            break;
//        case eACCEPT_ANY_POS:
//            if (strstr(temp,filter->subs)) result=true;
//            break;
//        case eACCEPT_ALL:
//        case eNO_FILTER:
//            result=true;
//            break;
//    }
//    return result;
//}//f()




// compile le calcul d'indice dans les variables vecteur
static void code_array_address(var_t *var){
    factor();// compile la valeur de l'index.
//    expect(eRPAREN);
    // limite l'index de 1 � ubound()
    _litc(1);
    bytecode(IMAX);
    lit((uint32_t)var->adr);
    bytecode(IFETCH);
    bytecode(IMIN);
    if (var->vtype==eVAR_INT || var->vtype==eVAR_STR){
        _litc(2);
        bytecode(ILSHIFT);//4*index
    }else{
        // �l�ment 0,1,2,3  r�serv� pour size
        // index 1 correspond � array[4]
        bytecode(IDUP);
        bytecode(IQBRAZ);
        cpush(dptr);
        dptr+=2;        
        _litc(3);
        bytecode(IPLUS);
        patch_fore_jump(cpop());
    }
    //index dans le tableau
    lit((uint32_t)var->adr);
    bytecode(IPLUS);
}//f

static void code_var_address(var_t *var){
    if (var->array){
        code_array_address(var);
    }else{
        if (var->local){
            bytecode(ILCADR);
            bytecode((uint8_t)var->n);
        }else{
            lit((uint32_t)&var->adr);
        }
    }
}

//v�rifie si le dernier token est une variable cha�ne ou une fonction cha�ne.
// compile l'adresse de la cha�ne si c'est le cas et retourne vrai.
// sinon retourne faux.
static bool try_string_var(){
    var_t *var;
    int i;
    
    var=var_search(token.str);
    if (var && (var->vtype==eVAR_STR)){
        unget_token=true;
        string_expression();
//        bytecode(IFETCH);
        return true;
    }
    return false;
}

static void parse_arg_list(unsigned arg_count){
    int count=0;
    expect(eLPAREN);
    next_token();
    char *lit_str;
    
    if (token.id==eRPAREN){
        if (arg_count) throw(eERR_MISSING_ARG);
        return;
    }
    unget_token=true;
    do{
        next_token();
        switch (token.id){
            case eIDENT:
                if (!try_string_var()){
                    unget_token=true;
                    expression();
                }
                break;
            case eSTRING:
                unget_token=true;
                string_expression();
//                bytecode(ISTRADR);
//                literal_string(token.str);
//                lit_str=alloc_var_space(strlen(token.str)+1);
//                strcpy(lit_str,token.str);
//                lit((uint32_t)lit_str);
                break;
            case eKWORD:
                unget_token=true;
                if (KEYWORD[token.kw].fntype==eFN_STR){
                    string_expression();
                }else{
                    unget_token=true;
                    expression();
                }
                break;
            default:
                unget_token=true;
                expression();
                break;
        }
        count++;
        next_token();
    }while (token.id==eCOMMA);
    if (token.id!=eRPAREN) throw(eERR_SYNTAX);
    if (count<arg_count) throw(eERR_MISSING_ARG);
    if (count>arg_count) throw(eERR_EXTRA_ARG);
}//f


static void factor(){
    var_t *var;
    int i,op=eNONE;
    
    if (try_addop()){
        op=token.id;
        unget_token=false;
    }
    next_token(); // print(con,token.str);
    switch(token.id){
        case eKWORD:
            if ((KEYWORD[token.kw].fntype==eFN_INT)){ //print_prog(program_end);
                KEYWORD[token.kw].cfn();
            }else  throw(eERR_SYNTAX);
            break;
        case eIDENT:
            if ((var=var_search(token.str))){
                switch(var->vtype){
                    case eVAR_FUNC:
                        _litc(0);
                        parse_arg_list(var->dim);
                        lit(((uint32_t)&var->adr));
                        bytecode(IFETCH);
                        bytecode(ICALL);
                        break;
                    case eVAR_BYTE:
                        code_var_address(var);
                        bytecode(ICFETCH);
                        break;
                    case eVAR_INT:
                    case eVAR_FLOAT:
                        code_var_address(var);
                        bytecode(IFETCH);
                        break;
                    default:
                        throw(eERR_SYNTAX);
                }//switch
            }else{
                throw(eERR_UNKNOWN);
            }
            break;
        case eCHAR:
            _litc(token.n&127);
            break;
        case eNUMBER:
            if (token.n>=0 && token.n<256){
                _litc((uint8_t)token.n);
            }else{
                lit(token.n);
            }
            break;
        case eLPAREN:
            expression();
            expect(eRPAREN);
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
    if (op==eMINUS){
        bytecode(INEG);
    }
}//f()

static void term(){
    int op;
    
    factor();
    while (try_mulop()){
        op=token.id;
        factor();
        switch(op){
            case eMUL:
                bytecode(ISTAR);
                break;
            case eDIV:
                bytecode(ISLASH);
                break;
            case eMOD:
                bytecode(IMOD);
                break;
        }//switch
    }
}//f 

static void expression(){
    int mark_dp=dptr;
    int op=eNONE;
    
    term();
    while (try_addop()){
        op=token.id;
        term();
        if (op==ePLUS){
            bytecode(IPLUS);
        }else{
            bytecode(ISUB);
        }
    }
    if (dptr==mark_dp) throw(eERR_SYNTAX);
}//f()

static void condition(){
    int rel;
    //print(con,"condition\n");
    expression();
    if (!try_relation()){
        return;
    }
    rel=token.id;
    expression();
    switch(rel){
        case eEQUAL:
            bytecode(IEQUAL);
            break;
        case eNOTEQUAL:
            bytecode(INEQUAL);
            break;
        case eGT:
            bytecode(IGREATER);
            break;
        case eGE:
            bytecode(IGTEQ);
            break;
        case eLT:
            bytecode(ILESS);
            break;
        case eLE:
            bytecode(ILTEQ);
            break;
    }//switch
}//f

static void string_compare(){
    bool free_first=false,free_second=false;
    int op_rel;
    string_expression();
    if (_ctop()){
        free_first=true;
        bytecode(IDUP);
    }
    cdrop();
    next_token(); // attend un op�rateur relationnel.
    if (!((token.id>=eEQUAL)&&(token.id<=eLE))){throw(eERR_SYNTAX);}
    op_rel=token.id;
    string_expression();
    if (_ctop()){
        free_second=true;
        bytecode(IDUP);
        bytecode(INROT);
    }
    cdrop();
    bytecode(ISTRCMP);
    if (free_second){
        bytecode(ISWAP);
        bytecode(ISTRFREE);
    }
    if (free_first){
        bytecode(ISWAP);
        bytecode(ISTRFREE);
    }
    switch(op_rel){
        case eEQUAL:
            bytecode(IBOOL_NOT);
            break;
        case INEQUAL:
            break;
        case eGT:
            _litc(0);
            bytecode(IGREATER);
            break;
        case eGE:
            _litc(0);
            bytecode(IGTEQ);
            break;
        case eLT:
            _litc(0);
            bytecode(ILESS);
            break;
        case eLE:
            _litc(0);
            bytecode(ILTEQ);
            break;
    }
}

static bool try_string_compare(){
    var_t *var;
    next_token();
    switch (token.id){
        case eSTRING:
            unget_token=true;
            string_compare();
            return true;
        case eIDENT:
            var=var_search(token.str);
            if (var->vtype==eVAR_STR){
                unget_token=true;
                string_compare();
                return true;
            }
            break;
        case eKWORD:
            if ((KEYWORD[token.n].fntype==eFN_STR)){
                unget_token=true;
                string_compare();
                return true;
            }
            break;
    }
    return false;
}

static void bool_factor(){
    int boolop=0;
    //print(con,"bool_factor\n");
    
    if (try_string_compare()){return;}
    if (try_boolop()){
        if (token.n!=eKW_NOT) throw(eERR_SYNTAX);
        boolop=eKW_NOT;
    }
    condition();
    if (boolop){
        bytecode(IBOOL_NOT);
    }
}//f


static void bool_term(){// print(con,"bool_term()\n");
    int fix_count=0; // pour �valuation court-circuit�e.
    bool_factor();
    while (try_boolop() && token.n==eKW_AND){
        //code pour �valuation court-circuit�e
        bytecode(IDUP);
        bytecode(IQBRAZ); // si premier facteur==faux saut court-circuit.
        ctrl_stack[csp++]=dptr;
        dptr+=2;
        fix_count++;
        bool_factor();
        bytecode(IBOOL_AND);
    }
    unget_token=true;
    while (fix_count){
        patch_fore_jump(cpop());
        fix_count--;
    }
}//f


// compile les expressions bool�enne
static void bool_expression(){// print(con,"bool_expression()\n");
    int fix_count=0; // pour �valuation court-circuit�e.
    bool_term();
    while (try_boolop() && token.n==eKW_OR){
        bytecode(IDUP);
        bytecode(IQBRA); // si premier facteur==vrai saut court-circuit.
        ctrl_stack[csp++]=dptr;
        dptr+=2;
        fix_count++;
        bool_term();
        bytecode(IBOOL_OR);
    }
    unget_token=true;
    while (fix_count){
        patch_fore_jump(cpop());
        fix_count--;
    }
}//f

static void bad_syntax(){
    throw(eERR_SYNTAX);
}//f

static  const char* compile_msg[]={
    "compiling ",
    "completed ",
};

#define COMPILING 0
#define COMP_END 1

static void compiler_msg(int msg_id, char *detail){
    char msg[CHAR_PER_LINE];
    strcpy(msg,compile_msg[msg_id]);
    print(con,msg);
    print(con,detail);
    crlf(con);
}//f

/*************************
 * commandes BASIC
 *************************/

static bool try_type_cast(char *var_name){
    var_t *var;
    int vtype;
    
    next_token();
    if ((token.id==eIDENT) && !strcmp(token.str,"AS")){
        next_token();
        if ((token.id==eIDENT) && (vtype=search_type(token.str))>-1){
            var=var_create(var_name,NULL);
            var->vtype=vtype;
        }else{
            throw(eERR_SYNTAX);
        }
        return true;
    }else{
        unget_token=true;
        return false;
    }
}

// '(' number {','number}')'
static void init_int_array(var_t *var){
    int *iarray=NULL;
    unsigned char *barray=NULL;
    int size,count=1,state=0;
    
    if (var->vtype==eVAR_INT){
        iarray=var->adr;
        size=*iarray;
    }else{
        barray=var->adr;
        size=*barray;
    }
    if (var->vtype==eVAR_BYTE){ count=4;}
    expect(eLPAREN);
    next_token();
    while (token.id!=eRPAREN){
        switch (token.id){
            case eCHAR:
            case eNUMBER: 
                if (state) throw(eERR_SYNTAX);
                if (var->vtype==eVAR_INT){
                    iarray[count++]=token.n;
                }else{
                    barray[count++]=(uint8_t)token.n;
                }
                state=1;
                break;
            case eCOMMA:
                if (!state){
                    throw(eERR_SYNTAX);
                }else{
                    state=0;
                }
                break;
            case eRPAREN:
                continue;
                break;
            case eSTOP:
                break;
            default:
                throw(eERR_SYNTAX);
        }//switch
        next_token();
    }
    if (count<size) throw(eERR_MISSING_ARG);
    if (((var->vtype==eVAR_BYTE) && (count>(size+sizeof(uint32_t)))) ||
        ((var->vtype!=eVAR_BYTE) && (count>(size+1)))) throw(eERR_EXTRA_ARG);
}//f

static void init_str_array(var_t *var){
    uint32_t *array,size;
    int count=1,state=0;
    char *newstr;
    
    complevel++;
    array=var->adr;
    size=array[0];
    expect(eLPAREN);
    next_token();
    while(token.id!=eRPAREN){
        switch (token.id){
            case eSTRING: 
                if (state) throw(eERR_SYNTAX);
                newstr=string_alloc(strlen(token.str));
                strcpy(newstr,token.str);
                array[count++]=(uint32_t)newstr;
                state=1;
                break;
            case eCOMMA:
                if (!state){
                    count++;
                }else{
                    state=0;
                }
                break;
            case eRPAREN:
                continue;
                break;
            case eSTOP:
                break;
            default:
                throw(eERR_SYNTAX);
        }
        next_token();
    }
    if (count<size) throw(eERR_MISSING_ARG);
    if (count>(size+1)) throw(eERR_EXTRA_ARG);
    complevel--;
}//f

static void *alloc_array_space(int size, int vtype){
    void *array;
    unsigned space;

    if (vtype==eVAR_BYTE){ //table d'octets
        space=sizeof(uint8_t)*size+sizeof(int);
    }else{ // table d'entiers ou de cha�nes
        space=sizeof(int)*(size+1);
    }
    array=alloc_var_space(space);
    memset(array,0,space);
    *((uint32_t*)array)=size;
    return array;
}

// r�cup�re la taille du tableau et initialise si '='
static void dim_array(char *var_name){
    int size=0, len;
    void *array;
    var_t *new_var, *var;
    
    next_token();
    if (token.id==eNUMBER){
        size=token.n;
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (var && !var->array){
            switch (var->vtype){
                case eVAR_INT:
                case eVAR_BYTE:
                    size=var->n;
                    break;
                default:
                    throw(eERR_BAD_ARG);        
            }//switch
        }else{
            throw(eERR_BAD_ARG);
        }
    }else{
        throw(eERR_BAD_ARG);
    }
    if (size<1) throw(eERR_BAD_ARG);
    expect(eRPAREN);
    if (!try_type_cast(var_name)){
        len=strlen(var_name);
        if (var_name[len-1]=='#'){ //table d'octets
            array=alloc_array_space(size,eVAR_BYTE);
        }else{ // table d'entiers ou de cha�nes
            array=alloc_array_space(size,eVAR_INT);
        }
        new_var=(var_t*)alloc_var_space(sizeof(var_t));
        new_var->array=true;
        new_var->dim=1;
        new_var->name=alloc_var_space(len+1);
        strcpy(new_var->name,var_name);
        new_var->adr=array;
        if (var_name[len-1]=='$'){
            new_var->vtype=eVAR_STR;
        }else if (var_name[len-1]=='#'){
            new_var->vtype=eVAR_BYTE;
        }else{
            new_var->vtype=eVAR_INT;
        }
        new_var->next=varlist;
        varlist=new_var;
        next_token();
        if (token.id==eEQUAL){
            if (new_var->vtype==eVAR_STR){
                init_str_array(new_var);
            }else{
                init_int_array(new_var);
            }
        }else{
            unget_token=true;
        }
    }else{
        varlist->array=true;
        varlist->dim=1;
        if (varlist->vtype==eVAR_BYTE){
            varlist->adr=alloc_array_space(size,eVAR_BYTE);
        }else{
            varlist->adr=alloc_array_space(size,eVAR_INT);
        }
    }
}//f



// DIM identifier['('number')'] {','identifier['('number')']}|
//     identifier['='expression] {','identifier['='expression]}|
//     identifier$= STRING|
//     identifier$='('string{,string}')'
//     identifier['#']='('number{,number}')'

static void kw_dim(){
    char var_name[32];
    var_t *new_var;
    
    if (var_local) throw(eERR_NOT_ARRAY);
    expect(eIDENT);
    while (token.id==eIDENT){
        if (var_search(token.str)) throw(eERR_DUPLICATE);
        strcpy(var_name,token.str);
        if (!try_type_cast(var_name)){
            next_token();
            switch(token.id){
                case eLPAREN:
                    dim_array(var_name);
                    break;
                case eCOMMA:
                    new_var=var_create(var_name,NULL);
                    break;
                case eEQUAL: 
                    new_var=var_create(var_name,NULL);
                    if (new_var->vtype==eVAR_STR){
//                        string_expression();
//                        lit((uint32_t)&new_var->adr);
//                        bytecode(ISTORE);
                        next_token();
                        if (token.id!=eSTRING) throw(eERR_BAD_ARG);
                        new_var->str=alloc_var_space(strlen(token.str)+1);
                        strcpy((char*)new_var->str,token.str);
                    }else{
                        expression();
                        lit((uint32_t)&new_var->n);
                        bytecode(ISTORE);
                    }
                    break;
                default:
                    new_var=var_create(var_name,NULL);
                    unget_token=true;
                    break;

            }//switch
        }
        next_token();
        if (token.id==eCOMMA){
            next_token();
        }else if (!(token.id==eSTOP)){
            throw(eERR_SYNTAX);
        }
    }//while
    unget_token=true;
}//f

static int count_arg(){
    int count=0;
    expect(eLPAREN);
    next_token();
    while (token.id!=eRPAREN){
        if (token.id!=eIDENT){
            throw(eERR_SYNTAX);
        }
        count++;
        next_token();
        if (token.id==eCOMMA){
            next_token();
        }else if (token.id!=eRPAREN){
            throw(eERR_SYNTAX);
        }
    }
    return count;
}

// DECLARE  SUB|FUNC name(liste-arg)
static void kw_declare(){
    char *name;
    var_t *var;
    int var_type;
    
    expect(eKWORD);
    switch(token.n){
        case eKW_SUB:
            var_type=eVAR_SUB;
            break;
        case eKW_FUNC:
            var_type=eVAR_FUNC;
            break;
        default:
            throw(eERR_SYNTAX);
    }
    expect(eIDENT);
    var=var_create(token.str,NULL);
    var->vtype=var_type;
    var->adr=(void*)undefined_sub;
    var->dim=count_arg();
}


//passe une variable par r�f�rence
static void kw_ref(){
    var_t *var;
    
    next_token();
    if (token.id!=eIDENT) throw(eERR_BAD_ARG);
    var=var_search(token.str);
    if (!var) throw(eERR_BAD_ARG);
    if (var->array){
        lit(((uint32_t)var->adr)+sizeof(int));
    }else{
        switch(var->vtype){
            case eVAR_INT:
            case eVAR_BYTE:
            case eVAR_FLOAT:
                lit((uint32_t)&var->n);
                break;
            case eVAR_STR:
                if (var->ro){
                    throw(eERR_BAD_ARG);
                }
            case eVAR_FUNC:
            case eVAR_SUB:
                lit((uint32_t)var->adr);
                break;
            default:
                throw(eERR_BAD_ARG);
        }//switch
    }
}//f

// UBOUND(var_name)
// retourne le dernier indice
// du tableau
static void kw_ubound(){
    var_t *var;
    char name[32];
    expect(eLPAREN);
    expect(eIDENT);
    strcpy(name,token.str);
    expect(eRPAREN);
    var=var_search(name);
    if (!(var && var->array)) throw(eERR_BAD_ARG);
    lit((uint32_t)var->adr);
    bytecode(IFETCH);
}//f

//USE fichier
// compilation d'un fichier source inclut dans un autre fichier source.
static void kw_use(){
    reader_t *old_reader, freader;
    FIL fh;
    uint32_t lcount;
    FRESULT result;
    
    if (activ_reader->device==eDEV_KBD) throw(eERR_SYNTAX);
    expect(eSTRING);
    uppercase(token.str);
    if (!(result=f_open(&fh,token.str,FA_READ))){
        reader_init(&freader,eDEV_SDCARD,&fh);
        old_reader=activ_reader;
        activ_reader=&freader;
        compiler_msg(COMPILING, token.str);
        lcount=line_count;
        line_count=1;
        compile();
        line_count=lcount+1;
        f_close(&fh);
        activ_reader=old_reader;
        compiler_msg(COMP_END,NULL);
    }else{
        throw(eERR_FILE_IO);
    }
}//f

//CURLINE()
// retourne la position ligne du curseur texte
static void kw_curline(){
    parse_arg_list(0);
    bytecode(ICURLINE);
}//f

//CURCOL()
// retourne la position colonne du curseur texte
static void kw_curcol(){
    parse_arg_list(0);
    bytecode(ICURCOL);
}//f

// EXEC @identifier
//static void kw_exec(){
//    expression();
//    bytecode(EXEC);
//}//f

// CONST  nom[$]=expr|string [, nom[$]=expr|string]
// d�finition d'une constante
static void kw_const(){
    char name[32];
    var_t *var;
    
    if (var_local) throw(eERR_CONST_FORBID);
    expect(eIDENT);
    while (token.id==eIDENT){
        strcpy(name,token.str);
        if ((var=var_search(name))) throw(eERR_REDEF);
        expect(eEQUAL);
        var=var_create(name,NULL);
        var->ro=true;
        if (var->vtype==eVAR_STR){
            expect(eSTRING);
            var->str=alloc_var_space(strlen(token.str)+1);
            strcpy(var->str,token.str);
            var->vtype=eVAR_STR;
        }else{
            expect(eNUMBER);
            var->n=token.n;
        }
        next_token();
        if (token.id==eCOMMA)
            expect(eIDENT);
        else
            break;
    }//while
    unget_token=true;
}//f

// ABS(expression)
// fonction retourne valeur absolue
static void kw_abs(){
    parse_arg_list(1);
    bytecode(IABS);
}//f

// SHL(expression,n)
// d�calage � gauche de 1 bit
static void kw_shl(){
    parse_arg_list(2);
    bytecode(ILSHIFT);
}//f

// SHR(expression,n)
// d�cale � droite de 1 bit
static void kw_shr(){
    parse_arg_list(2);
    bytecode(IRSHIFT);
}//f

// BTEST(expression1,expression2{0-31})
// fonction test bit de expression1
// retourne vrai si 1
static void kw_btest(){
    parse_arg_list(2);
    bytecode(IBTEST);
}//f

//BEEP
//produit un son � la fr�quence de 1Khz pour 20msec.
static void kw_beep(){
    bytecode(IBEEP);
}

//SOUND(freq,msec)
//fait entendre une fr�quence quelconque. Attend la fin de la note.
static void kw_sound(){
    parse_arg_list(2);
    bytecode(ISOUND);
}//f

//TUNE(@array)
// fait entendre une m�lodie
static void kw_tune(){
    parse_arg_list(1);
    bytecode(ITUNE);
}

//PLAY(string|var$)
//joue une m�lodie contenue dans un cha�ne.
static void kw_play(){
    parse_arg_list(1);
    bytecode(IPLAY);
}

//SLEEP(msec)
// suspend ex�cution
// argument en millisecondes
static void kw_sleep(){
    parse_arg_list(1);
    bytecode(ISLEEP);
}//f

//TICKS()
//retourne la valeur du compteur
// de millisecondes du syst�me
static void kw_ticks(){
    parse_arg_list(0);
    bytecode(ITICKS);
}//f

// SETTMR(msec)
// initialise la variable syst�me timer
static void kw_set_timer(){
    parse_arg_list(1);
    bytecode(ISETTMR);
}//f

// TIMEOUT()
// retourne vrai si la varialbe syst�me timer==0
static void kw_timeout(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(ITIMEOUT);
}//f

//COMMANDE BASIC: BYE
//quitte l'interpr�teur
static void kw_bye(){
    if (!complevel && (activ_reader->device==eDEV_KBD)){
        print(con,error_msg[eERR_NONE]);
        exit_basic=true;
    }else{
        bytecode(IBYE);
    }
}//f

// RETURN expression
// utilis� dans les fonctions
static void kw_return(){
        expression();
        bytecode(ILCSTORE);
        bytecode(0);
        bytecode(ILEAVE);
//        bytecode(FRAME_FUNC);
}//f

// EXIT SUB
// termine l'ex�cution d'une sous-routine
// en n'importe quel point de celle-ci.
// sauf � l'int�rieur d'une boucle FOR
static void kw_exit(){
    expect(eKWORD);
    if (token.n != eKW_SUB) throw(eERR_SYNTAX);
    bytecode(ILEAVE);
//    bytecode(FRAME_SUB);
}//f

// CLS
// efface �cran
static void kw_cls(){
    bytecode(ICLS);
}//f

// LOCATE(ligne,colonne)
//positionne le curseur texte
static void kw_locate(){
    parse_arg_list(2);
    bytecode(ILOCATE);
}//f

// REM commentaire
// aussi ' commentaire
static void kw_rem(){
    char c=0;
    while (!(activ_reader->eof || ((c=reader_getc(activ_reader))=='\n')));
    line_count++;
}//f()

// IF condition THEN bloc_instructions ELSE bloc_instructions END IF
static void kw_if(){
    complevel++;
    cpush(eKW_IF);
    bool_expression();
}//f

// THEN voir kw_if
static void kw_then(){
    bytecode(IQBRAZ);
    cpush(dptr);
    dptr+=2;
}//f


// ELSE voir kw_if
static void kw_else(){
    bytecode(IBRA);
    dptr+=2;
    patch_fore_jump(cpop());
    ctrl_stack[csp++]=dptr-2;
}//f

//d�place le code des sous-routine
//� la fin de l'espace libre
static void movecode(var_t *var){
    uint32_t size;
    void *pos, *adr;
    
    pos=var->adr;
    size=(uint32_t)&progspace[dptr]-(uint32_t)pos;
    if (size&3){ size=(size+4)&0xfffffffc;}
    print_hex(con,(uint32_t)(uint8_t*)endmark,0);
    adr=endmark-size;
    memmove(adr,pos,size);
    endmark=adr;
    dptr=(uint8_t*)pos-(uint8_t*)progspace;
    memset(&progspace[dptr],0,size);
    var->adr=adr; 
#undef DEBUG
#ifdef DEBUG
    {
        uint8_t * bc;   
        crlf(con);print_hex(con,(uint32_t)var->adr,0);put_char(con,':');
        for(bc=(uint8_t*)adr;bc<((uint8_t*)adr+size);bc++){
            print_int(con,*bc,0);
        }
        print_int(con,*bc,0);
        crlf(con);
    }
#endif
#define DEBUG
}//f

// END [IF|SUB|FUNC|SELECT]  termine les blocs conditionnels.
static void kw_end(){ // IF ->(C: blockend adr -- ) |
                      //SELECT -> (C: n*(addr,...) blockend n -- )
                      //SUB et FUNC -> (C: endmark blockend &var -- )
    int blockend,fix_count;
    var_t *var;
    
    expect(eKWORD);
    blockend=token.n;
    if (_cnext()!=blockend) throw(eERR_SYNTAX);
    switch (blockend){
        case eKW_IF:
            patch_fore_jump(cpop());
            cdrop(); //drop eKW_IF
            break;
        case eKW_SELECT:
            fix_count=cpop();
            if (!fix_count) throw(eERR_SYNTAX);
            cdrop(); //drop eKW_SELECT
            while (fix_count){ //print_int(con,fix_count,0); crlf(con);
                patch_fore_jump(cpop());
                fix_count--;
            } //print_int(con,csp,0);
            bytecode(IDROP);
            break;
        case eKW_FUNC:
        case eKW_SUB:
            bytecode(ILEAVE);
            varlist=globals;
            globals=NULL;
            var_local=false;
            var=(var_t*)cpop();
            endmark=(void*)_cnext();
            movecode(var);
            csp-=2; // drop eKW_xxx et endmark
            var_local=false;
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
    complevel--;
}

// SELECT CASE  expression
// CASE expression
//   bloc_instructions
// {CASE expression {,expression}
//   bloc_instructions}
// [CASE ELSE
//   bloc_instructions]
// END SELECT
static void kw_select(){
    expect(eKWORD);
    if (token.n!=eKW_CASE) throw(eERR_SYNTAX);
    complevel++;
    cpush(eKW_SELECT);
    cpush(0); // compteur clauses CASE
    expression();
}//f

// compile la liste des expression d'un CASE, voir kw_select
static void compile_case_list(){
    int fix_count=0;
    next_token();
    if (token.id==eSTOP) throw(eERR_SYNTAX);
    unget_token=true; 
    while (token.id != eSTOP){
        bytecode(IDUP); 
        expression();  
        bytecode(IEQUAL);  
        bytecode(IQBRA);  
        cpush(dptr);
        dptr+=2;
        fix_count++;
        next_token();
        if (token.id!=eCOMMA){
            unget_token=true;
            break;
        }
    }//while
    bytecode(IBRA);
    dptr+=2;
    while (fix_count){
        patch_fore_jump(cpop());
        fix_count--;
    }
    _ctop()++; //incr�mente le nombre de clause CASE
    cpush(dptr-2); // fente pour le IBRA de ce case (i.e. saut apr�s le END SELECT)
    ctrl_stack_nrot();
    
}//f

static void patch_previous_case(){
    uint32_t adr;
    
    ctrl_stack_rot();
    adr=cpop();
    bytecode(IBRA);  // branchement � la sortie du select case pour le case pr�c�dent
    cpush(dptr); // (adr blockend n -- dptr blockend n )
    ctrl_stack_nrot(); 
    dptr+=2;
    patch_fore_jump(adr); // fixe branchement vers ce case
    
}

// compile un CASE, voir kw_select
static void kw_case(){
    next_token();
    if (token.id==eKWORD && token.n==eKW_ELSE){
        if (_ctop()){
            patch_previous_case();
        }else
            throw(eERR_SYNTAX);
    }else{
        unget_token=true;
        if (_ctop()){//y a-t-il un case avant celui-ci?
            patch_previous_case();
        }
        compile_case_list();
    }
}//f

// RND())
// g�n�ration nombre pseudo-al�atoire
static void kw_rnd(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IRND);
}//f

// ex�cute le code BASIC compil�
static void exec_basic(){
    int vm_exit_code=0;
    
    if (run_it){
        vm_exit_code=StackVM(progspace);
        run_it=false;
    }else{
        bytecode(IBYE);
        vm_exit_code=StackVM(&progspace[program_end]);
        memset(&progspace[program_end],0,dptr-program_end);
        dptr=program_end;
    }
    if (vm_exit_code){
        throw(vm_exit_code);
    }
}

//compile un fichier basic
static void compile_file(const char *file_name){
    FIL fh;
    FRESULT result;
    
    if ((result=f_open(&fh,file_name,FA_READ))==FR_OK){
        reader_init(&file_reader,eDEV_SDCARD,&fh);
        activ_reader=&file_reader;
        clear();
        compiler_msg(COMPILING,token.str);
        line_count=1;
        compile();
        f_close(&fh);
        activ_reader=&std_reader;
        program_loaded=true;
        program_end=dptr;
        run_it=true;
        compiler_msg(COMP_END,NULL);
    }else{
        throw(eERR_FILE_IO);
    }
}//f

// compile et ex�cute un fichier BASIC.
static void run_file(const char *file_name){
    clear();
    compile_file(file_name);
    exec_basic();
    activ_reader=&std_reader;
}


//RUN "file_name"
// commande pour ex�cuter un fichier basic
static void kw_run(){
    char name[32], *ext;
    
    next_token();
    switch (token.id){
        case eSTRING:
            strcpy(name,token.str);
            uppercase(name);
            ext=strstr(name,".BAS");
            if (!ext){
                strcat(name,".BAS");
            }
            run_file(name);
            break;
        case eSTOP:
            if (program_loaded){
                run_it=true;
                exec_basic();
            }else{
                print(con,"Nothing to run\n");
            }
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
}// kw_run()

//RANDOMIZE()
static void kw_randomize(){
    parse_arg_list(0);
    bytecode(IRANDOMIZE);
}//f

//MAX(expr1,expr2)
static void kw_max(){
    parse_arg_list(2);
    bytecode(IMAX);
}//f

static void kw_mdiv(){
    parse_arg_list(3);
    bytecode(IMDIV);
}

//MIN(expr1,expr2)
static void kw_min(){
    parse_arg_list(2);
    bytecode(IMIN);
}//f

// PGET(x,y)
// retourne la couleur du pixel
// en position {x,y}
static void kw_getpixel(){
    parse_arg_list(2);
    bytecode(IPGET);
}//f

// PSET(x,y,p)
// fixe la couleur du pixel
// en position x,y
// p-> couleur {0,1}
static void kw_setpixel(){
    parse_arg_list(3);
    bytecode(IPSET);
}//f

// PXOR(x,y)
// inverse le pixel � la position {x,y}
static void kw_xorpixel(){
    parse_arg_list(2);
    bytecode(IPXOR);
}//f

// SCRLUP()
// glisse l'�cran vers le haut d'une ligne
static void kw_scrlup(){
    parse_arg_list(0);
    bytecode(ISCRLUP);
}//f

// SCRLDN()
// glisse l'�cran vers le bas d'une lignes
static void kw_scrldown(){
    parse_arg_list(0);
    bytecode(ISCRLDN);
    
}//f

//INSERTLN()
static void kw_insert_line(){
    parse_arg_list(0);
    bytecode(IINSERTLN);
}

/***********************/
/* fonction graphiques */
/***********************/

// LINE(x1,y,x2,y2,color)
// trace une ligne droite
static void kw_line(){
    parse_arg_list(4);
    bytecode(ILINE);
}//f

//BOX(x,y,width,height)
//desssine une rectangle plein
static void kw_box(){
    parse_arg_list(4);
    bytecode(IBOX);
}//f

//RECT(x0,y0,width,height)
//dessine un rectangle vide
static void kw_rect(){
    parse_arg_list(4);
    bytecode(IRECT);
}//f

//CIRCLE(xc,yc,r)
//dissine un cercle rayon r, centr� sur xc,yc
static void kw_circle(){
    parse_arg_list(3);
    bytecode(ICIRCLE);
}//f

//ELLIPSE(xc,yc,w,h)
//dessine une ellipse centr�e sur xc,yc
// et de largeur w, hauteur h
static void kw_ellipse(){
    parse_arg_list(4);
    bytecode(IELLIPSE);
}//f

//POLYGON(@points,n)
//dessine un polygon � partir d'un tableau de points.
void kw_polygon(){
    parse_arg_list(2);
    bytecode(IPOLYGON);
}
//FILL(x,y)
//remplissage d'une figure g�om�tirique ferm�e.
void kw_fill(){
    parse_arg_list(2);
    bytecode(IFILL);
}

// SPRITE(x,y,width,height,@sprite)
// applique le sprite � la position d�sign�
// il s'agit d'une op�ration xor
static void kw_sprite(){
    parse_arg_list(5);
    bytecode(ISPRITE);
}//f

// VGACLS
// commande pour effacer l'�cran graphique, ind�pendamment de la console active.
static void kw_vgacls(){
    bytecode(IVGACLS);
}

// FREE
// commande qui affiche la m�moire programme et cha�ne disponible
static void kw_free(){
    char fmt[64];
    
    crlf(con);
    sprintf(fmt,"Program space (bytes): used %d , available %d\n",program_end,
                (uint32_t)endmark-(uint32_t)&progspace[program_end]);
    print(con,fmt);
    sprintf(fmt,"strings space (bytes): available %d\n",free_heap());
    print(con,fmt);
}


// FOR var=expression TO expression [STEP expression]
//  bloc_instructions
// NEXT var
static void kw_for(){
    var_t *var;
    char name[32];
    
    bytecode(IFORSAVE);
    expect(eIDENT);
    strcpy(name,token.str);
    complevel++;
    unget_token=true;
    kw_let(); // valeur initale de la variable de boucle
    expect(eIDENT);
    if (strcmp(token.str,"TO")) throw (eERR_SYNTAX);
    expression(); // valeur de la limite
    next_token();
    if (token.id==eIDENT && !strcmp(token.str,"STEP")){
        expression(); //valeur de l'incr�ment
    }else{
        _litc(1);
        unget_token=true;
    }
    bytecode(IFOR); // sauvegarde limit et step qui sont sur la pile
    var=var_search(name);
    cpush(dptr);  // adresse cible instruction FORTEST de la boucle for.
    code_var_address(var);
//    if (var->vtype==eVAR_LOCAL){
//        bytecode(ILCADR);
//        bytecode(var->n);
//    }else{
//        lit((uint32_t)&var->n);
//    }
    bytecode(IFETCH);
    bytecode(IFORTEST);
    bytecode(IQBRAZ);
    cpush(dptr); // fente d�placement apr�s boucle FOR...NEXT
    dptr+=2;
}//f

// voir kw_for()
static void kw_next(){
    var_t *var;
    
    expect(eIDENT);
    var=var_search(token.str);
    if (!(var && ((var->vtype==eVAR_INT)))) throw(eERR_BAD_ARG);
    code_var_address(var);
//    if (var->vtype==eVAR_LOCAL){
//        bytecode(ILCADR);
//        bytecode(var->n);
//    }else{
//        lit((uint32_t)&var->adr);
//    }
    bytecode(IFORNEXT);
    bytecode(IBRA);
    patch_back_jump(_cnext()); //saut arri�re vers FORTEST
    patch_fore_jump(cpop());
    cdrop(); // jette cible de patch_back_jump() pr�c�dent
    bytecode(IFORREST); 
    complevel--;
}//kw_next()

// WHILE condition
//  bloc_instructions
// WEND
static void kw_while(){
    complevel++;
    cpush(dptr);
    bool_expression();
    bytecode(IQBRAZ);
    cpush(dptr);
    dptr+=2;
}//f

// voir kw_while()
static void kw_wend(){
    int n;
    
    bytecode(IBRA);
    patch_back_jump(_cnext());
    patch_fore_jump(cpop());
    cdrop();
    complevel--;
}//f

// DO 
//   bloc_instructions
// LOOP WHILE|UNTIL condition
static void kw_do(){
    complevel++;
    cpush(dptr);
}//f

// voir kw_do()
static void kw_loop(){
    uint8_t bc;
    
    expect(eKWORD);
    if (token.n==eKW_WHILE){
        bc=IQBRA;
    }else if (token.n==eKW_UNTIL){
        bc=IQBRAZ;  
    }else{
        throw(eERR_SYNTAX);
    }
    bool_expression();
    bytecode(bc);
    patch_back_jump(cpop());
    complevel--;
}//f

// compile une cha�ne lit�rale
static void literal_string(char *lit_str){
    int size;

    size=strlen(lit_str)+1;
    if ((void*)&progspace[dptr+size]>endmark) throw(eERR_PROGSPACE);
    strcpy((void*)&progspace[dptr],lit_str);
    dptr+=size;
}//f


//SRLOAD(addr,file_name)
//charge un fichier dans la SPI RAM
// retourne la grandeur en octet
static void kw_srload(){
    expect(eLPAREN);
    expression();
    expect(eCOMMA);
    expect(eSTRING);
    strcpy(pad,token.str);
    lit((uint32_t)pad);
    expect(eRPAREN);
    bytecode(ISRLOAD);
//    var_t *var;
//    
//    next_token();
//    if (token.id==eSTRING){
//        bytecode(ISTRADR);
//        literal_string(token.str);
//    }else if (token.id==eIDENT){
//        var=var_search(token.str);
//        if (!var || !(var->vtype==eVAR_STR || var->vtype==eVAR_STRARRAY)) throw(eERR_BAD_ARG);
//        if (var->vtype==eVAR_STRARRAY){
//            code_array_address(var);
//        }else{
//            lit((uint32_t)&var->str);
//        }
//        bytecode(IFETCH);
//    }else{
//        throw(eERR_BAD_ARG);
//    }
//    bytecode(ISRLOAD);
}//f

//SRSAVE(addr,file_name,size)
//sauvegarde un bloc de SPI RAM dans un fichier
static void kw_srsave(){
    expect(eLPAREN);
    expression();
    expect(eCOMMA);
    expect(eSTRING);
    strcpy(pad,token.str);
    lit((uint32_t)pad);
    expect(eCOMMA);
    expression();
    expect(eRPAREN);
    bytecode(ISRSAVE);
//    var_t *var;
//    
//    next_token();
//    if (token.id==eSTRING){
//        bytecode(ISTRADR);
//        literal_string(token.str);
//    }else if (token.id==eIDENT){
//        var=var_search(token.str);
//        if (!var || !(var->vtype==eVAR_STR || var->vtype==eVAR_STRARRAY)) throw(eERR_BAD_ARG);
//        if (var->vtype==eVAR_STRARRAY){
//            code_array_address(var);
//        }else{
//            lit((uint32_t)&var->str);
//        }
//        bytecode(IFETCH);
//    }else{
//        throw(eERR_BAD_ARG);
//    }
//    expect(eCOMMA);
//    expression();
//    bytecode(ISRSAVE);
}//f

//SRCLEAR(address,size)
//met � z�ro un bloc de m�moire SPIRAM
static void kw_srclear(){
    parse_arg_list(2);
    bytecode(ISRCLEAR);
}//f

//SRREAD(address,@var,size)
//lit un bloc de m�moire SPIRAM
//dans une variable
static void kw_srread(){
    parse_arg_list(3);
    bytecode(ISRREAD);
}//f

//SRWRITE(address,@var,size)
//copie le contenu d'une variable dans la SPIRAM
static void kw_srwrite(){
    parse_arg_list(3);
    bytecode(ISRWRITE);
}//f

//RESTSCR(adresse)
//restore le buffer vid�o � partir de la SPI RAM
// adresse est l'adresse sour dans SPI RAM
static void kw_restore_screen(){
    parse_arg_list(1);
    bytecode(IRESTSCR);
}//f

//SAVESCR(adresse)
//sauvegarde le buffer vid�o dans la SPI RAM
// adresse est la destination dans SPI RAM
static void kw_save_screen(){
    parse_arg_list(1);
    bytecode(ISAVESCR);
}//f

//compile l'assignation pour �l�ment de variable
// vecteur
static void array_let(char * name){
    var_t *var;
    char *adr;
    
    var=var_search(name);
    if (!var || !var->array) throw(eERR_NOT_ARRAY);
    code_array_address(var);
    expect(eEQUAL);
    switch (var->vtype){
        case eVAR_STR:
            expect(eSTRING);
            adr=string_alloc(strlen(token.str));
            strcpy(adr,token.str);
            lit((uint32_t)adr);
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_INT:
            expression();
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_BYTE:
            expression();
            bytecode(ISWAP);
            bytecode(ICSTORE);
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
}//f

// compile le code pour le stockage d'untier
static void store_integer(var_t *var){
    if (var->ro){
        throw(eERR_REDEF);
    }
    code_var_address(var);
    switch(var->vtype){
        case eVAR_INT:
            bytecode(ISTORE);
            break;
        case eVAR_BYTE:
            bytecode(ICSTORE);
            break;
        default:
            throw (eERR_BAD_ARG);
    }//switch
}//f

// KEY()
//attend une touche du clavier
// retourne la valeur ASCII
static void kw_waitkey(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IKEY);
}//f

// TKEY()
// v�rifie si une touche clavier est disponible
static void kw_tkey(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IQRX);
}

//LEN(var$|string)
static void kw_len(){
    var_t *var;
    expect(eLPAREN);
    next_token();
    if (token.id==eSTRING){
        bytecode(ISTRADR);
        literal_string(token.str);
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (!var) throw(eERR_BAD_ARG);
        if (var->vtype==eVAR_STR){
            code_var_address(var);
            bytecode(IFETCH);
        }else{
            throw(eERR_BAD_ARG);
        }//if
    }else{
        throw(eERR_BAD_ARG);
    }
    bytecode(ILEN);
    expect(eRPAREN);
}//f

// compile le code pour la saisie d'un entier
static void compile_input(var_t *var){
    _litc('?');
    bytecode(IEMIT);
    lit((uint32_t)pad); // ( var_addr -- var_addr pad* )
    _litc(CHAR_PER_LINE-1); // ( var_addr pad* -- var_addr pad* buf_size )
    bytecode(IREADLN); // ( var_addr pad* buf_size  -- var_adr pad* str_size )
}

// crit�res d'acceptation pour INPUT:
// Seule les variables tableaux existantes peuvent-�tre utilis�es par input.
// Dans un contexte var_local seules les variables pr�existantes
//  peuvent-�tre utilis�es.
static var_t  *var_accept(char *name){
    var_t *var;
    
    var=var_search(name);
    if (!var && var_local) throw(eERR_BAD_ARG);
    next_token();
    //seules les variables vecteur pr�existantes peuvent-�tre utilis�es
    if (!var && (token.id==eLPAREN)) throw(eERR_BAD_ARG);
    unget_token=true;
    if (!var) var=var_create(name,NULL);
    code_var_address(var);
    return var;
}

// INPUT [string ','] identifier (',' identifier)
// saisie au clavier de
// valeur de variables
static void kw_input(){
    char name[32];
    var_t *var;
    
    next_token();
    if (token.id==eSTRING){
        bytecode(IPSTR);
        literal_string(token.str);
        bytecode(ICR);
        expect(eCOMMA);
        expect(eIDENT);
    }else if (token.id!=eIDENT){
        throw(eERR_BAD_ARG); 
    }
    while (token.id==eIDENT){
        strcpy(name,token.str);
        var=var_accept(name); // ( -- var_addr )
        switch (var->vtype){ 
            case eVAR_STR:
                bytecode(IDUP);
                bytecode(IFETCH);
                bytecode(ISTRFREE); 
                compile_input(var); // (var_addr -- var_addr pad_addr)
                bytecode(ISTRALLOC);// ( var_addr pad_addr n -- var_addr pad_addr str_addr)  
                bytecode(ISTRCPY); //( var_addr pad_addr str_addr -- var_addr )
                bytecode(ISWAP);
                bytecode(ISTORE);
                break;
            case eVAR_INT:
            case eVAR_BYTE:
                compile_input(var);
                bytecode(IDROP);
                bytecode(I2INT);
                bytecode(ISWAP);
                if (var->vtype==eVAR_BYTE){
                    bytecode(ICSTORE);
                }else{
                    bytecode(ISTORE);
                }
                break;
//            case eVAR_INTARRAY:
//            case eVAR_BYTEARRAY:
//                compile_input(var);
//                bytecode(IDROP);
//                bytecode(I2INT);
//                bytecode(ISWAP);
//                bytecode(ICSTORE);
//                break;
        }//switch
        next_token();
        if (token.id==eCOMMA){
            next_token();
        }else{
            break;
        }
    }
    unget_token=true;
}//f


//compile un terme cha�ne
// string_term ::= cha�ne_constante|variable_cha�ne|fonction_cha�ne.
static void string_term(){
    char *string;
    var_t *svar;
    int len;
    
    next_token();
    switch (token.id){
        case eSTRING:
            bytecode(ISTRADR);
            len=strlen(token.str);
            if (!len){
                bytecode(0);
            }else{
                literal_string(token.str);
            }
            break;
        case eKWORD:
            if ((KEYWORD[token.n].fntype==eFN_STR)){
                KEYWORD[token.n].cfn();
            }else{
                throw(eERR_SYNTAX);
            }
            break;
        case eIDENT:
            if (!(svar=var_search(token.str))){throw(eERR_UNKNOWN);}
            if (!(svar->vtype==eVAR_STR)){throw(eERR_BAD_ARG);}
            code_var_address(svar);
            bytecode(IFETCH);
            break;
        default:
            throw(eERR_SYNTAX);
    }
}//string_term()

//compile les expressions cha�nes.
// expression_cha�ne::= string_term ['+' sring_term]*
// le caract�re '+' est un op�rateur de concat�nation
static void string_expression(){
    string_term();
    next_token();
    cpush(0);
    while (token.id==ePLUS){
        _ctop()++; //nombre de cha�ne interm�diaires
        string_term();
        bytecode(IAPPEND);
        next_token();
        if (token.id==ePLUS){
            bytecode(IDUP);
        }
    }
    unget_token=true;
    //lib�ration des cha�nes interm�diaires.
    while (_ctop()>1){
        bytecode(ISWAP);
        bytecode(ISTRFREE);
        _ctop()--;
    }
}//string_expression()

// assigne une valeur � une variable cha�ne.
static void code_let_string(var_t *var){
    bytecode(IDUP);
    bytecode(IFETCH);
    bytecode(ISWAP);
    string_expression();
    if (!_ctop()){
        bytecode(IDUP);
        bytecode(IQBRAZ);
        cpush(dptr);
        dptr+=2;
        bytecode(IDUP);
        bytecode(ILEN);
        bytecode(ISTRALLOC);
        bytecode(ISTRCPY);
        patch_fore_jump(cpop());
    }
    cdrop();
    bytecode(ISWAP); 
    bytecode(ISTORE);
    bytecode(ISTRFREE);
}

// LET varname=expression
//assigne une valeur � une variable
//si la variable n'existe pas elle est cr��e.
static void kw_let(){
    char name[32];
    var_t *var;
    int len;
    
    expect(eIDENT);
    strcpy(name,token.str);
    len=strlen(name);
    if (var_local && (name[len-1]=='$')){throw(eERR_BAD_ARG);}
    var=var_search(name);
    if (var && var->ro){ throw(eERR_REDEF);}
    if (!var && var_local){throw(eERR_BAD_ARG);}
    next_token();
    unget_token=true;
    if (token.id==eLPAREN){
        if (!var ||(var && !var->array)){
            throw(eERR_NOT_ARRAY);
        }
    }
    if (!var){
        var=var_create(name,NULL);
    }
    code_var_address(var);
    expect(eEQUAL);
    switch(var->vtype){
        case eVAR_STR:
            code_let_string(var);
            break;
        case eVAR_INT: 
        case eVAR_BYTE:
        case eVAR_FLOAT:
            expression();
            bytecode(ISWAP);
            if (var->vtype==eVAR_BYTE){
                bytecode(ICSTORE);
            }else{
                bytecode(ISTORE);
            }
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
}//f()

// LOCAL identifier {,identifier}
// cr�ation des variables locales
// � l'int�rieur des SUB|FUNC
static void kw_local(){
    var_t *var;
    int i,lc=0;
    if (!var_local) throw(eERR_SYNTAX);
    next_token();
    while(token.id==eIDENT){
        if (token.str[strlen(token.str)-1]=='#' || 
                token.str[strlen(token.str)-1]=='$' ) throw(eERR_BAD_ARG);
        if (globals>varlist){
            i=varlist->n+1; 
        }else{
            i=1;
        }
        var=var_create(token.str,(char*)&i);
        lc++;
        next_token();
        if (token.id!=eCOMMA) break;
        next_token();
    }//while
    unget_token=true;
    bytecode(ILCSPACE);
    bytecode(lc);
}//f

static void print_string(){
    unget_token=true;
    string_expression();
    if (_ctop()){
        bytecode(IDUP);
        bytecode(ITYPE);
        bytecode(ISTRFREE);
    }else{
        bytecode(ITYPE);
    }
    cdrop();
    _litc(1);
    bytecode(ISPACES);
}

// PRINT|? chaine|identifier|expression {,chaine|identifier|expression}
static void kw_print(){
    var_t *var;
    char *str;
    
    while (!activ_reader->eof){
        next_token();
        switch (token.id){
            case eSTRING:
                print_string();
                break;
            case eIDENT:
                if (!(var=var_search(token.str))){throw(eERR_UNKNOWN);}
                    if ((var->vtype==eVAR_STR)||((var->vtype==eVAR_FUNC)&&
                            (var->name[strlen(var->name-1)]=='$'))){
                        print_string();
                    }else{
                        unget_token=true;
                        expression();
                        bytecode(IDOT);
                    }
                break;
            case eKWORD:
                if (KEYWORD[token.n].fntype==eFN_STR){
                    print_string();
                }else{
                    unget_token=true;
                    expression();
                    bytecode(IDOT);
                }
                break;
            case eSTOP:
                bytecode(ICR);
                break;
            default:
                unget_token=true;
                expression();
                bytecode(IDOT);
                
        }//switch
        next_token();
        if (token.id==eSEMICOL){
            break;
        }
        if (token.id!=eCOMMA){
            bytecode(ICR);
            unget_token=true;
            break;
        }
    }//while
}//f()

// PUTC(expression)
// imprime un caract�re ASCII
static void kw_putc(){
    expression();
    bytecode(IEMIT);
}//f

// KEY()
// retourne indicateur touche clavier
// ou z�ro
static void kw_key(){ 
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IKEY);
}//f

// INVERTVID()
// inverse la sortie vid�o
// noir/blanc
static void kw_invert_video(){
    parse_arg_list(1);
    bytecode(IINV_VID);
}

// cr� la liste des arguments pour
// la compilation des SUB|FUNC
static uint8_t create_arg_list(){
    var_t *var;
    int i=0;
    expect(eLPAREN);
    next_token();
    while (token.id==eIDENT){
        i++;
        var=var_create(token.str,(char*)&i);
        next_token();
        if (token.id!=eCOMMA) break;
        next_token();
    }
    if (token.id!=eRPAREN) throw(eERR_SYNTAX);
    bytecode(IFRAME);
    bytecode(i); //nombre d'arguments de la FUNC|SUB
    return i&0x1f;
}//f

//cr� une nouvelle variable de type eVAR_SUB|eVAR_FUNC
//ces variables contiennent un pointer vers le point d'entr�
//du code.
static void subrtn_create(int var_type, int blockend){
    var_t *var;
    int arg_count;
    bool declared=true;
    
    if (var_local) throw(eERR_SYNTAX);
    expect(eIDENT);
//    if (token.str[strlen(token.str)-1]=='$') throw(eERR_SYNTAX);
    var=var_search(token.str);
    if (!var){
        var=var_create(token.str,NULL);
        var->vtype=var_type;
        var->adr=(void*)undefined_sub;
        var->dim=0;
        declared=false;
    }
    if ((var->vtype==var_type) && (var->adr==(void*)undefined_sub)){
        var->adr=(void*)&progspace[dptr];
        globals=varlist;
        var_local=true;
        cpush((uint32_t)endmark);
        complevel++;
        cpush(blockend);
        cpush((uint32_t)var);
        arg_count=create_arg_list();
        if (declared && arg_count!=var->dim){
            throw(eERR_BAD_ARG_COUNT);
        }
        var->dim=arg_count;
    }else{
        throw(eERR_DUPLICATE);
    }
}//f

// FUNC identifier (arg_list)
//   bloc_intructions
//   RETURN expression
// END FUNC
static void kw_func(){
    subrtn_create(eVAR_FUNC,eKW_FUNC);
}//f

// SUB idientifier (arg_list)
//  bloc_instructions
// END SUB
static void kw_sub(){
    subrtn_create(eVAR_SUB,eKW_SUB);
}//f


//ASC("c")
// retourne le code ASCII du premier caract�re de la cha�ne.
static void kw_asc(){
    parse_arg_list(1);
    bytecode(IASC);
}

//CHR$(expression)
// retourne une cha�ne de 1 caract�re correspondant � la valeur ASCII
// de l'expression.
static void kw_chr(){
    parse_arg_list(1);
    bytecode(ICHR);
}

//STR$(expression)
//convertie une expression num�rique en cha�ne
static void kw_string(){
    parse_arg_list(1);
    lit((uint32_t)pad);
    bytecode(I2STR);
}

//DATE$
// commande qui retourne une cha�ne date sous la forme AAAA/MM/JJ
static void kw_date(){
    bytecode(IDATE);
}

//TIME$
// commande qui retourne une cha�ne heure sous la forme HH:MM:SS
static void kw_time(){
    bytecode(ITIME);
}

//LEFT$(string,n)
// extrait les n premier caract�res de la cha�ne
static void kw_left(){
    parse_arg_list(2);
    bytecode(ILEFT);
}
//RIGHT$(string,n)
static void kw_right(){
    parse_arg_list(2);
    bytecode(IRIGHT);
}

//MID$(string,n1,n2)
static void kw_mid(){
    parse_arg_list(3);
    bytecode(IMID);
}

//SUBST$(s1,s2,n)
static void kw_subst(){
    parse_arg_list(3);
    bytecode(ISUBST);
}

//APPEND$(string1,string2)
// string1string2
static void kw_append(){
    parse_arg_list(2);
    bytecode(IAPPEND);
}

//PREPEND$(string1,string2)
// string2string1
static void kw_prepend(){
    parse_arg_list(2);
    bytecode(IPREPEND);
}

//INSERT$(s1,s2,n)
// ins�re s2 � la position n de s1
static void kw_insert(){
    parse_arg_list(3); // (s1 s2 n )
    bytecode(IINSERT);
}

//INSTR([start,]texte,cible)
// recherche la cha�ne "cible" dans le texte
// l'argument facultatif [start], indique la position de d�part dans le texte.
// Puisque le premier param�tre est facultatif on ne peut utiliser parse_arg_list().
static void kw_instr(){
    var_t *var;
    int count=0;
    
    expect(eLPAREN);
    do{
        next_token();
        switch (token.id){
            case eIDENT:
                if (!try_string_var()){
                    unget_token=true;
                    expression();
                }
                break;
            case eSTRING:
                bytecode(ISTRADR);
                literal_string(token.str);
                break;
            case eKWORD:
                if (KEYWORD[token.kw].fntype==eFN_STR){
                    KEYWORD[token.kw].cfn();
                }else{
                    unget_token=true;
                    expression();
                }
                break;
            default:
                unget_token=true;
                expression();
                break;
        }
        count++;
        next_token();
    }while (token.id==eCOMMA);
    if (token.id!=eRPAREN) throw(eERR_SYNTAX);
    if (count==2){
        _litc(1);
        bytecode(INROT);
        count++;
    }
    if (count!=3) throw(eERR_SYNTAX);
    bytecode(IINSTR);
}

//VAL(cha�ne|var$)
// convertie une valeur ch�ine en entier
static void kw_val(){
    parse_arg_list(1);
    bytecode(I2INT);
}

// TRACE(expression)
// active ou d�sactive le mode tra�age
// pour le d�bogage
// expression -> 0|1 {off|on}
static void kw_trace(){
    parse_arg_list(1); 
    bytecode(ITRACE);
}//f

static void compile(){
    var_t *var;
    uint32_t adr;
    
    do{
        next_token(); 
        switch(token.id){ 
            case eKWORD:
                if (KEYWORD[token.kw].fntype>eFN_NOT){
                    KEYWORD[token.kw].cfn();
                    bytecode(IDROP); // jette la valeur de retour
                }else{
                    KEYWORD[token.kw].cfn();
                }
                break;
            case eIDENT:
                if ((var=var_search(token.str))){
                    if ((var->vtype==eVAR_SUB)||(var->vtype==eVAR_FUNC)){
                        adr=(uint32_t)&var->adr;
                        if (var->vtype==eVAR_FUNC){
                            _litc(0);
                        } 
                        parse_arg_list(var->dim);
                        lit(adr);
                        bytecode(IFETCH);
                        bytecode(ICALL);
                        if (var->vtype==eVAR_FUNC){
                            bytecode(IDROP); //jette la valeur de retour
                        }
                    }else{
                        unget_token=true;
                        kw_let();
                    }
                }else{
                    unget_token=true;
                    kw_let();
                }
                break;
            case eSTOP:
                if (activ_reader->device==eDEV_KBD) activ_reader->eof=true;
                break;
            case eNONE:
                break;
            default:
                throw(eERR_SYNTAX);
                break;
        }//switch
    }while (!activ_reader->eof);
}//f

//efface le contenu de progspace
static void clear(){
    free_string_vars();
    dptr=0;
    varlist=NULL;
    globals=NULL;
    var_local=false;
    complevel=0;
    endmark=(void*)progspace+prog_size;
    memset((void*)progspace,0,prog_size);
    line_count=0;
    token_count=0;
    program_loaded=false;
    run_it=false;
    program_end=0;
    exit_basic=false;
    csp=0;
}//f


#ifdef DEBUG
static void print_prog(int start){
    int i;
    for (i=start;i<dptr;i++){
        print_int(con,progspace[i],3);
        put_char(con,' ');
    }
    crlf(con);
}

static void print_cstack(){
    int i;
    for (i=0;i<csp;i++){
        print_hex(con,ctrl_stack[i],0);
    }
    crlf(con);
        
}
#endif

extern bool f_trace;

void BASIC_shell(unsigned basic_heap, unsigned option, const char* file_or_string){

    pad=malloc(PAD_SIZE);
    prog_size=(biggest_chunk()-basic_heap)&0xfffffff0;
    progspace=malloc(prog_size);
    if (!(option==EXEC_STRING || option==EXEC_FILE)){
        clear_screen(con);
        sprintf((char*)progspace,"vpcBASIC v1.0\nRAM available: %d bytes\n"
                                 "strings space: %d bytes\n",prog_size,basic_heap);
        print(con,progspace);
    }
    clear();
//  initialisation lecteur source.
    reader_init(&std_reader,eDEV_KBD,NULL);
    activ_reader=&std_reader; 
    if (option==EXEC_FILE){
        if (!setjmp(failed)){
            run_file(file_or_string);
            exit_basic=true;
        }else{
            clear();
            reader_init(&std_reader,eDEV_KBD,NULL);
            activ_reader=&std_reader;
        }
    }else if ((option==EXEC_STRING) || (option==EXEC_STAY)){
        sram_device_t *sram_file;
        reader_t *sr_reader;
        int len;

        sram_file=malloc(sizeof(sram_device_t));
        sr_reader=malloc(sizeof(reader_t));
        sram_file->first=0;
        sram_file->pos=0;
        len=strlen(file_or_string)+1;
        sram_file->fsize=len;
        sram_write_block(0,file_or_string,len);
        reader_init(sr_reader,eDEV_SPIRAM,sram_file);
        if (!setjmp(failed)){
            activ_reader=sr_reader;
            compile();
            exec_basic();
        }else{
            clear();
        }
        activ_reader=&std_reader;
        free(sram_file);
        free(sr_reader);
        if (option==EXEC_STRING){
            exit_basic=true;
        }
    }
// boucle interpr�teur    
    while (!exit_basic){
        if (!setjmp(failed)){
            activ_reader->eof=false;
            compile();
            if (!complevel && (dptr>program_end)){
#ifdef DEBUG
                print_prog(program_end);
#endif
                exec_basic();
            }
        }else{
            if (exit_basic) break;
            clear();
            reader_init(&std_reader,eDEV_KBD,NULL);
            activ_reader=&std_reader;
        }// if (!setjump(failed))
    }//while(1)
    // d�sactive le mode trace avant de quitter.
    f_trace=false;
    free_string_vars();
    free(progspace);
    free(pad);
}//BASIC_shell()
