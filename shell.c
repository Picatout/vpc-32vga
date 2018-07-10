/*
* Copyright 2013,2018 Jacques Deschênes
* This file is part of VPC-32v.
*
*     VPC-32v is free software: you can redistribute it and/or modify
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
*     along with VPC-32v.  If not, see <http://www.gnu.org/licenses/>.
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
#include <setjmp.h>

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
#include "xmodem.h"

#define MAX_LINE_LEN 80

static jmp_buf failed;

static const char _version[]="1.0";
static const char *console_name[]={"VGA","COM"};
static const char _true[]="T";
static const char _false[]="F";
static const char _nil[]="";

//jmp_buf back_to_cmd_line;


static const env_var_t shell_version={NULL,"SHELL_VERSION",(char*)_version};
static const env_var_t T={(env_var_t*)&shell_version,"TRUE",(char*)_true};
static const env_var_t F={(env_var_t*)&T,"FALSE",(char*)_false};
static const env_var_t nil={(env_var_t*)&F,"NIL",(char*)_nil};
static env_var_t *shell_vars=(env_var_t*)&nil;

static const char *root_dir="0:/";
static char *activ_directory=NULL;

static env_var_t *search_var(const char *name);
static void erase_var(env_var_t *var);
static char* exec_script(const char *script);
static char *concat_tokens(int tok_count, char **tok_list, int from);

typedef struct{
    const char *script; // chaîne à analyser
    unsigned  len;  // longueur de la chaîne.
    unsigned  next; // position du curseur de l'analyseur.
    int err_pos;
} parse_str_t;

static void skip(parse_str_t *parse, const char *skip);

static const char *ERR_MSG[]={
    "no error\r",
    "unknown command or file.\r",
    "syntax error.\r",
    "not implemented yet.\r",
    "Memory allocation error.\r",
    "Bad usage.\r",
    "File open error.\r",
    "Copy error.\r",
    "Mkdir error.\r",
    "file does not exist.\r",
    "operation denied.\r",
    "disk operation error, code is %d\r",
    "no SD card detected.\r",
    "XMODEM operation failed\r"
};


static void throw(SH_ERROR err_code,const char *detail,FRESULT io_code){
    if (err_code==ERR_FIO){
            printf(ERR_MSG[ERR_FIO],io_code);
    }else if (err_code==ERR_XMODEM){
        printf("exit code was %d\r",io_code);
    }else{
       print(con,ERR_MSG[err_code]);
    }
    if (detail){
       printf(detail);
    }
    longjmp(failed,err_code);
}

static int nbr_cmd;

typedef struct shell_cmd{
    char *name;
    char *(*fn)(int, char**);
    const char *help;
}shell_cmd_t;

static const shell_cmd_t commands[];

static int search_command(const char *target){
    int i;
    for (i=nbr_cmd-1;i>=0;i--){
        if (!strcmp(target,commands[i].name)){
            break;
        }
    }
    return i;
}//search_command()

// help [cmd_name]
static const char HELP_HLP[]=
    "USAGE: help [command_name]\r"
    "Display commands list or specific command help message if a comamnd\r"
    "name is given.\r";
static char* cmd_help(int tok_count, char  **tok_list){
    int i;
    text_coord_t pos;
    if (tok_count==2){
        i=search_command(tok_list[1]);
        if (i>-1){
            printf(commands[i].help);
        }else{
            throw(ERR_NOT_CMD,tok_list[1],0);
        }
    }else{
        for(i=0;i<nbr_cmd;i++){
            pos.xy=get_curpos(con);
            if (pos.x>(CHAR_PER_LINE-strlen(commands[i].name)-2)){
                put_char(con,'\r');
            }
            printf(commands[i].name);
            if (i<(nbr_cmd-1)){
                put_char(con,' ');
            }
        }
        put_char(con,'\r');
    }
    return NULL;
}

//Si tok_count==2 et tok_list[1]=="-?" affiche l'aide et retourne true. 
//Sinon retourne false.
static bool try_help(int tok_count, char **tok_list){
    int i;
    if (tok_count==2 && !strcmp(tok_list[1],"-?")){
        i=search_command(tok_list[0]);
        if (i>-1){
            printf(commands[i].help);
            return true;
        }else{
            throw(ERR_NOT_CMD,tok_list[0],0);
        }
    }else{
        return false;
    }
}

// cls [-?]
static const char CLS_HLP[]=
    "USAGE: cls [-?]\rClear screen and move cursor to top left position.\r";
static char* cmd_cls(int tok_count, char  **tok_list){
    if (!try_help(tok_count,tok_list)){
        clear_screen(con);
    }
    return NULL;
}

static const char CTRIM_HLP[]=
    "USAGE: clktrim  [-?]|n\r"
    "RTCC oscillator calibration\r"
    "n is added to actual value\r"
    "n is in range {-127..127}\r"
    "If n is missing display actual trim value.\r";
//clktrim [n]
// calibration oscillateur du RTCC
// +-127 ppm
static char* cmd_clktrim(int tok_count, char  **tok_list){
    int trim=0;
    
    if (try_help(tok_count,tok_list)) return NULL;
    if (tok_count>1){
        trim=atoi(tok_list[1]);
    }
    trim=rtcc_calibration(trim);
    printf("Actual RTCC oscillator trim value: %d\r",trim);
    return NULL;
}

static const char UTIME_HLP[]=
    "USAGE: uptime [-?]\r"
    "Display computer power on period.\r"
    "Display format is ddDhhHmmMssS\r"
    "This command ignore any extra token on command line.\r";
// imprime le temps depuis
// le démarrage de l'ordinateur
static char* cmd_uptime(int tok_count, char  **tok_list){
    unsigned sys_ticks;
    unsigned day,hour,min,sec,remainder;
    
    if (!try_help(tok_count,tok_list)){
        sys_ticks=ticks();
        day=sys_ticks/86400000L;
        remainder=sys_ticks%86400000L;
        hour=remainder/3600000L;
        remainder%=3600000L;
        min=remainder/60000;
        remainder%=60000;
        sec=remainder/1000;
        printf("power on period: %02dD%02dH%02dM%02dS\r",day,hour,min,sec);
    }
    return NULL;
}

//static const char FRMT_HLP[]=
//    "USAGE: format [-?]\r"
//    "Format the SD card, system file is FAT32\r";
//static char* cmd_format(int tok_count, char  **tok_list){
//    if (!try_help(tok_count,tok_list)){
//        f_mkfs(0,0,4096);
//    }
//    return NULL;
//}

#include "vpcBASIC/BASIC.h"

static const char BASIC_HLP[]=
    "USAGE: basic [-?] | [-h size] [-c string]| [file_name]\r"
    "-?  display this help.\r"
    "-h size, fix heap size in bytes. Default is 4096.\r" 
    "-c  \"code\", execute the basic commands in quotes and exit to command shell.\r"
    "basic file_name, to execute a BASIC file and exit to command shell.\r"
    "Without parameters invoke BASIC shell in interactive mode.\r" ;
static char* cmd_basic(int tok_count, char  **tok_list){
    char *file_or_string=NULL;
    
    int i;
    unsigned heap=DEFAULT_HEAP;
    unsigned option=BASIC_PROMPT;

    for (i=1;i<tok_count;i++){
        if (!strcmp(tok_list[i],"-?")){
            i=tok_count;
            printf(BASIC_HLP);
            return NULL;
        }
        if (!strcmp(tok_list[i],"-h")){
            i++;
            heap=atoi(tok_list[i]);
        }else if (!strcmp(tok_list[i],"-c")){
            file_or_string=(char*)tok_list[++i];
            option=EXEC_STRING;
        }else{
            file_or_string=(char*)tok_list[i];
            option=EXEC_FILE;
        }
    }//for
    BASIC_shell(heap,option,file_or_string);
    return NULL;
}

static const char CD_HLP[]=
    "USAGE: cd -? | [directory]\r"
    "Change active dierctory or display active directory\r"
    "Without directory parameter, display active directory.\r"
    "Otherwise change active directory to given one.\r";
static char* cmd_cd(int tok_count, char  **tok_list){ // change le répertoire courant.
    char *path;
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
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
                printf("%s\r",&path[2]);
                if (activ_directory!=root_dir){
                    free(activ_directory);
                }
                activ_directory=path;
          }else{
              free(path);
          }
       }
   }else{
       printf("%s is not valid directory\r",tok_list[1]);
   }
   return NULL;
}//cmd_cd()

static const char DEL_HLP[]=
    "USAGE:del -? | [-y] file_spec\r"
    "Delete one or more files.\r"
    "-?, Display this help.\r"
    "-y, don't ask confirmation on each file.\r"
    "file_spec, file name, may include '*' character.\r";

static char* cmd_del(int tok_count, char  **tok_list){ // efface un fichier
    filter_t *filter=NULL;
    char *path=NULL;
    bool confirm=true, delete=true;
    FILINFO *fi=NULL;
    DIR *dir=NULL;
    FRESULT error=FR_OK;
    int i,count=0;
    
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
        }else{
            SDCardReady=TRUE;
        }
    }
    i=1;
    while (i<tok_count){
        if (tok_list[i][0]=='-'){
            switch(tok_list[i][1]){
                case 'y':
                    confirm=false;
                    break;
                case '?':
                default:
                    printf(DEL_HLP);
                    return NULL;
            }//switch
        }else{
            path=tok_list[i];
            break;
        }
        i++;
    }
    filter=malloc(sizeof(filter_t));
    fi=malloc(sizeof(FILINFO));
    dir=malloc(sizeof(DIR));
    if (path){
        path=set_filter(filter,path);
        error=f_opendir(dir,path);
        while (error==FR_OK){
            error=f_readdir(dir,fi);
            if (error==FR_OK && filter_accept(filter,fi->fname)){
                if (confirm){
                    printf("delete %s (y/n)?",fi->fname);
                    delete=wait_key(con)=='y';
                    put_char(con,'\r');
                }
                if (delete){
                    error=f_unlink(fi->fname);
                    count++;
                }
            }//if
        }//while
        printf("%d files deleted\r",count);
    }else{
        printf("Bad usage, try del -?\r");
    }
    free(fi);
    free(filter);
    free(dir);
    return NULL;
}//del()

static const char REN_HLP[]=
    "USAGE: -? | ren file_name new_name\r"
    "Rename an existing file\r"
    "file_name is a single file\r";
static char* cmd_ren(int tok_count, char  **tok_list){ // renomme un fichier
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
        }else{
            SDCardReady=TRUE;
        }
    }
    if (tok_count==3){
        f_rename(tok_list[1],tok_list[2]);
    }else{
        throw(ERR_USAGE,"rename file\rUSAGE: ren name new_name\r",0);
    }
    return NULL;
}//ren

static const char COPY_HLP[]=
    "USAGE: copy -? | src dest\r"
    "Copy a file to another name.\r"
    "-?, Display this help.\r"
    "src, name of file to copy.\r"
    "dest, new file name.\r";
static char* cmd_copy(int tok_count, char  **tok_list){ // copie un fichier
    FIL *fsrc, *fnew;
    char *buff;
    int n;
    
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
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
                throw(ERR_FIO,"copy failed.\r",error);
            }
        }else{
            print(con,ERR_MSG[ERR_ALLOC]);
        }
    }else{
        throw(ERR_USAGE,"Try copy -?\r",0);
    }
    return NULL;
}//copy()

static char *parse_xmodem_options(int tok_count, char **tok_list, unsigned *options){
    int i;
    char *file_name;
    
    *options=0;
    for (i=1;i<tok_count;i++){
        if (tok_list[i][0]=='-'){
            switch(tok_list[i][1]){
                case 'b':
                    *options|=XMODEM_BINARY;
                    break;
                case 'v':
                    *options|=XMODEM_VERBOSE;
                    break;
                default:
                    return NULL;
            }
        }else{
            file_name=tok_list[i];
        }
    }//for
    if (!file_name) throw(ERR_USAGE,"missing file name",0);
    return file_name;
}

static const char SEND_HLP[]=
    "USAGE: send -?|[-b] [-v] file_name\r"
    "Send a file via COM port using XMODEM protocol.\r"
    "-?, Display this help.\r"
    "-b, The file is binary file.\r"
    "-v, Display transmission detail.\r"
    "file_name, is a single file, no wildcard character.\r";
// envoie un fichier via le port COM
// send [-b] file_name
static char* cmd_send(int tok_count, char  **tok_list){ // envoie un fichier via uart
    int result;
    unsigned options;
    char *file_name;
    
   if (try_help(tok_count,tok_list)) return NULL; 
   if (tok_count>=2){
       file_name=parse_xmodem_options(tok_count,tok_list,&options);
       result=xsend(file_name,options);
       if (result){
           throw(ERR_XMODEM,"File transmission failed",result);
       }else{
           println(con,"Transmisson completed.");
       }
   }else{
       printf(SEND_HLP);
   }
   return NULL;
}//cmd_send()

static const char RCV_HLP[]=
    "USAGE: receive -?|[-b] [-v] file_name\r"
    "Receive a file via COM port using XMODEM protocol.\r"
    "-?, Display this help.\r"
    "-b, The file is binary.\r"
    "-v, Display reception detail.\r"
    "file_name is a single file, no wildcard character\r";
// receive [-b] [-v] file_name
static char* cmd_receive(int tok_count, char  **tok_list){ // reçois un fichier via uart
    int result;
    unsigned options;
    char *file_name;
    
   if (try_help(tok_count,tok_list)) return NULL;
   if (tok_count>=2){
       file_name=parse_xmodem_options(tok_count,tok_list,&options);
       result=xreceive(file_name,options);
       if (result){
           throw(ERR_XMODEM,"File reception failed",result);
       }else{
           println(con,"Reception completed");
       }
       
   }else{
       printf(RCV_HLP);
   }
   return NULL;
}//cmd_receive()

static const char HDUMP_HLP[]=
    "USAGE: hdump -?|file_name\r"
    "Display file data in hexadecimal, 16 bytes per row.\r"
    "-?, Display this help\r"
    "file_name, is the file to dump, no wildcard accepted.\r";
static char* cmd_hdump(int tok_count, char  **tok_list){ // affiche un fichier en hexadécimal
    FIL *fh;
    unsigned char *fmt, *buff, *rbuff, c,key,line[18];
    int n,col=0,scr_line=0;
    unsigned addr=0;
    
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
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
                line[16]=CR;
                line[17]=0;
                while (key!='q' && key!='Q' && f_read(fh,buff,512,&n)==FR_OK){
                    if (!n) break;
                    rbuff=buff;
                    for(;n && key!=ESC;n--){
                        if (!col){
                            printf("%08X  ",addr);
                            //print(con,fmt);
                        }
                        c=*rbuff++;
                        sprintf(fmt,"%02X ",c);
                        //print_hex(con,c,2); put_char(con,32);
                        if (c>=32) line[col]=c; else line[col]=32;
                        printf(fmt);
                        col++;
                        if (col==16){
                            print(con,line);
                            col=0;
                            addr+=16;
                            scr_line++;
                            if (scr_line==(LINE_PER_SCREEN-1)){
                                printf("more...");
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
                throw(ERR_ALLOC,"Can't display file.\r",0);
            }
        }else{
            throw(ERR_FIO,"File open failed.\r",error);
        }
   }else{
       throw(ERR_USAGE, "USAGE: hdump file_name\r",0);
   }
    return NULL;
}//f

static const char MOUNT_HLP[]=
    "USAGE: mount [-?]\r"
    "Mount SD card file system.\r"
    "-?, display this information.\r";
static char* cmd_mount(int tok_count, char  **tok_list){// mount SDcard drive
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
        }else{
            SDCardReady=TRUE;
        }
    }
    activ_directory=(char*)root_dir;
    return NULL;
}

static const char UMOUNT_HLP[]=
    "USAGE: umount [-?]\r"
    "Unmount SD card file system.\r"
    "-?, Display this help\r";
static char* cmd_umount(int tok_count, char  **tok_list){
    if (try_help(tok_count,tok_list)) return NULL;
    unmountSD();
    SDCardReady=FALSE;
    if (activ_directory && activ_directory!=root_dir){free(activ_directory);}
    activ_directory=NULL;
    return NULL;
}

static const char MORE_HLP[]=
    "USAGE: more -? | file_name\r"
    "Display text file pausing at each screen full\r"
    "file_name, is file to display, no wildcard accpeted\r";
// affiche à l'écran le contenu d'un fichier texte
static char* cmd_more(int tok_count, char  **tok_list){
    FIL *fh;
    char *fmt, *buff, *rbuff, c, prev,key;
    int n,lcnt,colcnt=0;
    text_coord_t cpos;
    
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
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
                while (key!='q' && key!='Q' && f_read(fh,buff,512,&n)==FR_OK){
                    if (!n) break;
                    rbuff=buff;
                    for(;n;n--){
                        c=*rbuff++;
                        if (c==A_LF){c=A_CR;}
                        if (!(c==A_TAB || c==A_CR) && (c<32 || c>126)) {c=32;}
                        put_char(con,c);
                        cpos.xy=get_curpos(con);
                        if (cpos.x==0){
                            if (cpos.y>=(LINE_PER_SCREEN-1)){
                                cpos.y=LINE_PER_SCREEN-1;
                                set_curpos(con,cpos.x,cpos.y);
                                invert_video(con,TRUE);
                                printf("-- next --");
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
                throw(ERR_ALLOC,"Can't display file.\r",0);
            }
        }else{
            throw(ERR_FIO,"File open failed.\r",error);
        }
   }else{
       throw(ERR_USAGE, "USAGE: more file_name\r",0);
   }
    return NULL;
}//more

static const char EDIT_HLP[]=
    "USAGE: edit [-?]|file_name\r"
    "Launch text file editor.\r"
    "file_name, is a single file to edit.\r"
    "If file_name doesn't exist it will be created.\r";
static char* cmd_edit(int tok_count, char  **tok_list){ // lance l'éditeur de texte
    if (try_help(tok_count,tok_list)) return NULL;
    if (tok_count>1){
        editor(tok_list[1]);
    }else{
        editor(NULL);
    }
    return NULL;
}//f

static const char MKDIR_HLP[]=
    "USAGE: mkdir -?| dir_name\r"
    "Create a new diectory\r"
    "The '/' character is path separator.\r"
    "To create from root directory use '/' as first character of dir_name\r"
    "Otherwise the directory is a sub-directory of active one.\r";
static char* cmd_mkdir(int tok_count, char  **tok_list){
    FRESULT error=FR_OK;
    //char *fmt;
    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
        }else{
            SDCardReady=TRUE;
        }
    }
    if (tok_count==2){
        //fmt=malloc(CHAR_PER_LINE+1);
        if ((error=f_mkdir(tok_list[1])==FR_OK)){
            printf("directory %s created\r",tok_list[1]);
            //print(con,fmt);
        }else{
            print(con,ERR_MSG[ERR_MKDIR]);
        }
    }else{
        throw(ERR_USAGE,"mkdir create a directory\rUSAGE: mkdir dir_name\r",0);
    }
    return NULL;
}// mkdir()

static const char RMDIR_HLP[]=
    "USAGE: rmdir -?|dir_name\r"
    "Delete an existing directory\r"
    "A directory containing files can't be deleted\r"
    "Path character separator is '/'.\r";
// RMDIR dir_name
static char* cmd_rmdir(int tok_count, char **tok_list){
    FRESULT result;
    
    if (try_help(tok_count,tok_list)) return NULL;
    if (tok_count<2 || !strcmp(tok_list[1],"-?")){
        printf("Delete a directory\rUSAGE: rmdir dir_name\r");
    }else{
        result=f_unlink(tok_list[1]);
        if (result) throw(ERR_FIO,"rmdir failed\r",result);
    }
    return NULL;
}

static const char DIR_HLP[]=
    "USAGE: dir [-?]| [dir_name]\r"
    "Display contain of diectory\r"
    "If dir_name is not given, display active directory.\r";
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

    if (try_help(tok_count,tok_list)) return NULL;
    if (!SDCardReady){
        if (!mount(0)){
            throw(ERR_NO_SDCARD,NULL,0);
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
        throw(ERR_ALLOC,NULL,0);
    }
    filter->criteria=eNO_FILTER;
    if (tok_count>1){
        path=set_filter(filter,tok_list[1]);
        if (path==current_dir){
            path=activ_directory;
        }
    }else{
        path=activ_directory;
    }
    error=listDir(path,filter);
    if ((error==FR_NO_PATH) && (error=f_stat(path,fi)==FR_OK)){
        print_fileinfo(fi);
    }
    free(fi);
    free(dir);
    free(filter);
    if (error) throw(ERR_FIO,"",error);
    return NULL;
}//list_directory()

static const char FREE_HLP[]=
    "USAGE: free [-?]\r"
    "Display available RAM size\r";
//display heap status
static char* cmd_free(int tok_count, char  **tok_list){
//    char *free_ram;
//    free_ram=calloc(sizeof(char),80);

    if (try_help(tok_count,tok_list)) return NULL;
    printf("free RAM %d/%d BYTES\r",free_heap(),heap_size);
    return NULL;
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

static const char DATE_HLP[]=
    "USAGE: date [-?]|[YYYY/MM/DD]\r"
    "Display or set current date.\r";
// affiche ou saisie de la date
// format saisie: [yy]yy/mm/dd
static char* cmd_date(int tok_count, char  **tok_list){
    char *fmt;
    sdate_t date;

    if (try_help(tok_count,tok_list)) return NULL;
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

static const char TIME_HLP[]=
    "USAGE: time [-?]|[HH:MM:SS]\r"
    "Display or set current time.\r";
// affiche ou saisie de  l'heure
// format saisie:  hh:mm:ss
static char* cmd_time(int tok_count, char  **tok_list){
    char *fmt;
    stime_t t;
    
    if (try_help(tok_count,tok_list)) return NULL;
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
    //char fmt[80];
    int i,wkday;
    
    sdate_t date;
    alm_state_t state[2];
    rtcc_get_date(&date);
    rtcc_get_alarms(state);
    for (i=0;i<2;i++){
        if (state[i].enabled){
            printf("alarm %d set to %s %d/%02d/%02d %02d:%02d:%02d  %s\r",i,
                    weekdays[state[i].wkday-1],date.year,state[i].month,
                    state[i].day,state[i].hour,state[i].min,state[i].sec,
                    (char*)state[i].msg);
           // print(con,fmt);
        }else{
            printf("alarm %d inactive\r",i);
            //print(con,fmt);
        }
    }
    
}

// alarm -?| -s n date time message | -d | -c n
static const char ALARM_HLP[]=
   "USAGE: alarm -?|-d|-c n|-s n date time message\r"
   "-?, To display this help mesage\r"
   "-d, To display actual alarms\r"
   "-c n, To reset an alarm where n is {0,1}\r"
   "-s n date time message, To set an alarm.\r"
   "   n is {0,1}\r"
   "   date format is AAAA/MM/DD\r"
   "   time format is HH:MM:SS\r"
   "   message is double quoted string\r"
   "At alarm a ring tone is heard and message displayed at screen top\r";
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
                    print(con, "Failed to set alarm, none free.\r");
                }
                break;
            }
        case 2:
            if (!strcmp(tok_list[1],"-d")){
                report_alarms_state();
                break;
            }else if (!strcmp(tok_list[1],"-?")){
                printf(ALARM_HLP);
            }
        case 3:
            if (!strcmp(tok_list[1],"-c")){
                rtcc_cancel_alarm(atoi(tok_list[2]));
                break;
            }
        default:
            printf(ALARM_HLP);
    }//switch
    return NULL;
}

static const char ECHO_HLP[]=
    "USAGE: echo [\"]texte to display[\"]\r"
    "Print message to screen.\r"
    "If text is not quoted it may contain environment variables preceded by '$'\r"
    "Environment variables can be concatenated without space between them.\r";
static char* cmd_echo(int tok_count, char  **tok_list){
    int j;
    
    for (j=1;j<tok_count;j++){
        print(con,tok_list[j]);
        spaces(con,1);
    }
    return NULL;
}

static const char RBT_HLP[]=
    "USAGE: reboot [-?]\r"
    "Reboot computer as from power on.\r";
static char* cmd_reboot(int tok_count, char  **tok_list){
    if (try_help(tok_count,tok_list)) return NULL;
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

// retourne la chaîne valeur d'une variable d'environnement
// à partir de son nom.
// Si la variable n'existe pas retourne NULL.
const char *var_value(char *var_name){
    env_var_t *var;
    char vname[32];
    
    strcpy(vname,var_name);
    uppercase(vname);
    var=search_var(vname);
    if (var){
        return var->value;
    }else{
        return NULL;
    }
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
    env_var_t *list;
    list=shell_vars;
    while (list){
        if (list->name){
            printf("%s=%s\r",list->name,list->value);
        }
        list=list->link;
    }
}

// concatenate tous les jetons à partir du 4ième.
static char *concat_tokens(int tok_count, char **tok_list, int from){
    int i, len=0;
    char *value;
    for (i=from;i<tok_count;i++){
        len+=strlen(tok_list[i])+1;
    }
    value=calloc(len,sizeof(char));
    for(i=from;i<tok_count;i++){
        value=strcat(value,tok_list[i]);
        value[strlen(value)]=' ';
    }
    return value;
}

static const char SET_HLP[]=
    "USAGE: set [-?] | [var_name=[value]]\r"
    "Set an environment variable.\r"
    "If no argument, display all variables.\r"
    "-?, Display this help.\r"
    "If value not given, delete the variable.\r"
    "Predefined variables can't be deleted or changed.\r"
    "Var_name is case insensitive, converted to uppercase.\r"
    "If there is a space in value it must be quoted.\r";
static char* cmd_set(int tok_count, char  **tok_list){
    env_var_t *var;
    char *name, *value;

    if (try_help(tok_count,tok_list)) return NULL;
    if (tok_count>=2){
        uppercase(tok_list[1]);
        var=search_var(tok_list[1]);
        if (var){
            if (!_is_ram_addr(var)){
                throw(ERR_DENIED,"Read only variable.",0);
            }else if (tok_count==3 && tok_list[2][0]=='='){
                    erase_var(var);
                }else if (tok_count>=4 && tok_list[2][0]=='='){
                    if (tok_count>4){
                        value=concat_tokens(tok_count,tok_list,3);
                    }else{
                        value=malloc(strlen(tok_list[3])+1);
                        strcpy(value,tok_list[3]);
                    }
                    free(var->value);
                    var->value=value;
                }else{
                    throw(ERR_SYNTAX,"Try set -?",0);
                }
        }else{
            if (tok_count>=4 && tok_list[2][0]=='='){//nouvelle variable
                var=malloc(sizeof(env_var_t));
                name=malloc(strlen(tok_list[1])+1);
                strcpy(name,tok_list[1]);
                uppercase(name);
                if (tok_count>4){
                    value=concat_tokens(tok_count,tok_list,3);
                }else{
                    value=malloc(strlen(tok_list[3])+1);
                    if (!(var && name && value)){
                        throw(ERR_ALLOC,"insufficiant memory",0);
                    }
                    strcpy(value,tok_list[3]);
                }
                var->link=shell_vars;
                var->name=name;
                var->value=value;
                shell_vars=var;
            }
        }
    }else{
        list_vars();
    }
    return NULL;
}

static const char CON_HLP[]=
    "USAGE: con -?|-n|remote|local\r"
    "Switch console between local and remote\r"
    "-n, Display active console name.\r"
    "remote, switch to remote console via COM port.\r"
    "local, switch to local console using VGA monitor and keyboard.\r";
// con -?|-n|local|remote
static char *cmd_con(int tok_count, char** tok_list){
#define DISPLAY_NAME (-2)
    char *result;
    int console_id=-1;
    
    if (try_help(tok_count,tok_list)) return NULL;
    result=malloc(80);
    if (tok_count==2){
        if (!strcmp("local",tok_list[1])){
            console_id=VGA_CONSOLE;
        }else if (!strcmp("remote",tok_list[1])){
            if (vt_init()){
                console_id=SERIAL_CONSOLE;
            }else{
                sprintf(result,"switching console failed, VT terminal not ready\r");
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
            sprintf(result,"Select console\rUSAGE: con -n|remote|lcoal.");
    }//switch
    return result;
}

static inline bool try_file_type(char *file_name,const char *ext){
    return strstr(file_name,ext);
}

static const char RUN_HLP[]=
    "USAGE: -? | run file_name\r"
    "Run a basic program.\r"
    "The .BAS extension is not required in file_name.\r";
// exécute un fichier basic (*.bas) ou binaire (*.com)
// revient au shell de commandes à la sortie du programme.
static char  *cmd_run(int tok_count, char** tok_list){
    int err;
    char fmt[80];
    FILINFO fi;
    FRESULT res;

    if (try_help(tok_count,tok_list)) return NULL;
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
                    throw(ERR_NOT_CMD,tok_list[0],0);
                }else{
                    throw(ERR_NOT_CMD,tok_list[0],0);
                }
            }
        }else{
            throw(ERR_NOT_CMD,tok_list[0],0);
        }
    } 
    return NULL;
}//f



static const shell_cmd_t commands[]={
    {"alarm",cmd_alarm,ALARM_HLP},
    {"basic",cmd_basic,BASIC_HLP},
    {"cd",cmd_cd,CD_HLP},
    {"clktrim",cmd_clktrim,CTRIM_HLP},
    {"cls",cmd_cls,CLS_HLP},
    {"con",cmd_con,CON_HLP},
    {"copy",cmd_copy,COPY_HLP},
    {"date",cmd_date,DATE_HLP},
    {"del",cmd_del,DEL_HLP},
    {"dir",cmd_dir,DIR_HLP},
    {"echo",cmd_echo,ECHO_HLP},
    {"edit",cmd_edit,EDIT_HLP},
    {"free",cmd_free,FREE_HLP},
//    {"format",cmd_format,FRMT_HLP},
    {"hdump",cmd_hdump,HDUMP_HLP},
    {"help",cmd_help,HELP_HLP},
    {"mkdir",cmd_mkdir,MKDIR_HLP},
    {"more",cmd_more,MORE_HLP},
    {"mount",cmd_mount,MOUNT_HLP},
    {"reboot",cmd_reboot,RBT_HLP},
    {"receive",cmd_receive,RCV_HLP},
    {"ren",cmd_ren,REN_HLP},
    {"rmdir",cmd_rmdir,RMDIR_HLP},
    {"run",cmd_run,RUN_HLP},
    {"send",cmd_send,SEND_HLP},
    {"set",cmd_set,SET_HLP},
    {"time",cmd_time,TIME_HLP},
    {"umount",cmd_umount,UMOUNT_HLP},
    {"uptime",cmd_uptime,UTIME_HLP},
};

static int nbr_cmd=sizeof(commands)/sizeof(shell_cmd_t);


static char *try_file(int tok_count, char **tok_list){
    FILINFO fi;
    FRESULT res;
    
    if (tok_count==1){
        tok_list[1]=tok_list[0];
        return cmd_run(2,tok_list);
    }else{
        throw(ERR_NOT_CMD,tok_list[0],0);
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

static const char *prompt="\r$";


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
static const char *parse_var(parse_str_t *parse){
    int first,len;
    char c, *var_name;
    const char *value;
    
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
    len=min(31,len);
    var_name=malloc(len+1);
    memcpy(var_name,&parse->script[first],len);
    var_name[len]=0;
    value=var_value(var_name);
    free(var_name);
    return value;
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
    
    quote=malloc(sizeof(char)*80);
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
        quote=NULL;
    }
    return quote;
}

static char *next_token(parse_str_t *parse){
#define TOK_BUF_INCR (32)
#define _expand_token() if (buf_len<=(slen+strlen(xparsed))){\
                        buf_len=slen+strlen(xparsed)+TOK_BUF_INCR/2;\
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
            case A_TAB: 
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
                if ((xparsed=(char*)parse_var(parse))){
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
            case '=':
                if (slen){
                    parse->next--;
                    loop=FALSE;
                }else{
                    token[0]='=';
                    token[1]=0;
                    slen++;
                    loop=FALSE;        
                }
                break;
            case '"':
                if (slen){
                    parse->next--;
                }else{
                    free(token);
                    token=parse_quote(parse);
                    slen=strlen(token);
                } 
                loop=FALSE;
                break;
            default:
                if (slen>=(buf_len-1)){
                    buf_len+=TOK_BUF_INCR/2;
                    token=(char*)realloc(token,buf_len);
                    if (!token){
                        println(con,"reallocation failed");
                        return NULL;
                    }
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
    static int tok_count;
    char *result;
    static char **tokens;

    if (!setjmp(failed)){
        tokens=tokenize(&tok_count,script);
        if (tok_count && tokens) {
            result=execute_cmd(tok_count,(char **)tokens);
            free_tokens(tok_count,tokens);
            return result;
        } // if
    }else{
        free_tokens(tok_count,tokens);
    }
    return NULL;
}

void shell(void){
    char *str, cmd_line[CHAR_PER_LINE];
    int len;
    
    printf("VPC-32 shell version %s\r",_version);
    activ_directory=(char*)root_dir;
    while (1){
        printf(prompt);
        len=read_line(con,cmd_line,CHAR_PER_LINE);
        if (len){
            str=exec_script((const char*)cmd_line);
            if (str){
                printf(str);
                free(str);
            }//if
        }// if
    }//while(1)
}//shell()

