/*
 * Real Time Clock Calendar
 * interface avec le composant MCP7940N   
 * Création: 2018-01-29 
 */

#include <stdio.h>
#include "../HardwareProfile.h"
#include "rtcc.h"
#include "../serial_comm/serial_comm.h"

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

#else


// MCP7940N registers
#define RTC_SEC (0)
#define RTC_MIN (1)
#define RTC_HOUR (2)
#define RTC_WKDAY (3)
#define RTC_DATE (4)
#define RTC_MTH (5)
#define RTC_YEAR (6)
#define RTC_CONTROL (7)
#define RTC_OSCTRIM (8)
#define RTC_ALM0SEC (0xa)
#define RTC_ALM0MIN (0xb)
#define RTC_ALM0HR (0xc)
#define RTC_ALM0WKDAY (0xd)
#define RTC_ALM0DATE (0xe)
#define RTC_ALM0MTH (0xf)
#define RTC_ALM1SEC (0x11)
#define RTC_ALM1MIN (0x12)
#define RTC_ALM1HR (0x13)
#define RTC_ALM1WKDAY (0x14)
#define RTC_ALM1DATE (0x15)
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

#define I2C_CTRL_BYTE (0xDE)
#define RTCC_READ (1)
#define RTCC_WRITE (0)
//#define _nop_nop() asm volatile ("nop\nnop\nnop\nnop\n");
#define _set_scl_high()  RTCC_SCL_LATSET=RTCC_SCL_PIN
#define _set_scl_low()   RTCC_SCL_LATCLR=RTCC_SCL_PIN
#define _set_sda_high()  RTCC_SDA_LATSET=RTCC_SDA_PIN
#define _set_sda_low()   RTCC_SDA_LATCLR=RTCC_SDA_PIN
#define _set_sda_as_input() RTCC_SDA_TRISSET=RTCC_SDA_PIN
#define _set_sda_as_output() RTCC_SDA_TRISCLR=RTCC_SDA_PIN

#define _i2c_delay(usec)  PR1=40*usec;IFS0bits.T1IF=0;\
                             T1CONbits.ON=1;\
                             while (!IFS0bits.T1IF);

// envoie 1 clock pulse I2C
// condition initiale
// SCL low
// SDA x
// sortie:
// SCL high
// SDA x
void i2c_clock(){
    _i2c_delay(5);
    _set_scl_high();
    _i2c_delay(5);
}

// réception d'un bit
// condition initiale
// SCL high
// SDA x en input mode
uint8_t i2c_receive_bit(){
     uint8_t bit;
    
    _set_scl_low();
    i2c_clock();
    _set_sda_as_input();
    bit=(RTCC_SDA_PORT&RTCC_SDA_PIN)>>RTCC_SDA_SHIFT;
    _set_sda_high();
    _set_sda_as_output();
    return bit;
}

// envoie un bit sur l'interface I2C
// condition initiale
// SDA en mode sortie
// SCL high
// SDA x
void i2c_send_bit(BOOL bit){
    _set_scl_low();
//    _nop_nop();
    if (bit){
        _set_sda_high();
    }else{
        _set_sda_low();
    }
    i2c_clock();
}


// initialise le bus I2C pour un début de transaction
// condition initiale
// SDA high
// SCL high
void i2c_start_bit(){
    _set_sda_high();
    _set_sda_as_output();
    _set_scl_high();
    _i2c_delay(1);
    _set_sda_low();
    _i2c_delay(5);
}

void i2c_sync(){
    _set_sda_as_input();
    while (!(RTCC_SDA_PORT&RTCC_SDA_PIN)){
        _set_scl_low();
        i2c_clock();
    }
    _set_sda_as_output();
}
// termine la transaction sur le bus I2C
// condition initiale
// SDA x
// SCL high
void i2c_stop_bit(){
    _set_scl_high();
    _set_sda_low();
    _set_sda_as_output();
    _set_sda_high();
    _i2c_delay(5);
   
}

// le MCU envoie un ACK bit au MCP7940N
// condition initiale
// SCL high
// SDA x
void i2c_send_ack_bit(){
    _set_scl_low();
//    _nop_nop();
    _set_sda_low();
    _set_sda_as_output();
    i2c_clock();
    _set_sda_as_input();
    _set_scl_low();
}

// retourne le ACK bit envoyé par le MCP7940N
// doit-être 0.
// condition initiale
// SCL low
// sda x
BOOL i2c_receive_ack_bit(){
    BOOL ack;

    _set_scl_low();
    _set_sda_as_input();
    i2c_clock();
    ack=((RTCC_SDA_PORT&RTCC_SDA_PIN)>>RTCC_SDA_SHIFT);
    _set_sda_high();
    _set_sda_as_output();
    return ack;
}

//envoie un octet au MCP7940N
// condition initiale
// la condition start est déjà initiée.
// SCL high
// SDA x
BOOL i2c_send_byte(uint8_t byte){
    uint8_t i;
    BOOL ack;
//    asm volatile ("di");
    for (i=128;i;i>>=1){
        i2c_send_bit(byte&i);
    }
    ack=i2c_receive_ack_bit();
//    asm volatile ("ei");
    return ack;
}

//reçoie un octet au MCP7940N
// condition initiale
// le préambule est déjà initiée.
// SCL high
// SDA mode entrée
uint8_t i2c_receive_byte(BOOL ack){
    uint8_t i, byte=0;
    
//    asm volatile ("di");
    _set_sda_as_input();
    for (i=0;i<8;i++){
        byte<<=1;
        byte+=i2c_receive_bit();
    }
    if (ack){
        i2c_send_bit(0);
    }else{
      //  i2c_send_bit(1);
        i2c_stop_bit();
    }
//    asm volatile ("ei");
    return byte;
}

uint8_t rtcc_read_byte(uint8_t addr){
    uint8_t byte;
    
//    i2c_sync();
    i2c_start_bit();
    i2c_send_byte(I2C_CTRL_BYTE|RTCC_WRITE);
    i2c_send_byte(addr);
    i2c_start_bit();
    i2c_send_byte(I2C_CTRL_BYTE|RTCC_READ);
    byte=i2c_receive_byte(FALSE);
}

void rtcc_write_byte(uint8_t addr, uint8_t byte){
//    i2c_sync();
    i2c_start_bit();
    i2c_send_byte(I2C_CTRL_BYTE|RTCC_WRITE);
    i2c_send_byte(addr);
    i2c_send_byte(byte);
    i2c_stop_bit();
};

void rtcc_init(){
    uint8_t byte;
    // l'entrée alarme recevant le signal du MCP7940N
    // la sortie sur le RTCC est open drain
    // on active le weak pullup sur l'entrée du MCU.
    RTCC_ALRM_WPUSET=RTCC_ALRM_PIN;
    // le signal clock de l'interface I2C avec le MCP7940N
    // on l'initialise en mode sortie open drain 
    // en haute impédance
//    RTCC_SCL_ODCSET=RTCC_SCL_PIN;
    RTCC_SCL_LATSET=RTCC_SCL_PIN;
    RTCC_SCL_TRISCLR=RTCC_SCL_PIN;
    // le signal data de l'interface I2C avec le MCP7940N
    // on l'initialise en mode sortie open drain.
    // en haute impédance
    RTCC_SDA_ODCSET=RTCC_SDA_PIN;
    _set_sda_high();
    _set_sda_as_input();
    _i2c_delay(5);
    byte=rtcc_read_byte(RTC_SEC);
    if (!(byte&128)){
        rtcc_write_byte(RTC_SEC,128);
    }
}
#endif


//void __ISR(_TIMER_1_VECTOR,IPL2SOFT) i2c_int_handler(){
//    IFS0bits.T1IF=0;
//    RTCC_SCL_LATINV=RTCC_SCL_PIN;
//}

/////////////////////////////////////////////////////

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


BOOL leap_year(unsigned short year){
    return (!(year/4) && (year/100)) || !(year/400); 
}
