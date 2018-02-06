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

// deux premier chiffres du millénaire    
#define MILL_TENS  (2)
#define MILL_UNITS (0)
    
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
    unsigned dow:3;
    unsigned date:5;
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

extern BOOL rtcc_error;
void rtcc_init();
uint8_t rtcc_read_next();
void rtcc_read_buf(uint8_t addr, uint8_t *buf, uint8_t size);
uint8_t rtcc_read_byte(uint8_t addr);
void rtcc_write_many(uint8_t addr,uint8_t *data, uint8_t size);
void rtcc_write_byte(uint8_t addr, uint8_t byte);
void rtcc_calibration(uint8_t trim);

BOOL leap_year(unsigned short year);
void get_time(stime_t* time);
void set_time(stime_t time);
void get_date(sdate_t* date);
void set_date(sdate_t date);
void get_date_str(char *date);
void get_time_str(char *time);
BOOL set_alarm(sdate_t date, stime_t time, uint8_t *msg);
void get_alarms(alm_state_t *alm_st);
void cancel_alarm(uint8_t n);

#ifndef RTCC
void update_mcu_dt();
#endif
#ifdef	__cplusplus
}
#endif

#endif	/* RTCC_H */

