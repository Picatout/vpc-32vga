/*
* Copyright 2013,2014,2017,2018 Jacques Deschênes
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
 * Interpréteur BASIC compilant du bytecode pour la machine à piles définie dans vm.S 
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

/* macros utilisées par le compilateur BASIC*/

// n=frame_type_e
#define _arg_frame(n) bytecode(IFRAME);\
                     bytecode(n)

#define _byte0(n) ((uint32_t)(n)&0xff)
#define _byte1(n)((uint32_t)(n>>8)&0xff)
#define _byte2(n)(((uint32_t)(n)>>16)&0xff)
#define _byte3(n)(((uint32_t)(n)>>24)&0xff)



#define _litc(c)  bytecode(ICLIT);\
                  bytecode(_byte0(c))


#define _prt_varname(v) lit((uint32_t)v->name);\
                      bytecode(ITYPE)

// pile de contrôle utilisée par le compilateur
#define CTRL_STACK_SIZE 32
//#define cdrop() csp--;ctrl_stack_check()
#define _ctop() ctrl_stack[csp-1]
#define _cnext() ctrl_stack[csp-2]
#define _cpick(n) ctrl_stack[csp-n-1]


static bool exit_basic;


#define PAD_SIZE CHAR_PER_LINE
static char *pad; // tampon de travail

extern int complevel;

static uint8_t *progspace; // espace programme.
static unsigned dptr; //data pointer
static void* endmark; // pointeur dernière variable globale
extern volatile uint32_t pause_timer;

static uint32_t ctrl_stack[CTRL_STACK_SIZE];
static int8_t csp;

static jmp_buf failed; // utilisé par setjmp() et lngjmp()

static reader_t std_reader; // source clavier
static reader_t file_reader; // source fichier
static reader_t *activ_reader=NULL; // source active
//static void *endmark;
static uint32_t prog_size;
static uint32_t line_count;
static uint32_t token_count;
static bool program_loaded;
static bool run_it=false;
static uint32_t program_end;

#define NAME_MAX 32 //longueur maximale nom incluant zéro terminal
#define LEN_MASK 0x1F   // masque longueur nom.
#define HIDDEN 0x20 // mot caché
#define FUNCTION 0x40 // ce mot est une fonction
#define AS_HELP 0x80 //commande avec aide
#define ADR  (void *)



typedef void (*fnptr)();

//entrée de commande dans liste
typedef struct{
    fnptr cfn; // pointeur fonction commande
    uint8_t len; // longueur nom commande, flags IMMED, HIDDEN
    char *name; // nom de la commande
} dict_entry_t;



typedef enum {eVAR_INT,eVAR_STR,eVAR_INTARRAY,eVAR_BYTEARRAY,eVAR_STRARRAY,
              eVAR_SUB,eVAR_FUNC,eVAR_LOCAL,eVAR_CONST,eVAR_BYTE,eVAR_CONST_STR
             }var_type_e;

typedef struct _var{
    struct _var* next; // prochaine variable dans la liste
    uint16_t len:5; // longueur du nom
    uint16_t ro:1; // constante
    uint16_t array:1; // c'est un tableau
    uint16_t vtype:4; // var_type_e
    uint16_t dim:5; // nombre de dimension du tableau
    char *name;
    union{
        uint8_t byte; // variable octet
        int32_t n;    // entier ou nombre d'arguments et de variables locales pour func et sub
        char *str;    // adresse chaîne asciiz
        void *adr;    // addresse tableau,sub ou func
    };
}var_t;

// type d'unité lexicales
typedef enum {eNONE,eSTOP,eCOLON,eIDENT,eNUMBER,eSTRING,ePLUS,eMINUS,eMUL,eDIV,
              eMOD,eCOMMA,eLPAREN,eRPAREN,eSEMICOL,eEQUAL,eNOTEQUAL,eGT,eGE,eLT,eLE,
              eEND, eELSE,eCMD,eKWORD,eCHAR} tok_id_t;

// longueur maximale d'une ligne de texte source.
#define MAX_LINE_LEN 128

// unité lexicale.              
typedef struct _token{
   tok_id_t id;
   int n; //  identifiant KEYWORD ou valeur si entier
   char str[MAX_LINE_LEN]; // chaîne représentant le jeton.
}token_t;



static token_t token;
static bool unget_token=false;
// prototypes des fonctions
static void clear();
static void kw_abs();
static void bad_syntax();
static void kw_box();
static void kw_btest();
static void kw_bye();
static void kw_case();
static void kw_circle();
static void kw_cls();
static void kw_const();
static void kw_curcol();
static void kw_curline();
static void kw_dim();
static void kw_do();
static void kw_ellipse();
static void kw_else();
static void kw_end();
static void kw_exit();
static void kw_for();
static void kw_func();
static void kw_getpixel();
static void kw_if();
static void kw_input();
static void kw_insert_line();
static void kw_invert_video();
static void kw_key();
static void kw_len();
static void kw_let();
static void kw_line();
static void kw_local();
static void kw_locate();
static void kw_loop();
static void kw_max();
static void kw_min();
static void kw_next();
static void kw_pause();
static void kw_polygon();
static void kw_print();
static void kw_putc();
static void kw_randomize();
static void kw_rect();
static void kw_ref();
static void kw_rem();
static void kw_restore_screen();
static void kw_return();
static void kw_rnd();
static void kw_save_screen();
static void kw_scrlup();
static void kw_scrldown();
static void kw_select();
static void kw_setpixel();
static void kw_set_timer();
static void kw_shl();
static void kw_shr();
static void kw_sprite();
static void kw_srclear();
static void kw_srload();
static void kw_srread();
static void kw_srsave();
static void kw_srwrite();
static void kw_sub();
static void kw_then();
static void kw_ticks();
static void kw_timeout();
static void kw_tone();
static void kw_tkey();
static void kw_trace();
static void kw_tune();
static void kw_ubound();
static void kw_use();
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
static unsigned get_arg_count(void* fn_code);
static void expression();
static void factor();
char* string_alloc(unsigned size);
void string_free(char *);
static void compile();
static void bool_expression();


#ifdef DEBUG
static void print_prog(int start);
static void print_cstack();
#endif

//identifiant KEYWORD doit-être dans le même ordre que
//dans la liste KEYWORD
enum {eKW_ABS,eKW_AND,eKW_BOX,eKW_BTEST,eKW_BYE,eKW_CASE,eKW_CIRCLE,eKW_CLEAR,eKW_CLS,
      eKW_CONST,eKW_CURCOL,eKW_CURLINE,eKW_DIM,eKW_DO,eKW_ELLIPSE,eKW_ELSE,eKW_END,eKW_EXIT,
      eKW_FOR,eKW_FUNC,eKW_GETPIXEL,eKW_IF,eKW_INPUT,eKW_INSERTLN,eKW_INVVID,eKW_KEY,eKW_LEN,
      eKW_LET,eKW_LINE,eKW_LOCAL,eKW_LOCATE,eKW_LOOP,eKW_MAX,eKW_MIN,eKW_NEXT,
      eKW_NOT,eKW_OR,eKW_PAUSE,eKW_POLYGON,
      eKW_PRINT,eKW_PUTC,eKW_RANDOMISIZE,eKW_RECT,eKW_REF,eKW_REM,eKW_RESTSCR,
      eKW_RETURN,eKW_RND,eKW_SAVESCR,eKW_SCRLUP,eKW_SCRLDN,
      eKW_SELECT,eKW_SETPIXEL,eKW_SETTMR,eKW_SHL,eKW_SHR,
      eKW_SPRITE,eKW_SRCLEAR,eKW_SRLOAD,eKW_SRREAD,eKW_SRSSAVE,eKW_SRWRITE,eKW_SUB,eKW_THEN,eKW_TICKS,
      eKW_TIMEOUT,eKW_TONE,eKW_TKEY,eKW_TRACE,eKW_TUNE,eKW_UBOUND,eKW_UNTIL,eKW_USE,
      eKW_WAITKEY,eKW_WEND,eKW_WHILE,eKW_XORPIXEL
};

//mots réservés BASIC
static const dict_entry_t KEYWORD[]={
    {kw_abs,3+FUNCTION,"ABS"},
    {bad_syntax,3,"AND"},
    {kw_box,3,"BOX"},
    {kw_btest,5+FUNCTION,"BTEST"},
    {kw_bye,3,"BYE"},
    {kw_case,4,"CASE"},
    {kw_circle,6,"CIRCLE"},
    {clear,5,"CLEAR"},
    {kw_cls,3+AS_HELP,"CLS"},
    {kw_const,5,"CONST"},
    {kw_curcol,6+FUNCTION,"CURCOL"},
    {kw_curline,7+FUNCTION,"CURLINE"},
    {kw_dim,3,"DIM"},
    {kw_do,2,"DO"},
    {kw_ellipse,7,"ELLIPSE"},
    {kw_else,4,"ELSE"},
    {kw_end,3,"END"},
    {kw_exit,4,"EXIT"},
    {kw_for,3,"FOR"},
    {kw_func,4,"FUNC"},
    {kw_getpixel,8+FUNCTION,"GETPIXEL"},
    {kw_if,2,"IF"},
    {kw_input,5,"INPUT"},
    {kw_insert_line,8,"INSERTLN"},
    {kw_invert_video,9,"INVERTVID"},
    {kw_key,3+FUNCTION,"KEY"},
    {kw_len,3+FUNCTION,"LEN"},
    {kw_let,3,"LET"},
    {kw_line,4,"LINE"},
    {kw_local,5,"LOCAL"},
    {kw_locate,6,"LOCATE"},
    {kw_loop,4,"LOOP"},
    {kw_max,3+FUNCTION,"MAX"},
    {kw_min,3+FUNCTION,"MIN"},
    {kw_next,4,"NEXT"},
    {bad_syntax,3,"NOT"},
    {bad_syntax,2,"OR"},
    {kw_pause,5,"PAUSE"},
    {kw_polygon,7,"POLYGON"},
    {kw_print,5,"PRINT"},
    {kw_putc,4,"PUTC"},
    {kw_randomize,9,"RANDOMIZE"},
    {kw_rect,4,"RECT"},
    {kw_ref,1+FUNCTION,"@"},
    {kw_rem,3,"REM"},
    {kw_restore_screen,7,"RESTSCR"},
    {kw_return,6,"RETURN"},
    {kw_rnd,3+FUNCTION,"RND"},
    {kw_save_screen,7,"SAVESCR"},
    {kw_scrlup,6,"SCRLUP"},
    {kw_scrldown,6,"SCRLDN"},
    {kw_select,6,"SELECT"},
    {kw_setpixel,8,"SETPIXEL"},
    {kw_set_timer,6,"SETTMR"},
    {kw_shl,3+FUNCTION,"SHL"},
    {kw_shr,3+FUNCTION,"SHR"},
    {kw_sprite,6,"SPRITE"},
    {kw_srclear,7,"SRCLEAR"},
    {kw_srload,6+FUNCTION,"SRLOAD"},
    {kw_srread,6,"SRREAD"},
    {kw_srsave,6+FUNCTION,"SRSAVE"},
    {kw_srwrite,7,"SRWRITE"},
    {kw_sub,3,"SUB"},
    {kw_then,4,"THEN"},
    {kw_ticks,5+FUNCTION,"TICKS"},
    {kw_timeout,7+FUNCTION,"TIMEOUT"},
    {kw_tone,4,"TONE"},
    {kw_tkey,4+FUNCTION,"TKEY"},
    {kw_trace,5,"TRACE"},
    {kw_tune,4,"TUNE"},
    {kw_ubound,6+FUNCTION,"UBOUND"},
    {bad_syntax,5,"UNTIL"},
    {kw_use,3,"USE"},
    {kw_waitkey,7+FUNCTION,"WAITKEY"},
    {kw_wend,4,"WEND"},
    {kw_while,5,"WHILE"},
    {kw_xorpixel,8,"XORPIXEL"},
    {NULL,0,""}
};


//recherche la commande dans
//le dictionnaire système
// ( uaddr -- n)
static int dict_search(const  dict_entry_t *dict){
    int i=0, len;
    
    len=strlen(token.str);
    while (dict[i].len){
        if (((dict[i].len&LEN_MASK)==len) && 
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
    println(con,token.str);
}

//code d'erreurs
//NOTE: eERR_DSTACK et eERR_RSTACK sont redéfinie dans stackvm.h
//      si leur valeur change elles doivent aussi l'être dans stackvm.h
enum {eERROR_NONE,eERR_DSTACK,eERR_RSTACK,eERR_MISSING_ARG,eERR_EXTRA_ARG,
      eERR_BAD_ARG,eERR_SYNTAX,eERR_ALLOC,eERR_REDEF,
      eERR_NOTDIM,eERR_BOUND,eERR_PROGSPACE,eERR_OUT_RANGE,
      eERR_STACK_UNDER,eERR_STACK_OVER,eERR_NO_STR,eERR_NO_DIM,eERR_NO_CONST
      };

 static  const char* error_msg[]={
    "no error\n",
    "data stack out of bound\n",
    "control stack out of bound\n",
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
 };
 
 
static void throw(int error){
    char message[64];
    
//    fat_close_all_files();
    crlf(con);
    if (activ_reader->device==eDEV_SDCARD){
        print(con,"line: ");
        print_int(con,line_count,0);
        crlf(con);
    }else{
        print(con,"token#: ");
        print_int(con,token_count,0);
        crlf(con);
    }
    strcpy(message,error_msg[error]);
    print(con,message);
    print_token_info();
#ifdef DEBUG    
    print_prog(program_end);
#endif    
    activ_reader->eof=true;
    longjmp(failed,error);
}


static var_t *varlist=NULL;
static var_t *globals=NULL;
static bool var_local=false;

// allocation de l'espace pour une chaîne asciiz sur le heap;
// length est la longueur de la chaine.
char* string_alloc(unsigned length){
    char *str;
    str=malloc(++length);
    if (!str){
        throw(eERR_NO_STR);
    }
    return str;
}


// libération de l'espace réservée pour une chaîne asciiz.
void string_free(char *str){
    if (str) free(str);
}



void free_string_vars(){
    var_t *var;
    void *limit;
    char **str_array;
    int i,count;
    
    
    limit=progspace+prog_size;
    var=varlist;
    while (var){
        if (var->vtype==eVAR_STR){
            string_free(var->str);
        }else if (var->vtype==eVAR_STRARRAY){
            str_array=(char**)var->adr;
            count=*(unsigned*)str_array;
            for (i=1;i<=count;i++){
                string_free(str_array[i]);
            }
        }
        var=var->next;
    }
}

// déclenche une exeption en cas d'échec.
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

// les variables sont allouées à la fin
// de progspace en allant vers le bas.
// lorsque endmark<=&progrspace[dptr] la mémoire est pleine
static var_t *var_create(char *name, void *value){
    int len;
    void *newend;
    var_t *newvar=NULL;
    void *varname=NULL;
    
    len=min(31,strlen(name));
    
    newend=endmark;
    newvar=alloc_var_space(sizeof(var_t));
    newend=newvar;
    varname=alloc_var_space(len+1);
    newend=varname;
    strcpy(varname,name);
    if (name[strlen(name)-1]=='$'){ // variable chaine
        if (value){
            newvar->str=alloc_var_space(strlen(value)+1);
            newend=newvar->adr;
            strcpy(newvar->str,value);
        }
        else{
            newvar->str=NULL;
        }
        newvar->vtype=eVAR_STR;
    }else if (name[strlen(name)-1]=='#'){ // variable octet
        newvar->vtype=eVAR_BYTE;
        if (value)
            newvar->byte=*((uint8_t*)value);
        else
            newvar->byte=0;
    }else{// entier 32 bits
        if (var_local){ 
            newvar->vtype=eVAR_LOCAL;
        }else{ 
            newvar->vtype=eVAR_INT;
        }
        if (value){
            newvar->n=*((int*)value);
        }else{
            newvar->n=0;
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

static void bytecode(uint8_t bc){
    if ((void*)&progspace[dptr]>=endmark) throw(eERR_PROGSPACE);
    progspace[dptr++]=bc;
}//f

// compile un entier litéral
static void lit(unsigned int n){
    int i;
    
    bytecode(ILIT);
    for (i=0;i<4;i++){
        bytecode(n&0xff);
        n>>=8;
    }
}

//calcul la distance entre slot_address+1 et dptr.
// le déplacement maximal authorizé est de 127 octets.
// la valeur du déplacement est déposée dans la fente 'slot_address'.
static void patch_fore_jump(int slot_addr){
    int displacement;
    
    displacement=dptr-slot_addr-1;
    if (displacement>127){throw(eERR_OUT_RANGE);}
    progspace[slot_addr]=_byte0(displacement);
}//f

//calcul la distance entre dptr+1 et target_address
//le déplacement maximal authorizé est de -128 octets.
// la valeur du déplacement est déposé à la position 'dptr'
static void patch_back_jump(int target_addr){
    int displacement;
    
    displacement=target_addr-dptr-1;
    if (displacement<-128){throw(eERR_OUT_RANGE);}
    progspace[dptr++]=_byte0(displacement);
}

void cpush(uint32_t n){
    if (csp==CTRL_STACK_SIZE){throw(eERR_STACK_OVER);}
    ctrl_stack[csp++]=n;
}

uint32_t cpop(){
    if (!csp) {throw(eERR_STACK_UNDER);}
    return ctrl_stack[--csp];
}

static void cdrop(){
    if (!csp){throw(eERR_STACK_UNDER);}
    csp--;
}

// vérifie les limites de ctrl_stack
static void ctrl_stack_check(){
    if (csp<0){
        throw(eERR_STACK_UNDER);
    }else if (csp==CTRL_STACK_SIZE){
        throw(eERR_STACK_OVER);
    }
}
// rotation des 3 éléments supérieur de la pile de contrôle
// de sorte que le sommet se retrouve en 3ième position
static void ctrl_stack_nrot(){
    int temp;
    
    temp=_cpick(2);
    _cpick(2)=_ctop();
    _ctop()=_cnext();
    _cnext()=temp;
}

//rotation des 3 éléments supérieur de la pile de contrôle
// de sorte que le 3ième élément se retrouve au sommet
static void ctrl_stack_rot(){
    int temp;
    
    temp=_cpick(2);
    _cpick(2)=_cnext();
    _cnext()=_ctop();
    _ctop()=temp;
}

//caractères qui séparent les unitées lexicale
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
        token.n=i;
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
                token.n=eKW_REM;
                token.str[0]='\'';
                token.str[1]=0;
                break;
            case '\\':
                token.id=eCHAR;
                token.n=reader_getc(activ_reader);
                token.str[0]=token.n;
                token.str[1]=0;
                break;
            case '"': // chaîne de caractères
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
                token.n=eKW_PRINT;
                token.str[0]='?';
                token.str[1]=0;
                break;
            case '@': // référence
                token.id=eKWORD;
                token.n=eKW_REF;
                token.str[0]='@';
                token.str[1]=0;
                break;
            case '\n':
            case '\r':
                line_count++;
                if (activ_reader->device==eDEV_SDCARD){ // considéré comme un espace
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

//devrait-être à la fin de la commande
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
//    char *subs; // sous-chaïne filtre.
//    filter_enum criteria; // critère
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
    if (var->vtype==eVAR_INTARRAY || var->vtype==eVAR_STRARRAY){
        _litc(2);
        bytecode(ILSHIFT);//4*index
    }else{
        // élément 0,1,2,3  réservé pour size
        // index 1 correspond à array[4]
        _litc(4);
        bytecode(IPLUS);
    }
    //index dans le tableau
    lit((uint32_t)var->adr);
    bytecode(IPLUS);
}//f


static void parse_arg_list(unsigned arg_count){
    int count=0;
    expect(eLPAREN);
    next_token();
    if (token.id==eRPAREN){
        if (arg_count) throw(eERR_MISSING_ARG);
        return;
    }
    unget_token=true;
    do{
        expression();
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
    next_token(); print(con,token.str);
    switch(token.id){
        case eKWORD:
            if ((KEYWORD[token.n].len&FUNCTION)){ //print_prog(program_end);
                KEYWORD[token.n].cfn();
            }else  throw(eERR_SYNTAX);
            break;
        case eIDENT:
            if ((var=var_search(token.str))){
                switch(var->vtype){
                    case eVAR_FUNC:
                        _arg_frame(FRAME_FUNC);
                        parse_arg_list(get_arg_count((void*)(var->adr)));
                        lit(((uint32_t)var->adr)+1);
                        bytecode(ICALL);
                        break;
                    case eVAR_LOCAL:
                        bytecode(ILCFETCH);
                        bytecode(var->n&(DSTACK_SIZE-1));
                        break;
                    case eVAR_BYTE:
                        lit((uint32_t)&var->n);
                        bytecode(ICFETCH);
                        break;
                    case eVAR_CONST:
                        lit(var->n);
                        break;
                    case eVAR_INT:
                        lit((uint32_t)&var->n);
                        bytecode(IFETCH);
                        break;
                    case eVAR_BYTEARRAY:
                        code_array_address(var);
                        bytecode(ICFETCH);
                        break;
                    case eVAR_INTARRAY: 
                        code_array_address(var);
                        bytecode(IFETCH);
                        break;
                    default:
                        throw(eERR_SYNTAX);
                }//switch
            }else if (!var){
                if (var_local){
                    i=varlist->n+1;
                    var=var_create(token.str,(char*)&i);
                    _litc(0);
                    bytecode(ILCSTORE);
                    bytecode(i);
                }else{
                    var=var_create(token.str,NULL);
                    next_token();
                    if (token.id==eLPAREN) throw(eERR_SYNTAX);
                    unget_token=true;
                    lit((uint32_t)&var->n);
                    bytecode(IFETCH);
                }
            }else{
                throw(eERR_SYNTAX);
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
        _litc(0);
        bytecode(IEQUAL);
        bytecode(INOT);
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

static void bool_factor(){
    int boolop=0;
    //print(con,"bool_factor\n");
    if (try_boolop()){
        if (token.n!=eKW_NOT) throw(eERR_SYNTAX);
        boolop=eKW_NOT;
    }
    condition();
    if (boolop){
        bytecode(INOT);
    }
}//f


static void bool_term(){// print(con,"bool_term()\n");
    int fix_count=0; // pour évaluation court-circuitée.
    bool_factor();
    while (try_boolop() && token.n==eKW_AND){
        //code pour évaluation court-circuitée
        bytecode(IDUP);
        bytecode(IQBRAZ); // si premier facteur==faux saut court-circuit.
        ctrl_stack[csp++]=dptr;
        dptr+=1;
        fix_count++;
        bool_factor();
        bytecode(IAND);
    }
    unget_token=true;
    while (fix_count){
        patch_fore_jump(cpop());
        fix_count--;
    }
}//f

static void bool_expression(){// print(con,"bool_expression()\n");
    int fix_count=0; // pour évaluation court-circuitée.
    bool_term();
    while (try_boolop() && token.n==eKW_OR){
        bytecode(IDUP);
        bytecode(IQBRA); // si premier facteur==vrai saut court-circuit.
        ctrl_stack[csp++]=dptr;
        dptr+=1;
        fix_count++;
        bool_term();
        bytecode(IOR);
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
    "file open error "
};

#define COMPILING 0
#define COMP_END 1
#define COMP_FILE_ERROR 2

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


// '(' number {','number}')'
static void init_int_array(var_t *var){
    int *iarray=NULL;
    unsigned char *barray=NULL;
    int size,count=1,state=0;
    
    if (var->vtype==eVAR_INTARRAY){
        iarray=var->adr;
        size=*iarray;
    }else{
        barray=var->adr;
        size=*barray;
    }
    if (var->vtype==eVAR_BYTEARRAY){ count=4;}
    expect(eLPAREN);
    next_token();
    while (token.id!=eRPAREN){
        switch (token.id){
            case eCHAR:
            case eNUMBER: 
                if (state) throw(eERR_SYNTAX);
                if (var->vtype==eVAR_INTARRAY){
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
    if (((var->vtype==eVAR_BYTEARRAY) && (count>(size+sizeof(uint32_t)))) ||
        ((var->vtype!=eVAR_BYTEARRAY) && (count>(size+1)))) throw(eERR_EXTRA_ARG);
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

static void dim_array(char *var_name){
    int size=0, len;
    void *array;
    var_t *new_var, *var;
    
    next_token();
    if (token.id==eNUMBER){
        size=token.n;
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (!(var && (var->vtype==eVAR_CONST))) throw(eERR_BAD_ARG);
        size=var->n;
    }else{
        throw(eERR_BAD_ARG);
    }
    expect(eRPAREN);
    len=strlen(var_name);
    if (size<1) throw(eERR_BAD_ARG);
    if (var_name[len-1]=='#'){ //table d'octets
        array=alloc_var_space(sizeof(uint8_t)*size+sizeof(int));
        memset(array,0,sizeof(uint8_t)*size+sizeof(int));
        *((uint32_t*)array)=size;
    }else{ // table d'entiers ou de chaînes
        array=alloc_var_space(sizeof(int)*(size+1));
        memset(array,0,sizeof(int)*(size+1));
        *((uint32_t*)array)=size;
    }
    new_var=(var_t*)alloc_var_space(sizeof(var_t));
    new_var->name=alloc_var_space(len+1);
    strcpy(new_var->name,var_name);
    new_var->adr=array;
    if (var_name[len-1]=='$'){
        new_var->vtype=eVAR_STRARRAY;
    }else if (var_name[len-1]=='#'){
        new_var->vtype=eVAR_BYTEARRAY;
    }else{
        new_var->vtype=eVAR_INTARRAY;
    }
    new_var->next=varlist;
    varlist=new_var;
    next_token();
    if (token.id==eEQUAL){
        if (new_var->vtype==eVAR_STRARRAY){
            init_str_array(new_var);
        }else{
            init_int_array(new_var);
        }
    }else{
        unget_token=true;
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
    
    if (var_local) throw(eERR_NO_DIM);
    expect(eIDENT);
    while (token.id==eIDENT){
        if (var_search(token.str)) throw(eERR_REDEF);
        strcpy(var_name,token.str);
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
        next_token();
        if (token.id==eCOMMA) next_token();
    }//while
    unget_token=true;
}//f

//passe une variable par référence
static void kw_ref(){
    var_t *var;
    
    next_token();
    if (token.id!=eIDENT) throw(eERR_BAD_ARG);
    var=var_search(token.str);
    if (!var) throw(eERR_BAD_ARG);
//    bytecode(ILIT);
    switch(var->vtype){
    case eVAR_INT:
    case eVAR_CONST:
        lit((uint32_t)&var->n);
        break;
    case eVAR_INTARRAY:
    case eVAR_STRARRAY:
    case eVAR_BYTEARRAY:
        lit(((uint32_t)var->adr)+sizeof(int));
        break;
    case eVAR_FUNC:
    case eVAR_SUB:
    case eVAR_STR:
        lit((uint32_t)var->adr);
        break;
    default:
        throw(eERR_BAD_ARG);
    }//switch
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
    if (!var || !(var->vtype>=eVAR_INTARRAY && var->vtype<=eVAR_STRARRAY)) throw(eERR_BAD_ARG);
    lit((uint32_t)var->adr);
    bytecode(IFETCH);
}//f

//USE fichier
// compilation d'un fichier source inclut dans un autre fichier source.
static void kw_use(){
    reader_t *old_reader, freader;
    FIL *fh;
    uint32_t lcount;
    FRESULT result;
    
    if (activ_reader->device==eDEV_KBD) throw(eERR_SYNTAX);
    expect(eSTRING);
    uppercase(token.str);
    if (!(result=f_open(fh,token.str,FA_READ))){
        reader_init(&freader,eDEV_SDCARD,fh);
        old_reader=activ_reader;
        activ_reader=&freader;
        compiler_msg(COMPILING, token.str);
        lcount=line_count;
        line_count=1;
        compile();
        line_count=lcount+1;
        f_close(fh);
        activ_reader=old_reader;
        compiler_msg(COMP_END,NULL);
    }else{
        compiler_msg(COMP_FILE_ERROR,NULL);
        throw(eERR_BAD_ARG);
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
// définition d'une constante
static void kw_const(){
    char name[32];
    var_t *var;
    
    if (var_local) throw(eERR_NO_CONST);
    expect(eIDENT);
    while (token.id==eIDENT){
        strcpy(name,token.str);
        if ((var=var_search(name))) throw(eERR_REDEF);
        expect(eEQUAL);
        var=var_create(name,NULL);
        if (var->vtype==eVAR_STR){
            expect(eSTRING);
            var->str=alloc_var_space(strlen(token.str)+1);
            strcpy(var->str,token.str);
            var->vtype=eVAR_CONST_STR;
        }else{
            var->vtype=eVAR_CONST;
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
// décalage à gauche de 1 bit
static void kw_shl(){
    parse_arg_list(2);
    bytecode(ILSHIFT);
}//f

// SHR(expression,n)
// décale à droite de 1 bit
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

//TONE(freq,msec)
//fait entendre un note de la gamme tempérée
static void kw_tone(){
    parse_arg_list(2);
    bytecode(ITONE);
}//f

//TUNE(@array)
// fait entendre une mélodie
static void kw_tune(){
    parse_arg_list(1);
    bytecode(ITUNE);
}

//PAUSE(msec)
// suspend exécution
// argument en millisecondes
static void kw_pause(){
    parse_arg_list(1);
    bytecode(IDELAY);
}//f

//TICKS()
//retourne la valeur du compteur
// de millisecondes du système
static void kw_ticks(){
    parse_arg_list(0);
    bytecode(ITICKS);
}//f

// SETTMR(msec)
// initialise la variable système timer
static void kw_set_timer(){
    parse_arg_list(1);
    bytecode(ISETTMR);
}//f

// TIMEOUT()
// retourne vrai si la varialbe système timer==0
static void kw_timeout(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(ITIMEOUT);
}//f

//COMMANDE BASIC: BYE
//quitte l'interpréteur
static void kw_bye(){
    if (!complevel && (activ_reader->device==eDEV_KBD)){
        exit_basic=true;
        return;
    }
    bytecode(IBYE);
}//f

// RETURN expression
// utilisé dans les fonctions
static void kw_return(){
        expression();
        bytecode(ILEAVE);
}//f

// EXIT SUB
// termine l'exécution d'une sous-routine
// en n'importe quel point de celle-ci.
// sauf à l'intérieur d'une boucle FOR
static void kw_exit(){
    expect(eKWORD);
    if (token.n != eKW_SUB) throw(eERR_SYNTAX);
    bytecode(ILEAVE);
}//f

// CLS
// efface écran
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
    dptr++;
}//f


// ELSE voir kw_if
static void kw_else(){
    bytecode(IBRA);
    bytecode(0);
    patch_fore_jump(cpop());
    ctrl_stack[csp++]=dptr-1;
}//f

//déplace le code des sous-routine
//à la fin de l'espace libre
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
//            bytecode(ILCSTORE);
//            bytecode(0);
            bytecode(ILEAVE);
            varlist=globals;
            globals=NULL;
            var_local=false;
            var=(var_t*)cpop();
            endmark=(void*)_cnext();
            movecode(var);
            csp-=2; // drop eKW_xxx et endmark
            var_local=false;
//#undef DEBUG
#ifdef DEBUG
    {
        uint8_t * bc;   
        crlf(con);print_hex(con,(uint32_t)var->adr,0);
        for(bc=(uint8_t*)var->adr;*bc!=ILEAVE;bc++){
            print_int(con,*bc,0);
        }
        print_int(con,*bc,0);
        crlf(con);
    }
#endif
#define DEBUG
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
        dptr++;
        fix_count++;
        next_token();
        if (token.id!=eCOMMA){
            unget_token=true;
            break;
        }
    }//while
    bytecode(IBRA);
    dptr++;
    while (fix_count){
        patch_fore_jump(cpop());
        fix_count--;
    }
    _ctop()++; //incrémente le nombre de clause CASE
    cpush(dptr-1); // fente pour le IBRA de ce case (i.e. saut après le END SELECT)
    ctrl_stack_nrot();
    
}//f

static void patch_previous_case(){
    uint32_t adr;
    
    ctrl_stack_rot();
    adr=cpop();
    bytecode(IBRA);  // branchement à la sortie du select case pour le case précédent
    cpush(dptr); // (adr blockend n -- dptr blockend n )
    ctrl_stack_nrot(); 
    dptr++;
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
// génération nombre pseudo-aléatoire
static void kw_rnd(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IRND);
}//f

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

//MIN(expr1,expr2)
static void kw_min(){
    parse_arg_list(2);
    bytecode(IMIN);
}//f

// GETPIXEL(x,y)
// retourne la couleur du pixel
// en position {x,y}
static void kw_getpixel(){
    parse_arg_list(2);
    bytecode(IGETPIXEL);
}//f

// SETPIXEL(x,y,p)
// fixe la couleur du pixel
// en position x,y
// p-> couleur {0,1}
static void kw_setpixel(){
    parse_arg_list(3);
    bytecode(IPUTPIXEL);
}//f

// XORPIXEL(x,y)
// XOR le pixel
static void kw_xorpixel(){
    parse_arg_list(2);
    bytecode(IXORPIXEL);
}//f

// SCRLUP()
// glisse l'écran vers le haut d'une ligne
static void kw_scrlup(){
    parse_arg_list(0);
    bytecode(ISCRLUP);
}//f

// SCRLDN()
// glisse l'écran vers le bas d'une lignes
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
//dissine un cercle rayon r, centré sur xc,yc
static void kw_circle(){
    parse_arg_list(3);
    bytecode(ICIRCLE);
}//f

//ELLIPSE(xc,yc,w,h)
//dessine une ellipse centrée sur xc,yc
// et de largeur w, hauteur h
static void kw_ellipse(){
    parse_arg_list(4);
    bytecode(IELLIPSE);
}//f

//POLYGON(@points)
//dessine un polygon à partir d'un tableau de points.
void kw_polygon(){
    parse_arg_list(2);
    bytecode(IPOLYGON);
}

// SPRITE(x,y,width,height,@sprite)
// applique le sprite à la position désigné
// il s'agit d'une opération xor
static void kw_sprite(){
    parse_arg_list(5);
    bytecode(ISPRITE);
}//f



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
    kw_let(); // vakeur initale de la variable de boucle
    expect(eIDENT);
    if (strcmp(token.str,"TO")) throw (eERR_SYNTAX);
    expression(); // valeur de la limite
    next_token();
    if (token.id==eIDENT && !strcmp(token.str,"STEP")){
        expression(); //valeur de l'incrément
    }else{
        _litc(1);
        unget_token=true;
    }
    bytecode(IFOR); // sauvegarde limit et step qui sont sur la pile
    var=var_search(name);
    cpush(dptr);  // adresse cible instruction FORTEST de la boucle for.
    if (var->vtype==eVAR_LOCAL){
        bytecode(ILCADR);
        bytecode(var->n);
    }else{
        lit((uint32_t)&var->n);
    }
    bytecode(IFETCH);
    bytecode(IFORTEST);
    bytecode(IQBRAZ);
    cpush(dptr); // fente déplacement après boucle FOR...NEXT
    dptr++;
}//f

// voir kw_for()
static void kw_next(){
    var_t *var;
    
    expect(eIDENT);
    var=var_search(token.str);
    if (!(var && ((var->vtype==eVAR_INT) || (var->vtype==eVAR_LOCAL)))) throw(eERR_BAD_ARG);
    if (var->vtype==eVAR_LOCAL){
        bytecode(ILCADR);
        bytecode(var->n);
    }else{
        lit((uint32_t)&var->adr);
    }
    bytecode(IFORNEXT);
    bytecode(IBRA);
    patch_back_jump(_cnext()); //saut arrière vers FORTEST
    patch_fore_jump(cpop());
    cdrop(); // jette cible de patch_back_jump() précédent
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
    dptr+=1;
}//f

// voir kw_while()
static void kw_wend(){
    int n;
    
    bytecode(IBRA);
    patch_back_jump(_cnext());
    patch_fore_jump(cpop());
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

// compile une chaîne litérale
static void literal_string(char *lit_str){
    int size;

    size=strlen(lit_str);
    if ((void*)&progspace[dptr+size+1]>endmark) throw(eERR_PROGSPACE);
    strcpy((void*)&progspace[dptr],lit_str);
    dptr+=size+1;
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
//met à zéro un bloc de mémoire SPIRAM
static void kw_srclear(){
    parse_arg_list(2);
    bytecode(ISRCLEAR);
}//f

//SRREAD(address,@var,size)
//lit un bloc de mémoire SPIRAM
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
//restore le buffer vidéo à partir de la SPI RAM
// adresse est l'adresse sour dans SPI RAM
static void kw_restore_screen(){
    parse_arg_list(1);
    bytecode(IRESTSCR);
}//f

//SAVESCR(adresse)
//sauvegarde le buffer vidéo dans la SPI RAM
// adresse est la destination dans SPI RAM
static void kw_save_screen(){
    parse_arg_list(1);
    bytecode(ISAVESCR);
}//f

//compile l'assignation pour élément de variable
// vecteur
static void array_let(char * name){
    var_t *var;
    char *adr;
    
    var=var_search(name);
    if (!var) throw(eERR_NOTDIM);
    if (!(var->vtype>=eVAR_INTARRAY && var->vtype<=eVAR_STRARRAY)) throw(eERR_BAD_ARG);
    code_array_address(var);
    expect(eEQUAL);
    switch (var->vtype){
        case eVAR_STRARRAY:
            expect(eSTRING);
            adr=string_alloc(strlen(token.str));
            strcpy(adr,token.str);
            lit((uint32_t)adr);
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_INTARRAY:
            expression();
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_BYTEARRAY:
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
    switch(var->vtype){
        case eVAR_LOCAL:
            bytecode(ILCSTORE);
            bytecode(var->n);
            break;
        case eVAR_INT:
            lit(((uint32_t)(int*)&var->n));
            bytecode(ISTORE);
            break;
        case eVAR_BYTE:
            lit(((uint32_t)(int*)&var->n));
            bytecode(ICSTORE);
            break;
        case eVAR_CONST:
            // ignore les assignations de constantes jette la valeur
            bytecode(IDROP);
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
// vérifie si une touche clavier est disponible
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
        switch(var->vtype){
            case eVAR_STR:
                lit((uint32_t)var->str);
                break;
            case eVAR_STRARRAY:
                lit((uint32_t)var->adr);
                expect(eLPAREN);
                expression();
                expect(eRPAREN);
                _litc(1);
                bytecode(IPLUS);
                _litc(2);
                bytecode(ILSHIFT);
                bytecode(IPLUS);
                break;
            default:
                throw(eERR_BAD_ARG);
        }//switch
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

// critères d'acceptation pour INPUT:
// Seule les variables tableaux existantes peuvent-être utilisées par input.
// Dans un contexte var_local seules les variables préexistantes
//  peuvent-être utilisées.
static var_t  *var_accept(char *name){
    var_t *var;
    
    var=var_search(name);
    if (!var && var_local) throw(eERR_BAD_ARG);
    next_token();
    //seules les variables vecteur préexistantes peuvent-être utilisées
    if (!var && (token.id==eLPAREN)) throw(eERR_BAD_ARG);
    unget_token=true;
    if (!var) var=var_create(name,NULL);
    if (var->vtype>=eVAR_INTARRAY && var->vtype<=eVAR_STRARRAY){
        code_array_address(var);
    }else if (var->vtype==eVAR_STR){
        lit((uint32_t)&var->adr);
    }else{
        lit((uint32_t)&var->n);
    }
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
            case eVAR_STRARRAY: // ( var_addr -- )
                bytecode(IDUP);
                bytecode(IFETCH);
                bytecode(ISTRFREE); 
                compile_input(var); // (var_addr -- var_addr pad_addr)
                bytecode(ISTRALLOC);// ( var_addr pad_addr n -- var_addr pad_addr str_addr)  
                bytecode(ISTRCPY); // ( var_addr pad_addr str_addr -- var_addr )
                bytecode(ISWAP);
                bytecode(ISTORE);
                break;
            case eVAR_LOCAL:
            case eVAR_INT:
            case eVAR_BYTE:
                compile_input(var);
                bytecode(IDROP);
                bytecode(I2INT);
                store_integer(var);
                break;
            case eVAR_INTARRAY:
            case eVAR_BYTEARRAY:
                compile_input(var);
                bytecode(IDROP);
                bytecode(I2INT);
                bytecode(ISWAP);
                bytecode(ICSTORE);
                break;
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

// LET varname=expression
//assigne une valeur à une variable
//si la variable n'existe pas elle est créée.
static void kw_let(){
    char name[32];
    var_t *var;
    int len;
    
    expect(eIDENT);
    strcpy(name,token.str);
    len=strlen(name);
    var=var_search(name);
    if (var && ((var->vtype==eVAR_CONST)||(var->vtype==eVAR_CONST_STR))) throw(eERR_REDEF);
    next_token();
    if (var_local){
        // on ne peut pas allouer de chaîne dans les sous-routines
        if (name[len-1]=='$') throw(eERR_SYNTAX);
         // pas d'auto création à l'intérieur des sous-routines
        if (!var) throw(eERR_BAD_ARG);
        if (token.id==eLPAREN && (var->vtype==eVAR_INTARRAY || var->vtype==eVAR_BYTEARRAY)){
            code_array_address(var);
            expect(eEQUAL);
            expression();
            bytecode(ISWAP);
            if (var->vtype==eVAR_INTARRAY){
                bytecode(ISTORE);
            }else{
                bytecode(ICSTORE);
            }
        }else if (token.id==eEQUAL){
            expression();
            store_integer(var);
        }else throw(eERR_SYNTAX);
    }else{
        if (token.id==eLPAREN){
            unget_token=true;
            array_let(name);
        }else{
            if (!var) var=var_create(name,NULL);
            unget_token=true;
            expect(eEQUAL);
            if (var->vtype==eVAR_STR){
                string_free(var->str);
                expect(eSTRING);
                var->str=string_alloc(strlen(token.str));
                strcpy(var->str,token.str);
            }else{
                expression();
                store_integer(var);
            }
        }//if
    }//if
}//f()

// LOCAL identifier {,identifier}
// création des variables locales
// à l'intérieur des SUB|FUNC
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

// PRINT|? chaine|identifier|expression {,chaine|identifier|expression}
static void kw_print(){
    var_t *var;

    next_token();
    while (!activ_reader->eof){
        switch (token.id){
            case eSTRING:
                bytecode(IPSTR);
                literal_string(token.str);
                _litc(1);
                bytecode(ISPACES);
                break;
            case eIDENT:
                if(token.str[strlen(token.str)-1]=='$'){
                    if ((var=var_search(token.str))){
                        if (var->vtype==eVAR_STRARRAY){
                            code_array_address(var);
                            bytecode(IFETCH);
                        }else{
                            lit((uint32_t)var->str);
                        }
                        bytecode(ITYPE);
                        _litc(1);
                        bytecode(ISPACES);
                    }
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
           // crlf(con);
            unget_token=true;
            break;
        }
        next_token();
    }//while
}//f()

// PUTC(expression)
// imprime un caractère ASCII
static void kw_putc(){
    expression();
    bytecode(IEMIT);
}//f

// KEY()
// retourne indicateur touche clavier
// ou zéro
static void kw_key(){ 
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IKEY);
}//f

// INVERTVID()
// inverse la sortie vidéo
// noir/blanc
static void kw_invert_video(){
    parse_arg_list(1);
    bytecode(IINV_VID);
}

// cré la liste des arguments pour
// la compilation des SUB|FUNC
static void create_arg_list(){
    var_t *var;
    int i=0;
    expect(eLPAREN);
    next_token();
    while (token.id==eIDENT){
        var=var_create(token.str,(char*)&i);
        i++;
        next_token();
        if (token.id!=eCOMMA) break;
        next_token();
    }
    if (token.id!=eRPAREN) throw(eERR_SYNTAX);
    bytecode(i); // enregistre le nombre d'argument avant le code de la fonction.
}//f

//cré une nouvelle variable de type eVAR_SUB|eVAR_FUNC
//ces variables contiennent un pointer vers le point d'entré
//du code.
static void subrtn_create(int var_type, int blockend){
    var_t *var;

    if (var_local) throw(eERR_SYNTAX);
    expect(eIDENT);
    if (token.str[strlen(token.str)-1]=='$') throw(eERR_SYNTAX);
    var=var_search(token.str);
    if (var) throw(eERR_REDEF);
    var=var_create(token.str,NULL);
    var->vtype=var_type;
    var->adr=(void*)&progspace[dptr];
    globals=varlist;
    var_local=true;
    cpush((uint32_t)endmark);
    complevel++;
    cpush(blockend);
    cpush((uint32_t)var);
    create_arg_list();
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

// TRACE(expression)
// active ou désactive le mode traçage
// pour le débogage
// expression -> 0|1 {off|on}
static void kw_trace(){
    parse_arg_list(1); 
    bytecode(ITRACE);
}//f

//retourne le nombre d'arguments inscris dans le
// code programme
// 1ier octet avant le code de la fonction.
static unsigned get_arg_count(void *fn_code){
    (uint8_t*)fn_code;
    return *(uint8_t*)fn_code;
}

static void compile(){
    var_t *var;
    uint32_t adr;
    
    do{
        next_token(); 
//#ifdef DEBUG
//        print_token_info();
//#endif        
        switch(token.id){ 
            case eKWORD:
                if ((KEYWORD[token.n].len&FUNCTION)==FUNCTION){
                    KEYWORD[token.n].cfn();
                    bytecode(IDROP); // jette la valeur de retour
                }else{
                    KEYWORD[token.n].cfn();
                }
                break;
            case eIDENT:
                if ((var=var_search(token.str))){
                    if ((var->vtype==eVAR_SUB)||(var->vtype==eVAR_FUNC)){
                        adr=(uint32_t)var->adr;
                        if (var->vtype==eVAR_SUB){_arg_frame(FRAME_SUB);}else{_arg_frame(FRAME_FUNC);} 
                        parse_arg_list(get_arg_count((void*)adr));
                        lit(adr+1);
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
    for (i=start;i<=dptr;i++){
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

int BASIC_shell(unsigned basic_heap, const char* file_name){
    clear_screen(con);
    FIL *fh;
    FRESULT result;
    int vm_exit_code;
    
    pad=malloc(PAD_SIZE);
    prog_size=(biggest_chunk()-basic_heap)&0xfffffff0;
    progspace=malloc(prog_size);
    sprintf((char*)progspace,"vpcBASIC v1.0\nRAM available: %d bytes\nstrings space: %d bytes\n",prog_size,basic_heap);
    print(con,progspace);
    clear();
//  initialisation lecteur source.
    if (file_name && !(result=f_open(fh,file_name,FA_READ))){
        reader_init(&file_reader,eDEV_SDCARD,fh);
    }else{
        reader_init(&std_reader,eDEV_KBD,NULL);
    }
    activ_reader=&std_reader; 
// boucle interpréteur    
    while (!exit_basic){
        if (!setjmp(failed)){
            activ_reader->eof=false;
            compile();
            if (fh){result=f_close(fh);}
            if (!complevel){
                if (program_loaded && run_it){
                    vm_exit_code=StackVM(progspace);
                    run_it=false;
                }else if (dptr>program_end){

#ifdef DEBUG
        print_prog(program_end);
#endif
                    bytecode(IBYE);
                    vm_exit_code=StackVM(&progspace[program_end]);
                    if (activ_reader->device==eDEV_KBD){
                        memset(&progspace[program_end],0,dptr-program_end);
                        dptr=program_end;
                        print_int(con,vm_exit_code,0);
                    }
                }
            }
        }else{
            clear();
            activ_reader=&std_reader;
            reader_init(activ_reader,eDEV_KBD,NULL);
        }
        
    }//while(1)
    if (fh){
        result=f_close(fh);
    }
    free_string_vars();
    free(progspace);
    free(pad);
    return vm_exit_code;
}//BASIC_shell()
