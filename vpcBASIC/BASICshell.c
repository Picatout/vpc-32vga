/*
 * PV16SOG retro-computer 
 * specifications:
 *  MCU: PIC24EP512MC202
 *  video out: 
 *     - NTSC signal
 *     - graphics 240x170 16 shades of gray
 *     - text 21 lines 40 colons.
 *  storage:
 *     - SDcard
 *     - temporary: SPI RAM 64Ko
 *  input:
 *     - PS/2 keyboard
 *     - Atari 2600 joystick
 * sound:
 *     - single tone or white noise.
 * software:
 *     - small command shell
 *     - virtual machine derived from SCHIP
 *     - editor
 *     - assembler for the virtual machine
 * 
 * Copyright 2015, 2016, Jacques Deschenes
 * 
 * This file is part of PV16SOG project.
 * 
 * ***  LICENCE ****
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; See 'copying.txt' in root directory of source.
 * If not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *  
 * to contact the author:  jd_temp@yahoo.fr
 * 
 */

/*
 * File:   shell.h
 * Author: Jacques Deschênes
 *
 * Created on 18 septembre 2013, 07:29
 * Description: un environnement de commande simple pour le VPC-32.
 *   liste des commandes:
 *      ls     liste des fichiers sur la carte SD
 *      rm     efface un fichier.
 *      mv     renomme un fichier.
 *      ed     ouvre l'éditeur
 *      as     assemble un fichier écris en assembleur pour la machine virtuelle.
 *      run    execute un programme compilé pour la machine virtuelle.
 *      cp     copie un fichier
 *      snd    envoie un fichier vers le port sériel
 *      rcv    reçois un fichier par le port sériel
 *      forth  lance l'environnement vpForth
 *      puts mot  imprime à l'écran le mot qui suis
 *      expr {expression}  évalue une expression et retourne le résultat
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>

#include "Hardware/hardware.h"
#include "Hardware/ps2_kbd.h"
#include "Hardware/spi_ram.h"
#include "text.h"
#include "filesys.h"
#include "graphics.h"
#include "font6x8.h"
#include "reader.h"
#include "vm.h"


/* macros utiléses par le compilateur BASIC*/

#define lit(n) bytecode(LIT);\
               bytecode(low(n));\
               bytecode(high(n))

#define litc(c)  bytecode(LITC);\
                 bytecode((c)&255)


#define prt_varname(v) lit((uint16_t)v->name);\
                      bytecode(TYPE)

static jmp_buf failed; // utilisé par setjmp() et lngjmp()

static reader_t std_reader; // source clavier
static reader_t file_reader; // source fichier
static reader_t *activ_reader=NULL; // source active
//static void *endmark;
static uint16_t prog_size;
static uint16_t line_count;
static bool program_loaded;
static bool run_it=false;
static uint16_t program_end;

#define NAME_MAX 32 //longueur maximale nom
#define LEN_MASK 0x1F   // masque longueur nom.
#define HIDDEN 0x20 // mot caché
#define FUNCTION 0x40 // ce mot est une fonction
#define AS_HELP 0x80 //commande avec aide
#define ADR  (void *)

//entrée de commande dans liste
typedef struct{
    fnptr cfn; // pointeur fonction commande
    uint8_t len; // longueur nom commande, flags IMMED, HIDDEN
    __eds__ char *name; // nom de la commande
} dict_entry_t;



typedef enum {eVAR_INT,eVAR_STR,eVAR_INTARRAY,eVAR_BYTEARRAY,eVAR_STRARRAY,
              eVAR_SUB,eVAR_FUNC,eVAR_LOCAL,eVAR_CONST,eVAR_BYTE}var_type_e;

typedef struct _var{
    struct _var* next;
    uint8_t len;
    uint8_t vtype;
    char *name;
    union{
        uint8_t byte;
        int16_t n;
        int16_t *nbr;
        char *str;
        void *adr;
    };
}var_t;

// type d'unité lexicales
typedef enum {eNONE,eSTOP,eCOLON,eIDENT,eNUMBER,eSTRING,ePLUS,eMINUS,eMUL,eDIV,
              eMOD,eCOMMA,eLPAREN,eRPAREN,eSEMICOL,eEQUAL,eNOTEQUAL,eGT,eGE,eLT,eLE,
              eEND, eELSE,eCMD,eKWORD,eCHAR} tok_id_t;


#define MAX_LINE_LEN 128

typedef struct _token{
    tok_id_t id;
    union{
        char str[MAX_LINE_LEN];
        int n;
    };
}token_t;



static token_t token;
static bool unget_token=false;

static void cmd_clear();
static void cmd_copy();
static void cmd_del();
void cmd_dir();
static void cmd_editor();
static void cmd_free_pool();
static void cmd_hexdump();
static void cmd_help();
static void cmd_more();
static void cmd_reboot();
static void cmd_ren();
static void cmd_run();

// commandes du shell
__eds__ static const dict_entry_t __attribute__((space(prog))) COMMAND[]={
    {cmd_clear,5+AS_HELP,"CLEAR"},
    {cmd_copy,4+AS_HELP,"COPY"},
    {cmd_del,3+AS_HELP,"DEL"},
    {cmd_dir,3+AS_HELP,"DIR"},
    {cmd_editor,4+AS_HELP,"EDIT"},
    {cmd_free_pool,4+AS_HELP,"FREE"},
    {cmd_hexdump,5+AS_HELP,"HDUMP"},
    {cmd_help,4+AS_HELP,"HELP"},
    {cmd_more,4+AS_HELP,"MORE"},
    {video_adjust,6+AS_HELP,"RASTER"},
    {cmd_reboot,6+AS_HELP,"REBOOT"},
    {cmd_ren,3+AS_HELP,"REN"},
    {cmd_run,3+AS_HELP,"RUN"},
    {NULL,0,""}
};
static void kw_abs();
static void bad_syntax();
static void kw_beep();
static void kw_box();
static void kw_btest();
static void kw_bye();
static void kw_case();
static void kw_circle();
static void kw_cls();
static void kw_color();
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
static void kw_key();
static void kw_len();
static void kw_let();
static void kw_line();
static void kw_local();
static void kw_locate();
static void kw_loop();
static void kw_max();
static void kw_mdiv();
static void kw_min();
static void kw_next();
static void kw_noise();
static void kw_pause();
static void kw_print();
static void kw_putc();
static void kw_randomize();
static void kw_read_jstick();
static void kw_rect();
static void kw_ref();
static void kw_rem();
static void kw_remove_sprite();
static void kw_restore_screen();
static void kw_return();
static void kw_rnd();
static void kw_save_screen();
static void kw_scrlup();
static void kw_scrldown();
static void kw_scrlright();
static void kw_scrlleft();
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
static void kw_trace();
static void kw_ubound();
static void kw_use();
static void kw_video();
static void kw_waitkey();
static void kw_wend();
static void kw_while();
static void kw_xorpixel();

//identifiant KEYWORD doit-être dans le même ordre que
//dans la liste KEYWORD
enum {eKW_ABS,eKW_AND,eKW_BEEP,eKW_BOX,eKW_BTEST,eKW_BYE,eKW_CASE,eKW_CIRCLE,eKW_CLS,eKW_COLOR,
      eKW_CONST,eKW_CURCOL,eKW_CURLINE,eKW_DIM,eKW_DO,eKW_ELLIPSE,eKW_ELSE,eKW_END,eKW_EXIT,
      eKW_FOR,eKW_FUNC,eKW_GETPIXEL,eKW_IF,eKW_INPUT,eKW_KEY,eKW_LEN,
      eKW_LET,eKW_LINE,eKW_LOCAL,eKW_LOCATE,eKW_LOOP,eKW_MAX,eKW_MDIV,eKW_MIN,eKW_NEXT,
      eKW_NOISE,eKW_NOT,eKW_OR,eKW_PAUSE,
      eKW_PRINT,eKW_PUTC,eKW_RANDOMISIZE,eKW_JSTICK,eKW_RECT,eKW_REF,eKW_REM,eKW_REMSPR,eKW_RESTSCR,
      eKW_RETURN,eKW_RND,eKW_SAVESCR,eKW_SCRLUP,eKW_SCRLDN,
      eKW_SCRLRT,eKW_SCRLFT,eKW_SELECT,eKW_SETPIXEL,eKW_SETTMR,eKW_SHL,eKW_SHR,
      eKW_SPRITE,eKW_SRCLEAR,eKW_SRLOAD,eKW_SRREAD,eKW_SRSSAVE,eKW_SRWRITE,eKW_SUB,eKW_THEN,eKW_TICKS,
      eKW_TIMEOUT,eKW_TONE,eKW_TRACE,eKW_UBOUND,eKW_UNTIL,eKW_USE,eKW_VIDEO,
      eKW_WAITKEY,eKW_WEND,eKW_WHILE,eKW_XORPIXEL
};

//mots réservés BASIC
__eds__ static const dict_entry_t __attribute__((space(prog))) KEYWORD[]={
    {kw_abs,3+FUNCTION,"ABS"},
    {bad_syntax,3,"AND"},
    {kw_beep,4,"BEEP"},
    {kw_box,3,"BOX"},
    {kw_btest,5+FUNCTION,"BTEST"},
    {kw_bye,3,"BYE"},
    {kw_case,4,"CASE"},
    {kw_circle,6,"CIRCLE"},
    {kw_cls,3+AS_HELP,"CLS"},
    {kw_color,5+AS_HELP,"COLOR"},
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
    {kw_key,3+FUNCTION,"KEY"},
    {kw_len,3+FUNCTION,"LEN"},
    {kw_let,3,"LET"},
    {kw_line,4,"LINE"},
    {kw_local,5,"LOCAL"},
    {kw_locate,6,"LOCATE"},
    {kw_loop,4,"LOOP"},
    {kw_max,3+FUNCTION,"MAX"},
    {kw_mdiv,4+FUNCTION,"MDIV"},
    {kw_min,3+FUNCTION,"MIN"},
    {kw_next,4,"NEXT"},
    {kw_noise,5,"NOISE"},
    {bad_syntax,3,"NOT"},
    {bad_syntax,2,"OR"},
    {kw_pause,5,"PAUSE"},
    {kw_print,5,"PRINT"},
    {kw_putc,4,"PUTC"},
    {kw_randomize,9,"RANDOMIZE"},
    {kw_read_jstick,6+FUNCTION,"JSTICK"},
    {kw_rect,4,"RECT"},
    {kw_ref,1+FUNCTION,"@"},
    {kw_rem,3,"REM"},
    {kw_remove_sprite,6,"REMSPR"},
    {kw_restore_screen,7,"RESTSCR"},
    {kw_return,6,"RETURN"},
    {kw_rnd,3+FUNCTION,"RND"},
    {kw_save_screen,7,"SAVESCR"},
    {kw_scrlup,6,"SCRLUP"},
    {kw_scrldown,6,"SCRLDN"},
    {kw_scrlright,6,"SCRLRT"},
    {kw_scrlleft,6,"SCRLLT"},
    {kw_select,6,"SELECT"},
    {kw_setpixel,8,"SETPIXEL"},
    {kw_set_timer,6,"SETTMR"},
    {kw_shl,3+FUNCTION,"SHL"},
    {kw_shr,3+FUNCTION,"SHR"},
    {kw_sprite,6,"SPRITE"},
    {kw_srclear,7,"SRCLEAR"},
    {kw_srload,6+FUNCTION,"SRLOAD"},
    {kw_srread,6,"SRREAD"},
    {kw_srsave,6,"SRSAVE"},
    {kw_srwrite,7,"SRWRITE"},
    {kw_sub,3,"SUB"},
    {kw_then,4,"THEN"},
    {kw_ticks,5+FUNCTION,"TICKS"},
    {kw_timeout,7+FUNCTION,"TIMEOUT"},
    {kw_tone,4,"TONE"},
    {kw_trace,5,"TRACE"},
    {kw_ubound,6+FUNCTION,"UBOUND"},
    {bad_syntax,5,"UNTIL"},
    {kw_use,3,"USE"},
    {kw_video,5,"VIDEO"},
    {kw_waitkey,7+FUNCTION,"WAITKEY"},
    {kw_wend,4,"WEND"},
    {kw_while,5,"WHILE"},
    {kw_xorpixel,8,"XORPIXEL"},
    {NULL,0,""}
};


//recherche la commande dans
//le dictionnaire système
// ( n1 -- n2)
static int dict_search(const __eds__ dict_entry_t *dict){
    int i=0, len;
    
    len=strlen(token.str);
    while (dict[i].len){
        if (((dict[i].len&LEN_MASK)==len) && !pstrcmp((const unsigned char*)token.str,
                (const __eds__ unsigned char*)dict[i].name)){
            break;
        }
        i++;
    }
    return dict[i].len?i:-1;
}//cmd_search()


//code d'erreurs
//NOTE: eERR_DSTACK et eERR_RSTACK sont redéfinie dans stackvm.h
//      si leur valeur change elles doivent aussi l'être dans stackvm.h
enum {eERROR_NONE,eERR_DSTACK=1,eERR_RSTACK=2,eERR_MISSING_ARG,eERR_EXTRA_ARG,
      eERR_BAD_ARG,eERR_SYNTAX,eERR_ALLOC,eERR_REDEF,eERR_ASSIGN,
      eERR_NOTDIM,eERR_CMDONLY,eERR_BOUND,eERR_PROGSPACE
      };

 __eds__ static  PSTR error_msg[]={
    "no error\n",
    "data stack out of bound\n",
    "control stack out of bound\n",
    "missing argument\n",
    "too much arguments\n",
    "bad argument\n",
    "syntax error\n",
    "memory allocation error\n",
    "can't be redefined\n",
    "can't be reassigned\n",
    "undefined array variable\n",
    "command line only directive\n",
    "array index out of range\n",
    "program space full\n",
 };
 
 
void throw(int error){
    char message[64];
    
    fat_close_all_files();
    new_line();
    if (activ_reader->device==eDEV_SDCARD){
        print("line: ");
        print_int(line_count,0);
        new_line();
    }
    pstrcpy(message,error_msg[error]);
    print(message);
    print("\ntok_id: ");
    print_int(token.id,0);
    print("\ntok_value: ");
    switch(token.id){
        case eCMD:
            pstrcpy(message,(const __eds__ char*)COMMAND[token.n].name);
            print(message);
            break;
        case eKWORD:
            pstrcpy(message,(const __eds__ char*)KEYWORD[token.n].name);
            print(message);
            break;
        case eNUMBER:
            print_int(token.n,0);
            break;
        case eSTOP:
            print("unspected end of command.");
            break;
        case eSTRING:
        case eIDENT:
            print(token.str);
            break;
        default:
            if (token.str[0]>=32 && token.str[0]<=127)
                print(token.str);
    }//switch
    new_line();
    activ_reader->eof=true;
    longjmp(failed,error);
}


var_t *varlist=NULL;
var_t *globals=NULL;
bool var_local=false;


// déclenche une exeption en cas d'échec.
static void *alloc_var_space(int size){
    void *newmark;
    
    if (size&1) size++;//alignement
    newmark=endmark-size;
    if (newmark<=(void*)&progspace[dp]){
        throw(eERR_ALLOC);
    }
    endmark=newmark;
    return endmark;
}//f

// les variables sont allouées à la fin
// de progspace en allant vers le bas.
// lorsque endmark<=&progrspace[dp] la mémoire est pleine
var_t *var_create(char *name, char *value){
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
        newvar->byte=*((uint8_t*)value);
    }else{// entier 16 bits
        if (var_local){ 
            newvar->vtype=eVAR_LOCAL;
        }else{ 
            newvar->vtype=eVAR_INT;
        }
        newvar->n=*((int*)value);
    }
    endmark=newend;
    newvar->len=len;
    newvar->name=varname;
    newvar->next=varlist;
    varlist=newvar;
    return varlist;
}//f()

var_t *var_search(const char* name){
    var_t* link;
    
    link=varlist;
    while (link){
        if (!strcmp(link->name,name)) break;
        link=link->next;
    }//while
    return link;
}//f()

static void bytecode(uint8_t bc){
    if ((void*)&progspace[dp]>=endmark) throw(eERR_PROGSPACE);
    progspace[dp++]=bc;
}//f


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
    if (i>32) i=32;
    token.str[i]=0;                  
    if ((i=dict_search(COMMAND))>-1){
        token.id=eCMD;
        token.n=i;
    }else if ((i=dict_search(KEYWORD))>-1){
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
    token.str[0]=0;
    if (activ_reader->eof){
        return;
    }
    skip_space();
    c=reader_getc(activ_reader);
    if (c==-1){
        token.id=eNONE;
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
                break;
            case '\\':
                token.id=eCHAR;
                token.n=reader_getc(activ_reader);
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
                break;
            case '@': // référence
                token.id=eKWORD;
                token.n=eKW_REF;
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

//static bool try_string(){
//    next_token();
//    if (token.id==eSTRING) return true;
//    unget_token=true;
//    return false;
//}//f

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

typedef enum {eNO_FILTER,eACCEPT_ALL,eACCEPT_END_WITH,eACCEPT_ANY_POS,
        eACCEPT_SAME,eACCEPT_START_WITH} filter_enum;

typedef struct{
    char *subs; // sous-chaïne filtre.
    filter_enum criteria; // critère
}filter_t;

static void parse_filter(){
    char c;
    int i=0;
    skip_space();
    while (!activ_reader->eof && 
            (isalnum((c=reader_getc(activ_reader))) || c=='*' || c=='_' || c=='.')){
        token.str[i++]=toupper(c);
    }
    reader_ungetc(activ_reader);
    token.str[i]=0;
    if (!i) token.id=eNONE; else token.id=eSTRING;
}//f

static void set_filter(filter_t *filter){
    filter->criteria=eACCEPT_ALL;
    parse_filter();
    if (token.id==eNONE){
        filter->criteria=eNO_FILTER;
        return;
    }
    if (token.str[0]=='*' && token.str[1]==0){
        return;
    }
    if (token.str[0]=='*'){
        filter->criteria++;
    }else{
        filter->criteria=eACCEPT_SAME;
    }
    if (token.str[strlen(token.str)-1]=='*'){
        token.str[strlen(token.str)-1]=0;
        filter->criteria++;
    }
    switch(filter->criteria){
        case eACCEPT_START_WITH:
        case eACCEPT_SAME:
            strcpy(filter->subs,token.str);
            break;
        case eACCEPT_END_WITH:
        case eACCEPT_ANY_POS:
            strcpy(filter->subs,&token.str[1]);
            break;
        case eNO_FILTER:
        case eACCEPT_ALL:
            break;
    }//switch
}//f()


static bool filter_accept(filter_t *filter, const char* text){
    bool result=false;
    char temp[32];
    
    strcpy(temp,text);
    uppercase(temp);
    switch (filter->criteria){
        case eACCEPT_SAME:
            if (!strcmp(filter->subs,temp)) result=true;
            break;
        case eACCEPT_START_WITH:
            if (strstr(temp,filter->subs)==temp) result=true;
            break;
        case eACCEPT_END_WITH:
            if (strstr(temp,filter->subs)==temp+strlen(temp)-strlen(filter->subs)) result=true;
            break;
        case eACCEPT_ANY_POS:
            if (strstr(temp,filter->subs)) result=true;
            break;
        case eACCEPT_ALL:
        case eNO_FILTER:
            result=true;
            break;
    }
    return result;
}//f()

//static void checkbound(var_t* var, int index);
static void expression();

// compile le calcul d'indice dans les variables vecteur
static void code_array_address(var_t *var){
    expression();
    expect(eRPAREN);
    if (var->vtype==eVAR_INTARRAY || var->vtype==eVAR_STRARRAY){
        bytecode(SHL);//2*index
    }else{
        // élément 0 et 1  réservé pour size
        // index 1 correspond à array[2]
        litc(1);
        bytecode(ADD);
    }
    //index dans le tableau
    lit((uint16_t)var->adr);
    bytecode(ADD);
}//f


static void parse_arg_list(int arg_count){
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
    int i;
    
    next_token();
    switch(token.id){
        case eKWORD:
            if (KEYWORD[token.n].len&FUNCTION){
                KEYWORD[token.n].cfn();
            }else  throw(eERR_SYNTAX);
            break;
        case eIDENT:
            if ((var=var_search(token.str))){
                switch(var->vtype){
                    case eVAR_FUNC:
                        litc(0);
                        parse_arg_list(*((uint8_t*)(var->adr+1)));
                        bytecode(CALL);
                        bytecode(low((uint16_t)&var->adr));
                        bytecode(high((uint16_t)&var->adr));
                        break;
                    case eVAR_LOCAL:
                        bytecode(LCFETCH);
                        bytecode(var->n&(DSTACK_SIZE-1));
                        break;
                    case eVAR_BYTE:
                        lit((uint16_t)&var->n);
                        bytecode(FETCHC);
                        break;
                    case eVAR_CONST:
                        lit(var->n);
                        break;
                    case eVAR_INT:
                        lit((uint16_t)&var->n);
                        bytecode(FETCH);
                        break;
                    case eVAR_BYTEARRAY:
                        expect(eLPAREN);
                        code_array_address(var);
//                        expression();
//                        expect(eRPAREN);
//                        lit((uint16_t)var->adr);
//                        bytecode(ADD);
                        bytecode(FETCHC);
                        break;
                    case eVAR_INTARRAY:
                        expect(eLPAREN);
                        code_array_address(var);
//                        expression();
//                        expect(eRPAREN);
//                        bytecode(SHL);
//                        lit((uint16_t)var->adr);
//                        bytecode(ADD);
                        bytecode(FETCH);
                        break;
                    default:
                        throw(eERR_SYNTAX);
                }//switch
            }else if (!var){
                if (var_local){
                    i=varlist->n+1;
                    var=var_create(token.str,(char*)&i);
                    litc(0);
                    bytecode(LCSTORE);
                    bytecode(i);
                }else{
                    var=var_create(token.str,NULL);
                    next_token();
                    if (token.id==eLPAREN) throw(eERR_SYNTAX);
                    unget_token=true;
                    lit((uint16_t)&var->n);
                    bytecode(FETCH);
                }
            }else{
                throw(eERR_SYNTAX);
            }
            break;
        case eCHAR:
            litc(token.n&127);
            break;
        case eNUMBER:
            if (token.n>=0 && token.n<256){
                litc((uint8_t)token.n);
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
}//f()

static void term(){
    int op;
    
    factor();
    while (try_mulop()){
        op=token.id;
        factor();
        switch(op){
            case eMUL:
                bytecode(MUL);
                break;
            case eDIV:
                bytecode(DIV);
                break;
            case eMOD:
                bytecode(MOD);
                break;
        }//switch
    }
}//f 

static void expression(){
    int mark_dp=dp;
    int op=eNONE;
    if (try_addop()){
        bytecode(LITC);
        bytecode(0);
        op=token.id;
    }
    term();
    switch(op){
        case ePLUS:
            bytecode(ADD);
            break;
        case eMINUS:
            bytecode(SUB);
    }    
    while (try_addop()){
        op=token.id;
        term();
        if (op==ePLUS){
            bytecode(ADD);
        }else{
            bytecode(SUB);
        }
    }
    if (dp==mark_dp) throw(eERR_SYNTAX);
}//f()

static void condition(){
    int rel;
    //print("condition\n");
    expression();
    if (!try_relation()){
        litc(0);
        bytecode(EQUAL);
        bytecode(NOT);
        return;
    }
    rel=token.id;
    expression();
    switch(rel){
        case eEQUAL:
            bytecode(EQUAL);
            break;
        case eNOTEQUAL:
            bytecode(NOTEQUAL);
            break;
        case eGT:
            bytecode(GT);
            break;
        case eGE:
            bytecode(GE);
            break;
        case eLT:
            bytecode(LT);
            break;
        case eLE:
            bytecode(LE);
            break;
    }//switch
}//f

static void bool_expression();

static void bool_factor(){
    int boolop=0;
    //print("bool_factor\n");
    if (try_boolop()){
        if (token.n!=eKW_NOT) throw(eERR_SYNTAX);
        boolop=eKW_NOT;
    }
    condition();
    if (boolop){
        bytecode(NOT);
    }
}//f

static void fix_branch_address();

static void bool_term(){// print("bool_term()\n");
    int fix_count=0; // pour évaluation court-circuitée.
    bool_factor();
    while (try_boolop() && token.n==eKW_AND){
        //code pour évaluation court-circuitée
        bytecode(DUP);
        bytecode(ZBRANCH); // si premier facteur==faux saut court-circuit.
        rstack[++rsp]=dp;
        dp+=2;
        fix_count++;
        bool_factor();
        bytecode(AND);
    }
    unget_token=true;
    while (fix_count){
        fix_branch_address();
        fix_count--;
    }
}//f

static void bool_expression(){// print("bool_expression()\n");
    int fix_count=0; // pour évaluation court-circuitée.
    bool_term(); //print("first bool_term done\n");
    while (try_boolop() && token.n==eKW_OR){
        bytecode(DUP);
        bytecode(NZBRANCH); // is premier facteur==vrai saut court-circuit.
        rstack[++rsp]=dp;
        dp+=2;
        fix_count++;
        bool_term();
        bytecode(OR);
    }
    unget_token=true;
    while (fix_count){
        fix_branch_address();
        fix_count--;
    }
}//f

static void bad_syntax(){
    throw(eERR_SYNTAX);
}//f

/**************************
 *  commande du SHELL
 **************************/

#define CMD_COUNT (13)
#define KW_BASIC CMD_COUNT
#define UNKNOWN_WORD CMD_COUNT+1
__eds__ PSTR HELP_MSG[]={
    "CLEAR, clear program memory.\n",
    "COPY file new_file\nCopy 'file' to 'new_file'\n",
    "DEL file\nDelete 'file', may use '*' filter\n",
    "DIR [filter]\nList file in root directory, may use '*' filter\n",
    "EDIT [file name]\nLaunch text editor.\n",
    "FREE\nDisplay free bytes in program space.\n",
    "HDUMP file\nDisplay content of 'file' in hexadecimal\n",
    "HELP [command]\nDisplay list of commands\nWith argument display command help.\n",
    "MORE file\nList content of text 'file' to screen\n",
    "RASTER\nUtility to center display on TV screen\n",
    "REBOOT\nCold restart of computer.\n",
    "REN file_name new_name\nRename 'file_name' to 'new_file\n",
    "RUN program.bas\nExecute a basic program stored on SD card.\n",
    "For help on BASIC read user's manual.\n",
    "Not a command or BASIC keyword\n"
};

//affiche aide de commande
static void usage(int cmd_id){
    char help_text[128];

    pstrcpy(help_text, HELP_MSG[cmd_id]);
    print(help_text);
}//f()


//help
// ( -- )
static void cmd_help(){
    int j=0;
    uint8_t col;
    char name[32];
    
    next_token();
    if (token.id==eSTOP){
        while(COMMAND[j].len){
            if ((COMMAND[j].len&AS_HELP)==0){j++;continue;}
            col=text_colon();
            if (col>(CHAR_PER_LINE-(COMMAND[j].len & LEN_MASK)-2)){
                put_char('\n');
            }
            pstrcpy(name,COMMAND[j].name);
            print(name);
            if (COMMAND[j].len){
                put_char(' ');
            }
            j++;
        }//while
    }else if (token.id==eCMD){
        usage(token.n);
    }else if (token.id==eKWORD){
        usage(KW_BASIC);
    }else{
        usage(UNKNOWN_WORD);
    }
}//f

static void cmd_del(){ // efface un fichier
    struct fat_dir_entry_struct dir_entry;
    int count=0;
    filter_t filter;
    char target[32];
    char key;
    int err;
    
    filter.subs=target;
    set_filter(&filter);
    if (filter.criteria==eNO_FILTER) throw(eERR_MISSING_ARG);
    fs_reset_dir();
    while (fs_read_dir(&dir_entry)==eERR_NONE){
        if (!(dir_entry.attributes&(FAT_ATTRIB_DIR+FAT_ATTRIB_VOLUME+FAT_ATTRIB_SYSTEM)) &&     
            filter_accept(&filter,dir_entry.long_name)){
            print("delete ");
            print(dir_entry.long_name);
            key=toupper(prompt(" (y/n)","yn"));
            if (key==ESC) break;
            if (key=='Y'){
                fs_delete_file(dir_entry.long_name);
                if ((err=fs_last_error())==eERR_NONE){
                    count++;
                    new_line();
                }else{
                    error(err);
                    break;
                }
            }
        }//if
    }//while
    print_int(count,0);
    print(" files deleted.\n");
    expect_end();
}//f

static void cmd_ren(){ // renomme un fichier
    char name[32];
    
    parse_filter();
    if (token.id==eNONE)throw(eERR_MISSING_ARG);
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    strcpy(name,token.str);
    parse_filter();
    if (token.id==eNONE)throw(eERR_MISSING_ARG);
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    fs_rename_file(name,token.str);
    expect_end();
}//cmd_ren

static void cmd_copy(){ // copie un fichier
    struct fat_file_struct *src, *dest;
    uint16_t count;
    uint8_t buffer[64];
    char src_name[32];
    
    parse_filter();
    if (token.id==eNONE) throw(eERR_MISSING_ARG);
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    strcpy(src_name,token.str);
    parse_filter();
    if (token.id==eNONE) throw(eERR_MISSING_ARG);
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    if ((src=fs_open_file(src_name))&&
        (fs_create_file(token.str)==eERR_NONE)&&
        (dest=fs_open_file(token.str))){
        while (1){
            count=fs_read_file(src,buffer,64);
            if (count)
                fs_write_file(dest,buffer,count);
            else
                break;
        }
        fs_close_file(src);
        fs_close_file(dest);
    }else{
        error(fs_last_error());
    }
    expect_end();
}//copy()

//dir
void cmd_dir(){
    struct fat_dir_entry_struct dir_entry;
    int fcount=0,count=0;
    char target[32];
    filter_t filter={target,eACCEPT_ALL};
   
    set_filter(&filter);
    if (filter.criteria==eNO_FILTER) filter.criteria=eACCEPT_ALL;
    fs_reset_dir();
    while (fs_read_dir(&dir_entry)==eERR_NONE){
        if (!(dir_entry.attributes&(FAT_ATTRIB_DIR+FAT_ATTRIB_VOLUME+FAT_ATTRIB_SYSTEM)) &&     
            filter_accept(&filter,dir_entry.long_name)){
                print(dir_entry.long_name);
                if (text_colon()<20) set_cursor(20,text_line());
                print_int(dir_entry.file_size,8);
                count++;
                new_line();
                if (count==(LINE_PER_SCREEN-1)){
                    if (prompt("more...","*")==ESC) break;
                    fcount=count;
                    count=0;
                    set_cursor(0,text_line()-1);
                }
        }//if
    }//while
    fcount+=count;
    print("\nFiles found: ");
    print_int(fcount,0);
}//cmd_dir()

// affiche à l'écran le contenu d'un fichier texte
static void cmd_more(){ 
    struct fat_file_struct *fh;
    uint16_t count, j;
    uint8_t buffer[CHAR_PER_LINE], line=0;
    bool loop=true;
    
    new_line();
    parse_filter();
    if (token.id==eNONE) throw(eERR_MISSING_ARG);
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    if ((fh=fs_open_file(token.str))){
        cls();
        while (loop && (count=fs_read_file(fh,buffer,CHAR_PER_LINE))>0){
            for (j=0;j<count;j++){
                switch(buffer[j]){
                    case '\r':
                        break;
                    case '\n':
                        put_char('\n');
                        break;
                    case 9:
                        put_char(' ');
                        while (text_colon()%4){
                            if (!text_colon()){
                                line++;
                                break;
                            }
                            put_char(' ');
                        }
                        break;
                    default:
                        if (buffer[j]>=' ' && buffer[j]<(32+FONT_SIZE)){
                            put_char(buffer[j]);
                        }else put_char(' ');

                }//switch
                if (text_colon()==0) line++;
                if (line==(LINE_PER_SCREEN-2)){
                    set_cursor(0,LINE_PER_SCREEN-1);
                    if ((prompt("more...","*")==ESC)){loop=false; break;}
                    line=0;
                    text_clear_line(LINE_PER_SCREEN-1);
                    set_cursor(0,LINE_PER_SCREEN-2);
                }
            }//for
        }//while
        fs_close_file(fh);
    }else{
        error(fs_last_error());
    }
    expect_end();
}//more

//affiche le contenu d'un fichier
//en hexadecimal
void cmd_hexdump(){
    struct fat_file_struct *fh;
    uint16_t count, j,k=0, addr=0;
    uint8_t buffer[64], line=0;
    bool loop=true;
    char ascii[9];
    
    new_line();
    parse_filter();
    if (token.id==eNONE) throw(eERR_MISSING_ARG);
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    if ((fh=fs_open_file(token.str))){
        ascii[0]=0;
        ascii[8]=0;
        while (loop){
            count=fs_read_file(fh,buffer,64);
            if (!count){
                ascii[k]=0;
                while (text_colon()<32) put_char(' ');
                print(ascii);
                break;
            }
            for (j=0;j<count;j++){
                if (!(j%8)){
                    put_char(' ');
                    print(ascii);
                    new_line();
                    line++;
                    if (line==(LINE_PER_SCREEN-1)){
                        if ((prompt("more...","*")==ESC)){
                            loop=false;
                            break;
                        }
                        line=0;
                    }
                    print_hex(addr,4);
                    put_char(' ');
                    addr+=8;
                    k=0;
                }
                if ((buffer[j]>=32) && (buffer[j]<128)){
                    ascii[k++]=buffer[j];
                }else{
                    ascii[k++]=' ';
                }
                print_hex(buffer[j],2);
            }//for
        }//while
        fs_close_file(fh);
    }else{
        error(fs_last_error());
    }
    expect_end();
}//cmd_hexdump

extern void editor(const char*);
static void cmd_editor(){ // lance l'éditeur de texte
    parse_filter();
    if (token.id==eNONE){
        editor(NULL);
    }else if (token.id==eSTRING){
        if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
        editor(token.str);
    }
    expect_end();
}//editor()

//affiche l'espace pool disponible
static void cmd_free_pool(){
    new_line();
    print("RAM free bytes: ");
    print_int(endmark-(void*)&progspace[dp],0);
}

//efface le contenu de progspace
static void cmd_clear(){
    rsp=-1;
    dp=0;
    varlist=NULL;
    globals=NULL;
    var_local=false;
    complevel=0;
    endmark=(void*)progspace+prog_size;
    memset((void*)progspace,0,prog_size);
    line_count=0;
    program_loaded=false;
    run_it=false;
    program_end=0;
}//f


static void cmd_reboot(){
    fat_close_all_files();
    asm("reset");
}//f()

static void compile();

__eds__ static  PSTR compile_msg[]={
    "compiling ",
    "completed ",
    "file open error "
};

#define COMPILING 0
#define COMP_END 1
#define COMP_FILE_ERROR 2

static void compiler_msg(int msg_id, char *detail){
    char msg[CHAR_PER_LINE];
    pstrcpy(msg,compile_msg[msg_id]);
    print(msg);
    print(detail);
    new_line();
}//f

//charge compile et exécute
// un fichier basic
static void cmd_run(){
    struct fat_file_struct *fh;
    
    parse_filter();
    if (token.id==eNONE){
        if (!program_loaded)
            throw(eERR_MISSING_ARG);
        else
            run_it=true;
        return;
    }
    if (strchr(token.str,'*')) throw(eERR_BAD_ARG);
    if ((fh=fs_open_file(token.str))){
        reader_init(&file_reader,eDEV_SDCARD,fh);
        activ_reader=&file_reader;
        cmd_clear();
        compiler_msg(COMPILING,token.str);
        line_count=1;
        compile();
        fs_close_file(fh);
        activ_reader=&std_reader;
        program_loaded=true;
        program_end=dp;
        run_it=true;
        compiler_msg(COMP_END,NULL);
    }else{
        compiler_msg(COMP_FILE_ERROR,token.str);
    }
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
    if (var->vtype==eVAR_BYTEARRAY) count=2;
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
    if (((var->vtype==eVAR_BYTEARRAY) && (count>(size+2))) ||
        ((var->vtype!=eVAR_BYTEARRAY) && (count>(size+1)))) throw(eERR_EXTRA_ARG);
}//f

static void init_str_array(var_t *var){
    uint16_t *array,size;
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
                newstr=alloc_var_space(strlen(token.str)+1);
                strcpy(newstr,token.str);
                array[count++]=(uint16_t)newstr;
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
        array=alloc_var_space(size+2);
        memset(array,0,size+2);
        *((uint16_t*)array)=size;
    }else{ // table d'entiers ou de chaînes
        array=alloc_var_space(sizeof(int)*(size+1));
        memset(array,0,sizeof(int)*(size+1));
        *((uint16_t*)array)=size;
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
                    lit((uint16_t)&new_var->n);
                    bytecode(STORE);
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
    bytecode(LIT);
    switch(var->vtype){
    case eVAR_INT:
    case eVAR_CONST:
        bytecode(low((uint16_t)&var->n));
        bytecode(high((uint16_t)&var->n));
        break;
    case eVAR_INTARRAY:
    case eVAR_STRARRAY:
    case eVAR_BYTEARRAY:
        bytecode(low(((uint16_t)var->adr)+2));
        bytecode(high(((uint16_t)var->adr)+2));
        break;
    case eVAR_FUNC:
    case eVAR_SUB:
    case eVAR_STR:
        bytecode(low((uint16_t)var->adr));
        bytecode(high((uint16_t)var->adr));
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
    lit((uint16_t)var->adr);
    bytecode(UBOUND);
}//f

static void kw_use(){
    reader_t *old_reader, freader;
    struct fat_file_struct *fh;
    uint16_t lcount;
    
    if (activ_reader->device==STDIN) throw(eERR_SYNTAX);
    expect(eSTRING);
    uppercase(token.str);
    if ((fh=fs_open_file(token.str))){
        reader_init(&freader,eDEV_SDCARD,fh);
        old_reader=activ_reader;
        activ_reader=&freader;
        compiler_msg(COMPILING, token.str);
        lcount=line_count;
        line_count=1;
        compile();
        line_count=lcount+1;
        fs_close_file(fh);
        activ_reader=old_reader;
        compiler_msg(COMP_END,NULL);
    }else{
        compiler_msg(COMP_FILE_ERROR,NULL);
        throw(eERR_BAD_ARG);
    }
}//f

//VIDEO(0|1)
// déactive|active la sortie vidéo
static void kw_video(){
    parse_arg_list(1);
    bytecode(VIDEOCTRL);
}//f

//CURLINE()
// retourne la position ligne du curseur texte
static void kw_curline(){
    parse_arg_list(0);
    bytecode(CURLINE);
}//f

//CURCOL()
// retourne la position colonne du curseur texte
static void kw_curcol(){
    parse_arg_list(0);
    bytecode(CURCOL);
}//f

// EXEC @identifier
//static void kw_exec(){
//    expression();
//    bytecode(EXEC);
//}//f

// COLOR(texte,fond)
// fixe couleur de police et du fond
static void kw_color(){
    parse_arg_list(2);
    bytecode(BACK_COLOR);
    bytecode(FONT_COLOR);
}//f

// CONST  nom[$]=expr|string [, nom[$]=expr|string]
// définition d'une constante
static void kw_const(){
    char name[32];
    var_t *var;
    
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
    bytecode(ABS);
}//f

// SHL(expression)
// décalage à gauche de 1 bit
static void kw_shl(){
    parse_arg_list(1);
    bytecode(SHL);
}//f

// SHR(expression)
// décale à droite de 1 bit
static void kw_shr(){
    parse_arg_list(1);
    bytecode(SHR);
}//f

// BTEST(expression1,expression2{0-15})
// fonction test bit de expression1
// retourne vrai si 1
static void kw_btest(){
    parse_arg_list(2);
    bytecode(BTEST);
}//f

// BEEP(freq,msec,wait{0,1})
// fait entendre une tonalité
// durée ne millisecondes
static void kw_beep(){
    parse_arg_list(3);
    bytecode(BEEP);
}//f


//TONE(note{0-47},msec,wait{0,1})
//fait entendre un note de la gamme tempérée
static void kw_tone(){
    parse_arg_list(3);
    bytecode(TONE);
}//f

//PAUSE(msec)
// suspend exécution
// argument en millisecondes
static void kw_pause(){
    parse_arg_list(1);
    bytecode(IDLE);
}//f

//TICKS()
//retourne la valeur du compteur
// de millisecondes du système
static void kw_ticks(){
    parse_arg_list(0);
    bytecode(TICKS);
}//f

// SETTMR(msec)
// initialise _pause_timer
static void kw_set_timer(){
    parse_arg_list(1);
    bytecode(SETTMR);
}//f

// TIMEOUT()
// retourne vrai si _pause_timer==0
static void kw_timeout(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(TIMEOUT);
}//f

//NOISE(msec)
// génère un bruit blanc
// durée en millisecondes
static void kw_noise(){
    parse_arg_list(1);
    bytecode(NOISE);
}//f

// FONCTION BASIC: JSTICK()
// retourne un entier entre 0 et 31
// chaque bit représente un bouton
// 1=enfoncé, 0=relâché
// joystick Atari 2600
// bit 0 bouton
// bit 1 droite
// bit 2 gauche
// bit 3 bas
// bit 4 haut
static void kw_read_jstick(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(JSTICK);
}//f

//COMMANDE BASIC: BYE
//quitte l'interpréteur
static void kw_bye(){
    bytecode(BYE);
}//f

// RETURN expression
// utilisé dans les fonctions
static void kw_return(){
        expression();
        bytecode(LCSTORE);
        bytecode(0);
        bytecode(LEAVE);
        if (globals>varlist){
            bytecode(varlist->n);
        }else{
            bytecode(0);
        }
}//f

// EXIT SUB
// termine l'exécution d'une sous-routine
// en n'importe quel point de celle-ci.
// sauf à l'intérieur d'une boucle FOR
static void kw_exit(){
    expect(eKWORD);
    if (token.n != eKW_SUB) throw(eERR_SYNTAX);
    bytecode(LEAVE);
    if (globals>varlist){
        bytecode(varlist->n);
    }else{
        bytecode(0);
    }
}//f

// CLS [color]
// efface écran
// si argument couleur
// fixe la couleur de fond
static void kw_cls(){
    uint8_t color;
   
    next_token();
    if (token.id==eNUMBER){
        color=token.n;
        litc(color);
        bytecode(BACK_COLOR);
    }else{
        unget_token=true;
    }
    bytecode(CLS);
}//f

// LOCATE(ligne,colonne)
//positionne le curseur texte
static void kw_locate(){
    parse_arg_list(2);
    bytecode(LOCATE);
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
    rstack[++rsp]=eKW_IF;
    bool_expression();
}//f

// THEN voir kw_if
static void kw_then(){
    bytecode(ZBRANCH);
    rstack[++rsp]=dp;
    bytecode(0);
    bytecode(0);
}//f

static void fix_branch_address(){
    progspace[rstack[rsp]]=low(dp-rstack[rsp]-2);
    progspace[rstack[rsp]+1]=high(dp-rstack[rsp]-2);
    rsp--;
}//f

// ELSE voir kw_if
static void kw_else(){
    bytecode(BRANCH);
    bytecode(0);
    bytecode(0);
    fix_branch_address();
    rstack[++rsp]=dp-2;
}//f

//déplace le code des sous-routine
//à la fin de l'espace libre
static void movecode(var_t *var){
    uint16_t size;
    void *pos, *adr;
    
    pos=var->adr;
    size=(uint16_t)&progspace[dp]-(uint16_t)pos;
    if (size&1) size++;
    adr=endmark-size;
    memmove(adr,pos,size);
    endmark=adr;
    dp=(uint8_t*)pos-(uint8_t*)progspace;
    memset(&progspace[dp],0,size);
    var->adr=adr; 
//    {
//    int i;
//    for (i=0;i<size;i++) print_int(*(uint8_t*)adr++,4);
//    }
}//f

// END [IF|SUB|FUNC|SELECT]  termine les blocs conditionnels.
static void kw_end(){ // IF ->(R: blockend adr -- ) |
                      //SELECT -> (R: n*(addr,...) blockend n -- )
                      //SUB et FUNC -> (R: endmark blockend &var -- )
    int blockend,fix_count;
    var_t *var;

    expect(eKWORD);
    blockend=token.n;
    if (rstack[rsp-1]!=blockend) throw(eERR_SYNTAX);
    switch (blockend){
        case eKW_IF:
            fix_branch_address();
            rsp--; //drop eKW_IF
            break;
        case eKW_SELECT:
            fix_count=rstack[rsp--];
            if (!fix_count) throw(eERR_SYNTAX);
            rsp--; //drop eKW_SELECT
            while (fix_count){ //print_int(fix_count,0); new_line();
                fix_branch_address();
                fix_count--;
            } //print_int(rsp,0);
            bytecode(DROP);
            break;
        case eKW_FUNC:
            bytecode(LCSTORE);
            bytecode(0);
        case eKW_SUB:
            bytecode(LEAVE);
            if (globals>varlist){
                bytecode(varlist->n);
            }else{
                bytecode(0);
            }
            varlist=globals;
            globals=NULL;
            var_local=false;
            var=(var_t*)rstack[rsp--];
            endmark=(void*)rstack[rsp-1];
            movecode(var);
            rsp-=2; // drop eKW_xxx et endmark
            var_local=false;
//#define DEBUG
#ifdef DEBUG
    {
        uint8_t * bc;    
        for(bc=(uint8_t*)var->adr;*bc!=LEAVE;bc++){
            print_int(*bc,0);put_char(' ');
        }
        print_int(*bc,0);put_char(' ');
        bc++;
        print_int(*bc,0);put_char(' ');
    }
#endif
//#undef DEBUG
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
    rstack[++rsp]=eKW_SELECT;
    rstack[++rsp]=0; // compteur clauses CASE
    expression();
}//f

// compile la liste des expression d'un CASE, voir kw_select
static void compile_case_list(){
    int fix_count=0;
    next_token();
    if (token.id==eSTOP) throw(eERR_SYNTAX);
    unget_token=true;
    while (token.id != eSTOP){
        bytecode(DUP);
        expression();
        bytecode(EQUAL);
        bytecode(NZBRANCH);
        rstack[++rsp]=dp;
        dp+=2;
        fix_count++;
        next_token();
        if (token.id!=eCOMMA){
            unget_token=true;
            break;
        }
    }//while
    bytecode(BRANCH);
    dp+=2;
    while (fix_count){
        fix_branch_address();
        fix_count--;
    }
    rsp++;
    rstack[rsp]=rstack[rsp-1]+1; //incrémente le nombre de clause CASE
    rstack[rsp-1]=rstack[rsp-2]; // blockend
    rstack[rsp-2]=dp-2;   // bra_adr
}//f

// compile un CASE, voir kw_select
static void kw_case(){
    uint16_t adr;
    next_token();
    if (token.id==eKWORD && token.n==eKW_ELSE){
        if (rstack[rsp]){
            adr=rstack[rsp-2]; //( br_adr blockend n  -- br_adr blockend n)
            bytecode(BRANCH);  // branchement à la sortie du select case
            rstack[rsp-2]=dp; // (adr blockend n -- dp blockend n )
            dp+=2;
            rstack[++rsp]=adr;
            fix_branch_address(); // fixe branchement vers CASE ELSE
        }else
            throw(eERR_SYNTAX);
    }else{
        unget_token=true;
        if (rstack[rsp]){//y a-t-il un case avant celui-ci?
            adr=rstack[rsp-2]; //( br_adr blockend n  -- br_adr blockend n)
            bytecode(BRANCH);  // branchement à la sortie du select case
            rstack[rsp-2]=dp; // (adr blockend n -- dp blockend n )
            dp+=2;
            rstack[++rsp]=adr;
            fix_branch_address(); // fixe branchement vers ce case
        }
        compile_case_list();
    }
}//f

// RND())
// génération nombre pseudo-aléatoire
static void kw_rnd(){
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(RND);
}//f

//RANDOMIZE()
static void kw_randomize(){
    parse_arg_list(0);
    bytecode(RANDOMIZE);
}//f

//MAX(expr1,expr2)
static void kw_max(){
    parse_arg_list(2);
    bytecode(MAX);
}//f

//MIN(expr1,expr2)
static void kw_min(){
    parse_arg_list(2);
    bytecode(MIN);
}//f

//MDIV(expr1,expr2,expr3)
//multiplie expr1*expr2
//garde résultat sur 32 bits
//divise résultat 32 bits par
// epxr3
static void kw_mdiv(){
    parse_arg_list(3);
    bytecode(MDIV);
}//f

// GETPIXEL(x,y)
// retourne la couleur du pixel
// en position {x,y}
static void kw_getpixel(){
    parse_arg_list(2);
    bytecode(GETPIXEL);
}//f

// SETPIXEL(x,y,c)
// fixe la couleur du pixel
// en position x,y
// c-> couleur {0-15}
static void kw_setpixel(){
    parse_arg_list(3);
    bytecode(SETPIXEL);
}//f

// XORPIXEL(x,y,xor_value)
// XOR le pixel avec xor_value {0-15}
static void kw_xorpixel(){
    parse_arg_list(3);
    bytecode(XORPIXEL);
}//f

// SCRLUP(n)
// glisse l'écran vers le haut de n lignes
static void kw_scrlup(){
    parse_arg_list(1);
    bytecode(SCRLUP);
}//f

// SCRLDN(n)
// glisse l'écran vers le bas de n lignes
static void kw_scrldown(){
    parse_arg_list(1);
    bytecode(SCRLDN);
    
}//f

// SRCLRT(n)
// glisse l'écran vers la droite de n pixel
// n doit-être pair
static void kw_scrlright(){
    parse_arg_list(1);
    bytecode(SCRLRT);
    
}//f

// SCRLLT(n)
// glisse l'afficage de n pixel vers la gauche
static void kw_scrlleft(){
    parse_arg_list(1);
    bytecode(SCRLLT);
    
}//f

// LINE(x1,y,x2,y2,color)
// trace une ligne droite
static void kw_line(){
    parse_arg_list(5);
    bytecode(LINE);
}//f

//BOX(x,y,width,height,color)
//desssine une rectangle plein
static void kw_box(){
    parse_arg_list(5);
    bytecode(BOX);
}//f

//RECT(x0,y0,width,height,color)
//dessine un rectangle vide
static void kw_rect(){
    parse_arg_list(5);
    bytecode(RECT);
}//f

//CIRCLE(xc,yc,r, color)
//dissine un cercle rayon r, centré sur xc,yc
static void kw_circle(){
    parse_arg_list(4);
    bytecode(CIRCLE);
}//f

//ELLIPSE(xc,yc,w,h,color)
//dessine une ellipse centrée sur xc,yc
// et de largeur w, hauteur h
static void kw_ellipse(){
    parse_arg_list(5);
    bytecode(ELLIPSE);
}//f


// SPRITE(x,y,width,height,@sprite,@save_back)
// desssine le sprite à la position désigné
// sprite est un vecteur de type octet
//save_back est un vecteur de type octet de même grandeur que sprite
//save_back est utilisé pour sauvegarder le fond d'écran
//en vue de sa restauration
static void kw_sprite(){
    parse_arg_list(6);
    bytecode(SPRITE);
}//f

//REMSPR(x,y,width,height,@rest_back)
//efface le sprite en restaurant les bits
// de fond d'écran sauvegardés par SPRITE()
static void kw_remove_sprite(){
    parse_arg_list(5);
    bytecode(REMSPR);
}//f

static void kw_let();

// FOR var=expression TO expression [STEP expression]
//  bloc_instructions
// NEXT var
static void kw_for(){
    var_t *var;
    char name[32];
    
    next_token();
    if (token.id!=eIDENT) throw(eERR_SYNTAX);
    strcpy(name,token.str);
    complevel++;
    bytecode(SAVELOOP);
    unget_token=true;
    kw_let();
    expect(eIDENT);
    if (strcmp(token.str,"TO")) throw (eERR_SYNTAX);
    expression();
    bytecode(SAVELIMIT);
    next_token();
    if (token.id==eIDENT && !strcmp(token.str,"STEP")){
        expression();
        bytecode(SAVESTEP);
    }else{
        litc(1);
        bytecode(SAVESTEP);
        unget_token=true;
    }
    var=var_search(name);
    if (var->vtype==eVAR_LOCAL){
        bytecode(LCADR);
        bytecode(var->n);
    }else{
        lit((uint16_t)&var->n);
    }
    bytecode(FETCH);
    bytecode(LOOPTEST);
    bytecode(ZBRANCH);
    rstack[++rsp]=dp;
    bytecode(0);
    bytecode(0);
}//f

// voir kw_for()
static void kw_next(){
    var_t *var;
    
    expect(eIDENT);
    var=var_search(token.str);
    if (!(var && ((var->vtype==eVAR_INT) || (var->vtype==eVAR_LOCAL)))) throw(eERR_BAD_ARG);
    if (var->vtype==eVAR_LOCAL){
        bytecode(LCADR);
        bytecode(var->n);
    }else{
        lit((uint16_t)&var->adr);
    }
    bytecode(NEXT);
    bytecode(BRANCH);
    progspace[dp]=low((rstack[rsp]-dp-4));
    progspace[dp+1]=high((rstack[rsp]-dp-4));
    dp+=2;
    fix_branch_address();
    bytecode(RESTLOOP); 
    complevel--;
}//f

// WHILE condition
//  bloc_instructions
// WEND
static void kw_while(){
    complevel++;
    rstack[++rsp]=dp;
    bool_expression();
    bytecode(ZBRANCH);
    rstack[++rsp]=dp;
    dp+=2;
}//f

// voir kw_while()
static void kw_wend(){
    bytecode(BRANCH);
    progspace[dp]=low(rstack[rsp-1]-dp-2);
    progspace[dp+1]=high(rstack[rsp-1]-dp-2);
    dp+=2;
    fix_branch_address();
    rsp--;
    complevel--;
}//f

// DO 
//   bloc_instructions
// LOOP WHILE|UNTIL condition
static void kw_do(){
    complevel++;
    rstack[++rsp]=dp;
}//f

// voir kw_do()
static void kw_loop(){
    expect(eKWORD);
    if (token.n==eKW_WHILE){
        bool_expression();
        bytecode(NZBRANCH);
        
    }else if (token.n==eKW_UNTIL){
        bool_expression();
        bytecode(ZBRANCH);  
    }else{
        throw(eERR_SYNTAX);
    }
    progspace[dp]=low(rstack[rsp]-dp-2);
    progspace[dp+1]=high(rstack[rsp]-dp-2);
    dp+=2;
    rsp--;
    complevel--;
}//f

static void literal_string(char *name){
    int size;
    char fname[32];
    strcpy(fname,name);
    uppercase(fname);
    size=strlen(fname)+1;
    if ((void*)&progspace[dp+size+2]>endmark) throw(eERR_PROGSPACE);
    bytecode(LITS);
    bytecode(size&0xff);
    strcpy((char*)&progspace[dp],token.str);
    dp+=size;
}//f


//SRLOAD file_name
//charge un fichier dans la SPIRAM
// retourne la grandeur en octet
static void kw_srload(){
    var_t *var;
    
    next_token();
    if (token.id==eSTRING){
        literal_string(token.str);
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (!var || !(var->vtype==eVAR_STR || var->vtype==eVAR_STRARRAY)) throw(eERR_BAD_ARG);
        if (var->vtype==eVAR_STRARRAY){
            expect(eLPAREN);
            code_array_address(var);
        }else{
            lit((uint16_t)&var->str);
        }
        bytecode(FETCH);
    }else{
        throw(eERR_BAD_ARG);
    }
    bytecode(SRLOAD);
}//f

//SRSAVE file_name, size
//sauvegarde SPIRAM dans un fichier
static void kw_srsave(){
    var_t *var;
    
    next_token();
    if (token.id==eSTRING){
        literal_string(token.str);
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (!var || !(var->vtype==eVAR_STR || var->vtype==eVAR_STRARRAY)) throw(eERR_BAD_ARG);
        if (var->vtype==eVAR_STRARRAY){
            expect(eLPAREN);
            code_array_address(var);
        }else{
            lit((uint16_t)&var->str);
        }
        bytecode(FETCH);
    }else{
        throw(eERR_BAD_ARG);
    }
    expect(eCOMMA);
    expression();
    bytecode(SRSAVE);
}//f

//SRCLEAR(address,size)
//met à zéro un bloc de mémoire SPIRAM
static void kw_srclear(){
    parse_arg_list(2);
    bytecode(SRCLEAR);
}//f

//SRREAD(address,@var,size)
//lit un bloc de mémoire SPIRAM
//dans une variable
static void kw_srread(){
    parse_arg_list(3);
    bytecode(SRREAD);
}//f

//SRWRITE(address,@var,size)
//copie le contenu d'une variable dans la SPIRAM
static void kw_srwrite(){
    parse_arg_list(3);
    bytecode(SRWRITE);
}//f

//RESTSCR(adresse)
//restore le buffer vidéo à partir de la SPI RAM
// adresse est l'adresse sour dans SPI RAM
void kw_restore_screen(){
    parse_arg_list(1);
    bytecode(RESTSCR);
}//f

//SAVESCR(adresse)
//sauvegarde le buffer vidéo dans la SPI RAM
// adresse est la destination dans SPI RAM
void kw_save_screen(){
    parse_arg_list(1);
    bytecode(SAVESCR);
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
            adr=(char*)alloc_var_space(strlen(token.str)+1);
            strcpy(adr,token.str);
            lit((uint16_t)adr);
            bytecode(SWAP);
            bytecode(STORE);
            break;
        case eVAR_INTARRAY:
            expression();
            bytecode(SWAP);
            bytecode(STORE);
            break;
        case eVAR_BYTEARRAY:
            expression();
            bytecode(SWAP);
            bytecode(STOREC);
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
}//f

// compile le code pour le stockage d'untier
static void store_integer(var_t *var){
    switch(var->vtype){
        case eVAR_LOCAL:
            bytecode(LCSTORE);
            bytecode(var->n);
            break;
        case eVAR_INT:
            lit(((uint16_t)(int*)&var->n));
            bytecode(STORE);
            break;
        case eVAR_BYTE:
            lit(((uint16_t)(int*)&var->n));
            bytecode(STOREC);
            break;
        case eVAR_CONST:
            // ignore les assignations de constantes jette la valeur
            bytecode(DROP);
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
    bytecode(WKEY);
}//f

//LEN var$|string
static void kw_len(){
    var_t *var;
    expect(eLPAREN);
    next_token();
    if (token.id==eSTRING){
        literal_string(token.str);
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (!var) throw(eERR_BAD_ARG);
        switch(var->vtype){
            case eVAR_STR:
                lit((uint16_t)var->str);
                break;
            case eVAR_STRARRAY:
                lit((uint16_t)var->adr);
                expect(eLPAREN);
                expression();
                expect(eRPAREN);
                litc(1);
                bytecode(ADD);
                bytecode(SHL);
                bytecode(ADD);
                break;
            default:
                throw(eERR_BAD_ARG);
        }//switch
    }else{
        throw(eERR_BAD_ARG);
    }
    bytecode(LEN);
    expect(eRPAREN);
}//f

//compile le code qui vérifie si une
// chaine est déjà assignée à cette variable
static void compile_accept_var(var_t* var){
    bytecode(FETCH); // (var->str -- char* )
    bytecode(ZBRANCH); // si NULL accepte
    rstack[++rsp]=dp;
    bytecode(0);
    bytecode(0);
    // variable chaîne déjà assignée on ignore cette variable
    bytecode(DROP); // la copie ne sera pas utilisée
    bytecode(BRANCH);
    bytecode(0);
    bytecode(0);
    fix_branch_address();
    rstack[++rsp]=dp-2;
}//f

// INPUT [string ','] identifier (',' identifier)
// saisie au clavier de
// valeur de variables
static void kw_input(){
    char name[32];
    var_t *var;
    
    next_token();
    if (token.id==eSTRING){
        literal_string(token.str);
        bytecode(TYPE);
        bytecode(CRLF);
        expect(eCOMMA);
        expect(eIDENT);
    }else if(token.id!=eIDENT) throw(eERR_BAD_ARG); 
    while (token.id==eIDENT){
        strcpy(name,token.str);
        var=var_search(name);
        //dans contexte SUB|FUNC seule les variables préexistante
        //peuvent-être utilisées
        if (!var && var_local) throw(eERR_BAD_ARG);
        next_token();
        //seules les variables vecteur préexistantes peuvent-être utilisées
        if (token.id==eLPAREN && !var) throw(eERR_BAD_ARG);
        unget_token=true;
        if (!var) var=var_create(name,NULL);
        if (var->vtype>=eVAR_INTARRAY && var->vtype<=eVAR_STRARRAY){
            parse_arg_list(1);
        }
        switch (var->vtype){ 
            case eVAR_STR:
                lit((uint16_t)&var->str);
                bytecode(DUP); //conserve une copie
                compile_accept_var(var);
                // var accepté, lecture de la  chaîne au clavier
                prt_varname(var);
                litc(CHAR_PER_LINE-2);
                bytecode(ACCEPT); // ( var_adr buffsize -- var_adr _pad n )
                litc(1);
                bytecode(ADD);
                bytecode(ALLOC);  //( var_adr _pad alloc_size -- var_adr _pad void* )
                bytecode(SWAP);  //( var_adr _pad void* -- var_adr void* _pad )
                bytecode(OVER);  //( var_adr void* _pad -- var_adr void* _pad void* )
                bytecode(STRCPY); // ( var_adr void* src* dest* -- var_adr void* )
                bytecode(SWAP); // ( var_adr void* -- void* var_adr )
                bytecode(STORE);   //( void* var_adr -- )
                fix_branch_address();
                break;
            case eVAR_STRARRAY:  // ( -- index )
                bytecode(SHL);   // ( index -- 2*index)
                lit((uint16_t)var->adr);
                bytecode(ADD);  // ( -- &var(index) )
                bytecode(DUP);  // garde une copie
                compile_accept_var(var);
                //var acceptée, lecture de la chaîne au clavier 
                prt_varname(var);  // ( var_adr -- var_adr )
                litc(CHAR_PER_LINE-2); //( var_adr -- var_adr buffsize )
                bytecode(ACCEPT); // ( var_adr buffsize -- var_adr _pad strlen )
                litc(1);
                bytecode(ADD);   // incrémente strlen
                bytecode(ALLOC);  //( var_adr _pad alloc_size -- var_adr _pad void* )
                bytecode(SWAP);   // ( var_adr _pad void* -- var_adr void* _pad  )
                bytecode(OVER);  // ( _var_adr void* _pad -- _var_adr void* _pad void*  )
                bytecode(STRCPY); // ( _var_adr void* src* dest* -- _var_adr void* )
                bytecode(SWAP);   // ( _var_adr void* -- void* _var_adr )
                bytecode(STORE);   //( void* _var_adr  -- )
                fix_branch_address();
                break;
            case eVAR_LOCAL:
            case eVAR_INT:
            case eVAR_BYTE:
                litc(17);
                bytecode(ACCEPT);
                bytecode(DROP);
                bytecode(INT);
                store_integer(var);
                break;
            case eVAR_INTARRAY:
                bytecode(SHL);
                lit((uint16_t)var->adr);
                bytecode(ADD);
                litc(17);
                bytecode(ACCEPT);
                bytecode(DROP);
                bytecode(INT);
                bytecode(SWAP);
                bytecode(STORE);
                break;
            case eVAR_BYTEARRAY:
                litc(1);
                bytecode(ADD);
                lit((uint16_t)var->adr);
                bytecode(ADD);
                litc(9);
                bytecode(ACCEPT);
                bytecode(DROP);
                bytecode(INT);
                bytecode(SWAP);
                bytecode(STOREC);
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
    next_token();
    if (var_local){ // on ne peut pas allouer de chaîne dans les sous-routine
        if (name[len-1]=='$') throw(eERR_SYNTAX);
        var=var_search(name);
        if (!var) throw(eERR_BAD_ARG); // pas d'auto création à l'intérieur des sous-routines
        if (token.id==eLPAREN && (var->vtype==eVAR_INTARRAY || var->vtype==eVAR_BYTEARRAY)){
            code_array_address(var);
            expect(eEQUAL);
            expression();
            bytecode(SWAP);
            if (var->vtype==eVAR_INTARRAY){
                bytecode(STORE);
            }else{
                bytecode(STOREC);
            }
        }else if (token.id==eEQUAL){
            expression();
            store_integer(var);
        }else throw(eERR_SYNTAX);
    }else{
        if (token.id==eLPAREN){
            array_let(name);
        }else{
            var=var_search(name);
            if (!var) var=var_create(name,NULL);
            unget_token=true;
            expect(eEQUAL);
            if (name[len-1]=='$'){
                if (var->str) throw(eERR_ASSIGN);
                expect(eSTRING);
                var->str=alloc_var_space(len+1);
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
    bytecode(LCVARSPACE);
    bytecode(lc);
}//f

// PRINT|? chaine|identifier|expression {,chaine|identifier|expression}
static void kw_print(){
    var_t *var;
    
    next_token();
    while (!activ_reader->eof){
        switch (token.id){
            case eSTRING:
                literal_string(token.str);
                bytecode(TYPE);
                bytecode(SPACE);
                break;
            case eIDENT:
                if(token.str[strlen(token.str)-1]=='$'){
                    if ((var=var_search(token.str))){
                        if (var->vtype==eVAR_STRARRAY){
                            expect(eLPAREN);
                            code_array_address(var);
                        }else{
                            lit((uint16_t)&var->str);
                        }
                        bytecode(FETCH);
                        bytecode(TYPE);
                        bytecode(SPACE);
                    }
                }else{ 
                    unget_token=true;
                    expression();
                    bytecode(DOT);
                }
               break;
            case eSTOP:
                bytecode(CRLF);
                break;
            default:
                unget_token=true;
                expression();
                bytecode(DOT);
                
        }//switch
        next_token();
        if (token.id==eSEMICOL){
            break;
        }
        if (token.id!=eCOMMA){
            bytecode(CRLF);
           // new_line();
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
    bytecode(EMIT);
}//f

// KEY()
// retourne indicateur touche clavier
// ou zéro
static void kw_key(){ 
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(KEY);
}//f

// cré la liste des arguments pour
// la compilation des SUB|FUNC
static void create_arg_list(){
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
    bytecode(FRAME);
    bytecode(i);
}//f

//cré une nouvelle variable de type eVAR_SUB|eVAR_FUNC
static void subrtn_create(int var_type, int blockend){
    var_t *var;

    if (var_local) throw(eERR_SYNTAX);
    expect(eIDENT);
    if (token.str[strlen(token.str)-1]=='$') throw(eERR_SYNTAX);
    var=var_search(token.str);
    if (var) throw(eERR_REDEF);
    var=var_create(token.str,NULL);
    var->vtype=var_type;
    var->adr=(void*)&progspace[dp];
    globals=varlist;
    var_local=true;
    rstack[++rsp]=(uint16_t)endmark;
    complevel++;
    rstack[++rsp]=blockend;
    rstack[++rsp]=(uint16_t)var;
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
    bytecode(TRACE);
}//f



void compile(){
    var_t *var;
    uint16_t adr;
    
    do{
        next_token();
        switch(token.id){ 
            case eCMD:
                if (complevel || activ_reader->device!=STDIN) throw (eERR_CMDONLY);
                COMMAND[token.n].cfn();
                break;
            case eKWORD:
                if ((KEYWORD[token.n].len&FUNCTION)==FUNCTION){
                    KEYWORD[token.n].cfn();
                    bytecode(DROP);
                }else{
                    KEYWORD[token.n].cfn();
                }
                break;
            case eIDENT:
                if ((var=var_search(token.str))){
                    if (var->vtype==eVAR_SUB){
                        adr=(uint16_t)var->adr;
                        parse_arg_list(*((uint8_t*)(adr+1)));
                        bytecode(CALL);
                        bytecode(low((uint16_t)&var->adr));
                        bytecode(high((uint16_t)&var->adr));
                    }else if (var->vtype==eVAR_FUNC){
                        litc(0);
                        adr=(uint16_t)var->adr;
                        parse_arg_list(*((uint8_t*)(adr+1)));
                        bytecode(CALL);
                        bytecode(low((uint16_t)&var->adr));
                        bytecode(high((uint16_t)&var->adr));
                        bytecode(DROP);
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
                if (activ_reader->device==STDIN) activ_reader->eof=true;
                break;
            case eNONE:
                break;
            default:
                throw(eERR_SYNTAX);
                break;
        }//switch
    }while (!activ_reader->eof);
}//f

void shell(void){
    cls();
    print("PV16SOG shell\nversion 1.0\n");
    new_line();
    prog_size=dfree_size();
    progspace=dalloc(prog_size); 
    cmd_clear();
    cmd_free_pool();
//  initialisation lecteur source.
    reader_init(&std_reader,STDIN,NULL);
    activ_reader=&std_reader;
    while (1){
        if (!setjmp(failed)){
            activ_reader->eof=false;
            compile();
            if (!complevel){
                if (program_loaded && run_it){
                    interpret(progspace);
                    if (vm_exit_code) throw(vm_exit_code);
                    run_it=false;
                }else if (dp>program_end){
//#define DEBUG
#ifdef DEBUG
    {
        int i;
        for (i=program_end;i<=dp;i++){print_int(progspace[i],3);put_char(' ');}new_line();
    }
#endif
//#undef DEBUG    
                    bytecode(BYE);
                    interpret(&progspace[program_end]);
                    if (vm_exit_code) throw(vm_exit_code);
                    if (activ_reader->device==STDIN){
                        memset(&progspace[program_end],0,dp-program_end);
                        dp=program_end;
                    }
                }
            }
        }else{
            cmd_clear();
            activ_reader=&std_reader;
            reader_init(activ_reader,STDIN,NULL);
        }
    }//while(1)
}//shell()
