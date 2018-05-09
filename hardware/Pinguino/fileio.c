/*	----------------------------------------------------------------------------
	FILE:			fileio.c
	PROJECT:		pinguino32
	PURPOSE:		SD Card file system functions
	AUTHORS:		Alfred Broda <alfredbroda@gmail.com>
					Mark Harper <markfh@f2s.com>
					Regis Blanchot <rblanchot@gmail.com>
	FIRST RELEASE:	23 dec. 2011
	LAST RELEASE:	06 jan. 2012
	----------------------------------------------------------------------------
	based on original code by Regis Blanchot and FatFS example for PIC24
	----------------------------------------------------------------------------
 	[30-03-12][hgmvanbeek@gmail.com][Some cards have no card detect and no write protect]
	07 May 2012	As part of providing support for PIC32 Pinguino Micro and
					potentially other cards removed #if defined (PIC32_Pinguino) etc
					and #endif in function mount() so that SDCS is set via mount 
					for all cards.
*/
/*
 * Original code modified for this project.
 */
#ifndef __FILEIO_C__
#define __FILEIO_C__

// standard C libraries used
#include <stdio.h>
#include <ctype.h>      		// toupper...
#include <string.h>     		// memcpy...
//#include <malloc.h>				// malloc, free?
#include <GenericTypeDefs.h>

#include "fileio.h"
#include "sdmmc.h"
#include "ff.h"					// Fat Filesystem
#include "diskio.h"				// card access functions
#include "../serial_comm/serial_comm.h"
#include "../../console.h"

//#define SD_DEBUG

//#ifdef SD_DEBUG
//    #include <__cdc.c>          // USB CDC functions
//#endif

/*	----------------------------------------------------------------------------
 mount
 initializes a MEDIA structure for FILEIO access
 will mount only the first partition on the disk/card
 --------------------------------------------------------------------------*/

BOOL SDCardReady;

static FATFS _Fat;

char mount(unsigned char pin) {
	int flag, i;
	FRESULT r;

	SDCS = pin;

	// 0. init the I/Os
	initSD();


#ifdef SD_DEBUG
	DebugPrint("Looking for SD slot... ");
#endif
	// 1. check if the card is in the slot
	if (!getCD()) {
		FError = FE_NOT_PRESENT;
#ifdef SD_DEBUG
		DebugPrint( " getCD Failed!\n");
#endif
		return FALSE;
	}
#ifdef SD_DEBUG
	DebugPrint("card in slot\n");
#endif

	// 2. initialize the card
#ifdef SD_DEBUG
	DebugPrint("Initializing SD card... ");
#endif
	initMedia();
    if (disk_initialize(0)==STA_NOINIT){
#ifdef SD_DEBUG
            DebugPrint("disk_initialize() failed!\n");
#endif
            return 0;
        };
#ifdef SD_DEBUG
        DebugPrint("disk_initialize() OK\n");
#endif
	// We're skipping the old step 3 because there's no need for malloc
	// This takes 6k off the code size if malloc is not used elsewhere.
	// Instead, just point it to our _Fat var.
	// The FATFS struct takes only 560 bytes of mem.
	Fat = &_Fat;

	// Mount media
#ifdef SD_DEBUG
	DebugPrint("Mounting FAT filesystem... ");
#endif
	r = f_mount(0, Fat);
	if (r != FR_OK) {
		FError = r;
#ifdef SD_DEBUG
		DebugPrint("Failed!\n");
#endif
		//free(Fat);
		return FALSE;
	}
#ifdef SD_DEBUG
	DebugPrint( "OK\n");
#endif

#ifdef SD_DEBUG
	DebugPrint("Checking FAT filesystem... ");
#endif
	const TCHAR * pth = "/";
	r = chk_mounted(&pth, &Fat, 0);
	if (r != FR_OK) {
		FError = r;
#ifdef SD_DEBUG
		DebugPrint( "Failed!\n");
                 //put_rc(r);
#endif
		unmount();
		return FALSE;
	}
#ifdef SD_DEBUG
	DebugPrint( "OK\n");
#endif

	return TRUE;
} // mount

/*	----------------------------------------------------------------------------
 unmount    initializes a MEDIA structure for FILEIO access
 --------------------------------------------------------------------------*/

void unmount(void) {
	f_mount(0, NULL);
	//free(Fat);
	SPI2CONCLR = 0x8000; // SPI2 OFF
} // unmount

/*	----------------------------------------------------------------------------
 present   test if a SD card is present
 --------------------------------------------------------------------------*/

char SD_present(unsigned char pin) {
	if (mount(pin)) {
		unmount();
		return TRUE;
	} else {
		return FALSE;
	}
}



// retourne l'adresse du dernier slash ou NULL
static char *last_slash(char *path){
    int i=0;
    char *last=NULL;
    
    while (path[i]){
        if (path[i]=='/'){
            last=&path[i];
        }
        i++;
    }
    return last;    
}

const char *current_dir=".";

char* set_filter(filter_t *filter, char *path){
    char *star, *slash, *file_spec;
    
    filter->criteria=eLIST_DIR;
    
    uppercase(path);
    star=strchr(path,'*');
    slash=last_slash(path);
    if (!star && !slash){
        filter->subs=path;
        filter->criteria=eACCEPT_SAME;
        path=(char*)current_dir;
        return path;
    }
    if (star==path && path[1]==0){
        path[0]='.';
        return path;
    }
    if (slash){
        file_spec=slash+1;
        *slash=0;
    }else{
        file_spec=path;
        path=(char*)current_dir;
    }
    if (file_spec[0]=='*'){
        filter->criteria++;
    }else{
        filter->criteria=eACCEPT_SAME;
    }
    if (file_spec[strlen(file_spec)-1]=='*'){
        file_spec[strlen(file_spec)-1]=0;
        filter->criteria++;
    }
    switch(filter->criteria){
        case eACCEPT_START_WITH:
        case eACCEPT_SAME:
            filter->subs=file_spec;
            break;
        case eACCEPT_END_WITH:
        case eACCEPT_ANY_POS:
            filter->subs=&file_spec[1];
            break;
        case eNO_FILTER:
        case eLIST_DIR:
            break;
    }//switch
    return path;
}//f()


bool filter_accept(filter_t *filter,const char* text){
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
        case eLIST_DIR:
        case eNO_FILTER:
            result=true;
            break;
    }
    return result;
}//f()


/*	----------------------------------------------------------------------------
 Scans the current disk and compiles a list of files with a given extension
 list     array of file names max * 8
 max      number of entries
 ext      file extension we are searching for
 return   number of files found
 --------------------------------------------------------------------------*/

//unsigned listTYPE(char *listname, long *listsize, int max, const char *ext )
unsigned listTYPE(DIRTABLE *list, int max, const char *ext)
//unsigned listTYPE(char *list, int max, const char *ext )
{
	//TODO: implement

	return 0;
} // listTYPE

// imprime les informations sur le fichier
void print_fileinfo(FILINFO *fi){
    char fmt[64];
    
// what about other outputs ?
    sprintf(fmt,"%c%c%c%c%c ",
            (fi->fattrib & AM_DIR) ? 'D' : '-',
            (fi->fattrib & AM_RDO) ? 'R' : '-',
            (fi->fattrib & AM_HID) ? 'H' : '-',
            (fi->fattrib & AM_SYS) ? 'S' : '-',
            (fi->fattrib & AM_ARC) ? 'A' : '-');
            print(con, fmt);
    sprintf(fmt,"%u/%02u/%02u %02u:%02u ",
            (fi->fdate >> 9) + 1980,
            (fi->fdate >> 5) & 15, fi->fdate & 31, (fi->ftime >> 11),
            (fi->ftime >> 5) & 63);
            print(con, fmt);
            sprintf(fmt," %9u ", fi->fsize);
            print(con, fmt);
    sprintf(fmt, " %-12s %s\n", fi->fname,
#if _USE_LFN
	Lfname);
#else
    "");
#endif
    print(con,fmt);
}

/* Prints the directory contents */
unsigned listDir(const char *path, filter_t *filter) {
	//TODO: remove all CDC references
	long p1;
	PF_BYTE res, b;
	UINT s1, s2;
	DIR dir; /* Directory object */
        char * fmt;
        dir.fs=Fat;
	res = f_opendir(&dir, path);
        if (!res) {
            p1 = s1 = s2 = 0;
            fmt=malloc(64);
            if (!fmt) {
                res=-1;
            }else{
                sprintf(fmt,"\nreading dirctory: ('%s')\n", path);
                print(con,fmt);
            }
        }else{
            return res;
        }
        while (!res) {
            res = f_readdir(&dir, &Finfo);
#ifdef SD_DEBUG
//          put_rc(res);
#endif
            if ((res != FR_OK) || !Finfo.fname[0]) {
                break;
            }
            if (filter_accept(filter,Finfo.fname)){
                if (Finfo.fattrib & AM_DIR) {
                    s2++;
                } else {
                    s1++;
                    p1 += Finfo.fsize;
                }
                print_fileinfo(&Finfo);
            }
        }// while (!res)
        if (res=FR_NO_FILE){
            res=FR_OK;
        }
        if (!res){
            sprintf(fmt, "\nfile count %d\ndirectory count %d\ntotal size %d\n",s1,s2,p1);
            print(con, fmt);
        }else{
            if (!fmt){
                print(con,"Memory allocation error.\n");
            }
        }
        free(fmt);
	return res;
} // listDir

char isDirectory(FILINFO file) {
	if (file.fattrib & AM_DIR) {
		return TRUE;
	} else {
		return FALSE;
	}
}

char isReadOnly(FILINFO file) {
	if (file.fattrib & AM_RDO) {
		return TRUE;
	} else {
		return FALSE;
	}
}

char isHidden(FILINFO file) {
	if (file.fattrib & AM_HID) {
		return TRUE;
	} else {
		return FALSE;
	}
}

char isSystem(FILINFO file) {
	if (file.fattrib & AM_SYS) {
		return TRUE;
	} else {
		return FALSE;
	}
}

char isArchive(FILINFO file) {
	if (file.fattrib & AM_ARC) {
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif /* __FILEIO_C__ */

