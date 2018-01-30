/* 
 * File:   rtcc.h
 * Author: jacques Deschênes
 *
 * Real Time Clock Calendar
 * interface avec le composant MCP7940N   
 * Création: 2018-01-29 
 */


#ifndef RTCC_H
#define	RTCC_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../HardwareProfile.h"
 
//#define RTCC

// deux premier chiffres du millénaire    
#define MILL_TENS  (2)
#define MILL_UNITS (0)
    
typedef struct time_struct{
    unsigned s:6;
    unsigned m:6;
    unsigned h:5;
} stime_t;

typedef struct date_struct{
    unsigned y:16;
    unsigned m:4;
    unsigned d:5;
} sdate_t;

BOOL leap_year(unsigned short year);

BOOL get_time(stime_t* time);
BOOL set_time(stime_t time);
BOOL get_date(sdate_t* date);
BOOL set_date(sdate_t date);
BOOL get_date_str(char *date);
BOOL get_time_str(char *time);


#ifndef RTCC
void update_mcu_dt();
#endif
#ifdef	__cplusplus
}
#endif

#endif	/* RTCC_H */

