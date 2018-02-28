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


#include <stdlib.h>
#include <string.h>
#include <stdint.h>
//#include <stdBOOL.h>
#include <ctype.h>
#include <stdio.h>

#include "console.h"
#include "hardware/spiram/spiram.h"
#include "hardware/ps2_kbd/keyboard.h"
#include "hardware/Pinguino/fileio.h"
#include "hardware/sound/sound.h"
#include "graphics.h"
#include "font.h"
#include "reader.h"


#define LAST_COL (CHAR_PER_LINE-1)
#define LINE_MAX_LEN (CHAR_PER_LINE-1)
#define LAST_LINE (LINE_PER_SCREEN-1)
#define SCREEN_SIZE (LINE_PER_SCREEN*CHAR_PER_LINE)
#define ED_BUFF_SIZE (SRAM_SIZE-2)
#define MAX_SIZE (ED_BUFF_SIZE)

#define refused()  tone(500,100);

#define MODE_INSERT 1
#define MODE_OVERWR 0
#define NAME_MAX_LEN 32

#define _screen_get(l,c) *(screen+l*CHAR_PER_LINE+c)
#define _screen_put(l,c,chr) *(screen+l*CHAR_PER_LINE+c)=chr

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
    struct {
      uint8_t insert:1;
      uint8_t modified:1;
      uint8_t new:1;
    }flags;
} ed_state_t;


//static char *split_buffer; // ligne en edition
static ed_state_t *state;

static char *fname;

typedef struct search_struct{
   BOOL ignore_case;
   BOOL loop;
   BOOL found;
   uint8_t col;
   char target[CHAR_PER_LINE];
} search_t;

static search_t *search_info;

static BOOL file_exist(const char *name);
static void open_file();
static void load_file(const char *name);
static void save_file();
static void save_file_as();
static void line_up();
static void line_down();
static void char_left();
static void char_right();
static void delete_left();
static void delete_at();
static void delete_to_end();
static void delete_to_start();
static void insert_char(char c);
static void replace_char(char c);
static void line_home();
static void line_end();
static void enter();
static void file_home();
static void file_end();
static void update_display();
static void page_up();
static void page_down();
static void word_right();
static void word_left();
static void goto_line();
static uint8_t get_line_forward(uint8_t* line, uint32_t from);
static uint8_t get_line_back(uint8_t* line, uint32_t from);
static void jump_to_line(uint16_t line_no);


static void print_line(uint8_t line){
    set_curpos(con,0,line);
    clear_eol(con);
    print(con,screen+line*CHAR_PER_LINE);
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
static BOOL ask_confirm(){
    char key;
    BOOL answer=state->flags.modified;
    if (answer){
        tone(1000,100);
        invert_video(con,TRUE);
        set_curpos(con,0,0);
        clear_eol(con);
        print(con,"file unsaved! continue (y/n)?");
        key= wait_key(con);
        answer=(key=='y')||(key=='Y');
        invert_video(con,FALSE);
        set_curpos(con,0,0);
        clear_eol(con);
        print_line(0);
        set_curpos(con,state->scr_col,state->scr_line);
    }else{
        answer=TRUE;
    }
    return answer;
}

static void new_file(){
    if (ask_confirm()){
        clear_screen(con);
        invert_video(con,TRUE);
        sram_clear_block(0,ED_BUFF_SIZE);
        memset(state,0,sizeof(ed_state_t));
        memset(screen,0,SCREEN_SIZE);
        state->flags.insert=MODE_INSERT;
        state->flags.new=1;
        state->flags.modified=FALSE;
        state->tail=MAX_SIZE;
        clear_screen(con);
        fname[0]=0;
        search_info->target[0]=0;
        search_info->ignore_case=FALSE;
        search_info->loop=FALSE;
        search_info->found=FALSE;
        invert_video(con,FALSE);
        update_display();
    }
}//f()

static BOOL quit;

static void leave_editor(){
    if (ask_confirm()){
        free(fname);
        free(state);
        free(search_info);
        free(screen);
        vga_set_cursor(CR_UNDER);
        quit=TRUE;
    }
}
 
static void list_files(){
    clear_screen(con);
    invert_video(con,TRUE);
    listDir(".");
    prompt_continue();
    invert_video(con,FALSE);
    update_display();
}//f

//initialement le curseur est au début de la ligne
static void mark_target(){
    int i,len;

    len=strlen(search_info->target);
    set_curpos(con,state->scr_col,state->scr_line);
    invert_video(con,TRUE);
    for (i=0;i<len;i++) put_char(con,_screen_get(state->scr_line,state->scr_col+i));
    invert_video(con,FALSE);
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
    uint16_t saddr;
    
    if (!search_info->target[0] || (!search_info->loop && (state->gap_first>=(state->fsize-1)))){
        refused();
        return;
    }
    search_info->found=FALSE;
    strcpy(line,screen+(state->scr_line*CHAR_PER_LINE));
    if (search_info->ignore_case) uppercase(line);
    pos=search_line(line,search_info->target,state->scr_col+1);
    if (pos>state->scr_col){
        search_info->col=pos;
        state->scr_col=pos;
        mark_target();
        return;
    }
    len=strlen(screen+state->scr_line*CHAR_PER_LINE);
    saddr=state->tail+(len-state->scr_col)+1;
    while (saddr<ED_BUFF_SIZE){
        lcount++;
        len=get_line_forward((uint8_t*)line,saddr);
        if (search_info->ignore_case) uppercase(line);
        pos=search_line(line,search_info->target,0);
        if (pos>-1){
            search_info->found=TRUE;
            search_info->col=(uint8_t)pos;
            break;
        }
        saddr+=len+1;
    }//while
    if (search_info->found){
        pos=state->scr_line+lcount;
        line_down();
        saddr=state->tail;
        state->scr_col=search_info->col;
        while (lcount){
            len=get_line_forward((uint8_t*)line,saddr);
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
            update_display();
        }else{
            state->scr_line=pos;
            set_curpos(con,state->scr_col,state->scr_line);
        }
        mark_target();
    }else if (search_info->loop){
        saddr=0;
        lcount=1;
        while (saddr<state->gap_first){
            len=get_line_forward((uint8_t*)line,saddr);
            if (search_info->ignore_case) uppercase(line);
            pos=search_line(line,search_info->target,0);
            if (pos>-1){
                search_info->found=TRUE;
                search_info->col=(uint8_t)pos;
                break;
            }
            lcount++;
            saddr+=len+1;
        }//while
        if (search_info->found){
            jump_to_line(lcount);
            while (state->scr_col<search_info->col) char_right();
            mark_target();
        }else{
            refused();
        }
    }else{
        refused();
    }
}//f

static BOOL parse_search_line(){
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
                    refused();
                    return FALSE;
                }
                search_info->loop=TRUE;
                options|=1; 
                if (str[pos+1] && str[pos+1]!=' '){
                    refused();
                    return FALSE;
                }
                break;
            case 'i':
            case 'I':
                if (prev!='-'){
                    refused();
                    return FALSE;
                }
                search_info->ignore_case=TRUE;
                options|=2;
                uppercase(str);
                if (str[pos+1] && str[pos+1]!=' '){
                    refused();
                    return FALSE;
                }
                break;
            default:
                refused();
                return FALSE;
        }//switch
        prev=str[pos];
        pos++;
    }//while
    return TRUE;
}//f

static void search(){
    int len;
    invert_video(con,TRUE);
    clear_screen(con);
    print(con,"USAGE: target [-I] [-L]\n");
    search_info->found=FALSE;
    search_info->loop=FALSE;
    search_info->ignore_case=FALSE;
    print(con,"? ");
    len=read_line(con,search_info->target,CHAR_PER_LINE);
    if (search_info->target[len-1]=='\n'){
        search_info->target[len-1]=0;
        len--;
    }
    invert_video(con,FALSE);
    update_display();
    if (len && parse_search_line()){
        search_next();
    }
}//f

static BOOL get_file_name(char *name){
    int len;
    
    crlf(con);
    print(con,"file name? ");
    len=read_line(con,name,NAME_MAX_LEN);
    uppercase(name);
    return len;
}//f()

static void load_file(const char *name){
    uint32_t saddr,fsize;
    FRESULT result;
    FIL fh;
    int count, line_no=0;
    char prev_c=0, c=0, buffer[CHAR_PER_LINE];
    reader_t r;
    
    new_file();
    strcpy(fname,name);
    result=f_open(&fh,fname,FA_READ);
    if (result){
        ed_error("File load failed!\n",result);
        return;
    }
    if (fh.fsize>MAX_SIZE){
        ed_error("File too big!\n",0);
        return;
    }
    count=0;
    state->tail=MAX_SIZE;
    saddr=0;
    print(con,"loading file...\n");
    print_int(con,fsize,0);
    print(con," bytes");
    reader_init(&r,eDEV_SDCARD,&fh);
    while (!r.eof){
        c=reader_getc(&r);
        switch(c){
            case -1:
            case CR:
            case LF:
                break;
            default:
                if (c>=32 && c<(32+FONT_SIZE)) buffer[count++]=c; else buffer[count++]=' ';
        }//switch(c)
        if ((c==CR) || ((c==LF) && (prev_c!=CR)) || (count==(CHAR_PER_LINE-1))){
            buffer[count++]=0;
            line_no++;
            sram_write_block(saddr,(uint8_t*)buffer,count);
            saddr+=count;
            count=0;
        }
        prev_c=c;
    }
    if (count){
        sram_write_block(saddr,(uint8_t*)buffer,count);
        saddr+=count;
        line_no++;
    }
    f_close(&fh);
    invert_video(con,FALSE);
    state->fsize=saddr;
    state->gap_first=saddr;
    state->scr_first=saddr;
    file_home();
}//f()

BOOL file_exist(const char *name){
    FIL fh;
    FRESULT result;
    
    result=f_open(&fh,name,FA_READ);
    if (!result) f_close(&fh);
    return !result;
}

static void open_file(){
    FIL fh;
    FRESULT result;
    char name[32];

    if (ask_confirm()){
        clear_screen(con);    
        invert_video(con,TRUE);
        print(con,"open file\n");
        if (get_file_name(name) && file_exist(name)){
                load_file(name);
                return;
        }else{
                ed_error("failed to open file.",result);
        }
        
        invert_video(con,FALSE);
        clear_screen(con);
        update_display();
    }
}//f()

inline static void replace_nulls(uint8_t *buffer,int len){
    int i;
    for(i=0;i<=len;i++) if (!buffer[i]) buffer[i]=CR;
}//f

static void save_file(){
#define BUFFER_SIZE 128
    uint8_t buffer[BUFFER_SIZE];
    int size;
    uint32_t saddr=0;
    FRESULT result;
    FIL fh;

    if ((result=f_open(&fh,fname,FA_WRITE+FA_CREATE_ALWAYS))){
        sprintf(buffer,"Failed to create '%s' file\r",fname);
        ed_error(buffer,result);
        return;
    }
    invert_video(con,TRUE);
    clear_screen(con);
    print(con,"saving file...\n");
    while ((result==FR_OK) && (saddr < state->gap_first)){
        size=min(BUFFER_SIZE,state->gap_first-saddr);
        sram_read_block(saddr,buffer,size);
        saddr+=size;
        replace_nulls(buffer,size);
        result=f_write(&fh,buffer,size,&size);
    }
    saddr=state->tail;
    while((result==FR_OK) && saddr < ED_BUFF_SIZE){
        size=min(BUFFER_SIZE,ED_BUFF_SIZE-saddr);
        sram_read_block(saddr,buffer,size);
        saddr+=size;
        replace_nulls(buffer,size);
        result=f_write(&fh,buffer,size,&size);
    }
    f_close(&fh);
    if (result){
        ed_error("diksI/O error...\n",result);
        return;
    }
    state->flags.modified=FALSE;
    state->flags.new=0;
    crlf(con);
    print(con,fname);
    print(con," saved, size: ");
    print_int(con,state->fsize,0);
    prompt_continue();
    invert_video(con,FALSE);
    update_display();
}//f()

static void save_file_as(){
    char name[32];
    invert_video(con,TRUE);
    clear_screen(con);
    if (get_file_name(name)){
        if (strlen(name)){
            strcpy(fname,name);
            save_file();
        }
    }
    invert_video(con,FALSE);
    update_display();
}//f()

static void file_info(){
    char line[CHAR_PER_LINE];
    set_curpos(con,0,0);
    invert_video(con,TRUE);
    clear_eol(con);
    if (strlen(fname)){
        sprintf(line,"file%c: %s, size: %d",state->flags.modified?'*':' ',fname,state->fsize);
    }else{
        sprintf(line,"no name%c, size: %d",state->flags.modified?'*':' ',state->fsize);
    }
    print(con,line);
    prompt_continue();
    invert_video(con,FALSE);
    print_line(0);
    print_line(1);
    set_curpos(con,state->scr_col,state->scr_line);
}


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
  "<F2> file information\n",
  "<F3> set search criterion\n",
  "<F4> search next\n",
  ""
};

//affiche les touches d'action
static void hot_keys(){
    int i=0;

    invert_video(con,TRUE);
    clear_screen(con);
    while (strlen(hkeys[i])){    
        print(con,hkeys[i++]);
    }
    crlf(con);
    prompt_continue();
    invert_video(con,FALSE);
    update_display();
}

static void editor_init(){
    screen=calloc(sizeof(char),SCREEN_SIZE);
    search_info=malloc(sizeof(search_t));
    state=malloc(sizeof(ed_state_t));
    state->flags.modified=FALSE;
    state->flags.insert=TRUE;
    state->flags.new=TRUE;
    fname=calloc(sizeof(char),NAME_MAX_LEN);
    invert_video(con,FALSE);
    vga_set_cursor(CR_UNDER);
    clear_screen(con);
    fname[0]=0;
    new_file();
}//f()


void editor(const char* name){
    unsigned short key;
    FATFS fh;
    FRESULT result;
    
    editor_init();
    if (name){
        if (file_exist(name)){
            load_file(name);
        }else{
            strcpy(fname,name);
        }
    }
    quit=FALSE;
    while(!quit){
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
                enter();
                break;
            case VK_F1: // display hotkeys
                hot_keys();
                break;
            case VK_F2: // display file information
                file_info();
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
                new_file();
                break;
            case VK_CO: // <CTRL>-O  ouvrir un fichier
                open_file();
                break;
            case VK_CQ: // <CTRL>-Q  quitter
                leave_editor();
                break;
            case VK_CS: // <CTRL>-S  sauvegarde
                invert_video(con,TRUE);
                clear_screen(con);
                if (strlen(fname)) save_file(); else save_file_as();
                invert_video(con,FALSE);
                update_display();
                break;
            default:
                if ((key>=32)&(key<FONT_SIZE+32)){
                    if (state->flags.insert){
                        insert_char(key);
                    }else{
                        replace_char(key);
                    }
                }else{
                    refused();
                }
            }//switch
    }
    invert_video(con,FALSE);
    clear_screen(con);
}//f()


// cherche ligne avant from
// condition: from doit pointé le premier caractère d'une ligne
// argurments:
//   line-> buffer recevant la ligne
//   from -> adresse SRAM début ligne courante
// retourne la longueur de la ligne
uint8_t get_line_back(uint8_t *line, uint32_t from){
    int j,size;

    if (from==0){
        line[0]=0;
        return 0;
    }
    if (from<=state->gap_first){
        size=min(CHAR_PER_LINE,from);
        sram_read_block(from-size,line+(CHAR_PER_LINE-size),size);
    }else{
        size=min(from-state->tail,CHAR_PER_LINE);
        sram_read_block(from-size,line+(CHAR_PER_LINE-size),size);
        if (size<CHAR_PER_LINE && state->gap_first>0){
            j=min(CHAR_PER_LINE-size,state->gap_first);
            size+=j;
            sram_read_block(state->gap_first-j,line+(CHAR_PER_LINE-size),j);
        }
    }
    if (size<CHAR_PER_LINE) line[CHAR_PER_LINE-size-1]=0;
    j=CHAR_PER_LINE-1;
    if (!line[j]){
        j--;
    }
    while (j>0 && line[j]) j--;
    if (!line[j]) strcpy((char*)line,(char*)&line[j+1]);
    return strlen((char*)line);
}//f()


//cherche ligne débutant a partir de from
//condition: from pointe le premier caractère de la ligne courante
// arguments:
//   from -> adresse SRAM début de ligne
//   line -> buffer recevant la ligne
// retourne la longueur de la ligne
uint8_t get_line_forward(uint8_t *line, uint32_t from){
    int j,size;
    
    if (from <= state->gap_first){
        size=min(CHAR_PER_LINE-1, state->gap_first-from);
        sram_read_block(from,line,size);
        if (size<(CHAR_PER_LINE-1) && state->tail<MAX_SIZE){
            from=state->tail;
            j=min(CHAR_PER_LINE-size-1,MAX_SIZE-from);
            sram_read_block(from,&line[size],j);
            size+=j;
        }
    }else{
        size=min(CHAR_PER_LINE-1,MAX_SIZE-from);
        sram_read_block(from,line,size);
    }
    line[size]=0;
    size=strlen((char*)line);
    j=size;
    while (j<CHAR_PER_LINE) line[j++]=0;
    return size;
}//f()


static void update_display(){
    uint32_t from;
    uint8_t scr_line=0, llen=0;
    
    clear_screen(con);
    memset(screen,0,SCREEN_SIZE);
    from=state->scr_first;
    while (from < state->gap_first && scr_line<LINE_PER_SCREEN){
        llen=get_line_forward((uint8_t*)screen+scr_line*CHAR_PER_LINE,from);
        print_line(scr_line);
        scr_line++;
        from+=llen+1;
    }
    if (from<state->tail) from=state->tail;
    while (from<MAX_SIZE && scr_line<LINE_PER_SCREEN){
        llen=get_line_forward((uint8_t*)screen+scr_line*CHAR_PER_LINE,from);
        print_line(scr_line);
        scr_line++;
        from+=llen+1;
    }
    set_curpos(con,state->scr_col,state->scr_line);
}//f();


static void line_up(){
    uint8_t llen, line[CHAR_PER_LINE];
    
    line_home();
    if (state->gap_first){ 
        llen=get_line_back(line,state->gap_first);
        state->gap_first-=llen+1;
        sram_clear_block(state->gap_first,llen);
        state->tail-=llen+1;
        sram_write_block(state->tail,line,llen+1);
        state->scr_col=0;
        if (state->gap_first < state->scr_first){
            state->scr_first=state->gap_first;
            scroll_down(con);
            memmove(screen+CHAR_PER_LINE,screen, SCREEN_SIZE-CHAR_PER_LINE);
            memmove(screen,line,CHAR_PER_LINE);
            print_line(0);
        }else{
            state->scr_line--;
            memmove(screen+state->scr_line*CHAR_PER_LINE,line,CHAR_PER_LINE);
            print_line(state->scr_line);
            
        }
        set_curpos(con,state->scr_col,state->scr_line);
    }
}//f()

static void line_down(){
    uint8_t llen,line[CHAR_PER_LINE];
    
    line_end();
    if (state->tail==MAX_SIZE){
        return;
    }
    sram_write_byte(state->tail++,0);
    sram_write_byte(state->gap_first++,0);
    state->scr_col=0;
    state->scr_line++;
    if (state->scr_line==LINE_PER_SCREEN){
        state->scr_line--;
        state->scr_first+=strlen(screen)+1;
        scroll_up(con);
        memmove(screen,(void*)screen+CHAR_PER_LINE,SCREEN_SIZE-CHAR_PER_LINE);
        llen=get_line_forward(line,state->tail);
        strcpy((char*)screen+LAST_LINE*CHAR_PER_LINE,(char*)line);
        print_line(LAST_LINE);
    }
    set_curpos(con,state->scr_col,state->scr_line);
}//f()

//déplace le curseur vers la gauche d'un caractère
static void char_left(){
    if (state->scr_col){
        state->scr_col--;
        sram_write_byte(--state->tail,_screen_get(state->scr_line,state->scr_col));
        sram_write_byte(state->gap_first--,0);
        set_curpos(con,state->scr_col,state->scr_line);
    }else if (state->gap_first){
        line_up();
        line_end();
    }
}//f()

//déplace le curseur vers la droite d'un caractère
static void char_right(){
    uint8_t c;
    
    if (state->gap_first==state->fsize) return;
    if (state->scr_col<strlen((char*)screen+state->scr_line*CHAR_PER_LINE)){
        c=sram_read_byte(state->tail);
        sram_write_byte(state->gap_first++,c);
        sram_write_byte(state->tail++,0);
        state->scr_col++;
        set_curpos(con,state->scr_col,state->scr_line);
    }else{
        line_down();
    }
}//f()

static void delete_left(){
    if (state->gap_first){
        char_left();
        delete_at();
    }
} //f()

static void delete_at(){
//    int llen,i;
//    char c;
//    uint8_t line[CHAR_PER_LINE];
    
    if (state->tail==MAX_SIZE) return;
    sram_write_byte(state->tail++,0);
    state->fsize--;
    if (state->scr_col<strlen(screen+state->scr_line*CHAR_PER_LINE)){
        strcpy(screen+state->scr_line*CHAR_PER_LINE+state->scr_col,
                screen+state->scr_line*CHAR_PER_LINE+state->scr_col+1);
        print_line(state->scr_line);
        set_curpos(con,state->scr_col,state->scr_line);
    }else{
        update_display();
    }
}//f()

//efface tous les caractères après le cursor
//jusqu'à la fin de ligne.
//si le curseur est au début de ligne
//la ligne est supprimée au complet '\n' inclus.
static void delete_to_end(){
    uint8_t llen,c;
    
    if (state->fsize==0) return;
    llen=strlen((char*)screen+state->scr_line*CHAR_PER_LINE);
    if (state->scr_col){
        memset(screen+state->scr_line*CHAR_PER_LINE+state->scr_col,0,CHAR_PER_LINE-state->scr_col);
        c=llen-state->scr_col;
        sram_clear_block(state->tail,c);
        state->fsize-=c;
        state->tail+=c;
        print_line(state->scr_line);
    }else{
        memset(screen+state->scr_line*CHAR_PER_LINE,0,CHAR_PER_LINE);
        sram_clear_block(state->tail,llen);
        state->tail+=llen;
        state->fsize-=llen;
        if (state->tail<ED_BUFF_SIZE){
            c=sram_read_byte(state->tail);
            if (c==0){
                sram_write_byte(state->tail,0);
                state->tail++;
                state->fsize--;
            }
        }
        update_display();
    }
    state->flags.modified=TRUE;
}//f()

static void delete_to_start(){
    uint8_t buffer[CHAR_PER_LINE];
    
    if (state->scr_col){
        memset(buffer,0,CHAR_PER_LINE);
        state->gap_first-=state->scr_col;
        sram_write_block(state->gap_first,buffer,state->scr_col);
        memmove(screen+state->scr_line*CHAR_PER_LINE,
                screen+state->scr_line*CHAR_PER_LINE+state->scr_col,CHAR_PER_LINE-state->scr_col);
        state->fsize-=state->scr_col;
        state->scr_col=0;
        print_line(state->scr_line);
        state->flags.modified=TRUE;
    }
}//f()

static void insert_char(char c){
    if (state->fsize==MAX_SIZE){
        refused();
        return;
    }
    print_int(SERIO,strlen(screen+state->scr_line*CHAR_PER_LINE),0);crlf(SERIO);
    if (strlen(screen+state->scr_line*CHAR_PER_LINE)<LINE_MAX_LEN){
        sram_write_byte(state->gap_first++,c);
        state->fsize++;
        memmove(screen+state->scr_line*CHAR_PER_LINE+state->scr_col+1,
                screen+state->scr_line*CHAR_PER_LINE+state->scr_col,CHAR_PER_LINE-state->scr_col-1);
        _screen_put(state->scr_line,state->scr_col,c);
        print_line(state->scr_line);
        state->scr_col++;
        set_curpos(con,state->scr_col,state->scr_line);
        state->flags.modified=TRUE;
    }else{
        sram_write_byte(state->gap_first++,0);
        sram_write_byte(state->gap_first++,c);
        state->fsize+=2;
        state->scr_col=1;
        state->scr_line++;
        if (state->scr_line>LAST_LINE){
            state->scr_first+=strlen(screen)+1;
            state->scr_line=LAST_LINE;
        }
        state->flags.modified=TRUE;
        update_display();
    }
}//f()

static void replace_char(char c){
    int llen;
    llen=strlen((char*)screen+state->scr_line*CHAR_PER_LINE);
    if (state->scr_col<llen){
        sram_write_byte(state->gap_first++,c);
        state->tail++;
        _screen_put(state->scr_line,state->scr_col,c);
        put_char(con,c);
        state->scr_col++;
        state->flags.modified=TRUE;
        set_curpos(con,state->scr_col,state->scr_line);
    }else{
        insert_char(c);
    }
}//f();

static void line_home(){
    if (state->scr_col){
        state->tail-=state->scr_col;
        sram_write_block(state->tail,
                (uint8_t*)screen+state->scr_line*CHAR_PER_LINE,state->scr_col);
        state->gap_first-=state->scr_col;
        sram_clear_block(state->gap_first,state->scr_col);
        state->scr_col=0;
        set_curpos(con,0,state->scr_line);
    }
}//f()

static void line_end(){
    uint8_t llen,forward;
    
    llen=strlen((char*)screen+state->scr_line*CHAR_PER_LINE);
    forward=llen-state->scr_col;
    if (forward){
        sram_write_block(state->gap_first,(uint8_t*)screen+state->scr_line*CHAR_PER_LINE+state->scr_col,forward);
        state->gap_first+=forward;
        sram_clear_block(state->tail,forward);
        state->tail+=forward;
        state->scr_col=llen;
        set_curpos(con,state->scr_col,state->scr_line);
    }
}//f()

 static void enter(){
    if ((state->flags.insert) && (state->fsize==MAX_SIZE)){
        refused();
        return;
    }
    if (state->flags.insert){
        state->fsize++;
    }else{
        sram_write_byte(state->tail++,0);
    }
    sram_write_byte(state->gap_first++,0);
    state->scr_col=0;
    state->scr_line++;
    if (state->scr_line==LINE_PER_SCREEN){
        state->scr_first+=strlen(screen)+1;
        state->scr_line--;
    }
    update_display();
}//f()

static void file_home(){
    uint8_t size, maxlen, line[CHAR_PER_LINE];
    if (state->fsize==MAX_SIZE){
        state->scr_first=0;
        state->gap_first=0;
        state->tail=0;
    }else{
        maxlen=min(CHAR_PER_LINE,state->tail - state->gap_first);
        while (state->gap_first){
            size=min(maxlen,state->gap_first);
            state->gap_first-=size;
            sram_read_block(state->gap_first,line,size);
            sram_clear_block(state->gap_first,size);
            state->tail-=size;
            sram_write_block(state->tail,line,size);
        }
        state->scr_first=0;
    }
    state->scr_col=0;
    state->scr_line=0;
    update_display();
}//f()

static void file_end(){
    int size;
    uint8_t maxlen, line[CHAR_PER_LINE];
    
    if (state->gap_first==state->fsize) return;
    if (state->fsize==MAX_SIZE){
        state->scr_first=MAX_SIZE-1;
        state->gap_first=MAX_SIZE-1;
        state->tail=MAX_SIZE;
    }else{
        maxlen=min(CHAR_PER_LINE,state->tail - state->gap_first);
        while (state->gap_first < state->fsize){
            size=min(maxlen,MAX_SIZE-state->tail);
            sram_read_block(state->tail,line,size);
            sram_clear_block(state->tail,size);
            sram_write_block(state->gap_first,line,size);
            state->tail+=size;
            state->gap_first+=size;
        }
        state->scr_first=state->gap_first;
        state->scr_col=0;
        state->scr_line=0;
    }
    update_display();
}//f()

static void page_up(){
    int i=0;

    while (state->scr_line) line_up();
    i=LINE_PER_SCREEN;
//    dis_video=TRUE;
    while (state->scr_first && i){
        line_up();
        i--;
    }
//    dis_video=FALSE;
}

static void page_down(){
     while (state->gap_first<(state->fsize-1) && state->scr_line<LAST_LINE){
         line_down();
     }
    if ((state->scr_line==LAST_LINE) && (state->scr_col==0)){
        state->scr_first=state->gap_first;
        state->scr_line=0;
    }
    update_display();
}


static void word_right(){
    uint8_t c, scr_col;
    uint8_t line[CHAR_PER_LINE];
    
    if (state->scr_col==strlen((char*)screen+state->scr_line*CHAR_PER_LINE)) return;
    scr_col=state->scr_col;
    c=_screen_get(state->scr_line,scr_col++);
    if (!isalnum(c)){
        while (c && !isalnum(c)){
            c=_screen_get(state->scr_line,scr_col++);
        }
    }else{
        while (c  && isalnum(c)){
            c=_screen_get(state->scr_line,scr_col++);
        }    
    }
    scr_col--;
    if (scr_col>state->scr_col){
        sram_read_block(state->tail,line,scr_col-state->scr_col);
        sram_clear_block(state->tail,scr_col-state->scr_col);
        state->tail+=scr_col-state->scr_col;
        sram_write_block(state->gap_first,line,scr_col-state->scr_col);
        state->gap_first+=scr_col-state->scr_col;
        state->scr_col=scr_col;
        set_curpos(con,scr_col,state->scr_line);
    }
}

static void word_left(){
    uint8_t c, scr_col, diff;
    uint8_t line[CHAR_PER_LINE];
    
    if (!state->scr_col) return;
    scr_col=state->scr_col-1;
    c=_screen_get(state->scr_line,scr_col);
    if (!isalnum(c)){
        while (scr_col && !isalnum(c)){
           --scr_col;
           c=_screen_get(state->scr_line,scr_col);
        }
    }else{
        while (scr_col && isalnum(c)){
           --scr_col;
           c=_screen_get(state->scr_line,scr_col);
            
        }
    }
    if (scr_col) scr_col++;
    if (scr_col<state->scr_col){
        diff=state->scr_col-scr_col;
        state->gap_first-=diff;
        sram_read_block(state->gap_first,line,diff);
        sram_clear_block(state->gap_first,diff);
        state->tail-=diff;
        sram_write_block(state->tail,line,diff);
        state->scr_col=scr_col;
        set_curpos(con,scr_col,state->scr_line);
    }
}

static void jump_to_line(uint16_t line_no){
    uint16_t llen,count;
    uint8_t line[CHAR_PER_LINE];
    
    file_home();
    if (!line_no) return;
    line_no--;
    if (line_no<LINE_PER_SCREEN){
        while(state->scr_line<line_no) line_down();
        return;
    }
//    _disable_video();
    count=0;
    while ((state->tail<MAX_SIZE) && (count<line_no)){
        llen=get_line_forward(line,state->tail);
        sram_write_block(state->gap_first,line,llen+1);
        state->gap_first+=llen+1;
        sram_clear_block(state->tail,llen+1);
        state->tail+=llen+1;
        count++;
    }
    state->scr_first=state->gap_first;
    set_curpos(con,0,0);
    update_display();
}//f

static void goto_line(){
    uint8_t llen;
    char line[16];
    uint16_t line_no=0;
    
    invert_video(con,TRUE);
    set_curpos(con,0,0);
    clear_eol(con);
    print(con,"goto line: ");
    llen=read_line(con,line,16);
    invert_video(con,FALSE);
    if (llen){
        line_no=atoi((const char*)line);
        jump_to_line(line_no);
    }else{
        print_line(0);
    }
    
}
