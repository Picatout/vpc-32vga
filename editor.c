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

/* NOTES:
*   La mémoire SPI RAM est utilisée comme tampon texte.
*   La technique du 'split buffer' est utilisée. 
*   Il y a un fente au milieu du texte entre la position
 *  du curseur texte et la fin du texte. Les nouveaux caractères
 *  sont insérés dans cette fente et lorsque le curseur texte est
 *  déplacé les caractères sont déplacés d'une extrémité à l'autre de la fente.
 *  'state->gap_first' est l'adresse du début de  la fente et représente la position
 *  d'insertion d'un caractère dans la mémoire tampon.
 *  'state->tail' est l'adresse du premier caractère après la fente et pointe le
 *  caractère à la position du curseur texte. 
 *  Chaque ligne de texte est terminée par un caractère ASCII LF (0xA).
 *  Une ligne a une longueur maximale de 80 caractères incluant le LF.    
 */ 



#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "hardware/spiram/spiram.h"
#include "hardware/ps2_kbd/keyboard.h"
#include "hardware/Pinguino/fileio.h"
#include "hardware/sound/sound.h"
#include "graphics.h"
#include "font.h"
#include "reader.h"


#define EDITOR_LINES (LINE_PER_SCREEN-1)
#define LAST_LINE (EDITOR_LINES-1)
#define LINE_MAX_LEN (CHAR_PER_LINE-1)
#define LAST_COL (LINE_MAX_LEN-1)
#define SCREEN_SIZE (EDITOR_LINES*CHAR_PER_LINE)
#define ED_BUFF_SIZE (SRAM_SIZE)
#define MAX_SIZE (ED_BUFF_SIZE)
#define STATUS_LINE (LINE_PER_SCREEN-1)

#define _refused()  tone(500,100)

#define MODE_INSERT 1
#define MODE_OVERWR 0
#define NAME_MAX_LEN 32

#define _screen_get(l,c) *(screen+l*CHAR_PER_LINE+c)
#define _screen_put(l,c,chr) *(screen+l*CHAR_PER_LINE+c)=chr
#define _screen_line(l) (screen+l*CHAR_PER_LINE)
#define _screen_addr(l,c) (screen+l*CHAR_PER_LINE+c)

//contient 1'écran texte
//typedef char text_line_t[CHAR_PER_LINE];
//text_line_t *screen[LINE_PER_SCREEN];
char *screen;

typedef struct editor_state{
    uint32_t fsize; // grandeur du fichier en octets.
    uint32_t scr_first; //premier caractère visible
    uint32_t gap_first; //position début gap
    uint32_t tail; // position fin gap
    int8_t scr_line; // ligne du curseur
    int8_t scr_col; //colonne du curseur
    uint32_t file_line; // ligne courante dans le fichier.
    uint32_t lines_count; // nombre de lignes dans le fichier.
    struct {
      uint8_t insert:1;
      uint8_t modified:1;
      uint8_t new:1;
      uint8_t update:1;
    }flags;
} ed_state_t;


//static char *split_buffer; // ligne en edition
static ed_state_t *state;

static char *fname;

typedef struct search_struct{
   bool ignore_case;
   bool loop;
   bool found;
   uint32_t file_line;
   uint8_t col;
   char target[CHAR_PER_LINE];
} search_t;

static search_t *search_info;

static void update_status_line();
static bool file_exist(const char *name);
static void open_file();
static void load_file(const char *name);
static void save_file();
static void save_file_as();
static void line_up();
static void line_down();
static bool char_left();
static bool char_right();
static void delete_left();
static void delete_at();
static void delete_to_end();
static void delete_to_start();
static void insert_char(char c);
static void replace_char(char c);
static void line_home();
static void line_end();
static void line_break();
static void file_home();
static void file_end();
static void fill_screen();
static void update_display();
static void page_up();
static void page_down();
static void word_right();
static void word_left();
static void goto_line();
static int get_line(char *line, uint32_t *from);
static uint32_t get_line_back();
static void jump_to_line(uint16_t line_no);
static void insert_tabulation();


static void invert_display(bool enable){
    if (enable){
        clear_screen(con);
        invert_video(con,true);
    }else{
        invert_video(con,false);
        state->flags.update=1;
    }
}

static void print_line(uint8_t ln){
    char *line;
    int count=0;
    line=_screen_line(ln);
    set_curpos(con,0,ln);
    clear_eol(con);
    print(con,line);
}

static void prompt_continue(){
    print(con,"\nany key...");//static uint8_t *llen;
    wait_key(con);
}

static void ed_error(const char *msg, int code){//static uint8_t *llen;
    println(con,msg);
    print(con,"error code: ");
    print_int(con,code,0);
    if (con==VGA_CONSOLE){
        prompt_continue();
        clear_screen(con);
    }
}

//si fichier modifié confirme 
//avant de continuer
static bool ask_confirm(){
    char key;
    
    bool answer=state->flags.modified;
    if (answer){
        tone(1000,100);
        invert_display(true);
        print(con,"file unsaved! continue (y/n)?");
        key= wait_key(con);
        answer=(key=='y')||(key=='Y');
        invert_display(false);
    }else{
        answer=true;
    }
    return answer;
}// ask_confirm()

static void clear_all_states(){
    fname[0]=0;
    sram_clear_block(0,ED_BUFF_SIZE);
    memset(state,0,sizeof(ed_state_t));
    memset(screen,0,SCREEN_SIZE);
    state->flags.insert=MODE_INSERT;
    state->flags.new=true;
    state->flags.update=false;
    state->flags.modified=false;
    state->tail=MAX_SIZE;
}//clear_all_states()

static void new_file(const char *name){
    if (!state->flags.modified || ask_confirm()){
        clear_all_states();
        if (file_exist(name)){
            load_file(name);
        }
    }
}//new_file()

static bool quit;

static void leave_editor(){
    if (ask_confirm()){
        free(fname);
        free(state);
        free(search_info);
        free(screen);
        vga_set_cursor(CR_UNDER);
        quit=true;
    }
}
 
static void list_files(){
    clear_screen(con);
    invert_display(true);
    listDir(".");
    prompt_continue();
    invert_display(false);
}//f

//initialement le curseur est au début de la ligne
static void mark_target(){
    int i,len;

    len=strlen(search_info->target);
    set_curpos(con,state->scr_col,state->scr_line);
    invert_video(con,true);
    for (i=0;i<len;i++) put_char(con,_screen_get(state->scr_line,state->scr_col+i));
    invert_video(con,false);
    set_curpos(con,state->scr_col,state->scr_line);
}//f

static int search_line(char *line, char *target, int from){
    int i,j=0;
    for (i=from;line[i];i++){
        if (search_info->target[j]==line[i]){
            j++;
            if (j==strlen(search_info->target)){
                i++;
                break;
            }
        }else{
             j=0; 
        }
    }
    return j==strlen(search_info->target)?i-j:-1;
}//f

static void search_next(){
    char line[CHAR_PER_LINE];
    int len,lcount=0,pos;
    uint32_t saddr;
    
    if (!search_info->target[0] || (!search_info->loop && (state->gap_first>=(state->fsize-1)))){
        _refused();
        return;
    }
    search_info->found=false;
    strcpy(line,_screen_line(state->scr_line));
    if (search_info->ignore_case) uppercase(line);
    pos=search_line(line,search_info->target,state->scr_col+1);
    if (pos>state->scr_col){
        search_info->col=pos;
        state->scr_col=pos;
        mark_target();
        return;
    }
    len=strlen(_screen_line(state->scr_line));
    saddr=state->tail+(len-state->scr_col)+1;
    while (saddr<ED_BUFF_SIZE){
        lcount++;
        len=get_line((uint8_t*)line,&saddr);
        if (search_info->ignore_case) uppercase(line);
        pos=search_line(line,search_info->target,0);
        if (pos>-1){
            search_info->found=true;
            search_info->col=(uint8_t)pos;
            break;
        }
    }//while
    if (search_info->found){
        pos=state->scr_line+lcount;
        line_down();
        saddr=state->tail;
        state->scr_col=search_info->col;
        while (lcount){
            len=get_line((uint8_t*)line,&saddr);
            lcount--;
            if (lcount){
                sram_write_block(state->gap_first,(uint8_t*)line,len+1);
                state->gap_first+=len+1;
                sram_clear_block(state->tail,len+1);
                state->tail+=len+1;
                saddr=state->tail;
            }else{
                sram_write_block(state->gap_first,(uint8_t*)line,search_info->col);
                state->gap_first+=search_info->col;
                sram_clear_block(state->tail,search_info->col);
                state->tail+=search_info->col;
            }
        }//while
        if (pos>LAST_LINE){
            state->scr_first=state->gap_first-search_info->col;
            state->scr_line=0;
            state->flags.update=1;
        }else{
            state->scr_line=pos;
            set_curpos(con,state->scr_col,state->scr_line);
        }
        mark_target();
    }else if (search_info->loop){
        saddr=0;
        lcount=1;
        while (saddr<state->gap_first){
            len=get_line((uint8_t*)line,&saddr);
            if (search_info->ignore_case) uppercase(line);
            pos=search_line(line,search_info->target,0);
            if (pos>-1){
                search_info->found=true;
                search_info->col=(uint8_t)pos;
                break;
            }
            lcount++;
        }//while
        if (search_info->found){
            jump_to_line(lcount);
            while (state->scr_col<search_info->col) char_right();
            mark_target();
        }else{
            _refused();
        }
    }else{
        _refused();
    }
}//f

static bool parse_search_line(){
    int pos=0;
    char *str, prev=0;
    int options=0;
    
    str=search_info->target;
    while (str[pos]){
        if (str[pos]=='-' && prev==' '){
            str[pos-1]=0;
            prev='-';
            pos++;
            break;
        }
        prev=str[pos];
        pos++;
    }    
    while (str[pos] && options!=3){
        switch(str[pos]){
            case '-':
            case ' ':
                break;
            case 'l':
            case 'L':
                if (prev!='-'){
                    _refused();
                    return false;
                }
                search_info->loop=true;
                options|=1; 
                if (str[pos+1] && str[pos+1]!=' '){
                    _refused();
                    return false;
                }
                break;
            case 'i':
            case 'I':
                if (prev!='-'){
                    _refused();
                    return false;
                }
                search_info->ignore_case=true;
                options|=2;
                uppercase(str);
                if (str[pos+1] && str[pos+1]!=' '){
                    _refused();
                    return false;
                }
                break;
            default:
                _refused();
                return false;
        }//switch
        prev=str[pos];
        pos++;
    }//while
    return true;
}//f

static void search(){
    int len;
    invert_display(true);
    clear_screen(con);
    print(con,"USAGE: target [-I] [-L]\n");
    search_info->found=false;
    search_info->loop=false;
    search_info->ignore_case=false;
    print(con,"? ");
    len=read_line(con,search_info->target,CHAR_PER_LINE);
    if (search_info->target[len-1]=='\n'){
        search_info->target[len-1]=0;
        len--;
    }
    invert_display(false);
    if (len && parse_search_line()){
        search_next();
    }
}//f

static bool get_file_name(char *name){
    int len;

    invert_display(true);
    print(con,"file name? ");
    len=read_line(con,name,NAME_MAX_LEN);
    uppercase(name);
    invert_display(false);
    return len;
}//f()

static void load_file(const char *name){
    uint32_t saddr,fsize;
    FRESULT result;
    FIL fh;
    int count, lines=0;
    char prev_c=0, c=0, buffer[CHAR_PER_LINE];
    reader_t r;
    
    clear_all_states();
    strcpy(fname,name);
    result=f_open(&fh,fname,FA_READ);
    if (result){
        ed_error("File open failed!\n",result);
        return;
    }
    if (fh.fsize>MAX_SIZE){
        ed_error("File too big!\n",0);
        return;
    }
    count=0;
    saddr=MAX_SIZE-fh.fsize;
    state->tail=saddr;
    state->fsize=fh.fsize;
    print(con,"loading file...\n");
    print_int(con,fsize,0);
    print(con," bytes");
    reader_init(&r,eDEV_SDCARD,&fh);
    while (!r.eof){
        c=reader_getc(&r);
        switch(c){
            case -1:
            case A_CR:
                break;
            case A_LF:
               break;
            default:
                if (c>=32 && c<(32+FONT_SIZE)) buffer[count++]=c; else buffer[count++]=' ';
        }//switch(c)
        if ((c==A_LF) || (count==(CHAR_PER_LINE-1))){
            buffer[count++]=A_LF;
            lines++;
            sram_write_block(saddr,(uint8_t*)buffer,count);
            saddr+=count;
            count=0;
        }
        prev_c=c;
    }//while
    if (count){
        sram_write_block(saddr,(uint8_t*)buffer,count);
        saddr+=count;
        lines++;
    }
    memset(buffer,' ',CHAR_PER_LINE);
    while (saddr<MAX_SIZE){
        count=min(CHAR_PER_LINE,MAX_SIZE-saddr);
        sram_write_block(saddr,(uint8_t*)buffer,count);
        saddr+=count;
    }
    f_close(&fh);
    state->lines_count=lines;
    fill_screen();
    state->flags.update=true;
}//f()

bool file_exist(const char *name){
    FIL fh;
    FRESULT result;
    
    if (!name) return false;
    result=f_open(&fh,name,FA_READ);
    if (!result) f_close(&fh);
    return !result;
}// file_exist()

static void open_file(){
    FIL fh;
    FRESULT result;
    char name[32];

    if (ask_confirm()){
        invert_display(true);
        print(con,"open file\n");
        if (get_file_name(name) && file_exist(name)){
                load_file(name);
        }else{
                ed_error("failed to open file.",result);
        }
        
        invert_display(false);
    }
}//open_file()


static void save_file(){
#define BUFFER_SIZE 128
    uint8_t buffer[BUFFER_SIZE];
    int size;
    uint32_t saddr=0;
    FRESULT result;
    FIL fh;

    if (!strlen(fname)){
        save_file_as();
    }else{
        if ((result=f_open(&fh,fname,FA_WRITE+FA_CREATE_ALWAYS))){
            sprintf(buffer,"Failed to create '%s' file\r",fname);
            ed_error(buffer,result);
            return;
        }
        invert_display(true);
        print(con,"saving file...\n");
        saddr=0;
        while ((result==FR_OK) && (saddr < MAX_SIZE)){
            size=get_line(buffer,&saddr);
            buffer[size]=A_CR;
            result=f_write(&fh,buffer,size+1,&size);
        }
        f_close(&fh);
        if (result){
            ed_error("diksI/O error...\n",result);
            return;
        }
        state->flags.modified=false;
        state->flags.new=0;
        crlf(con);
        print(con,fname);
        print(con," saved, size: ");
        print_int(con,state->fsize,0);
        prompt_continue();
        invert_display(false);
    }
}//f()

static void save_file_as(){
    char name[32];

    if (get_file_name(name)){
        if (strlen(name)){
            strcpy(fname,name);
            save_file();
        }
    }
}//f()


const char* hkeys[]={
  "<CTRL-DEL> delete to end of line\n",
  "<CTRL-A> save as\n",
  "<CTRL-END> file end\n",
  "<CTRL-F> list SDcard files\n",
  "<CTRL-G> goto line...\n",
  "<CTRL-HOME> file start\n",
  "<CTRL_LEFT> word left\n",
  "<CTRL-N> new file...\n",
  "<CTRL-O> open file...\n",
  "<CTRL-Q> Quit editor\n",
  "<CTRL-RIGHT> word right\n",
  "<CTRL-S> save file\n",
  "<F1> display hotkeys\n",
  "<F3> set search criterion\n",
  "<F4> search next\n",
  ""
};

//affiche les touches d'action
static void hot_keys(){
    int i=0;

    invert_display(true);
    clear_screen(con);
    while (strlen(hkeys[i])){    
        print(con,hkeys[i++]);
    }
    crlf(con);
    prompt_continue();
    invert_display(false);
}

static void editor_init(const char *file_name){
    clear_screen(con);
    screen=malloc(SCREEN_SIZE);
    search_info=malloc(sizeof(search_t));
    state=malloc(sizeof(ed_state_t));
    fname=malloc(NAME_MAX_LEN);
    clear_all_states();
    invert_video(con,false);
    vga_set_cursor(CR_UNDER);
    new_file(file_name);
}//f()


void editor(const char* file_name){
    unsigned short key;
    FATFS fh;
    FRESULT result;
    
    editor_init(file_name);
    quit=false;
    while(!quit){
        if (state->flags.update){
            update_display();
        }
        update_status_line();
        key=wait_key(con);
        //print_int(SERIAL_CONSOLE,key,0);
        switch(key){
            case VK_UP:
                line_up();
                break;
            case VK_DOWN:
                line_down();
                break;
            case VK_LEFT:
                char_left();
                break;
            case VK_CLEFT:
                word_left();
                break;
            case VK_RIGHT:
                char_right();
                break;
            case VK_CRIGHT:
                word_right();
                break;
            case VK_HOME:
                line_home();
                break;
            case VK_CHOME:
                file_home();
                break;
            case VK_END:
                line_end();
                break;
            case VK_CEND:
                file_end();
                break;
            case VK_PGUP:
                page_up();
                break;
            case VK_PGDN:
                page_down();
                break;
            case VK_INSERT:
                state->flags.insert=~state->flags.insert;
                if (state->flags.insert){
                    vga_set_cursor(CR_UNDER);
                }else{
                    vga_set_cursor(CR_BLOCK);
                }
                break;
            case VK_TAB:
                insert_tabulation();
                break;
            case VK_DELETE:
                delete_at();
                break;
            case VK_CDEL:
                delete_to_end();
                break;
            case VK_BACK:
                delete_left();
                break;
            case VK_CBACK: 
                delete_to_start();
                break;
            case VK_ENTER:
                line_break();
                break;
            case VK_F1: // display hotkeys
                hot_keys();
                break;
            case VK_F3:
                search();
                break;
            case VK_F4:
                search_next();
            case VK_CA: // <CTRL>-A  sauvegarde sous...
                save_file_as();
                break;
            case VK_CF: // <CTRL>-F liste des fichiers
                list_files();
                break;
            case VK_CG:// <CTRL>-G  va à la ligne
                goto_line();
                break;
            case VK_CN: // <CTRL>-N  nouveau fichier
                new_file(NULL);
                break;
            case VK_CO: // <CTRL>-O  ouvrir un fichier
                open_file();
                break;
            case VK_CQ: // <CTRL>-Q  quitter
                leave_editor();
                break;
            case VK_CS: // <CTRL>-S  sauvegarde
                save_file();
                break;
            default:
                if ((key>=32)&(key<FONT_SIZE+32)){
                    insert_char(key);
                }else{
                    _refused();
                }
            }//switch
    }//while
    invert_video(con,false);
    clear_screen(con);
}//editor()

/**************************************/
/* OPRÉATIONS SUR LA MÉMOIRE SPI RAM  */
/**************************************/

// char sram_get_char()
// retourne le caractère à la position du curseur.
static char sram_get_char(){
    if (state->tail<MAX_SIZE){
        return sram_read_byte(state->tail);
    }else{
        return 0;
    }
}// char sram_get_char()


//bool sram_insert_char(char c)
// insère ou remplace un caractère à la position du curseur.
// met à jour les états: fsize, tail et gap_first.
// Retourne vrai s'il y a de l'espace sinon retourne faux.
static bool sram_insert_char(char c){
    bool result=true;
    char tail_char;
    int llen;
    
    if ((state->flags.insert && (state->fsize==MAX_SIZE))){
        result=false;
    }else if (state->flags.insert){
        llen=strlen(_screen_line(state->scr_line));
        if ((llen<LINE_MAX_LEN)||(c==A_LF)){
            sram_write_byte(state->gap_first++,c);
            state->fsize++;
        }else{
            result=false;
        }
    }else{
        tail_char=sram_get_char();
        sram_write_byte(state->gap_first++,c);
        if (state->tail<MAX_SIZE){state->tail++;}else{state->fsize++;}
    }
    return result;
} // bool sram_insert_char(char c)


// char sram_char_left()
// recule d'un caractère dans la mémoire SPI RAM
// renvoie le caractère ou zéro si déjà au début du texte.
static char sram_char_left(){
    char c;
    
    if (state->gap_first){
        c=sram_read_byte(--state->gap_first);
        sram_write_byte(--state->tail,c);
        return c;
    }else{
        return 0;
    }
}// sram_cahr_left()

// char sram_char_right()
//avance d'un caractère dans la mémoire SPI RAM
//renvoie le caractère ou zéro si déjà à la fin du texte.
static char sram_char_right(){
    char c;
    if (state->tail<MAX_SIZE){
        c=sram_read_byte(state->tail++);
        sram_write_byte(state->gap_first++,c);
        return c;
    }else{
        return 0;
    }
}

// int sram_line_home(char *line, bool move_back)
// Compte les caractères à gauche du curseur jusqu'au début de la ligne.
// dépose tous les caractères situés avant le curseur 
// jusqu'au début dans 'line'.
// Si 'move_back' est vrai recule le curseur au début de la ligne
// retourne le nombre de caractères déposés dans 'line'.
int sram_line_home(char *line, bool move_back){
    int j,llen,size;
    uint32_t from;
    char c,buf[CHAR_PER_LINE];
    
    if ((from=state->gap_first)){
        size=min(from,CHAR_PER_LINE);
        sram_read_block(from-size,buf,size);
        j=size-1;
        llen=0;
        while((j>-1) && (llen<LINE_MAX_LEN) && ((c=buf[j--])!=A_LF)){
            llen++;
        }//while
        if (llen){
            memcpy((void*)line,(void*)(buf+(size-llen)),llen);
        }
        line[llen]=0;
        if (move_back){
            state->gap_first-=llen;
            state->tail-=llen;
            sram_write_block(state->tail,(uint8_t*)line,llen);
        }
        return llen;
    }else{
        line[0]=0;
        return 0;
    }
}//int sram_line_home(char *line,, bool move_back)

// int sram_line_end(char *line, bool move_forward)
// compte le nombre de caractères jusqu'à la fin de la ligne excluant le LF.
// Si 'move_back' est vrai déplace le curseur texte à cette position.
// dépose les caractères à droite du curseur dans 'line'
// retourne le nombre de caractères comptés.
int sram_line_end(char *line, bool move_forward){
    int llen,size;
    char c;
    
    c=sram_get_char();
    if (c && (c!=A_LF)){
        size=min(LAST_COL,MAX_SIZE-state->tail);
        sram_read_block(state->tail,(uint8_t*)line,size);
        llen=0;
        while ((llen<size) && (line[llen]!=A_LF)){
            llen++;
        }
        line[llen]=0;
        if (move_forward){
            sram_write_block(state->gap_first,(uint8_t*)line,llen);
            state->gap_first+=llen;
            state->tail+=llen;
        }
        return llen;
    }else{
        line[0]=0;
        return 0;
    }
}//int sram_line_end(char *line, bool move_forward)

// void sram_delete_to_end()
// supprime tous les caractères de la ligne à partir du curseur
// jusqu'à la fin de la ligne excepté le LF
static void sram_delete_to_end(){
    int size, j=0,deleted=0;
    char buf[CHAR_PER_LINE];
    
    size=min(CHAR_PER_LINE,MAX_SIZE-state->gap_first);
    if (size){
        sram_read_block(state->tail,buf,size);
        while(j<size && buf[j++]!=A_LF){
            deleted++;
        }
        state->tail+=deleted;
        state->fsize-=deleted;
    }
    
}// void sram_delete_to_end()

bool in_slot(uint32_t saddr){
    return (saddr>=state->gap_first) && (saddr<state->tail);
}

// int get_line(char *line, uint32_t from)
// accumule dans 'line' les caractères à partir de 'from' jusqu'à la fin de ligne excluant le LF
// retourne le nombre de caractères accumulés.
// initialement 'from' devrait pointé le premier caractère d'une ligne.
// conditions:
//   line est un tampon d'au moins CHAR_PER_LINE caractères.
static int get_line(char *line, uint32_t *from){
    int size, llen;
    uint32_t saddr;
    char c;
    
    size=0;
    saddr=*from;
    if (saddr<state->gap_first){
        size=min(CHAR_PER_LINE,state->gap_first-saddr);
        sram_read_block(saddr,(uint8_t*)line,size);
        saddr+=size;
    }
    if (in_slot(saddr)){
        saddr=state->tail;
    }
    if (size<CHAR_PER_LINE){
        llen=size;
        size=min(CHAR_PER_LINE-size,MAX_SIZE-saddr);
        sram_read_block(saddr,(uint8_t*)(line+llen),size);
        size+=llen;
    }
    llen=0;
    while ((llen<size) && ((c=line[llen])!=A_LF)){llen++;}
    line[llen]=0;
    saddr=(*from)+llen;
    if (c==A_LF){saddr++;}
    if (((*from)<state->tail) && (saddr>=state->gap_first)){
        *from=state->tail+saddr-state->gap_first;
    }else{
        *from=saddr;
    }
    return llen;
}// int get_line(char *line, uint32_t from)

static void fill_screen(){
    char buf[CHAR_PER_LINE];
    uint32_t from;
    int i;
    
    memset(screen,0,SCREEN_SIZE);
    from=state->scr_first;
    i=0;
    while ((i<EDITOR_LINES) && (from<MAX_SIZE)){
        get_line(buf,&from);
        strcpy(_screen_line(i++),buf);
    }
}


static void update_display(){
    int i=0;
    
    clear_screen(con);
    while ((i<EDITOR_LINES)){
        println(con,_screen_line(i++));
    }
    set_curpos(con,state->scr_col,state->scr_line);
    state->flags.update=0;
}//f();


const char no_name[]="unnamed";
void update_status_line(){
    char status[80];
    char *file_name;
//    text_coord_t curpos;
    
//    curpos=get_curpos(con);
    set_curpos(con,0,STATUS_LINE);
    invert_video(con,true);
    clear_eol(con);
//    file_name=fname[0]?fname:no_name;
    sprintf(status,"%s,\tsize: %d",fname[0]?fname:no_name,state->fsize);
    print(con,status);
    set_curpos(con,40,STATUS_LINE);
    sprintf(status,"line: %0d/%0d,\tcol: %0d",
            state->file_line+1,state->lines_count+1,state->scr_col+1);
    print(con,status);
    invert_video(con,false);
    set_curpos(con,state->scr_col,state->scr_line);
}

static void line_up(){
    uint8_t col,llen, line[CHAR_PER_LINE];
    
    col=state->scr_col;
    line_home();
    char_left();
    while ((col<state->scr_col) && char_left()){}
}//f()

static void line_down(){
    uint8_t col,llen;
    
    col=state->scr_col;
    line_end();
    char_right();
    llen=strlen(_screen_line(state->scr_line));
    if (llen<col){
        line_end();
    }else{
        while ((col>state->scr_col)){char_right();}
    }
}//f()

static int screen_scroll_down(){
    char buf[CHAR_PER_LINE];
    int llen;
    
    llen=sram_line_home(buf,false);
    state->scr_first=state->gap_first-llen;
    fill_screen();
    state->flags.update=true;
    return llen;
}

//déplace le curseur vers la gauche d'un caractère
static bool char_left(){
    int llen;
    
    if (sram_char_left()){
        if (state->scr_col){
            state->scr_col--;
        }else{
            state->file_line--;
            if (!state->scr_line){
                llen=screen_scroll_down();
            }else{
                state->scr_line--;
                llen=strlen(_screen_line(state->scr_line));
            }
            state->scr_col=llen;
            set_curpos(con,state->scr_col,state->scr_line);
        }
        return true;
    }else{
        return false;
    }
}//f()

static void screen_scroll_up(){
    state->scr_first+=strlen(screen)+1;
    fill_screen();
    state->flags.update=true;
}

//déplace le curseur vers la droite d'un caractère
static bool char_right(){
    char buf[CHAR_PER_LINE];
    uint32_t from;
    
    if (sram_char_right()){
        if (state->scr_col<strlen(_screen_line(state->scr_line))){
            state->scr_col++;
        }else{
            state->file_line++;
            state->scr_col=0;
            if (state->scr_line==LAST_LINE){
                screen_scroll_up();
            }else{
                state->scr_line++;
            }
        } 
        set_curpos(con,state->scr_col,state->scr_line);
        return true;
    }//if
    return false;
}//f()

static void delete_left(){
    if (char_left()){
        delete_at();
    }else{
        _refused();
    }
} //f()

// supprime le caractère à la position du curseur
static void delete_at(){
    int col,count,llen,llen_next;
    
    if (state->tail<MAX_SIZE){
        col=state->scr_col;
        llen=strlen(_screen_line(state->scr_line));
        if (col<llen){
            state->tail++;
            state->fsize--;
            memmove(_screen_line(state->scr_line)+col,
                    _screen_line(state->scr_line)+col+1,
                    CHAR_PER_LINE-col-1);
            set_curpos(con,0,state->scr_line);
            clear_eol(con);
            print(con,_screen_line(state->scr_line));
            set_curpos(con,state->scr_col,state->scr_line);
        }else{
           llen_next=strlen(_screen_line(state->scr_line+1));
           if ((llen+llen_next)<=LINE_MAX_LEN){
               state->tail++;
               state->fsize--;
               state->lines_count--;
           }else{
               count=LINE_MAX_LEN-llen;
               sram_move(state->tail,state->tail+1,count);
               sram_write_byte(state->tail+count,A_LF);
           }
           fill_screen();
           state->flags.update=true;
        }
    }else{
        _refused();
        return;
    }
    
}//delete_at()


//efface tous les caractères à partir du curseur
//jusqu'à la fin de ligne.
//si le curseur est au début de ligne
//la ligne est supprimée au complet LF inclus.
static void delete_to_end(){
    int count,col,llen;

    llen=strlen(_screen_line(state->scr_line));
    if (state->scr_col){
        count=llen-state->scr_col;
        if (count){
            state->tail+=count;
            state->fsize-=count;
            _screen_put(state->scr_line,state->scr_col,0);
            clear_eol(con);
        }
    }else{
        state->tail+=llen;
        state->fsize-=llen;
        state->scr_col=0;
        if (sram_get_char()==A_LF){
            state->tail++;
            state->fsize--;
            state->lines_count--;
        }
        fill_screen();
        state->flags.update=true;
    }
 }//f()

//efface tous les caratères à gauche du curseur
//jusqu'au début de la ligne.
static void delete_to_start(){
    int col;
    
    col=state->scr_col;
    if (col){
        state->gap_first-=col;
        state->fsize-=col;
        memmove(_screen_line(state->scr_line),_screen_line(state->scr_line)+col,CHAR_PER_LINE-col);
        state->scr_col=0;
        set_curpos(con,0,state->scr_line);
        clear_eol(con);
        print(con,_screen_line(state->scr_line));
        set_curpos(con,0,state->scr_line);
    }
}//f()


static void insert_char(char c){
    if (sram_insert_char(c)){
        if (state->flags.insert){
            fill_screen();
        }else{
            _screen_put(state->scr_line,state->scr_col,c);
        }
        set_curpos(con,0,state->scr_line);
        print(con,_screen_line(state->scr_line));
        state->scr_col++;
        set_curpos(con,state->scr_col,state->scr_line);
    }else{
        _refused();
        return;
    }
}//f()

// insère des espaces jusqu'au début de la prochaine colonne.
static void insert_tabulation(){
    int w,count,llen;
    
    w=get_tab_width(con);
    count=w-state->scr_col%w;
    llen=strlen(_screen_line(state->scr_line));
    count=min(count,LINE_MAX_LEN-llen);
    while (count--){
        insert_char(' ');
    }
}// insert_tabulation()

static void line_home(){
    char buf[CHAR_PER_LINE];
    int llen;
    llen=sram_line_home(buf,true);
    if (llen){
        state->scr_col=0;
        set_curpos(con,state->scr_col,state->scr_line);
    }
}//f()

static void line_end(){
    int llen;
    char buf[CHAR_PER_LINE];
    llen=sram_line_end(buf,true);
    if (llen){
        state->scr_col=strlen(_screen_line(state->scr_line));
        set_curpos(con,state->scr_col,state->scr_line);
    }
}//f()

 static void line_break(){
     if (sram_insert_char(A_LF)){
         state->lines_count++;
         state->file_line++;
         state->scr_col=0;
         state->scr_line++;
         if (state->scr_line>LAST_LINE){
             state->scr_first+=strlen(screen)+1;
         }
         fill_screen();
         state->flags.update=true;
     }else{
         _refused();
     }
}//f()

static void file_home(){
    while (state->file_line){
        line_up();
    }
    line_home();
}//f()

static void file_end(){
    while (state->file_line < state->lines_count){
        line_down();
    }
    line_end();
}//f()

static void page_up(){
    int count;

    count=state->scr_line+EDITOR_LINES;
    while (state->file_line && count){
        line_up();
        count--;
    }
    line_home();
}

static void page_down(){
    int count;
    count=EDITOR_LINES-state->scr_line;
    while ((state->file_line<state->lines_count) && count){
        line_down();
        count--;
    }
    line_home();
    if (state->file_line<state->lines_count){
        state->scr_first=state->gap_first;
        state->scr_line=0;
        fill_screen();
    }
}

// avance le curseur au prochain caractère alphanumérique.
static char forward_alnum(char c){
    while (c && !isalnum(c)){
        char_right();
        c=_screen_get(state->scr_line,state->scr_col);
    }
    return c;
}// forward_next_alnum()

// avance le curseur au prochain caractère qui n'est pas alphanumérique.
static char forward_not_alnum(char c){
    while (c && isalnum(c)){
        char_right();
        c=_screen_get(state->scr_line,state->scr_col);
    }
    return c;
}

static void word_right(){
    char c;

    c=_screen_get(state->scr_line,state->scr_col);
    while (c){
        if (!isalnum(c)){
            c=forward_alnum(c);
            break;
        }else{
            c=forward_not_alnum(c);
        }
    }//if
}//word_right()

static char back_alnum(char c){
    while (state->scr_col && !isalnum(c)){
        char_left();
        c=_screen_get(state->scr_line,state->scr_col);
    }
    return c;
}//back_next_alnum()

static char back_not_alnum(char c){
    while (state->scr_col && isalnum(c)){
        char_left();
        c=_screen_get(state->scr_line,state->scr_col);
    }
    return c;
}// back_next_not_alnum()


static void word_left(){
    char c;
    
    c=_screen_get(state->scr_line,state->scr_col);
    while (state->scr_col){
        if (!isalnum(c)){
            c=back_alnum(c);
            break;
        }else{
            c=back_not_alnum(c);
        }
    }
}//word_left()

static void jump_to_line(uint16_t line_no){
    line_no--;
    if (line_no>state->lines_count){
        file_end();
    }else if (line_no<state->file_line){
                while (line_no<state->file_line){line_up();}
        }else{
                while (state->file_line<line_no){line_down();}
        }//if
    line_home();
    if (state->scr_line){
        state->scr_first=state->gap_first;
        state->scr_line=0;
        fill_screen();
    }
}//f

static void goto_line(){
    uint8_t llen;
    char line[16];
    uint16_t line_no=0;
    
    invert_display(true);
    print(con,"goto line: ");
    llen=read_line(con,line,16);
    invert_display(false);
    if (llen){
        line_no=atoi((const char*)line);
        jump_to_line(line_no);
    }
}

   