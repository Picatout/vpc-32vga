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
#include <math.h>

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

#define _addr(ptr) ((uint32_t)ptr)
#define _value(var) (*(uint32_t*)var) 

#define _prt_varname(v) code_lit32(_addr(v->name));\
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

// utilisé comme code par défaut pour les DECLARE SUB|FUNC
const uint8_t undefined_sub[3]={ICLIT,eERR_NOT_DEFINED,IABORT};

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
static uint32_t prog_size;
static uint32_t line_count;
static uint32_t token_count;
static bool program_loaded;
static bool run_it=false;
static uint32_t program_end;


#define NAME_MAX 32 //longueur maximale nom incluant zéro terminal
#define LEN_MASK 0x1F   // masque longueur nom.
#define ADR  (void *)



typedef void (*fnptr)();

// type de fonctions
enum FN_TYPE{
    eFN_NOT, // ce n'est pas une fonction
    eFN_INT, // la fonction retourne un entier 32 bits ou un octet non signé
    eFN_STR, // la fonction retourne une chaîne
    eFN_FPT  // la fonction retourne un nombre virgule flottante, float32
};

//types de données
typedef enum {
    eVAR_UNKNOWN, // type indéfinie
    eVAR_INT, // INTEGER
    eVAR_BYTE, // BYTE
    eVAR_FLOAT, // FLOAT
    eVAR_STR, // STRING
    eVAR_SUB,   // SUB
    eVAR_FUNC   // FUNC
}var_type_e;

// nom des types de données
const char *TYPE_NAME[]={
    "UNKNOWN",
    "INTEGER",
    "BYTE",
    "FLOAT",
    "STRING",
    0
};

// le type d'une expression numérique 
// est déterminé soit par le type de la variable
// qui sera assignée ou par le type du permier facteur.
//static int expr_type=eVAR_UNKNOWN;

//entrée de commande dans liste
typedef struct{
    fnptr cfn; // pointeur fonction commande
    uint8_t len:5;//longueur du nom.
    uint8_t fntype:3; // type de donnée retourné par la fonction. 
    char *name; // nom de la commande
} dict_entry_t;


typedef struct _var{
    struct _var* next; // prochaine variable dans la liste
    uint16_t len:5; // longueur du nom
    uint16_t ro:1; // booléen, c'est une constante
    uint16_t array:1; // booléen, c'est un tableau
    uint16_t local:1; // bookéen, c'est une variable locale.
    uint16_t vtype:3; // var_type_e
    uint16_t dim:5; // nombre de dimension du tableau ou nombre arguments FUNC|SUB
    char *name;  // nom de la variable
    union{
        uint8_t byte; // variable octet
        int32_t n;    // entier ou nombre d'arguments et de variables locales pour func et sub
        float f;   // nombre virgule flottante 32 bits
        char *str;    // adresse chaîne asciiz
        void *adr;    // addresse tableau,sub ou func
    };
}var_t;

// type d'unité lexicales
typedef enum {eNONE,eNL,eCOLON,eIDENT,eINT,eFLOAT,eSTRING,ePLUS,eMINUS,eMUL,eDIV,
              eMOD,eCOMMA,eLPAREN,eRPAREN,eSEMICOL,eEQUAL,eNOTEQUAL,eGT,eGE,eLT,eLE,
              eEND, eELSE,eCMD,eKWORD,eCHAR,eFILENO} tok_id_t;

// longueur maximale d'une ligne de texte source.
#define MAX_LINE_LEN 128

// unité lexicale.              
typedef struct _token{
   tok_id_t id;
   union{
        int n; //  entier
        float f; // nombre virgule flottante 32bits
        int kw; // indice dans le dictionnaire KEYWORD
   };
   char str[MAX_LINE_LEN]; // chaîne représentant le jeton.
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
static void kw_close();
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
static void kw_eof();
static void kw_exist();
static void kw_exit();
static void kw_fill();
static void kw_for();
static void kw_free();
static void kw_func();
static void kw_fgetc();
static void kw_getpixel();
static void kw_hex();
static void kw_if();
static void kw_input();
static void kw_insert();
static void kw_insert_line();
static void kw_instr();
static void kw_invert_video();
static void kw_key();
static void kw_lcase();
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
static void kw_open();
static void kw_peek();
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
static void kw_seek();
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
static void kw_ucase();
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
static int expression();
static int factor();
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
static int var_type_from_name(char *name);
static void optional_parens();
static void expect(tok_id_t t);
static int get_file_free();
static FIL *open_file(const char *file_name,int mode);
static void close_file(FIL *fh);
void close_all_files();
static int get_file_no(FIL *fh);
static FIL *get_file_handle(int i);
static void throw(int error);
static void string_term();

#ifdef DEBUG
static void print_prog(int start);
static void print_cstack();
#endif


char *float_to_str(char * buffer, float f){
    sprintf(buffer,"%G",f);
    return buffer;
}

#define MAX_FILES 5
static FIL *files_handles[MAX_FILES]={NULL,NULL,NULL,NULL,NULL};
static file_count; // nombre de fichiers ouverts.

// retourne un numéro de fichier libre.
static int get_file_free(){
    int i;
    if (file_count==MAX_FILES){return -1;}//aucun disponible
    for (i=0;i<MAX_FILES;i++){
        if (!files_handles[i]) return i;
    }
    return -1;
}

// retourne le numéro du fichier à partir du handle
static int get_file_no(FIL *fh){
    int i;
    for (i=0;i<MAX_FILES;i++){
        if (files_handles[i]==fh){return i;}
    }
    return -1;
}

// retourne le handle d'un fichier à partir de son numéro.
static FIL *get_file_handle(int i){
    if ((i<0) || (i>=MAX_FILES)){ return NULL;}
    return files_handles[i];
}


static FIL *open_file(const char *file_name,int mode){
    int file_no;
    FIL *fh;
    FRESULT result;
    
    file_no=get_file_free();
    if (file_no==-1){return NULL;}
    fh=malloc(sizeof(FIL));
    if (!fh){return NULL;}
    result=f_open(fh,file_name,mode);
    if (!result){
        files_handles[file_no]=fh;
        file_count++;
        return fh;
    }
    free(fh);
    return NULL;
}

static void close_file(FIL *fh){
    int i;
    for (i=0;i<MAX_FILES;i++){
        if (files_handles[i]==fh){
            f_close(fh);
            free(fh);
            file_count--;
            files_handles[i]=NULL;
            return;
        }
    }
}

void close_all_files(){
    int i;
    for (i=0;i<MAX_FILES;i++){
        if (files_handles[i]){
            f_close(files_handles[i]);
            free(files_handles[i]);
        }
    }
    file_count=0;
}

bool basic_fexist(const char *file_name){
    FIL *fh;
    FRESULT result;
    fh=malloc(sizeof(FIL));
    if (!fh){throw(eERR_ALLOC);}
    result=f_open(fh,file_name,FA_OPEN_EXISTING);
    if (result==FR_OK){f_close(fh);}
    free(fh);
    return !result;
}

void basic_fopen(const char *file_name,int mode, unsigned file_no){
    if (files_handles[file_no]){throw(eERR_FILE_ALREADY_OPEN);}
    FIL *fh;
    FRESULT result;
    fh=malloc(sizeof(FIL));
    if (!fh){throw(eERR_ALLOC);}
    result=f_open(fh,file_name,mode);
    if (result){throw(eERR_FILE_IO);}
    files_handles[file_no]=fh;
}

void basic_fclose(unsigned file_no){
    if (files_handles[file_no]){
        f_close(files_handles[file_no]);
        free(files_handles[file_no]);
        files_handles[file_no]=NULL;
    }
}

bool basic_feof(unsigned file_no){
    if (!files_handles[file_no]){throw(eERR_FILE_NOT_OPENED);}
    return f_eof(files_handles[file_no]);
}

void basic_fseek(unsigned file_no,int pos){
    if (!files_handles[file_no]){throw(eERR_FILE_NOT_OPENED);}
    if (pos==-1){
        pos=files_handles[file_no]->fsize;
    }
    if (f_lseek(files_handles[file_no],pos)){throw(eERR_FILE_IO);}
}

void basic_fputc(unsigned file_no, char c){
    if (!files_handles[file_no]){throw(eERR_FILE_NOT_OPENED);}
    if (f_putc(c,files_handles[file_no])!=1){throw(eERR_FILE_IO);}
}

// écris un champ dans un fichier CSV
void basic_write_field(unsigned file_no,const char *str){
    int n;
    if (!files_handles[file_no]){throw(eERR_FILE_NOT_OPENED);}
    n=f_puts(str,files_handles[file_no]);
    if (n<0 || n!=strlen(str)){throw(eERR_FILE_IO);}
    if (str[strlen(str)-1]!='\n'){
        basic_fputc(file_no,',');
    }
}

char basic_fgetc(unsigned file_no){
    unsigned u,n;
    FRESULT result;
    
    if (!files_handles[file_no]){throw(eERR_FILE_NOT_OPENED);}
    result=f_read(files_handles[file_no],&u,1,&n);
    if (result){throw(eERR_FILE_IO);}
    return (char)u;    
}

//char *basic_fgets(unsigned file_no){
//    FRESULT result;
//    TCHAR *str;
//    
//    if (!files_handles[file_no]){throw(eERR_FILE_NOT_OPENED);}
//    str=f_gets(pad,PAD_SIZE,files_handles[file_no]);
//    if (str!=pad){throw(eERR_FILE_IO);}
//    return pad;
//}

// lit un champ à partir d'un fichier texte CSV
// un champ se termine par une virgule ou une fin de ligne.
// sauf pour les chaîne entre guillemets.
// Les espaces sont ignorés sauf entre guillemets.
char *file_read_field(unsigned file_no){
    FRESULT result;
    TCHAR c;
    FIL *fh;
    int n, i=0;
    bool in_quote=false;
    bool escaped=false;
    unsigned u;
    
    fh=files_handles[file_no];
    if (!fh){throw(eERR_FILE_NOT_OPENED);}
    pad[0]=0;
    while (!f_eof(fh) && i<PAD_SIZE){
        result=f_read(fh,&u,1,&n);
        if (result) {throw(eERR_FILE_IO);}
        c=u&0xff;
        switch(c){
            case '"':
                if (escaped){
                    pad[i]=c;
                    escaped=false;
                }else if (! in_quote){
                    in_quote=true;
                }else{
                    in_quote=false;
                    pad[i]=0;
                    return pad;
                }
                break;
            case '\\':
                if (in_quote){
                    escaped=true;
                }else{
                    pad[i]=c;
                }
                break;
            case ' ':
            case TAB:
                pad[i]=32;
                break;
            case ',':
            case '\n':
                if (in_quote){
                    pad[i]=c;
                }else if (i){
                    pad[i]=0;
                    return pad;
                }else{
                    i--;
                }
                
                break;
            default:
                pad[i]=c;
        }
        i++;
    }
    return pad;
}


//identifiant KEYWORD doit-être dans le même ordre que
//dans la liste KEYWORD
enum {eKW_ABS,eKW_AND,eKW_FILE_APPEND,eKW_APPEND,eKW_AS,eKW_ASC,eKW_BEEP,eKW_BOX,
      eKW_BTEST,eKW_BYE,eKW_CASE,
      eKW_CHR,eKW_CIRCLE,
      eKW_CLEAR,eKW_CLOSE,eKW_CLS,
      eKW_CONST,eKW_CURCOL,eKW_CURLINE,eKW_DATE,eKW_DECLARE,eKW_DIM,eKW_DO,
      eKW_ELLIPSE,eKW_ELSE,
      eKW_END,eKW_EOF,eKW_EXIST,eKW_EXIT,eKW_FGETC,eKW_FILL,
      eKW_FOR,eKW_FREE,eKW_FUNC,eKW_HEX,eKW_GETPIXEL,eKW_IF,
      eKW_INPUT,eKW_INSERT,eKW_INSTR,
      eKW_INSERTLN,
      eKW_INVVID,eKW_KEY,eKW_LCASE,eKW_LEFT,eKW_LEN,
      eKW_LET,eKW_LINE,eKW_LOCAL,eKW_LOCATE,eKW_LOOP,eKW_MAX,eKW_MDIV,eKW_MID,eKW_MIN,eKW_NEXT,
      eKW_NOT,eKW_OPEN,eKW_OR,eKW_FILE_OUTPUT,eKW_PEEK,eKW_PLAY,eKW_POLYGON,eKW_PREPEND,
      eKW_PRINT,eKW_PSET,eKW_PUTC,eKW_RANDOMISIZE,eKW_RECT,eKW_REF,eKW_REM,eKW_RESTSCR,
      eKW_RETURN,eKW_RIGHT,eKW_RND,eKW_RUN,eKW_SAVESCR,eKW_SCRLUP,eKW_SCRLDN,eKW_SEEK,
      eKW_SELECT,eKW_SETTMR,eKW_SHL,eKW_SHR,eKW_SLEEP,eKW_SOUND,
      eKW_SPRITE,eKW_SRCLEAR,eKW_SRLOAD,eKW_SRREAD,eKW_SRSSAVE,eKW_SRWRITE,eKW_STR,
      eKW_SUB,eKW_SUBST,eKW_TIME,eKW_THEN,eKW_TICKS,
      eKW_TIMEOUT,eKW_TKEY,eKW_TRACE,eKW_TUNE,eKW_UBOUND,eKW_UCASE,eKW_UNTIL,eKW_USE,eKW_VAL,
      eKW_VGACLS,
      eKW_WAITKEY,eKW_WEND,eKW_WHILE,eKW_XOR,eKW_XORPIXEL
};

//mots réservés BASIC
static const dict_entry_t KEYWORD[]={
    {kw_abs,3,eFN_INT,"ABS"},
    {bad_syntax,3,eFN_NOT,"AND"},
    {bad_syntax,6,eFN_NOT,"APPEND"},
    {kw_append,7,eFN_STR,"APPEND$"},
    {bad_syntax,2,eFN_NOT,"AS"},
    {kw_asc,3,eFN_INT,"ASC"},
    {kw_beep,4,eFN_NOT,"BEEP"},
    {kw_box,3,eFN_NOT,"BOX"},
    {kw_btest,5,eFN_INT,"BTEST"},
    {kw_bye,3,eFN_NOT,"BYE"},
    {kw_case,4,eFN_NOT,"CASE"},
    {kw_chr,4,eFN_STR,"CHR$"},
    {kw_circle,6,eFN_NOT,"CIRCLE"},
    {clear,5,eFN_NOT,"CLEAR"},
    {kw_close,5,eFN_NOT,"CLOSE"},
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
    {kw_eof,3,eFN_NOT,"EOF"},
    {kw_exist,5,eFN_INT,"EXIST"},
    {kw_exit,4,eFN_NOT,"EXIT"},
    {kw_fgetc,5,eFN_INT,"FGETC"},
    {kw_fill,4,eFN_NOT,"FILL"},
    {kw_for,3,eFN_NOT,"FOR"},
    {kw_free,4,eFN_NOT,"FREE"},
    {kw_func,4,eFN_NOT,"FUNC"},
    {kw_hex,4,eFN_STR,"HEX$"},
    {kw_getpixel,4,eFN_INT,"PGET"},
    {kw_if,2,eFN_NOT,"IF"},
    {kw_input,5,eFN_NOT,"INPUT"},
    {kw_instr,5,eFN_INT,"INSTR"},
    {kw_insert,7,eFN_STR,"INSERT$"},
    {kw_insert_line,8,eFN_NOT,"INSERTLN"},
    {kw_invert_video,9,eFN_NOT,"INVERTVID"},
    {kw_key,3,eFN_INT,"KEY"},
    {kw_lcase,6,eFN_STR,"LCASE$"},
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
    {kw_open,4,eFN_NOT,"OPEN"},
    {bad_syntax,2,eFN_NOT,"OR"},
    {bad_syntax,6,eFN_NOT,"OUTPUT"},
    {kw_peek,4,eFN_INT,"PEEK"},
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
    {kw_seek,4,eFN_NOT,"SEEK"},
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
    {kw_ucase,6,eFN_STR,"UCASE$"},
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
//le dictionnaire système
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

// messages d'erreurs correspondants à l'enumération BASIC_ERROR_CODES dans BASIC.h
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
    "File already open",
    "File not opened",
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

// name est-til un type de donnée valide?
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

// allocation de l'espace pour une chaîne asciiz sur le heap;
// length est la longueur de la chaine.
// réserve un octet avant le début de la chaîne
// pour le compte des références
// string_free() doit-être utilisé  pour libérer des chaînes.
char* string_alloc(unsigned length){
    void *dstr;
    char *str;
    dstr=malloc(length+2);
    if (!dstr){
        throw(eERR_STR_ALLOC);
    }
    *(uint8_t*)dstr=0;
    str=(char*)dstr+1;
    *str=0; // chaîne de longueur zéro
    return str;
}

// libération de l'espace réservée pour une chaîne asciiz sur le heap.
// Il doit s'agir d'une chaîne allouée avec string_alloc().
// La chaîne n'est libéré que si le compte des référence est zéro.
// Sinon le compte est simplement décrémenté.
// les chaînes imbriquées dans le code ont un ref_count==128,
// ce ref_count n'est jamais modifié.
void string_free(char *str){
    uint8_t *dstr;
    uint8_t ref_count;
    if (!str) return;
    
    dstr=(uint8_t*)str-1;
    ref_count=*dstr;
    if (!ref_count){
        free(dstr);
    }else if (ref_count<128){
        ref_count--;
        *dstr=ref_count;
    }
}


void free_string_vars(){
    var_t *var;
    void *limit;
    char **str_array;
    int i,count;
    char *dstr;
    
    limit=progspace+prog_size;
    var=varlist;
    while (var){
        if ((var->vtype==eVAR_STR) && !(var->local)){
            if (!var->array){
                dstr=var->str;
                if (dstr){
                    *((uint8_t*)dstr-1)=0;
                    string_free(dstr);
                }
            }else{
                str_array=(char**)var->adr;
                count=*(unsigned*)str_array;
                for (i=1;i<=count;i++){
                    dstr=(void*)str_array[i];
                    if (dstr){
                        *((uint8_t*)dstr-1)=0;
                        string_free(dstr);
                    }
                }
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

// retourne le type de la variable définie
// par le dernier caractère de son nom.
// '#'  eVAR_BYTE
// '$'  eVAR_STR
// '!'  eVAR_FLOAT
//  autre eVAR_INT
static int var_type_from_name(char *name){
    int vtype;
    
    switch (name[strlen(name)-1]){
        case '#':
            vtype=eVAR_BYTE;
            break;
        case '$':
            vtype=eVAR_STR;
            break;
        case '!':
            vtype=eVAR_FLOAT;
            break;
        default:
            vtype=eVAR_INT;
    }
    return vtype;
}

// les variables sont allouées à la fin
// de progspace en allant vers le bas.
// lorsque endmark<=&progrspace[dptr] la mémoire est pleine
static var_t *var_create(char *name,int vtype,void *value){
    int len;
    void *newend;
    var_t *newvar=NULL;
    void *varname=NULL;
    
    len=strlen(name);
    newvar=var_search(name);
    if ((!var_local && newvar)||(var_local && newvar && newvar->local)){
        throw(eERR_DUPLICATE);
    }
    if (vtype==-1){
        vtype=var_type_from_name(name);
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
    newvar->vtype=vtype;
    if (var_local){
        newvar->local=true;
        newvar->n=*(int*)value;
    }else{
        switch (newvar->vtype){
            case eVAR_STR:
                if (value){
                    newvar->str=string_alloc(strlen(value));
                    strcpy(newvar->str,value);
                    *(newvar->str-1)=1; // reference count
                }
                else{
                    newvar->str=NULL;
                }
                break;
            case eVAR_BYTE:
                if (value){
                    newvar->byte=*((uint8_t*)value);
                }else{
                    newvar->byte=0;
                }
                break;
            case eVAR_INT:
                if (value){
                    newvar->n=*((int*)value);
                }else{
                    newvar->n=0;
                }
                break;
            case eVAR_FLOAT:
                if (value){
                    newvar->f=*((float*)value);
                }else{
                    newvar->f=0.0;
                }
                break;
            case eVAR_FUNC:
            case eVAR_SUB:
                newvar->adr=value;
                break;
        }
//        if (newvar->vtype==eVAR_STR){ // variable chaine
//        }else if (newvar->vtype=eVAR_BYTE){ // variable octet
//        }else{// entier 32 bits
//        }
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
// qui retourne l'adresse d'une chaîne.
static bool string_func(var_t *var){
    return (var->vtype==eVAR_FUNC) && (var->name[strlen(var->name)-1]=='$');
}

static void bytecode(uint8_t bc){
    if ((void*)&progspace[dptr]>=endmark) throw(eERR_PROGSPACE);
    progspace[dptr++]=bc;
}//f

// compile un literal 32 bits
static void code_lit32(uint32_t u){
    int i;
    
    bytecode(ILIT);
    for (i=0;i<4;i++){
        bytecode(u&0xff);
        u>>=8;
    }
}

// compile le contenu de la variable n
static void code_value(void *p){
    int i;
    uint32_t u;
    u=*(uint32_t*)p; // print_hex(2,u,0);
    bytecode(ILIT); 
    for (i=0;i<4;i++){
        bytecode(u&0xff); //print_hex(2,u&0xff,0);
        u>>=8;
    }
}

// compile l'adresse du pointeur
static void code_ptr(void *ptr){
    int i;
    uint32_t addr;
    
    addr=(uint32_t)&ptr;
    bytecode(ILIT);
    for (i=0;i<4;i++){
        bytecode(addr&0xff);
        addr>>=8;
    }
}


//calcul la distance entre slot_address+1 et dptr.
// le déplacement maximal authorizé est de 32767 octets.
// la valeur du déplacement est déposée dans la fente 'slot_address'.
static void patch_fore_jump(int slot_addr){
    int displacement;
    
    displacement=dptr-slot_addr-2;
    if (displacement>32767){throw(eERR_JUMP);}
    progspace[slot_addr++]=_byte0(displacement);
    progspace[slot_addr]=_byte1(displacement);
}//f

//calcul la distance entre dptr+1 et target_address
//le déplacement maximal authorizé est de -32768 octets.
// la valeur du déplacement est déposé à la position 'dptr'
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

// vérifie les limites de ctrl_stack
static void ctrl_stack_check(){
    if (csp<0){
        throw(eERR_CSTACK_UNDER);
    }else if (csp==CTRL_STACK_SIZE){
        throw(eERR_CSTACK_OVER);
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
    while (((c=reader_getc(activ_reader))==' ') || (c=='\t'));
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
        if (!(isalnum(c) || c=='_'  || c=='$' || c=='#' || c=='!')){
            reader_ungetc(activ_reader);
            break;
        }
        token.str[i++]=toupper(c);
        if (c=='$' || c=='#' || c=='!') break;
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
    token.id=eINT;
    token.n=n;
}//f()

static const char NBR_CHARS[]="0123456789.eE-+";

static void parse_number(){
    char c;
    int i=0;
    bool leave=false,decimal=false,exponent=false,esign=false;
    c=reader_getc(activ_reader);
    while (strchr(NBR_CHARS,c) && !leave){
        switch (c){
            case '-':
            case '+':
                if (!exponent){
                    leave=true;
                    continue;
                }else{
                    if (!esign){
                        pad[i++]=c;
                        esign=true;
                    }else{
                        leave=true;
                    }
                }
                break;
            case 'e':
            case 'E':
                if (exponent){
                    leave=true;
                    continue;
                }else{
                    pad[i++]=c;
                    exponent=true;
                }
                break;
            case '.':
                if (decimal){
                    leave=true;
                    continue;
                }else{
                    pad[i++]=c;
                    decimal=true;
                }
                break;
            default:
                pad[i++]=c;
        }
        c=reader_getc(activ_reader);
    }
    if (!activ_reader->eof){reader_ungetc(activ_reader);}
    pad[i]=0;
    if (decimal || exponent){
        token.id=eFLOAT;
        token.f=strtof(pad,NULL);
    }else{
        token.id=eINT;
        token.n=atoi(pad);
    }
    
}

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
        parse_number();
    }else{
        switch(c){
            case '$': // nombre hexadecimal
                parse_integer(16);
                break;
            case '&': // nombre binaire
                parse_integer(2);
                break;
            case '#': // numéro de fichier
                token.id=eFILENO;
                c=reader_getc(activ_reader);
                if (!isdigit(c)){throw(eERR_SYNTAX);}
                token.n=c-'0';
                if (!(token.n>0 && token.n<=MAX_FILES)){throw(eERR_SYNTAX);}
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
                token.kw=eKW_PRINT;
                token.str[0]='?';
                token.str[1]=0;
                break;
            case '@': // référence
                token.id=eKWORD;
                token.kw=eKW_REF;
                token.str[0]='@';
                token.str[1]=0;
                break;
            case '\n':
            case '\r':
                line_count++;
                if (complevel){
                    next_token();
                }else{
                    token.id=eNL;
                    token.str[0]=0;
                }
                break;
            case ':':    
                token.id=eCOLON;
                token.str[0]=':';
                token.str[1]=0;
                break;
        }//switch(c)
    }//if
}//next_token()

// Pour les commandes qui n'utilise aucun arguments
// les parenthèses sont optionnelles.
static void optional_parens(){
    next_token();
    if (token.id==eLPAREN){
        expect(eRPAREN);
    }else{
        unget_token=true;
    }
}

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

// compile le calcul d'indice dans les variables vecteur
static void code_array_address(var_t *var){
    int factor_type;
    //obtien l'index.
    factor_type=factor();
    if (factor_type!=eVAR_INT){throw(eERR_SYNTAX);}
    // limite l'index de 1 à ubound()
    _litc(1);
    bytecode(IMAX);
    code_lit32(_addr(var->adr));
    bytecode(IFETCH);
    bytecode(IMIN);
    if (var->vtype==eVAR_INT || var->vtype==eVAR_FLOAT || var->vtype==eVAR_STR){
        _litc(2);
        bytecode(ILSHIFT);//4*index
    }else{
        // élément 0,1,2,3  réservé pour size
        // index 1 correspond à array[4]
        bytecode(IDUP);
        bytecode(IQBRAZ);
        cpush(dptr);
        dptr+=2;        
        _litc(3);
        bytecode(IPLUS);
        patch_fore_jump(cpop());
    }
    //index dans le tableau
    code_lit32(_addr(var->adr));
    bytecode(IPLUS);
}//f

static void code_var_address(var_t *var){
    if (var->array){
        code_array_address(var);
    }else{
        if (var->local){
            bytecode(ILCADR);
            bytecode((uint8_t)var->n);
        }else{ //println(2,"code_lit32 ((void*)&var->adr)");
            code_lit32(_addr(&var->adr));
        }
    }
}

//vérifie si le dernier token est une variable chaîne ou une fonction chaîne.
// compile l'adresse de la chaîne si c'est le cas et retourne vrai.
// sinon retourne faux.
static bool try_string_var(){
    var_t *var;
    int i;
    
    var=var_search(token.str);
    if (var && (var->vtype==eVAR_STR)){
        unget_token=true;
        string_expression();
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
            case eFILENO:
                _litc(token.n);
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

// si type2 différent de type1 convertie type2 en type1
// une expression numérique n'a que 2 types: eVAR_INT ou eVAR_FLOAT
static void convert_term_type(int type1, int type2){
    if (type1!=type2){
        if (type2==eVAR_INT){
            bytecode(II2FLOAT);
        }else{
            bytecode(IF2INT);
        }
    }
}

// retourne le type du facteur. i.e. entier ou float
static int factor(){
    var_t *var;
    int i,op=eNONE;
    int factor_type;
    if (try_addop()){
        op=token.id;
        unget_token=false;
    }
    next_token();
    while (token.id==eNL){next_token();}
    switch(token.id){
        case eKWORD:
            if ((KEYWORD[token.kw].fntype==eFN_INT)){ 
                KEYWORD[token.kw].cfn();
                factor_type=eVAR_INT;
            }else if ((KEYWORD[token.kw].fntype==eFN_INT)){  
                KEYWORD[token.kw].cfn();
                factor_type=eVAR_FLOAT;
            }else{
                throw(eERR_SYNTAX);
            }
            break;
        case eIDENT:
            if ((var=var_search(token.str))){
                factor_type=var->vtype;
                switch(var->vtype){ 
                    case eVAR_FUNC:
                        _litc(0);
                        parse_arg_list(var->dim);
                        code_lit32(_addr(&var->adr));
                        bytecode(IFETCH);
                        bytecode(ICALL);
                        if (var->name[strlen(var->name)-1]=='!'){
                            factor_type=eVAR_FLOAT;
                        }else{
                            factor_type=eVAR_INT;
                        }
                        break;
                    case eVAR_BYTE:
                        code_var_address(var);
                        bytecode(ICFETCH);
                        factor_type=eVAR_INT;
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
            factor_type=eVAR_INT;
            _litc(token.n&127);
            break;
        case eINT:
            factor_type=eVAR_INT; // println(2,"factor_type==eVAR_INT");
            if (token.n>=0 && token.n<256){
                _litc((uint8_t)token.n);
            }else{
                code_lit32(token.n);
            }
            break;
        case eFLOAT:
            factor_type=eVAR_FLOAT; // println(2,"factor_type==eVAR_FLOAT");
            code_lit32(_value(&token.f));
            break;
        case eLPAREN:
            factor_type=expression();
            expect(eRPAREN);
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
    if (op==eMINUS){
        if (factor_type==eVAR_INT){ 
            bytecode(INEG);
        }else{
            bytecode(IFNEG);
        }
    } 
    return factor_type;
}//f()

// retourne le type du terme. i.e. entier ou float
static int term(){
    int op;
    int f1_type,f2_type;
    //le type du premier facteur détermine le type du terme.
    f1_type=factor();
    while (try_mulop()){
        op=token.id;
        f2_type=factor();
        convert_term_type(f1_type,f2_type);
        switch(op){
            case eMUL:
                if (f1_type==eVAR_INT){
                    bytecode(ISTAR);
                }else{
                    bytecode(IFMUL);
                }
                break;
            case eDIV:
                if (f1_type==eVAR_INT){
                    bytecode(ISLASH);
                }else{
                    bytecode(IFDIV);
                }
                break;
            case eMOD:
                if (f1_type==eVAR_INT){
                    bytecode(IMOD);
                }else{
                    throw(eERR_SYNTAX);
                }
                break;
        }//switch
    }
    return f1_type;
}//f 

// retourne le type de l'expression. i.e. entier ou float
static int expression(){
    int mark_dp=dptr;
    int op=eNONE;
    int expr_type, term_type;
    
    expr_type=term(); // le type du permier terme détermine le type de l'expression
    while (try_addop()){
        op=token.id;
        term_type=term();
        convert_term_type(expr_type,term_type);
        if (op==ePLUS){
            if (expr_type==eVAR_INT){
                bytecode(IPLUS);
            }else{
                bytecode(IFADD);
            }
        }else{
            if (expr_type==eVAR_INT){
                bytecode(ISUB);
            }else{
                bytecode(IFSUB);
            }
        }
    }
    if (dptr==mark_dp) throw(eERR_SYNTAX);
    return expr_type;
}//f()

// les relation se font entre nombre entiers
// les 2 termes de la comparaison sont convertis en entier
// avant comparaison.
static void condition(){
    int rel;
    int expr_type;
    //print(con,"condition\n");
    expr_type=expression();
    if (expr_type==eVAR_FLOAT){
        bytecode(IF2INT);
    }
    if (!try_relation()){
        return;
    }
    rel=token.id;
    expr_type=expression();
    if (expr_type==eVAR_FLOAT){
        bytecode(IF2INT);
    }
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
    int op_rel;
    string_expression();
    next_token(); // attend un opérateur relationnel.
    if (!((token.id>=eEQUAL)&&(token.id<=eLE))){throw(eERR_SYNTAX);}
    op_rel=token.id;
    string_expression();
    bytecode(ISTRCMP);
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
    unget_token=true;
    switch (token.id){
        case eSTRING:
            string_compare();
            return true;
        case eIDENT:
            var=var_search(token.str);
            if (var->vtype==eVAR_STR){
                string_compare();
                return true;
            }
            break;
        case eKWORD:
            if ((KEYWORD[token.n].fntype==eFN_STR)){
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
    int fix_count=0; // pour évaluation court-circuitée.
    bool_factor();
    while (try_boolop() && token.n==eKW_AND){
        //code pour évaluation court-circuitée
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


// compile les expressions booléenne
static void bool_expression(){// print(con,"bool_expression()\n");
    int fix_count=0; // pour évaluation court-circuitée.
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

static void compiler_msg(int msg_id,const char *detail){
    char msg[CHAR_PER_LINE];
    strcpy(msg,compile_msg[msg_id]);
    print(con,msg);
    print(con,detail);
    crlf(con);
}//f

/*************************
 * commandes BASIC
 *************************/


static int try_type_cast(){
    int vtype;
    
    next_token();
    if ((token.id==eKWORD) && (token.kw==eKW_AS)){
        next_token();
        if ((token.id==eIDENT) && (vtype=search_type(token.str))>-1){
            return vtype;
        }else{
            throw(eERR_SYNTAX);
        }
    }else{
        unget_token=true;
        return -1;
    }
}



// '(' number {','number}')'
static void init_int_array(var_t *var){
    int *iarray=NULL;
    unsigned char *barray=NULL;
    int size,count=1,state=0;
    int rparen=0;
    
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
    while (!rparen){
        switch (token.id){
            case eCHAR:
            case eINT: 
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
                rparen=1;
                continue;
                break;
            case eNL:
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
    int rparen=0;
    
    array=var->adr;
    size=array[0];
    expect(eLPAREN);
    next_token();
    while(!rparen){
        switch (token.id){
            case eSTRING: 
                if (state) throw(eERR_SYNTAX);
                newstr=string_alloc(strlen(token.str));
                strcpy(newstr,token.str);
                array[count++]=(uint32_t)newstr;
                *(newstr-1)=1; // reference count
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
                rparen=1;
                continue;
                break;
            case eNL:
                break;
            default:
                throw(eERR_SYNTAX);
        }
        next_token();
    }
    if (count<size) throw(eERR_MISSING_ARG);
    if (count>(size+1)) throw(eERR_EXTRA_ARG);
}//f

static void *alloc_array_space(int size, int vtype){
    void *array;
    unsigned space;

    if (vtype==eVAR_BYTE){ //table d'octets
        space=sizeof(uint8_t)*size+sizeof(int);
    }else{ // table d'entiers ou de chaînes
        space=sizeof(int)*(size+1);
    }
    array=alloc_var_space(space);
    memset(array,0,space);
    *((uint32_t*)array)=size;
    return array;
}

// récupère la taille du tableau et initialise si '='
static void dim_array(char *var_name){
    int size=0,len,vtype;
    void *array;
    var_t *new_var, *var;
    
    len=strlen(var_name);
    next_token();
    if (token.id==eINT){
        size=token.n;
    }else if (token.id==eIDENT){
        var=var_search(token.str);
        if (var && !var->array){
            switch (var->vtype){
                case eVAR_INT:
                case eVAR_BYTE:
                case eVAR_FLOAT:
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
    vtype=try_type_cast();
    if (vtype==-1){
        vtype=var_type_from_name(var_name);
    }
    array=alloc_array_space(size,vtype);
    new_var=var_create(var_name,vtype,NULL);
    new_var->array=true;
    new_var->dim=1;
    new_var->adr=array;
    next_token();
    if (token.id==eEQUAL){
        complevel++;
        if (new_var->vtype==eVAR_STR){
            init_str_array(new_var);
        }else{
            init_int_array(new_var);
        }
        complevel--;
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
    int vtype;
    
    if (var_local) throw(eERR_DIM_FORBID);
    expect(eIDENT);
    while (token.id==eIDENT){
        if (var_search(token.str)) throw(eERR_DUPLICATE);
        strcpy(var_name,token.str);
        vtype=try_type_cast();
        next_token();
        switch(token.id){
            case eLPAREN:
                if (vtype>-1){
                    throw(eERR_SYNTAX);
                }
                dim_array(var_name);
                break;
            case eEQUAL: 
                new_var=var_create(var_name,vtype,NULL);
                if (new_var->vtype==eVAR_STR){
                    string_expression();
                    code_lit32(_addr(&new_var->adr));
                    bytecode(ISTRSTORE);
                }else{
                    expression();
                    code_lit32(_addr(&new_var->n));
                    bytecode(ISTORE);
                }
                break;
            default:
                new_var=var_create(var_name,vtype,NULL);
                unget_token=true;
                break;

        }//switch
        next_token();
        if (token.id==eCOMMA){
            next_token();
        }else if (token.id==eIDENT){
            throw(eERR_SYNTAX);
        }
    }//while
    unget_token=true;
}//f

// compte le nombre d'arguments de la FUNC|SUB
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
    var=var_create(token.str,var_type,(void*)undefined_sub);
    var->dim=count_arg();
}


//passe une variable par référence
static void kw_ref(){
    var_t *var;
    
    next_token();
    if (token.id!=eIDENT) throw(eERR_BAD_ARG);
    var=var_search(token.str);
    if (!var) throw(eERR_BAD_ARG);
    if (var->array){
        code_lit32(_addr((void*)var->adr+sizeof(int)));
    }else{
        switch(var->vtype){
            case eVAR_INT:
            case eVAR_BYTE:
            case eVAR_FLOAT:
                code_lit32(_addr(&var->n));
                break;
            case eVAR_STR:
                if (var->ro){
                    throw(eERR_BAD_ARG);
                }
            case eVAR_FUNC:
            case eVAR_SUB:
                code_lit32(_addr(var->adr));
                break;
            default:
                throw(eERR_BAD_ARG);
        }//switch
    }
}//f

// UBOUND(var_name)
// retourne la taille du tableau
static void kw_ubound(){
    var_t *var;
    char name[32];
    expect(eLPAREN);
    expect(eIDENT);
    strcpy(name,token.str);
    expect(eRPAREN);
    var=var_search(name);
    if (!(var && var->array)) throw(eERR_BAD_ARG);
    code_lit32(_addr(var->adr));
    bytecode(IFETCH);
}//f

//USE fichier
// compilation d'un fichier source inclut dans un autre fichier source.
static void kw_use(){
    reader_t *old_reader, freader;
    FIL *fh;
    int fhandle;
    uint32_t lcount;
    
    if (activ_reader->device==eDEV_KBD) throw(eERR_SYNTAX);
    expect(eSTRING);
    uppercase(token.str);
    if ((fh=open_file(token.str,FA_READ))){
        reader_init(&freader,eDEV_SDCARD,fh);
        old_reader=activ_reader;
        activ_reader=&freader;
        compiler_msg(COMPILING, token.str);
        lcount=line_count;
        line_count=1;
        compile();
        line_count=lcount+1;
        close_file(fh);
        activ_reader=old_reader;
        compiler_msg(COMP_END,NULL);
    }else{
        throw(eERR_FILE_IO);
    }
}//f

//CURLINE()
// retourne la position ligne du curseur texte
static void kw_curline(){
    optional_parens();
    bytecode(ICURLINE);
}//f

//CURCOL()
// retourne la position colonne du curseur texte
static void kw_curcol(){
    optional_parens();
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
    
    if (var_local) throw(eERR_CONST_FORBID);
    expect(eIDENT);
    while (token.id==eIDENT){
        strcpy(name,token.str);
        if ((var=var_search(name))) throw(eERR_REDEF);
        expect(eEQUAL);
        var=var_create(name,-1,NULL);
        var->ro=true;
        if (var->vtype==eVAR_STR){
            expect(eSTRING);
            var->str=string_alloc(strlen(token.str));
            strcpy(var->str,token.str);
            var->vtype=eVAR_STR;
            *(var->str-1)=1; // compte référence.
        }else{
            expect(eINT);
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

//PEEK(adresse)
//retourne l'entier 32 bits contenu à l'adresse
// adresse doit-être alignée sinon ça plante.
static void kw_peek(){
    parse_arg_list(1);
    bytecode(IFETCH);
}

//BEEP
//produit un son à la fréquence de 1Khz pour 20msec.
static void kw_beep(){
    bytecode(IBEEP);
}

//SOUND(freq,msec)
//fait entendre une fréquence quelconque. Attend la fin de la note.
static void kw_sound(){
    parse_arg_list(2);
    bytecode(ISOUND);
}//f

//TUNE(@array)
// fait entendre une mélodie
static void kw_tune(){
    parse_arg_list(1);
    bytecode(ITUNE);
}

//PLAY(string|var$)
//joue une mélodie contenue dans un chaîne.
static void kw_play(){
    parse_arg_list(1);
    bytecode(IPLAY);
}

//SLEEP(msec)
// suspend exécution
// argument en millisecondes
static void kw_sleep(){
    parse_arg_list(1);
    bytecode(ISLEEP);
}//f

//TICKS()
//retourne la valeur du compteur
// de millisecondes du système
static void kw_ticks(){
    optional_parens();
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
        print(con,error_msg[eERR_NONE]);
        exit_basic=true;
    }else{
        if (complevel){
            bytecode(IDP0);
        }
        bytecode(IBYE);
    }
}//f

// RETURN expression
// utilisé dans les fonctions
static void kw_return(){
    expression();
    bytecode(ILCSTORE);
    bytecode(0);
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
//    bytecode(FRAME_SUB);
}//f

// CLS[()]
// efface écran
static void kw_cls(){
    optional_parens();
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

//déplace le code des sous-routine
//à la fin de l'espace libre
static void movecode(var_t *var){
    uint32_t size;
    void *pos, *adr;
    
    pos=var->adr;
    size=(uint32_t)&progspace[dptr]-(uint32_t)pos;
    if (size&3){ size=(size+4)&0xfffffffc;}
    adr=endmark-size;
    memmove(adr,pos,size);
    endmark=adr;
    dptr=(uint8_t*)pos-(uint8_t*)progspace;
    memset(&progspace[dptr],0,size);
    var->adr=adr; 

#ifdef DEBUG
    {
        uint8_t * bc;   
        crlf(con);
        print_hex(con,(uint32_t)var->adr,0);
        print(con,var->name);
        put_char(con,':');
        for(bc=(uint8_t*)adr;bc<((uint8_t*)adr+size);bc++){
            print_int(con,*bc,0);
        }
        print_int(con,*bc,0);
        print(con,"\nsize: "); 
        print_int(con,size,0);
        crlf(con);
    }
#endif

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
    if (token.id==eNL) throw(eERR_SYNTAX);
    unget_token=true; 
    while (token.id != eNL){
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
    _ctop()++; //incrémente le nombre de clause CASE
    cpush(dptr-2); // fente pour le IBRA de ce case (i.e. saut après le END SELECT)
    ctrl_stack_nrot();
    
}//f

static void patch_previous_case(){
    uint32_t adr;
    
    ctrl_stack_rot();
    adr=cpop();
    bytecode(IBRA);  // branchement à la sortie du select case pour le case précédent
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

// RND[()]
// génération nombre pseudo-aléatoire
static void kw_rnd(){
    optional_parens();
    bytecode(IRND);
}//f

// exécute le code BASIC compilé
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
    FIL *fh;
    
    if ((fh=open_file(file_name,FA_READ))){
        reader_init(&file_reader,eDEV_SDCARD,fh);
        activ_reader=&file_reader;
        compiler_msg(COMPILING,file_name);
        line_count=1;
        compile();
        close_file(fh);
        activ_reader=&std_reader;
        program_loaded=true;
        program_end=dptr;
        run_it=true;
        compiler_msg(COMP_END,NULL);
    }else{
        throw(eERR_FILE_IO);
    }
}//f

// compile et exécute un fichier BASIC.
static void run_file(const char *file_name){
    clear();
    compile_file(file_name);
    exec_basic();
    activ_reader=&std_reader;
}


//RUN "file_name"
// commande pour exécuter un fichier basic
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
        case eNL:
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

//RANDOMIZE[()]
// initialise le générateur pseudo-hasard.
static void kw_randomize(){
    optional_parens();
    bytecode(IRANDOMIZE);
}//f

//MAX(expr1,expr2)
static void kw_max(){
    parse_arg_list(2);
    bytecode(IMAX);
}//f

// MDIV(n1,n2,n3)
// retourne n1*n2/n3
// le produit est conservé en double précision.
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
// inverse le pixel à la position {x,y}
static void kw_xorpixel(){
    parse_arg_list(2);
    bytecode(IPXOR);
}//f

// SCRLUP[()]
// glisse l'écran vers le haut d'une ligne
static void kw_scrlup(){
    optional_parens();
    bytecode(ISCRLUP);
}//f

// SCRLDN[()]
// glisse l'écran vers le bas d'une lignes
static void kw_scrldown(){
    optional_parens();
    bytecode(ISCRLDN);
    
}//f

//INSERTLN[()]
static void kw_insert_line(){
    optional_parens();
    bytecode(IINSERTLN);
}

/***********************/
/*  fonctions fichiers */
/***********************/

// CLOSE  ferme tous les fichiers ouverts
//CLOSE(#n) ferme le fichier numéro 'n'
static void kw_close(){
    next_token();
    if (token.id==eLPAREN){
        unget_token=true;
        parse_arg_list(1);
        bytecode(IFCLOSE);
    }else{
        bytecode(IFCLOSEALL);
    }
}

// EOF(#n)
// retourne vrai si le fichier 'n' est à la fin.
static void kw_eof(){
    parse_arg_list(1);
    bytecode(IEOF);
}

// EXIST(name$)
// vérifie si un fichier existe
static void kw_exist(){
    parse_arg_list(1);
    bytecode(IEXIST);
}


// OPEN string_term FOR INPUT|OUTPUT|APPEND AS #n
// ouvre le fichier nommé 'file_name' en mode 'mode' avec l'identifiant 'n'
// 'n' est dans l'intervalle 1..5
// le programme s'arrête si le fichier ne peut-être ouvert.
static void kw_open(){
    char *fname;
    PF_BYTE mode;

    string_term();
    expect(eKWORD);
    if (token.kw!=eKW_FOR){throw(eERR_SYNTAX);}
    expect(eKWORD);
    switch (token.kw){
        case eKW_INPUT:
            mode=FA_READ;
            break;
        case eKW_FILE_OUTPUT:
            mode=FA_CREATE_ALWAYS|FA_WRITE;
            break;
        case eKW_FILE_APPEND:
            mode=FA_OPEN_ALWAYS|FA_WRITE;
            break;
        default:
            throw(eERR_SYNTAX);
    }
    _litc(mode);
    expect(eKWORD);
    if (token.kw!=eKW_AS){throw(eERR_SYNTAX);}
    expect(eFILENO);
    _litc(token.n);
    bytecode(IFOPEN);
    if (mode==(FA_OPEN_ALWAYS|FA_WRITE)){
        _litc(token.n);
        code_lit32(-1);
        bytecode(ISEEK);
    }
}

// SEEK(#n,pos)
// position le curseur du fichier 'n' à la position 'pos'
static void kw_seek(){
    parse_arg_list(2);
    bytecode(ISEEK);
}


// FGETC(#n)
// lit un caractère d'un fichier
static void kw_fgetc(){
    parse_arg_list(1);
    bytecode(IGETC);
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

//POLYGON(@points,n)
//dessine un polygon à partir d'un tableau de points.
void kw_polygon(){
    parse_arg_list(2);
    bytecode(IPOLYGON);
}
//FILL(x,y)
//remplissage d'une figure géométirique fermée.
void kw_fill(){
    parse_arg_list(2);
    bytecode(IFILL);
}

// SPRITE(x,y,width,height,@sprite)
// applique le sprite à la position désigné
// il s'agit d'une opération xor
static void kw_sprite(){
    parse_arg_list(5);
    bytecode(ISPRITE);
}//f

// VGACLS[()]
// commande pour effacer l'écran graphique, indépendamment de la console active.
static void kw_vgacls(){
    optional_parens();
    bytecode(IVGACLS);
}

// FREE[()]
// commande qui affiche la mémoire programme et chaîne disponible
static void kw_free(){
    char fmt[64];
    
    optional_parens();
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
    int var_type;
    char name[32];
    
    bytecode(IFORSAVE);
    expect(eIDENT);
    strcpy(name,token.str);
    if ((var=var_search(name))){
        var_type=var->vtype;
    }else{
        var_type=var_type_from_name(name);
    }
    if (!(var_type==eVAR_INT || var_type==eVAR_BYTE)){throw(eERR_SYNTAX);}
    complevel++;
    unget_token=true;
    kw_let(); // valeur initale de la variable de boucle
    expect(eIDENT);
    if (strcmp(token.str,"TO")) throw (eERR_SYNTAX);
    if (expression()==eVAR_FLOAT){throw(eERR_SYNTAX);} // valeur de la limite
    next_token();
    if (token.id==eIDENT && !strcmp(token.str,"STEP")){
       if (expression()==eVAR_FLOAT){throw(eERR_SYNTAX);} //valeur de l'incrément
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
    cpush(dptr); // fente déplacement après boucle FOR...NEXT
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

// compile une chaîne litérale
static void literal_string(char *lit_str){
    int size;
    
    size=strlen(lit_str)+2;
    if ((void*)&progspace[dptr+size]>endmark) throw(eERR_PROGSPACE);
    strcpy((void*)&progspace[dptr+1],lit_str);
    progspace[dptr]=128;
    dptr+=size;
}//f


//SRLOAD(addr,file_name)
//charge un fichier dans la SPI RAM
// retourne la grandeur en octet
static void kw_srload(){
    parse_arg_list(2);
    bytecode(ISRLOAD);
}//f

//SRSAVE(addr,file_name,size)
//sauvegarde un bloc de SPI RAM dans un fichier
static void kw_srsave(){
    parse_arg_list(3);
    bytecode(ISRSAVE);
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
    if (!var || !var->array) throw(eERR_NOT_ARRAY);
    code_array_address(var);
    expect(eEQUAL);
    switch (var->vtype){
        case eVAR_STR:
            expect(eSTRING);
            adr=string_alloc(strlen(token.str));
            strcpy(adr,token.str);
            code_lit32(_addr(adr));
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_INT:
            if (expression()!=eVAR_INT){
                bytecode(IF2INT);
            }
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_BYTE:
            if (expression()!=eVAR_INT){
                bytecode(IF2INT);
            }
            bytecode(ISWAP);
            bytecode(ICSTORE);
            break;
        case eVAR_FLOAT:
            if (expression()!=eVAR_FLOAT){
                bytecode(II2FLOAT);
            }
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
}//f

// compile le code pour le stockage d'untier
//static void store_integer(var_t *var){
//    if (var->ro){
//        throw(eERR_REDEF);
//    }
//    code_var_address(var);
//    switch(var->vtype){
//        case eVAR_INT:
//            bytecode(ISTORE);
//            break;
//        case eVAR_BYTE:
//            bytecode(ICSTORE);
//            break;
//        case eVAR_FLOAT:
//            
//        default:
//            throw (eERR_BAD_ARG);
//    }//switch
//}//f

// KEY[()]
//attend une touche du clavier
// retourne la valeur ASCII
static void kw_waitkey(){
    optional_parens();
    bytecode(IKEY);
}//f

// TKEY()
// vérifie si une touche clavier est disponible
static void kw_tkey(){
    optional_parens();
    bytecode(IQRX);
}

//LEN(var$|string)
static void kw_len(){
    var_t *var;

    parse_arg_list(1);
    bytecode(ILEN);
}//f

// compile le code pour la saisie d'un entier
static void compile_input(int file_no){
    if (file_no){
        _litc(file_no);
        bytecode(IREADFIELD);
        bytecode(IDUP);
        bytecode(ILEN);
    }else{
        _litc('?');
        bytecode(IEMIT);
        code_lit32(_addr(pad)); // ( var_addr -- var_addr pad* )
        _litc(PAD_SIZE); // ( var_addr pad* -- var_addr pad* buf_size )
        bytecode(IREADLN); // ( var_addr pad* buf_size  -- var_adr pad* str_size )
    }
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
    if (!var) var=var_create(name,-1,NULL);
    code_var_address(var);
    return var;
}

// INPUT [string ','] identifier (',' identifier)
// saisie au clavier de
// valeur de variables
static void kw_input(){
    char name[32];
    var_t *var;
    int file_no=0;
    
    next_token();
    switch (token.id){
        case eFILENO:
            file_no=token.n;
            expect(eCOMMA);
            expect(eIDENT);
            break;
        case eSTRING:
            bytecode(IPSTR);
            literal_string(token.str);
            bytecode(ICR);
            expect(eCOMMA);
            expect(eIDENT);
            break;
        case eIDENT:
            break;
        default:
           throw(eERR_BAD_ARG); 
    }//switch
    while (token.id==eIDENT){
        strcpy(name,token.str);
        var=var_accept(name); // ( -- var_addr )
        switch (var->vtype){ 
            case eVAR_STR:
                bytecode(IDUP);
                bytecode(IFETCH);
                bytecode(ISTRFREE); 
                compile_input(file_no); // (var_addr -- var_addr pad_addr)
                bytecode(ISTRALLOC);// ( var_addr pad_addr n -- var_addr pad_addr str_addr)  
                bytecode(ISTRCPY); //( var_addr pad_addr str_addr -- var_addr )
                bytecode(ISWAP);
                bytecode(ISTRSTORE);
                break;
            case eVAR_INT:
            case eVAR_BYTE:
            case eVAR_FLOAT:
                compile_input(file_no);
                bytecode(IDROP);
                if (var->vtype==eVAR_FLOAT){
                    bytecode(ISTR2F);
                }else{
                    bytecode(I2INT);
                }
                bytecode(ISWAP);
                if (var->vtype==eVAR_BYTE){
                    bytecode(ICSTORE);
                }else{
                    bytecode(ISTORE);
                }
                break;
//            case eVAR_INTARRAY:
//            case eVAR_BYTEARRAY:
//                compile_input();
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


//compile un terme chaîne
// string_term ::= chaîne_constante|variable_chaîne|fonction_chaîne.
static void string_term(){
    char *string;
    var_t *svar;
    int len;
    
    next_token();
    while (token.id==eNL){next_token();}
    switch (token.id){
        case eSTRING:
            string=string_alloc(strlen(token.str));
            strcpy(string,token.str);
            code_lit32(_addr(string));
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

//compile les expressions chaînes.
// expression_chaîne::= string_term ['+' sring_term]*
// le caractère '+' est un opérateur de concaténation
static void string_expression(){
    string_term();
    next_token();
    while (token.id==ePLUS){
        string_term();
        bytecode(IAPPEND);
        next_token();
    }
    unget_token=true;
}//string_expression()

// assigne une valeur à une variable chaîne.
static void code_let_string(var_t *var){
    bytecode(IDUP);
    bytecode(IFETCH);
    bytecode(ISWAP);
    string_expression();
    bytecode(ISWAP); 
    bytecode(ISTRSTORE);
    bytecode(IDUP);
    bytecode(IDECREF);
    bytecode(ISTRFREE);
}

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
        var=var_create(name,-1,NULL);
    }
    code_var_address(var);
    expect(eEQUAL);
    switch(var->vtype){
        case eVAR_STR:
            code_let_string(var);
            break;
        case eVAR_INT: 
            if (expression()!=eVAR_INT){
                bytecode(IF2INT);
            }
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        case eVAR_BYTE:
            if (expression()!=eVAR_INT){
                bytecode(IF2INT);
            }
            bytecode(ISWAP);
            bytecode(ICSTORE);
            break;
        case eVAR_FLOAT:
            if (expression()!=eVAR_FLOAT){
                bytecode(II2FLOAT);
            }
            bytecode(ISWAP);
            bytecode(ISTORE);
            break;
        default:
            throw(eERR_SYNTAX);
    }//switch
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
        if (//token.str[strlen(token.str)-1]=='#' || 
                token.str[strlen(token.str)-1]=='$' ) throw(eERR_BAD_ARG);
        if (globals>varlist){
            i=varlist->n+1; 
        }else{
            i=1;
        }
        var=var_create(token.str,-1,(char*)&i);
        lc++;
        next_token();
        if (token.id!=eCOMMA) break;
        next_token();
    }//while
    unget_token=true;
    bytecode(ILCSPACE);
    bytecode(lc);
}//f

static void print_string(int file_no){
    unget_token=true;
    string_expression();
    bytecode(IDUP);
    if (file_no){
        _litc(file_no);
        bytecode(ISWAP);
        bytecode(IWRITEFIELD);
    }else{
        bytecode(ITYPE);
    }
    bytecode(IDUP);
    bytecode(IGETREF);
    bytecode (IQBRA);
    cpush(dptr);
    dptr+=2;
    bytecode(ISTRFREE);
    bytecode(IBRA);
    cpush(dptr);
    dptr+=2;
    patch_fore_jump(_cnext());
    bytecode(IDROP);
    patch_fore_jump(cpop());
    cdrop();
    if (!file_no){
        _litc(1);
        bytecode(ISPACES);
    }
}

// parse une expression arithmétique et imprime le résultat.
static void print_expr(int file_no){
    int expr_type;
    expr_type=expression(); //print_int(2,expr_type,0);
    if (file_no){
        code_lit32(_addr(pad));
        if (expr_type==eVAR_FLOAT){
            bytecode(IF2STR);
        }else{
            bytecode(I2STR);
        }
        _litc(file_no);
        bytecode(ISWAP);
        bytecode(IWRITEFIELD);
    }else{
        if (expr_type==eVAR_INT){
            bytecode(IDOT);
        }else{
            code_lit32(_addr(pad));
            bytecode(IF2STR);
            bytecode(ITYPE);
        }
        _litc(1);
        bytecode(ISPACES);
    }
}

// PRINT|? [#n,] chaine|identifier|expression {,chaine|identifier|expression}

static void kw_print(){
    var_t *var;
    char *str;
    int file_no=0;
    
    next_token();
    if (token.id==eFILENO){
        file_no=token.n;
        expect(eCOMMA);
    }else{
        unget_token=true;
    }
    while (!activ_reader->eof){
        next_token();
        switch (token.id){
            case eSTRING:
                print_string(file_no);
                break;
            case eIDENT: // println(2,"eIDENT"); println(2,var->name);
                if (!(var=var_search(token.str))){throw(eERR_UNKNOWN);}
                    if ((var->vtype==eVAR_STR)||((var->vtype==eVAR_FUNC)&&
                            (var->name[strlen(var->name-1)]=='$'))){
                        print_string(file_no);
                    }else{
                        unget_token=true;
                        print_expr(file_no);
                    }
                break;
            case eKWORD:
                if (KEYWORD[token.n].fntype==eFN_STR){
                    print_string(file_no);
                }else{
                    unget_token=true;
                    print_expr(file_no);
                }
                break;
            case eNL:
                bytecode(ICR);
                break;
            default:
                unget_token=true;
                print_expr(file_no);
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

// PUTC   \c|ascii
// PUTC  #n, \c|ascii
// imprime un caractère ASCII à la console ou l'envoie dans un fichier
static void kw_putc(){
    int expr_type, file_no;
    
    next_token();
    if (token.id==eFILENO){
        _litc(token.n);
        expect(eCOMMA);
        if (expression()!=eVAR_INT){throw(eERR_BAD_ARG);}
        bytecode(IPUTC);
    }else{
        unget_token=true;
        if (expression()!=eVAR_INT){throw(eERR_BAD_ARG);}
        bytecode(IEMIT);
    }
}//f

// KEY()
// retourne indicateur touche clavier
// ou zéro
static void kw_key(){ 
    expect(eLPAREN);
    expect(eRPAREN);
    bytecode(IKEY);
}//f

// INVERTVID(0|1)
// inverse la sortie vidéo
// noir/blanc
static void kw_invert_video(){
    parse_arg_list(1);
    bytecode(IINV_VID);
}

// cré la liste des arguments pour
// la compilation des SUB|FUNC
static uint8_t create_arg_list(){
    var_t *var;
    int i=0;
    expect(eLPAREN);
    next_token();
    while (token.id==eIDENT){
        i++;
        var=var_create(token.str,-1,(char*)&i);
        next_token();
        if (token.id!=eCOMMA) break;
        next_token();
    }
    if (token.id!=eRPAREN) throw(eERR_SYNTAX);
    bytecode(IFRAME);
    bytecode(i); //nombre d'arguments de la FUNC|SUB
    return i&0x1f;
}//f

//cré une nouvelle variable de type eVAR_SUB|eVAR_FUNC
//ces variables contiennent un pointer vers le point d'entré
//du code.
static void subrtn_create(int var_type, int blockend){
    var_t *var;
    int arg_count;
    bool declared=true;
    
    if (var_local) throw(eERR_SYNTAX);
    expect(eIDENT);
    var=var_search(token.str);
    if (!var){
        var=var_create(token.str,var_type,(void*)&progspace[dptr]);
        declared=false;
    }else{
        if (var->adr==(void*)undefined_sub){
            var->adr=(void*)&progspace[dptr];
        }else{
            throw(eERR_REDEF);
        }
    }
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
// retourne le code ASCII du premier caractère de la chaîne.
static void kw_asc(){
    parse_arg_list(1);
    bytecode(IASC);
}

//CHR$(expression)
// retourne une chaîne de 1 caractère correspondant à la valeur ASCII
// de l'expression.
static void kw_chr(){
    parse_arg_list(1);
    bytecode(ICHR);
}

//STR$(expression)
//convertie une expression numérique en chaîne
static void kw_string(){
    parse_arg_list(1);
    code_lit32(_addr(pad));
    bytecode(I2STR);
}

//hex$(expression)
// convertie une expression numérique en chaîne
// de caractère
static void kw_hex(){
    parse_arg_list(1);
    code_lit32(_addr(pad));
    bytecode(ISTRHEX);
}



//DATE$[()]
// commande qui retourne une chaîne date sous la forme AAAA/MM/JJ
static void kw_date(){
    optional_parens();
    bytecode(IDATE);
}

//TIME$[()]
// commande qui retourne une chaîne heure sous la forme HH:MM:SS
static void kw_time(){
    optional_parens();
    bytecode(ITIME);
}

//LEFT$(string,n)
// extrait les n premier caractères de la chaîne
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
// insère s2 à la position n de s1
static void kw_insert(){
    parse_arg_list(3); // (s1 s2 n )
    bytecode(IINSERT);
}

//LCASE$(s)
//converti la chaîne en minicules retourne
//un pointeur vers la nouvelle chaîne.
static void kw_lcase(){
    parse_arg_list(1);
    bytecode(ILCASE);
}

//UCASE$(s)
//converti la chaîne en majuscule retourne
//un pointeur vers la nouvelle chaîne
static void kw_ucase(){
    parse_arg_list(1);
    bytecode(IUCASE);
}


//INSTR([start,]texte,cible)
// recherche la chaîne "cible" dans le texte
// l'argument facultatif [start], indique la position de départ dans le texte.
// Puisque le premier paramètre est facultatif on ne peut utiliser parse_arg_list().
static void kw_instr(){
    var_t *var;
    int expr_type, count=0;
    
    expect(eLPAREN);
    do{
        next_token();
        switch (token.id){
            case eIDENT:
                if (!try_string_var()){
                    unget_token=true;
                    if (expression()!=eVAR_INT){throw(eERR_BAD_ARG);}
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
                    if (expression()!=eVAR_INT){throw(eERR_BAD_ARG);}
                }
                break;
            default:
                unget_token=true;
                if (expression()!=eVAR_INT){throw(eERR_BAD_ARG);}
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

//VAL(chaîne|var$)
// convertie une valeur châine en entier
static void kw_val(){
    parse_arg_list(1);
    bytecode(I2INT);
}

// TRACE(expression)
// active ou désactive le mode traçage
// pour le débogage
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
                        code_lit32(adr);
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
            case eNL:
                if (activ_reader->device==eDEV_KBD) activ_reader->eof=true;
                break;
            case eNONE:
            case eCOLON:
                break;
            default:
                throw(eERR_SYNTAX);
                break;
        }//switch
    }while (!activ_reader->eof);
}//f

//efface le contenu de progspace
static void clear(){
    close_all_files();
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

    pad=string_alloc(PAD_SIZE);
    *(pad-1)=128;
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
// boucle interpréteur    
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
    // désactive le mode trace avant de quitter.
    f_trace=false;
    free_string_vars();
    free(progspace);
    *(pad-1)=0;
    string_free(pad);
}//BASIC_shell()



