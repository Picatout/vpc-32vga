/*
 * Real Time Clock Calendar
 * interface avec le composant MCP7940N   
 * Création: 2018-01-29 
 */

#include <stdio.h>

#include "rtcc.h"

#ifndef RTCC

static volatile stime_t  mcu_time;
static volatile sdate_t  mcu_date;


static void mcu_set_time(stime_t time){
    mcu_time.h=time.h;
    mcu_time.m=time.m;
    mcu_time.s=time.s;
}

static void mcu_get_time(stime_t *t){
    t->h=mcu_time.h;
    t->m=mcu_time.m;
    t->s=mcu_time.s;
}

static void mcu_set_date(sdate_t date){
    mcu_date.y=date.y;
    mcu_date.m=date.m;
    mcu_date.d=date.d;
}

static void mcu_get_date(sdate_t *d){
    d->y=mcu_date.y;
    d->m=mcu_date.m;
    d->d=mcu_date.d;
}

static const unsigned day_in_month[12]={31,28,31,30,31,30,31,31,30,31,30,31};

static void next_day(){
    mcu_date.d++;
    if (mcu_date.d>day_in_month[mcu_date.m-1]){
        if(mcu_date.m==2){
            if (!leap_year(mcu_date.y)|| (mcu_date.d==30)){mcu_date.m++;}
            mcu_date.d=1;
        }else{
            mcu_date.m++;
            mcu_date.d=1;
        }
        if (mcu_date.m>12){
            mcu_date.y++;
            mcu_date.m=1;
        }
    }
}

// met à jour date et heure
// à toute les millisecondes
// appellé à partir de CoreTimerHandler()
void update_mcu_dt(){
    mcu_time.s++;
    if (!(mcu_time.s%60)){
        mcu_time.s=0;
        mcu_time.m++;
        if (!(mcu_time.m%60)){
            mcu_time.m=0;
            mcu_time.h++;
            if (mcu_time.h==24){
                mcu_time.h=0;
                next_day();
            }
        }
    }
    
}

#endif

BOOL leap_year(unsigned short year){
    return (!(year/4) && (year/100)) || !(year/400); 
}



BOOL get_time(stime_t* time){
#ifdef RTCC
#else
    mcu_get_time(time);
    return TRUE;
#endif    
}

BOOL set_time(stime_t time){
#ifdef RTCC
#else
    mcu_set_time(time);
    return TRUE;
#endif    
}

BOOL get_date(sdate_t* date){
#ifdef RTCC
#else 
    mcu_get_date(date);
    return TRUE;
#endif    
}

BOOL set_date(sdate_t date){
#ifdef RTCC 
#else
    mcu_set_date(date);
    return TRUE;
#endif     
}

BOOL get_date_str(char *date_str){
    sdate_t d;
    if (get_date(&d)){
        sprintf(date_str,"%02d/%02d/%02d",d.y,d.m,d.d);
        return TRUE;
    }else{
        return FALSE;
    }
}

BOOL get_time_str(char *time_str){
    stime_t t;
    if (get_time(&t)){
        sprintf(time_str,"%02d:%02d:%02d",t.h,t.m,t.s);
        return TRUE;
    }else{
        return FALSE;
    }
}


