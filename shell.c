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
 *      forth  lance l'environnement vpForth
 *      puts mot  imprime à l'écran le mot qui suis
 *      expr {expression}  évalue une expression et retourne le résultat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plib.h>

#include "hardware/HardwareProfile.h"
#include "hardware/ps2_kbd/keyboard.h"
#include "console.h"
#include "hardware/Pinguino/ff.h"
#include "hardware/Pinguino/fileio.h"
#include "shell.h"
#include "vpcBASIC/vpcBASIC.h"
#include "hardware/rtcc/rtcc.h"
//#include "hardware/serial_comm/serial_comm.h"
#include "console.h"

#define MAX_LINE_LEN 80

const char version[]="1.0";
const char local_console[]="VGA";
const char remote_console[]="SERIAL";

//dev_t con=SERIAL_CONSOLE;
dev_t con=VGA_CONSOLE;

const env_var_t shell_version={NULL,"SHELL_VERSION",(char*)version};
env_var_t console={(env_var_t*)&shell_version,"CONSOLE",(char*)local_console};
env_var_t *shell_vars=&console;

env_var_t *search_var(char *name);
void erase_var(env_var_t *var);

const char *ERR_MSG[]={
    "no error\r",
    "not implemented yet.\r",
    "Memory allocation error.\r",
    "Bad usage.\r",
    "File open error.\r",
    "Copy error.\r",
    "Mkdir error.\r",
    "file does not exist.\r",
    "operation denied.\r",
    "disk operation error, code is %d \r",
    "no SD card detected.\r"
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

typedef struct{
    char buff[MAX_LINE_LEN]; // chaîne saisie par l'utilisateur.
    unsigned char len;  // longueur de la chaîne.
    unsigned char first; // position du premier caractère du mot
    unsigned char next; // position du curseur de l'analyseur.
} input_buff_t;

static input_buff_t cmd_line;

#define MAX_TOKEN 5

static char *cmd_tokens[MAX_TOKEN];

extern int nbr_cmd;

typedef struct shell_cmd{
    char *name;
    void (*fn)(int);
}shell_cmd_t;

extern const shell_cmd_t commands[];

int cmd_search(char *target){
    int i;
    for (i=nbr_cmd-1;i>=0;i--){
        if (!strcmp(target,commands[i].name)){
            break;
        }
    }
    return i;
}//cmd_search()

void cmd_help(){
    int i;
    text_coord_t pos;
    for(i=0;i<nbr_cmd;i++){
        pos=get_curpos(con);
        if (pos.x>(CHAR_PER_LINE-strlen(commands[i].name)-2)){
            put_char(con,'\r');
        }
        print(con,commands[i].name);
        if (i<(nbr_cmd-1)){
            print(con," ");
        }
    }
    put_char(con,'\r');
}

void cmd_cls(int i){
    clear_screen(con);
}

// calibration oscillateur du RTCC
// +-127 ppm
void cmd_clktrim(int i){
    int trim;
    char fmt[64];
    if (i>1){
        trim=atoi(cmd_tokens[1]);
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
}


// imprime le temps depuis
// le démarrage de l'ordinateur
void cmd_uptime(){
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
}


void cmd_format(int i){
    if (i==2){
        print_error_msg(ERR_NOT_DONE,NULL,0);
    }else{
        print(con,"USAGE: format volume_name\r");
    }
}

void cmd_forth(int i){
//    test_vm();
}

static int next_token(void){
    unsigned char loop,quote,escape;
    cmd_line.first=cmd_line.next;
    while (cmd_line.first<cmd_line.len && (cmd_line.buff[cmd_line.first]==' ' ||
            cmd_line.buff[cmd_line.first]==9)){
        cmd_line.first++;
    }
    cmd_line.next=cmd_line.first;
    loop=TRUE;
    quote=FALSE;
    escape=FALSE;
    while (loop && (cmd_line.next<cmd_line.len)){
        switch (cmd_line.buff[cmd_line.next]){
            case ' ':
            case 9: // TAB
                if (!quote){
                    cmd_line.next--;
                    loop=FALSE;
                }
                break;
            case '#':
                if (!quote){
                    loop=FALSE;
                    cmd_line.next--;
                }
                break;
            case '\\':
                if (quote){
                    if (!escape){
                       escape=TRUE;
                    }
                    else{
                        escape=FALSE;
                    }
                }
                break;
            case '"':
                if (!quote){
                    quote=1;
                }
                else if (!escape){
                    loop=FALSE;
                }else{
                    escape=FALSE;
                }
                break;
            default:
                if (quote && escape){
                    escape=FALSE;
                }
        }//switch
        cmd_line.next++;
    } // while
    if (cmd_line.next>cmd_line.first)
        return 1;
    else
        return 0;
}//next_token()

void cmd_cd(int i){ // change le répertoire courant.
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
   if (i==2){
       error=f_chdir(cmd_tokens[1]);
   }
   if (!error){
       path=malloc(255);
       if (path){
          error=f_getcwd(path,255);
          if(!error){
              print(con,path);
              put_char(con,'\r');
          }
          free(path);
       }
   }
}//cd()

void cmd_del(int i){ // efface un fichier
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
    if (i==2){
        fi=malloc(sizeof(FILINFO));
        if (fi){
            error=f_stat(cmd_tokens[1],fi);
            if (!error){
                if (fi->fattrib & (ATT_DIR|ATT_RO)){
                    print_error_msg(ERR_DENIED,"can't delete directory or read only file.\r",0);
                }
                else{
                    error=f_unlink(cmd_tokens[1]);
                }
            }
            free(fi);
            if (error){
                print_error_msg(ERR_FIO,"",error);
            }
        }else{
               print_error_msg(ERR_ALLOC,"delete failed.\r",0);
        }
   }else{
       print_error_msg(ERR_USAGE, "delete file USAGE: del file_name\r",0);
   }
}//del()

void cmd_ren(int i){ // renomme un fichier
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    if (i==3){
        f_rename(cmd_tokens[1],cmd_tokens[2]);
    }else{
        print_error_msg(ERR_USAGE,"rename file, USAGE: ren name new_name\r",0);
    }
}//ren

void cmd_copy(int i){ // copie un fichier
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
    if (i==3){
        fsrc=malloc(sizeof(FIL));
        fnew=malloc(sizeof(FIL));
        buff=malloc(512);
        error=FR_OK;
        if (fsrc && fnew && buff){
            if ((error=f_open(fsrc,cmd_tokens[1],FA_READ)==FR_OK) &&
                (error=f_open(fnew,cmd_tokens[2],FA_CREATE_NEW|FA_WRITE)==FR_OK)){
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
                print_error_msg(ERR_FIO,"copy failed.\r",error);
            }
        }else{
            print(con,ERR_MSG[ERR_ALLOC]);
        }
    }else{
        print_error_msg(ERR_USAGE,"copy file USAGE: copy file_name new_file_name\r",0);
    }
}//copy()

void cmd_send(int i){ // envoie un fichier via uart
    // to do
   if (i==2){
       print_error_msg(ERR_NOT_DONE,NULL,0);
   }else{
       print(con, "send file via serial, USAGE: send file_name\r");
   }
}//cmd_send()

void cmd_receive(int i){ // reçois un fichier via uart
    // to do
   if (i==2){
       print_error_msg(ERR_NOT_DONE,NULL,0);
   }else{
       print(con, "receive file from serial, USAGE: receive file_name\r");
   }
}//receive()

void cmd_hdump(int i){ // affiche un fichier en hexadécimal
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
    if (i==2){
        fh=malloc(sizeof(FIL));
        if (fh && ((error=f_open(fh,cmd_tokens[1],FA_READ))==FR_OK)){
            if (con==VGA_CONSOLE) clear_screen(con);
            buff=malloc(512);
            fmt=malloc(CHAR_PER_LINE);
            if (fmt && buff){
                key=0;
                line[16]=CR;
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
                print_error_msg(ERR_ALLOC,"Can't display file.\r",0);
            }
        }else{
            print_error_msg(ERR_FIO,"File open failed.\r",error);
        }
   }else{
       print_error_msg(ERR_USAGE, "USAGE: more file_name\r",0);
   }
}//f

void cmd_mount(int i){// mount SDcard drive
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
}

void cmd_umount(int i){
    unmountSD();
    SDCardReady=FALSE;
}

// affiche à l'écran le contenu d'un fichier texte
void cmd_more(int i){
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
    if (i==2){
        clear_screen(con);
        fh=malloc(sizeof(FIL));
        if (fh && ((error=f_open(fh,cmd_tokens[1],FA_READ))==FR_OK)){
            buff=malloc(512);
            fmt=malloc(CHAR_PER_LINE);
            if (fmt && buff){
                key=0;
                while (key!=ESC && f_read(fh,buff,512,&n)==FR_OK){
                    if (!n) break;
                    rbuff=buff;
                    for(;n;n--){
                        c=*rbuff++;
                        if ((c!=TAB && c!=CR && c!=LF) && (c<32 || c>126)) {c=32;}
                        put_char(con,c);
                        cpos=get_curpos(con);
                        if (cpos.x==0){
                            if (cpos.y>=(LINE_PER_SCREEN-1)){
                                cpos.y=LINE_PER_SCREEN-1;
                                invert_video(con,TRUE);
                                print(con,"-- next --");
                                invert_video(con,FALSE);
                                key=wait_key(con);
                                if (key=='q' || key==ESC){key=ESC; break;}
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
                print_error_msg(ERR_ALLOC,"Can't display file.\r",0);
            }
        }else{
            print_error_msg(ERR_FIO,"File open failed.\r",error);
        }
   }else{
       print_error_msg(ERR_USAGE, "USAGE: more file_name\r",0);
   }
}//more

void cmd_edit(int i){ // lance l'éditeur de texte
    if (i>1){
        editor(cmd_tokens[1]);
    }else{
        editor(NULL);
    }
}//f

void cmd_mkdir(int i){
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
    if (i==2){
        fmt=malloc(CHAR_PER_LINE+1);
        if (fmt && (error=f_mkdir(cmd_tokens[1])==FR_OK)){
            sprintf(fmt,"directory %s created\r",cmd_tokens[1]);
            print(con,fmt);
        }else{
            if (!fmt){
                print(con,ERR_MSG[ERR_ALLOC]);
            }else{
                print(con,ERR_MSG[ERR_MKDIR]);
            }
        }
    }else{
        print_error_msg(ERR_USAGE,"mkdir create a directory, USAGE: mkdir dir_name\r",0);
    }
}// mkdir()

void cmd_dir(int i){
    FRESULT error;
    FIL *fh;
    char fmt[55];
    if (!SDCardReady){
        if (!mount(0)){
            print_error_msg(ERR_NO_SDCARD,NULL,0);
            return;
        }else{
            SDCardReady=TRUE;
        }
    }
    if (i>1){
        error=listDir(cmd_tokens[1]);
        if (error==FR_NO_PATH){// not a directory, try file
            fh=malloc(sizeof(FIL));
            if (fh && ((error=f_open(fh,cmd_tokens[1],FA_READ))==FR_OK)){
                sprintf(fmt,"File: %s, size %d bytes\r",cmd_tokens[1],fh->fsize);
                print(con,fmt);
                f_close(fh);
                free(fh);
            }
        }
    }else{
        error=listDir(".");
    }
    if (error) print_error_msg(ERR_FIO,"",error);
}//list_directory()

void cmd_puts(int i){
    print(con, "puts, to be done.\r");
}//puts()

void cmd_expr(int i){
    print(con, "expr, to be done.\r");
}//expr()

//display heap status
void cmd_free(int i){
    char fmt[55];
    sprintf(fmt,"free RAM %d/%d BYTES\r",free_heap(),heap_size);
    print(con,fmt);
}

void parse_time(char *time_str,stime_t *time){
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

void parse_date(char *date_str,sdate_t *date){
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
void cmd_date(int i){
    char fmt[32];
    sdate_t date;
    
    if (i>1){
        parse_date(cmd_tokens[1],&date);
        rtcc_set_date(date);
        if (rtcc_error) print(con,"rtcc_set_date() error\r");
    }else{
        rtcc_get_date_str(fmt);
        print(con,fmt);
    }
}

// affiche ou saisie de  l'heure
// format saisie:  hh:mm:ss
void cmd_time(int i){
    char fmt[16];
    stime_t t;
    if (i>1){
        parse_time(cmd_tokens[1],&t);
        rtcc_set_time(t);
        if (rtcc_error) print(con,"error set_time\r");
    }else {
        rtcc_get_time_str(fmt);
        print(con,fmt);
    }
}

void report_alarms_state(){
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

void cmd_alarm(int i){
    sdate_t date;
    stime_t time;
    char msg[32];
    
    switch (i){
        case 5:
            if (!strcmp(cmd_tokens[1],"-s")){
                parse_date(cmd_tokens[2],&date);
                parse_time(cmd_tokens[3],&time);
                strcpy(msg,cmd_tokens[4]); print(SERIO,msg);
                msg[31]=0;
                if (!rtcc_set_alarm(date,time,(uint8_t*)msg)){
                    print(con, "Failed to set alarm, none free.\n");
                }
                break;
            }
        case 2:
            if (!strcmp(cmd_tokens[1],"-d")){
                report_alarms_state();
                break;
            }
        case 3:
            if (!strcmp(cmd_tokens[1],"-c")){
                rtcc_cancel_alarm(atoi(cmd_tokens[2]));
                break;
            }
        default:
            print(con,"USAGE: alarm []|[-c 0|1]|[-d ]|[-s date time \"message\"]\n");
    }//switch
}

void cmd_echo(int i){
    int j;
    
    for (j=1;j<i;j++){
        print(con,cmd_tokens[j]);
        spaces(con,1);
    }
}

void cmd_reboot(int i){
    asm("lui $t0, 0xbfc0"); // _on_reset
    asm("j  $t0\n nop");
}


env_var_t *search_var(char *name){
    env_var_t *list;
    list=shell_vars;
    while (list){
        if (!strcmp(name,list->name)) break;
        list=list->link;
    }
    return list;
}

void erase_var(env_var_t *var){
    shell_vars=var->link;
    free(var->name);
    free(var->value);
    free(var);
}

void list_vars(){
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

void cmd_set(int i){
    env_var_t *var;
    char *name, *value;
    if (i>=2){
        var=search_var(cmd_tokens[1]);
        if (var){
            if (i==2){
                erase_var(var);
            }else{
                value=malloc(strlen(cmd_tokens[2])+1);
                strcpy(value,cmd_tokens[2]);
                free(var->value);
                var->value=value;
            }
        }else if (i==3){
            var=malloc(sizeof(env_var_t));
            name=malloc(strlen(cmd_tokens[1]+1));
            value=malloc(strlen(cmd_tokens[2])+1);
            if (!(var && name && value)){
                print_error_msg(ERR_ALLOC,"insufficiant memory",0);
                return;
            }
            strcpy(name,cmd_tokens[1]);
            strcpy(value,cmd_tokens[2]);
            var->link=shell_vars;
            var->name=name;
            var->value=value;
            shell_vars=var;
        }
    }else{
        list_vars();
    }
}

const shell_cmd_t commands[]={
    {"alarm",cmd_alarm},
    {"cd",cmd_cd},
    {"clktrim",cmd_clktrim},
    {"cls",cmd_cls},
    {"copy",cmd_copy},
    {"date",cmd_date},
    {"del",cmd_del},
    {"dir",cmd_dir},
    {"echo",cmd_echo},
    {"edit",cmd_edit},
    {"expr",cmd_expr},
    {"free",cmd_free},
    {"format",cmd_format},
    {"forth",cmd_forth},
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
    {"uptime",cmd_uptime}
};

int nbr_cmd=sizeof(commands)/sizeof(shell_cmd_t);


void execute_cmd(int i){
    int cmd;
        cmd=cmd_search(cmd_tokens[0]);
        if (cmd>=0){
            commands[cmd].fn(i);
        }else{
            print(con,"unknown command!\r");
        }
}// execute_cmd()

const char *prompt="\r$";


void free_tokens(){
    int i;
    for (i=MAX_TOKEN-1;i>=0;i--){
        if (cmd_tokens[i]){
            free(cmd_tokens[i]);
            cmd_tokens[i]=NULL;
        }
    }
}//free_tokens()

char *var_substitution(char *token){
#define W_INCR (80)
    
    int wlen;
    char *dollar, *word, *var_name;
    env_var_t *var=NULL;

    dollar=strchr(token,'$');
    if (dollar){
        wlen=W_INCR;
        word=calloc(wlen,sizeof(char));
        *dollar=0;
        strcpy(word,token);
        do {
            var_name=++dollar;
            dollar=strchr(var_name,'$');
            if (dollar){
                *dollar=0;
            }    
            var=search_var(var_name);
            if (var){
                if (wlen<=(strlen(word)+strlen(var->value))){
                    wlen+=W_INCR;
                    word=realloc(word,wlen);
                }
                strcat(word,var->value);
            }//if
        } while (dollar);
        free(token);
        word=realloc(word,strlen(word)+1);
        return word;
    }else{
        return token;
    }
}

int tokenize(){ // découpe la ligne d'entrée en mots
    int i;
    char *token;
    i=0;
    cmd_line.first=0;
    cmd_line.next=0;
    while ((i<MAX_TOKEN) && next_token()){
        token=malloc(sizeof(char)*(cmd_line.next-cmd_line.first+1));
        memcpy(token,&cmd_line.buff[cmd_line.first],cmd_line.next-cmd_line.first);
        *(token+cmd_line.next-cmd_line.first)=(char)0;
        token=var_substitution(token);
        cmd_tokens[i]=token;
        i++;
    }//while
    return i;
}//tokenize()

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

void shell(void){
    int i;
    char fmt[32];
    sprintf(fmt,"\rVPC-32 shell version %s \r",version);
    print(con,fmt);
    free_tokens();
    while (1){
        print(con,prompt);
        cmd_line.len=read_line(con,cmd_line.buff,CHAR_PER_LINE);
        if (cmd_line.len){
            i=tokenize();
            if (i) {
                execute_cmd(i);
                free_tokens();
            } // if
        }// if
    }//while(1)
    asm("lui $t0, 0xbfc0"); // _on_reset
    asm("j  $t0");
}//shell()

