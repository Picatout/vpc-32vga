/*
* Copyright 2013, Jacques Deschênes
* This file is part of VPC-32.
*
*     VPC-32 is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     VPC-32 is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with VPC-32.  If not, see <http://www.gnu.org/licenses/>.
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
 *      forth  lance l'environnement vpForthsearch_command
 *      puts mot  imprime à l'écran le mot qui suis
 *      expr {expression}  évalue une expression et retourne le résultat
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <plib.h>

#include "hardware/HardwareProfile.h"
#include "hardware/ps2_kbd/keyboard.h"
#include "console.h"
#include "hardware/Pinguino/ff.h"
#include "hardware/Pinguino/fileio.h"
#include "shell.h"
//#include "vpcBASIC/vpcBASIC.h"
#include "hardware/rtcc/rtcc.h"
//#include "hardware/serial_comm/serial_comm.h"
#include "console.h"

#define MAX_LINE_LEN 80

static const char _version[]="1.0";
static const char *console_name[]={"VGA","SERIAL"};
static const char _true[]="T";
static const char _false[]="F";
static const char _nil[]="";

//jmp_buf back_to_cmd_line;

//dev_t con=SERIAL_CONSOLE;
dev_t con=VGA_CONSOLE;

static const env_var_t shell_version={NULL,"SHELL_VERSION",(char*)_version};
static const env_var_t T={(env_var_t*)&shell_version,"TRUE",(char*)_true};
static const env_var_t F={(env_var_t*)&T,"FALSE",(char*)_false};
static const env_var_t nil={(env_var_t*)&F,"NIL",(char*)_nil};
static env_var_t *shell_vars=(env_var_t*)&nil;

static env_var_t *search_var(const char *name);
static void erase_var(env_var_t *var);
static char* exec_script(const char *script);


typedef struct{
    const char *script; // chaîne à analyser
    unsigned  len;  // longueur de la chaîne.
    unsigned  next; // position du curseur de l'analyseur.
    int err_pos;
} parse_str_t;

static void skip(parse_str_t *parse, const char *skip);

static const char *ERR_MSG[]={
    "no error\n",
    "unknown command.\n",
    "syntax error.\n",
    "not implemented yet.\n",
    "Memory allocation error.\n",
    "Bad usage.\n",
    "File open error.\n",
    "Copy error.\n",
    "Mkdir error.\n",
    "file does not exist.\n",
    "operation denied.\n",
    "disk operation error, code is %d\n",
    "no SD card detected.\n"
};


void print_error_msg(SH_ERROR err_code,const char *detail,FRESULT io_code){
    char *fmt;
    if (err_code==ERR_FIO){
        fmt=malloc(64);
        if (fmt){
            sprintf(fmt,ERR_MSG[ERR_FIO],io_code);
            print(con,fmt);
            free(fmt);
        }
    }else{
       print(con,ERR_MSG[err_code]);
    }
    if (detail){
       print(con,detail);
    }
}//print_error_msg()


static int nbr_cmd;

typedef struct shell_cmd{
    char *name;
    char *(*fn)(int, char**);
}shell_cmd_t;

static const shell_cmd_t commands[];

//Affichage de la date et heure du dernier shutdown
//enerigistré dans le RTCC.
void last_shutdown(){
    alm_state_t shutdown;
    char fmt[32];
    
    rtcc_power_down_stamp(&shutdown);
    if (shutdown.day){
        sprintf(fmt,"Last power down: %s %02d/%02d %02d:%02d\n",weekdays[shutdown.wkday],
                shutdown.month,shutdown.day,shutdown.hour,shutdown.min);
        print(con,fmt);
    }
}

static int search_command(const char *target){
    int i;
    for (i=nbr_cmd-1;i>=0;i--){
        if (!strcmp(target,commands[i].name)){
            break;
        }
    }
    return i;
}//search_command()

static char* cmd_help(int tok_count, char  **tok_list){
    int i;
    text_coord_t pos;
    for(i=0;i<nbr_cmd;i++){
        pos.xy=get_curpos(con);
        if (pos.x>(CHAR_PER_LINE-strlen(commands[i].name)-2)){
            put_char(con,'\n');
        }
        print(con,commands[i].name);
        if (i<(nbr_cmd-1)){
            print(con," ");
        }
    }
    put_char(con,'\n');
    return NULL;
}

static char* cmd_cls(int tok_count, char  **tok_list){
    clear_screen(con);
    return NULL;
}

// calibration oscillateur du RTCC
// +-127 ppm
static char* cmd_clktrim(int tok_count, char  **tok_list){
    int trim;
    char fmt[64];
    if (tok_count>1){
        trim=atoi(tok_list[1]);
        trim=rtcc_calibration(trim);
    }else{
        print(con,
            "RTCC oscillator calibration\n"
            "USAGE: clktrim n\n"
            "n is added to actual value\n"
            "n is in range {-127..127}\n");
        trim=rtcc_calibration(0);
    }
    sprintf(fmt,"Actual RTCC oscillator trim value: %d",trim);
    print(con,fmt);
    return NULL;
}


// imprime le temps depuis
// le démarrage de l'ordinateur
static char* cmd_uptime(int tok_count, char  **tok_list){
    unsigned sys_ticks;
    unsigned day,hour,min,sec,remainder;
    char fmt[32];
    
    sys_ticks=ticks();
    day=sys_ticks/86400000L;
    remainder=sys_ticks%86400000L;
    hour=remainder/3600000L;
    remainder%=3600000L;
    min=remainder/60000;
    remainder%=60000;
    sec=remainder/1000;
    sprintf(fmt,"%02dd%02dh%02dm%02ds\n",day,hour,min,sec);
    print(con,fmt);
    return NULL;
}


static char* cmd_format(int tok_count, char  **tok_list){
    if (tok_count==2){
        print_error_msg(ERR_NOT_DONE,NULL,0);
    }else{
        print(con,"USAGE: format volume_name\n");
    }
    return NULL;
}

#include "vpcBASIC/BASIC.h"

static void cmd_basic_help(){
    println(con,"USAGE: basic [-?]|[-c|-k string]| file_name");
    println(con,"-?  display this help.");
    println(con,"-c or -k \"code\", \"code\" is BASIC code to be executed.");
    println(con,"Use -c to leave BASIC or -k to stay after code execution.");
    println(con,"file_name, is a basic file to execute. Stay in BASIC at program exit.");
}

// basic [-h n] [fichier.bas]
// -h -> espace réservé pour l'allocation dynamique 4096 octets par défaut.
static char* cmd_basic(int tok_count, char  **tok_list){
    char *fmt=NULL, *basic_file=NULL;
    int i;
    unsigned heap=DEFAULT_HEAP;
    unsigned option=BASIC_PROMPT;
    
//#define TEST_VM
#ifdef TEST_VM
    int exit_code;
    exit_code=test_vm();
    fmt=malloc(32);
    sprintf(fmt,"VM exit code: %d\n",exit_code);
#else    
    for (i=1;i<tok_count;i++){
        if (!strcmp(tok_list[i],"-?")){
            i=tok_count;
            cmd_basic_help();
            return fmt;
        }
        if (!strcmp(tok_list[i],"-h")){
            i++;
            heap=atoi(tok_list[i]);
        }else if (!strcmp(tok_list[i],"-c")){
            i++;
            basic_file=(char*)tok_list[i];
            option=EXEC_STRING;
        }else if (!strcmp(tok_list[i],"-k")){
            i++;
            basic_file=(char*)tok_list[i];
            option=EXEC_STAY;
        }else{
            basic_file=(char*)tok_list[i];
            option=EXEC_FILE;
        }
    }//for
    BASIC_shell(heap,option,basic_file);
#endif    
    return fmt;
}

static char* cmd_cd(int tok_count, char  **tok_list){ // change le répertoire courant.
    char *path;
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    FRESULT error=FR_OK;
   if (tok_count==2){
       error=f_chdir(tok_list[1]);
   }
   if (!error){
       path=malloc(255);
       if (path){
          error=f_getcwd(path,255);
          if(!error){
              print(con,path);
              put_char(con,'\n');
          }
          free(path);
       }
   }
    return NULL;
}//cmd_cd()

static char* cmd_del(int tok_count, char  **tok_list){ // efface un fichier
    FILINFO *fi;
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    FRESULT error=FR_OK;
    if (tok_count==2){
        fi=malloc(sizeof(FILINFO));
        if (fi){
            error=f_stat(tok_list[1],fi);
            if (!error){
                if (fi->fattrib & (ATT_DIR|ATT_RO)){
                    print_error_msg(ERR_DENIED,"can't delete directory or read only file.\n",0);
                }
                else{
                    error=f_unlink(tok_list[1]);
                }
            }
            free(fi);
            if (error){
                print_error_msg(ERR_FIO,"",error);
            }
        }else{
               print_error_msg(ERR_ALLOC,"delete failed.\n",0);
        }
   }else{
       print_error_msg(ERR_USAGE, "delete file USAGE: del file_name\n",0);
   }
    return NULL;
}//del()

static char* cmd_ren(int tok_count, char  **tok_list){ // renomme un fichier
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    if (tok_count==3){
        f_rename(tok_list[1],tok_list[2]);
    }else{
        print_error_msg(ERR_USAGE,"rename file, USAGE: ren name new_name\n",0);
    }
    return NULL;
}//ren

static char* cmd_copy(int tok_count, char  **tok_list){ // copie un fichier
    FIL *fsrc, *fnew;
    char *buff;
    int n;
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    FRESULT error;
    if (tok_count==3){
        fsrc=malloc(sizeof(FIL));
        fnew=malloc(sizeof(FIL));
        buff=malloc(512);
        error=FR_OK;
        if (fsrc && fnew && buff){
            if ((error=f_open(fsrc,tok_list[1],FA_READ)==FR_OK) &&
                (error=f_open(fnew,tok_list[2],FA_CREATE_NEW|FA_WRITE)==FR_OK)){
                while ((error=f_read(fsrc,buff,512,&n))==FR_OK){
                    if (n){
                        if (!((error=f_write(fnew,buff,n,&n))==FR_OK)){
                            break;
                        }
                    }else{
                         break;
                    }
                }//while
                f_close(fsrc);
                f_close(fnew);
                free(buff);
                free(fsrc);
                free(fnew);
            }
            if (error){
                print_error_msg(ERR_FIO,"copy failed.\n",error);
            }
        }else{
            print(con,ERR_MSG[ERR_ALLOC]);
        }
    }else{
        print_error_msg(ERR_USAGE,"copy file USAGE: copy file_name new_file_name\n",0);
    }
    return NULL;
}//copy()

static char* cmd_send(int tok_count, char  **tok_list){ // envoie un fichier via uart
    // to do
   if (tok_count==2){
       print_error_msg(ERR_NOT_DONE,NULL,0);
   }else{
       print(con, "send file via serial, USAGE: send file_name\n");
   }
   return NULL;
}//cmd_send()

static char* cmd_receive(int tok_count, char  **tok_list){ // reçois un fichier via uart
    // to do
   if (tok_count==2){
       print_error_msg(ERR_NOT_DONE,NULL,0);
   }else{
       print(con, "receive file from serial, USAGE: receive file_name\n");
   }
   return NULL;
}//cmd_receive()

static char* cmd_hdump(int tok_count, char  **tok_list){ // affiche un fichier en hexadécimal
    FIL *fh;
    unsigned char *fmt, *buff, *rbuff, c,key,line[18];
    int n,col=0,scr_line=0;
    unsigned addr=0;
    
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    FRESULT error=FR_OK;
    if (tok_count==2){
        fh=malloc(sizeof(FIL));
        if (fh && ((error=f_open(fh,tok_list[1],FA_READ))==FR_OK)){
            if (con==VGA_CONSOLE) clear_screen(con);
            buff=malloc(512);
            fmt=malloc(CHAR_PER_LINE);
            if (fmt && buff){
                key=0;
                line[16]=LF;
                line[17]=0;
                while (key!=ESC && f_read(fh,buff,512,&n)==FR_OK){
                    if (!n) break;
                    rbuff=buff;
                    for(;n && key!=ESC;n--){
                        if (!col){
                            sprintf(fmt,"%08X  ",addr);
                            print(con,fmt);
                        }
                        c=*rbuff++;
                        sprintf(fmt,"%02X ",c);
                        //print_hex(con,c,2); put_char(con,32);
                        if (c>=32) line[col]=c; else line[col]=32;
                        print(con,fmt);
                        col++;
                        if (col==16){
                            print(con,line);
                            col=0;
                            addr+=16;
                            scr_line++;
                            if (scr_line==(LINE_PER_SCREEN-1)){
                                print(con,"more...");
                                key=wait_key(con);
                                clear_screen(con);
                                scr_line=0;
                            }
                        }
                    }
                }
                if (col){
                    strcpy(fmt,"   ");
                    while (col<16){
                        print(con,fmt);
                        line[col]=32;
                        col++;
                    }
                    print(con,line);
                }
                f_close(fh);
                free(fh);
                free(buff);
                free(fmt);
            }else{
                print_error_msg(ERR_ALLOC,"Can't display file.\n",0);
            }
        }else{
            print_error_msg(ERR_FIO,"File open failed.\n",error);
        }
   }else{
       print_error_msg(ERR_USAGE, "USAGE: more file_name\n",0);
   }
    return NULL;
}//f

static char* cmd_mount(int tok_count, char  **tok_list){// mount SDcard drive
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    return NULL;
}

static char* cmd_umount(int tok_count, char  **tok_list){
    unmountSD();
    SDCardReady=FALSE;
    return NULL;
}

// affiche à l'écran le contenu d'un fichier texte
static char* cmd_more(int tok_count, char  **tok_list){
    FIL *fh;
    char *fmt, *buff, *rbuff, c, prev,key;
    int n,lcnt,colcnt=0;
    text_coord_t cpos;
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    FRESULT error=FR_OK;
    if (tok_count==2){
        clear_screen(con);
        fh=malloc(sizeof(FIL));
        if (fh && ((error=f_open(fh,tok_list[1],FA_READ))==FR_OK)){
            buff=malloc(512);
            fmt=malloc(CHAR_PER_LINE);
            if (fmt && buff){
                key=0;
                while ((key!='q' || key!='Q') && f_read(fh,buff,512,&n)==FR_OK){
                    if (!n) break;
                    rbuff=buff;
                    for(;n;n--){
                        c=*rbuff++;
                        if (!(c==TAB || c==LF) && (c<32 || c>126)) {c=32;}
                        put_char(con,c);
                        cpos.xy=get_curpos(con);
                        if (cpos.x==0){
                            if (cpos.y>=(LINE_PER_SCREEN-1)){
                                cpos.y=LINE_PER_SCREEN-1;
                                set_curpos(con,cpos.x,cpos.y);
                                invert_video(con,TRUE);
                                print(con,"-- next --");
                                invert_video(con,FALSE);
                                key=wait_key(con);
                                if (key==CR){
                                    set_curpos(con,cpos.x,cpos.y);
                                    clear_eol(con);
                                }else{
                                    clear_screen(con);
                                }//if
                            }//if
                        }//if
                    }//for
                }//while
                f_close(fh);
                free(fh);
                free(buff);
                free(fmt);
            }else{
                print_error_msg(ERR_ALLOC,"Can't display file.\n",0);
            }
        }else{
            print_error_msg(ERR_FIO,"File open failed.\n",error);
        }
   }else{
       print_error_msg(ERR_USAGE, "USAGE: more file_name\n",0);
   }
    return NULL;
}//more

static char* cmd_edit(int tok_count, char  **tok_list){ // lance l'éditeur de texte
    if (tok_count>1){
        editor(tok_list[1]);
    }else{
        editor(NULL);
    }
    return NULL;
}//f

static char* cmd_mkdir(int tok_count, char  **tok_list){
    FRESULT error=FR_OK;
    char *fmt;
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    if (tok_count==2){
        fmt=malloc(CHAR_PER_LINE+1);
        if (fmt && (error=f_mkdir(tok_list[1])==FR_OK)){
            sprintf(fmt,"directory %s created\n",tok_list[1]);
            print(con,fmt);
        }else{
            if (!fmt){
                print(con,ERR_MSG[ERR_ALLOC]);
            }else{
                print(con,ERR_MSG[ERR_MKDIR]);
            }
        }
    }else{
        print_error_msg(ERR_USAGE,"mkdir create a directory, USAGE: mkdir dir_name\n",0);
    }
    return NULL;
}// mkdir()


// liste les fichiers 
// S'il n'y a pas de spécification après la commande DIR affiche
// les fichiers du répertoire courant.
// S'il y a une spécification et qu'il s'agit d'un chemin de répertoire
// affiche les fichiers de ce répertoire.
// Si le chemin se termine par un filtre seul les fichiers
// correspondant à ce filtre sont affichés.
// filtre:
//   *chaîne   Affiche les fichiers dont le nom se termine par "chaîne"
//   chaîne*   Affiche les fichiers dont le nom débute par "chaîne"
//   *chaîne*  Affiche les fichiers dont le nom contient "chaîne"
//   chaine    Affiche les spécifications de ce fichier unique.
static char* cmd_dir(int tok_count, char **tok_list){
    FRESULT error;
    DIR *dir;
    FILINFO *fi;
    char *path;
    
    char fmt[55];
    filter_t *filter;
    
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return NULL;
        }else{
            SDCardReady=TRUE;
        }
    }
    filter=malloc(sizeof(filter_t));
    dir=malloc(sizeof(DIR));
    fi=malloc(sizeof(FILINFO));
    if (!(filter && dir && fi)){
        free(dir);
        free(filter);
        print_error_msg(ERR_ALLOC,NULL,0);
        return NULL;
    }
    filter->criteria=eNO_FILTER;
    if (tok_count>1){
        path=set_filter(filter,tok_list[1]);// println(con,path); println(con,filter->subs);
    }else{
        path=(char*)current_dir;
    }
    error=listDir(path,filter);
    if ((error==FR_NO_PATH) && (error=f_stat(path,fi)==FR_OK)){
        print_fileinfo(fi);
    }
    free(fi);
    free(dir);
    free(filter);
    if (error) print_error_msg(ERR_FIO,"",error);
    return NULL;
}//list_directory()

static char* cmd_puts(int tok_count, char  **tok_list){
    print(con, "puts, to be done.\n");
    return NULL;
}//puts()

static char* cmd_expr(int tok_count, char  **tok_list){
    print(con, "expr, to be done.\n");
    return NULL;
}//expr()

//display heap status
static char* cmd_free(int tok_count, char  **tok_list){
    char *free_ram;
    free_ram=calloc(sizeof(char),80);
    sprintf(free_ram,"free RAM %d/%d BYTES\n",free_heap(),heap_size);
    return free_ram;
}

static void parse_time(char *time_str,stime_t *time){
    char *str;
    unsigned short hr,min=0,sec=0;
    
    hr=atoi(time_str);
    str=strchr(time_str,':');
    if (str){
        min=atoi(++str);
        str=strchr(str,':');
        if (str) sec=atoi(++str);
    }
    time->hour=hr;
    time->min=min;
    time->sec=sec;
}

static void parse_date(char *date_str,sdate_t *date){
    unsigned y,m=1,d=1;    
    char *str;

    y=atoi(date_str);
    y+=y<100?2000:0;
    str=strchr(date_str,'/');
    if (str){
        m=atoi(++str);
        str=strchr(str,'/');
        if (str){
            d=atoi(++str);
        }else{
            d=1;
        }
    }else{
        m=1;
    }
    date->day=d;
    date->month=m;
    date->year=y;
    date->wkday=day_of_week(date);
}

// affiche ou saisie de la date
// format saisie: [yy]yy/mm/dd
static char* cmd_date(int tok_count, char  **tok_list){
    char *fmt;
    sdate_t date;
    
    fmt=calloc(sizeof(char),80);
    if (tok_count>1){
        parse_date((char*)tok_list[1],&date);
        rtcc_set_date(date);
        if (rtcc_error) sprintf(fmt,"rtcc_set_date() error.");
    }else{
        rtcc_get_date_str(fmt);
    }
    return fmt;
}

// affiche ou saisie de  l'heure
// format saisie:  hh:mm:ss
static char* cmd_time(int tok_count, char  **tok_list){
    char *fmt;
    stime_t t;
    
    fmt=calloc(sizeof(char),80);
    if (tok_count>1){
        parse_time((char*)tok_list[1],&t);
        rtcc_set_time(t);
        if (rtcc_error) sprintf(fmt,"error set_time");
    }else {
        rtcc_get_time_str(fmt);
    }
    return fmt;
}

static void report_alarms_state(){
    char fmt[80];
    int i,wkday;
    
    sdate_t date;
    alm_state_t state[2];
    rtcc_get_date(&date);
    rtcc_get_alarms(state);
    for (i=0;i<2;i++){
        if (state[i].enabled){
            sprintf(fmt,"alarm %d set to %s %d/%02d/%02d %02d:%02d:%02d  %s\n",i,
                    weekdays[state[i].wkday-1],date.year,state[i].month,
                    state[i].day,state[i].hour,state[i].min,state[i].sec,
                    (char*)state[i].msg);
            print(con,fmt);
        }else{
            sprintf(fmt,"alarm %d inactive\n",i);
            print(con,fmt);
        }
    }
    
}

static char* cmd_alarm(int tok_count, char **tok_list){
    sdate_t date;
    stime_t time;
    char msg[32];
    
    switch (tok_count){
        case 5:
            if (!strcmp(tok_list[1],"-s")){
                parse_date((char*)tok_list[2],&date);
                parse_time((char*)tok_list[3],&time);
                strcpy(msg,tok_list[4]);
                msg[31]=0;
                if (!rtcc_set_alarm(date,time,(uint8_t*)msg)){
                    print(con, "Failed to set alarm, none free.\n");
                }
                break;
            }
        case 2:
            if (!strcmp(tok_list[1],"-d")){
                report_alarms_state();
                break;
            }
        case 3:
            if (!strcmp(tok_list[1],"-c")){
                rtcc_cancel_alarm(atoi(tok_list[2]));
                break;
            }
        default:
            print(con,"USAGE: alarm []|[-c 0|1]|[-d ]|[-s date time \"message\"]\n");
    }//switch
    return NULL;
}

static char* cmd_echo(int tok_count, char  **tok_list){
    int j;
    
    for (j=1;j<tok_count;j++){
        print(con,tok_list[j]);
        spaces(con,1);
    }
    return NULL;
}

static char* cmd_reboot(int tok_count, char  **tok_list){
    asm("lui $t0, 0xbfc0"); // _on_reset
    asm("j  $t0\n nop");
}


static env_var_t *search_var(const char *name){
    env_var_t *list;
    list=shell_vars;
    while (list){
        if (!strcmp(name,list->name)) break;
        list=list->link;
    }
    return list;
}

static void erase_var(env_var_t *var){
    env_var_t *prev, *list;
   
    if (!(var && _is_ram_addr(var))) return;
    prev=NULL;
    list=shell_vars;
    while (list && (list!=var)){
        prev=list;
        list=list->link;
    }
    if (!prev){
        shell_vars=var->link;
        free(var->name);
        free(var->value);
        free(var);
    }else if (list) {
        prev->link=var->link;
        free(var->name);
        free(var->value);
        free(var);
    }
}

static void list_vars(){
    char *name, *value;
    env_var_t *list;
    list=shell_vars;
    while (list){
        name=list->name;
        if (name){
            print(con,name);
            put_char(con,'=');
            println(con,list->value);
        }
        list=list->link;
    }
}

static char* cmd_set(int tok_count, char  **tok_list){
    env_var_t *var;
    char *name, *value;
    if (tok_count>=2){
        var=search_var(tok_list[1]);
        if (var){
            if (!_is_ram_addr(var)){
                print_error_msg(ERR_DENIED,"Read only variable.",0);
            }else if (tok_count==2){
                    erase_var(var);
                }else{
                    value=malloc(strlen(tok_list[2])+1);
                    strcpy(value,tok_list[2]);
                    free(var->value);
                    var->value=value;
                }
        }else if (tok_count==3){//nouvelle variable
            var=malloc(sizeof(env_var_t));
            name=malloc(strlen(tok_list[1])+1);
            value=malloc(strlen(tok_list[2])+1);
            if (!(var && name && value)){
                print_error_msg(ERR_ALLOC,"insufficiant memory",0);
                return;
            }
            strcpy(name,tok_list[1]);
            strcpy(value,tok_list[2]);
            var->link=shell_vars;
            var->name=name;
            var->value=value;
            shell_vars=var;
        }
    }else{
        list_vars();
    }
    return NULL;
}

static char *cmd_con(int tok_count, char** tok_list){
#define DISPLAY_NAME (-2)
    char *result;
    int console_id=-1;
    
    result=malloc(80);
    if (tok_count==2){
        if (!strcmp("local",tok_list[1])){
            console_id=VGA_CONSOLE;
        }else if (!strcmp("serial",tok_list[1])){
            if (vt_init()){
                console_id=SERIAL_CONSOLE;
            }else{
                sprintf(result,"switching console failed, VT terminal not ready\n");
                return result;
            }
        }else if (!strcmp("-n",tok_list[1])){
            console_id=DISPLAY_NAME;
        }
    }
    switch (console_id){
        case VGA_CONSOLE:
        case SERIAL_CONSOLE:
            if (con!=console_id){
                sprintf(result,"switched to %s console.",console_name[console_id]);
                println(con,result);
                con=console_id;
                sprintf(result,"new console %s",console_name[con]);
            }
            break;
        case DISPLAY_NAME:
            sprintf(result,"%s",console_name[con]);
            break;
        default:
            sprintf(result,"Select console\nUSAGE: con -n|local|serial.");
    }//switch
    return result;
}

static inline bool try_file_type(char *file_name,const char *ext){
    return strstr(file_name,ext);
}

// run file
// exécute un fichier basic (*.bas) ou binaire (*.com)
// revient au shell de commandes à la sortie du programme.
static char  *cmd_run(int tok_count, char** tok_list){
    int err;
    char fmt[80];
    FILINFO fi;
    FRESULT res;
    
    if (tok_count<2){
        println(con,"missing argument");
        println(con,"USAGE: run file_name");
        return NULL;
    }
    uppercase(tok_list[1]);
    if (try_file_type(tok_list[1],".BAS")){
        BASIC_shell(DEFAULT_HEAP,EXEC_FILE,tok_list[1]);
    }else if (try_file_type(tok_list[1],".COM")){
        //to be done
    }else{ 
        if (!strchr(tok_list[1],'.')){
            sprintf(fmt,"%s%s",tok_list[1],".BAS");
            res=f_stat(fmt,&fi);
            if (!res){
                BASIC_shell(DEFAULT_HEAP,EXEC_FILE,fmt);
            }else{
                sprintf(fmt,"%s%s",tok_list[1],".COM");
                res=f_stat(fmt,&fi);
                if (!res){
                    // to be done
                }else{
                    print_error_msg(ERR_NOT_CMD,tok_list[0],0);
                }
            }
        }
    } 
    return NULL;
}//f



static const shell_cmd_t commands[]={
    {"alarm",cmd_alarm},
    {"cd",cmd_cd},
    {"clktrim",cmd_clktrim},
    {"cls",cmd_cls},
    {"con",cmd_con},
    {"copy",cmd_copy},
    {"date",cmd_date},
    {"del",cmd_del},
    {"dir",cmd_dir},
    {"echo",cmd_echo},
    {"edit",cmd_edit},
    {"expr",cmd_expr},
    {"free",cmd_free},
    {"format",cmd_format},
    {"basic",cmd_basic},
    {"hdump",cmd_hdump},
    {"help",cmd_help},
    {"mkdir",cmd_mkdir},
    {"mount",cmd_mount},
    {"more",cmd_more},
    {"puts",cmd_puts},
    {"reboot",cmd_reboot},
    {"receive",cmd_receive},
    {"ren",cmd_ren},
    {"send",cmd_send},
    {"set",cmd_set},
    {"time",cmd_time},
    {"umount",cmd_umount},
    {"uptime",cmd_uptime},
    {"run",cmd_run}
};

static int nbr_cmd=sizeof(commands)/sizeof(shell_cmd_t);


static char *try_file(int tok_count, char **tok_list){
    FILINFO fi;
    FRESULT res;
    
    if (tok_count==1){
        tok_list[1]=tok_list[0];
        return cmd_run(2,tok_list);
    }else{
        print_error_msg(ERR_NOT_CMD,tok_list[0],0);
    }
    return NULL;
}

static char* execute_cmd(int tok_count, char  **tok_list){
    int cmd;
        cmd=search_command(tok_list[0]);
        if (cmd>=0){
            return commands[cmd].fn(tok_count,tok_list);
        }else{
            return try_file(tok_count,tok_list);
        }
}// execute_cmd()

static const char *prompt="\n$";


static void free_tokens(int tok_count , char **tok_list){
    while (tok_count>0){
        --tok_count;
        free(tok_list[tok_count]);
    }
    free(tok_list);
}//free_tokens()

static char expect_char(parse_str_t *parse){
    if (parse->next>=parse->len){
        parse->err_pos=parse->len;
        return 0;
    }else{
        return parse->script[parse->next++];
    }
}

// skip() avance l'index parse->next jusqu'au premier caractère
// non compris dans l'ensemble skip.
static void skip(parse_str_t *parse, const char *skip){
    while (parse->next<parse->len && strchr(skip, parse->script[parse->next++]));
    parse->next--;
}

// scan() retourne la position du premier caractère faisant
// partie de l'ensemble target.
static int scan(parse_str_t *parse, const char *target){
    int pos;
    pos=parse->next;
    while (pos<parse->len && !strchr(target,parse->script[pos++]));
    return pos;
}
// parse_var() retourne la valeur d'une variable.
// le parser a rencontré le caractère '$' qui introduit
// le nom d'une variable.
// Les noms de variables commencent par une lette ou '_'
// suivie d'un nombre quelconque de lettres,chiffres et '_'
static char *parse_var(parse_str_t *parse){
    int first,len;
    char c, *var_name;
    env_var_t *var;
    
    if (parse->next>=parse->len){
        parse->err_pos=parse->next;
        return NULL;
    }
    first=parse->next;
    c=parse->script[parse->next++];
    if (!(isalpha(c)||(c=='_'))) {
        parse->err_pos=--parse->next;
        return NULL;
    }
    while ((parse->err_pos==-1) && (parse->next<parse->len) && 
            (isalnum((c=parse->script[parse->next]))|| (c=='_')))parse->next++;
    len=parse->next-first;
    var_name=malloc(len+1);
    memcpy(var_name,&parse->script[first],len);
    var_name[len]=0;
    var=search_var(var_name);
    free(var_name);
    if (var){
        return var->value;
    }else{
        return NULL;
    }
}

// extrait un mot délimité par des accolades
static void parse_brace(){
    int level=1;
    BOOL escape=FALSE;
    
    while (level){
        if (escape){
            
        }else{
            
        }
    }
}

static const char bkslashed_char[]="abfnrtv";
static const char bkslashed_subst[]={0x7,0x8,0xc,0xa,0xd,0x9,0xb};

static char parse_backslash(parse_str_t *parse){
    char n=0, c, *s;
    int i;
    c= expect_char(parse);
    switch (c){
        case 0:
            return 0;
            break;
        case 'x':
            for (i=0;i<2;i++){
                c=expect_char(parse);
                if (isxdigit(c)){
                    n*=16;
                    n+=(c-'0')>9?c-'0'+7:c-'0';
                }else{
                    if (c){
                        parse->err_pos=--parse->next;
                    }
                    return 0;
                }
            }
            return n;
        case '0':
            for (i=0;i<3;i++){
                c=expect_char(parse);
                if (isdigit(c)&& (c<'8')){
                    n*=8;
                    n+=(c-'0');
                }else{
                    if (c){
                        parse->err_pos=--parse->next;
                    }
                    return 0;
                }
            }
            break;
        default:
            s=strchr(bkslashed_char,c);
            if (s){
                return bkslashed_subst[s-bkslashed_char];
            }else{
                return c;
            }
    }//switch
}

static char *parse_quote(parse_str_t *parse){
    char c,*quote;
    int slen=0;
    BOOL loop=TRUE;
    
    quote=calloc(sizeof(char),80);
    while (loop && (parse->err_pos==-1) && (parse->next<parse->len)){
        c=expect_char(parse);
        switch(c){
            case 0:
                parse->err_pos=parse->next;
                loop=FALSE;
                break;
            case '\\':
                c=parse_backslash(parse);
                if (c){
                    quote[slen++]=c;
                    quote[slen]=0;
                    break;
                }
            case '"':
                loop=FALSE;
                break;
            default:
                quote[slen++]=c;
                quote[slen]=0;
        }//switch
    }//while
    if (parse->err_pos>-1){
        free(quote);
    }else{
        quote=realloc(quote,slen+1);
    }
    return quote;
}

static char *next_token(parse_str_t *parse){
#define TOK_BUF_INCR (64)
#define _expand_token() if (buf_len<=(slen+strlen(xparsed))){\
                        buf_len=slen+strlen(xparsed)+TOK_BUF_INCR;\
                        token=(char*)realloc(token,buf_len);\
                    }
   
    unsigned char loop;
    char *token, *xparsed, c;
    int buf_len,slen;
    BOOL subst=TRUE;
    
    slen=0;
    buf_len=TOK_BUF_INCR;
    token=malloc(buf_len);
    token[0]=0;
    skip(parse," \t");
    loop=TRUE;
    while (loop && (parse->err_pos==-1) && (parse->next<parse->len)){
        switch ((c=parse->script[parse->next++])){
            case ' ':
            case 9: // TAB
                loop=FALSE;
                break;
//            case '{':
//                if (!escape){
//                    parse->next++;
//                    subst=FALSE;
//                    parse_brace();
//                }else{
//                    escape=FALSE;
//                }
//                break;
            case '$':
                if ((xparsed=parse_var(parse))){
                    _expand_token();
                    strcat(token,xparsed);
                    xparsed=NULL;
                    slen=strlen(token);
                }
                break;
            case '#':
                loop=FALSE;
                parse->next=parse->len;
                break;
            case '"':
                if (slen){
                    free(token);
                    parse->err_pos=--parse->next;
                }else{
                   
                    if ((xparsed=parse_quote(parse))){
                        _expand_token();
                        strcat(token,xparsed);
                        slen=strlen(token);
                        free(xparsed);
                    }
                } 
                loop=FALSE;
                break;
            default:
                if (slen>=buf_len){
                    buf_len+=TOK_BUF_INCR;
                    token=(char*)realloc(token,buf_len);
                }
                token[slen++]=c;
                token[slen]=0;
        }//switch
    } // while
    token=realloc(token,sizeof(char)*(slen+1));
    token[slen]=0;
    return token;
}//next_token()

 // découpe la ligne d'entrée en mots
static char** tokenize(int *i,const char *script){
#define TOK_COUNT_INCR (5)
    
    int j, slen,array_size=TOK_COUNT_INCR;
    char **tokens, *token;
    parse_str_t parse;
    
    parse.script=script;
    parse.next=0;
    parse.len=strlen(script);
    parse.err_pos=-1;
    j=0;
    tokens=malloc(sizeof(char*)*array_size);
    while ((parse.next<parse.len) && (parse.err_pos==-1) && (token=next_token(&parse))){
        tokens[j++]=token;
        if (j>=array_size){
            array_size+=TOK_COUNT_INCR;
            tokens=(char **)realloc(tokens,sizeof(char*)*array_size);
        }
    }//while
    if (parse.err_pos>-1){
        println(con,"syntax error.");
        println(con,script);
        if (parse.err_pos){
            spaces(con,parse.err_pos);
        }
        put_char(con,'^');
        free_tokens(j,tokens);
        j=0;
        return NULL;
    }
    tokens=(char**)realloc(tokens,sizeof(char*)*j);
    *i=j;
    return tokens;
}//tokenize()

static char* exec_script(const char *script){
    int tok_count;
    char *result, **tokens;

    tokens=tokenize(&tok_count,script);
    if (tok_count && tokens) {
        result=execute_cmd(tok_count,(char **)tokens);
        free_tokens(tok_count,tokens);
        return result;
    } // if
    return NULL;
}

void shell(void){
    char *str, cmd_line[CHAR_PER_LINE];
    int len;
    str=malloc(32);
    sprintf(str,"\nVPC-32 shell version %s \n",_version);
    print(con,str);
    free(str);
    while (1){
        print(con,prompt);
        len=read_line(con,cmd_line,CHAR_PER_LINE);
        if (len){
            str=exec_script((const char*)cmd_line);
            if (str){
                print(con,str);
                free(str);
            }//if
        }// if
    }//while(1)
    asm("lui $t0, 0xbfc0"); // _on_reset
    asm("j  $t0");
}//shell()

