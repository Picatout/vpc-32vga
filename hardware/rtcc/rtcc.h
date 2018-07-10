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
 
#define RTCC

// MCP7940N registers
#define RTC_SEC (0)
#define RTC_MIN (1)
#define RTC_HOUR (2)
#define RTC_WKDAY (3)
#define RTC_DAY (4)
#define RTC_MTH (5)
#define RTC_YEAR (6)
#define RTC_CONTROL (7)
#define RTC_OSCTRIM (8)
#define RTC_ALM0SEC (0xa)
#define RTC_ALM0MIN (0xb)
#define RTC_ALM0HR (0xc)
#define RTC_ALM0WKDAY (0xd)
#define RTC_ALM0DAY (0xe)
#define RTC_ALM0MTH (0xf)
#define RTC_ALM1SEC (0x11)
#define RTC_ALM1MIN (0x12)
#define RTC_ALM1HR (0x13)
#define RTC_ALM1WKDAY (0x14)
#define RTC_ALM1DAY (0x15)
#define RTC_ALM1MTH (0x16)
#define RTC_PWRDNMIN (0x18)
#define RTC_PWRDNHR (0x19)
#define RTC_PWRDNDATE (0x1a)
#define RTC_PWRDNMTH (0x1b)
#define RTC_PWRUPMIN (0x1c)
#define RTC_PWRUPHR (0x1d)
#define RTC_PWRUPDATE (0x1e)
#define RTC_PWRUPMTH (0x1f)
#define RTC_RAMBASE (0x20)
#define RTC_RAMSIZE (64)

//bits dans RTC_CONTROL
#define ALMIF (1<<3)
#define ALM0EN_MSK  (1<<4)
#define ALM1EN_MSK (1<<5)
// bit dans WKDAY
#define VBATEN_MSK (1<<3)
#define OSCRUN_MSK (1<<5)    
#define PWRFAIL_MSK (1<<4)

// mode bits dans RTC_ALMxWKDAY  bits:4..6
// mode 7 ->  compare tous les champs sec,min,heure,jour,date,mois
#define MODE_ALLFIELDS (7<<4) 
    
    
typedef struct time_struct{
    unsigned sec:6;
    unsigned min:6;
    unsigned hour:5;
} stime_t;

typedef struct date_struct{
    unsigned year:16;
    unsigned month:4;
    unsigned day:5;
    unsigned wkday:3;
} sdate_t;

typedef struct alm_state{
    unsigned sec:6;
    unsigned min:6;
    unsigned hour:5;
    unsigned wkday:3;
    unsigned day:5;
    unsigned month:4;
    unsigned enabled:1;
    char msg[32];
}alm_state_t;

enum WEEKAYS{
    SUNDAY,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY
};

extern const char *weekdays[7];
extern BOOL rtcc_error;

typedef enum RTCC_ERROR{
    RTCC_ERR_NONE,
    RTCC_ERR_COMM,
    RTCC_ERR_OSCRUN,
    RTCC_ERR_VBATEN
}rtcc_error_t;

rtcc_error_t rtcc_init();

int rtcc_calibration(int trim);
void rtcc_get_time(stime_t* time);
void rtcc_set_time(stime_t time);
void rtcc_get_date(sdate_t* date);
void rtcc_set_date(sdate_t date);
void rtcc_get_date_str(char *date);
void rtcc_get_time_str(char *time);
BOOL rtcc_set_alarm(sdate_t date, stime_t time, uint8_t *msg);
void rtcc_get_alarms(alm_state_t *alm_st);
void rtcc_cancel_alarm(uint8_t n);
void rtcc_power_down_stamp(alm_state_t *pdown);

BOOL leap_year(unsigned short year);
uint8_t day_of_week(sdate_t *date);

#ifndef RTCC
void update_mcu_dt();
#endif
#ifdef	__cplusplus
}
#endif

#endif	/* RTCC_H */

