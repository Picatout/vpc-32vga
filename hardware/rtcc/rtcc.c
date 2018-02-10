/*
 * Real Time Clock Calendar
 * interface avec le composant MCP7940N   
 * Création: 2018-01-29 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <plib.h>

#include "../HardwareProfile.h"
#include "rtcc.h"
#include "../serial_comm/serial_comm.h"
#include "../../console.h"
#include "../tvout/vga.h"
#include "../sound/sound.h"


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
#define PWRFAIL_MSK (1<<4)

// mode bits dans RTC_ALMxWKDAY  bits:4..6
// mode 7 ->  compare tous les champs sec,min,heure,jour,date,mois
#define MODE_ALLFIELDS (7<<4) 



#define I2C_CTRL_BYTE (0xDE)
#define RTCC_READ (1)
#define RTCC_WRITE (0)
#define _set_scl_high()  RTCC_SCL_LATSET=RTCC_SCL_PIN
#define _set_scl_low()   RTCC_SCL_LATCLR=RTCC_SCL_PIN
#define _set_sda_high()  RTCC_SDA_LATSET=RTCC_SDA_PIN
#define _set_sda_low()   RTCC_SDA_LATCLR=RTCC_SDA_PIN
#define _set_sda_as_input() RTCC_SDA_TRISSET=RTCC_SDA_PIN
#define _set_sda_as_output() RTCC_SDA_LATSET=RTCC_SDA_PIN;\
                             RTCC_SDA_TRISCLR=RTCC_SDA_PIN

#define I2C_CLK_PER (3)

//#define _usec_delay(usec)   PR1=40*usec;IFS0bits.T1IF=0;\
//                            T1CONbits.ON=1;\
//                            while (!IFS0bits.T1IF);

#define _usec_delay(x)   asm volatile (".set noreorder\n");\
                         asm volatile ("addiu $t0,$zero,%0"::"I"(x*20));\
                         asm volatile ("1: bne $t0,$zero,1b\naddiu $t0,$t0,-1\n");\
                         asm volatile (".set reorder\n");

BOOL rtcc_error;
static jmp_buf  env;

// envoie 1 clock pulse I2C
// condition initiale
// SCL low
// SDA x
// sortie:
// SCL low
// SDA x
static void i2c_clock(){
    asm volatile ("nop\n nop\n nop\n nop");
    asm volatile ("nop\n nop\n nop\n nop");
    _set_scl_high();
    _usec_delay(I2C_CLK_PER);
    _set_scl_low();
    _usec_delay(I2C_CLK_PER);
}

// réception d'un bit
// condition initiale
// SCL low
// SDA x en input mode
static uint8_t i2c_receive_bit(){
     uint8_t bit;
     
    _set_sda_as_input();
    _set_scl_high();
    _usec_delay(I2C_CLK_PER);
    bit=(RTCC_SDA_PORT&RTCC_SDA_PIN)>>RTCC_SDA_SHIFT;
    _set_scl_low();
    _usec_delay(I2C_CLK_PER);
    _set_sda_as_output();
    return bit;
}

// envoie un bit sur l'interface I2C
// condition initiale
// SDA en mode sortie
// SCL high
// SDA x
static void i2c_send_bit(BOOL bit){
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
static void i2c_start_bit(){
    _set_sda_high();
    _set_sda_as_output();
    _set_scl_high();
    _usec_delay(1);
    _set_sda_low();
    _usec_delay(I2C_CLK_PER);
    _set_scl_low();
    _usec_delay(I2C_CLK_PER);
}

// termine la transaction sur le bus I2C
// condition initiale
// SDA x
// SCL high
static void i2c_stop_bit(){
    _set_scl_high();
    _set_sda_low();
    _set_sda_as_output();
    _set_sda_high();
    _usec_delay(I2C_CLK_PER);
}

// le MCU envoie un ACK bit au MCP7940N
// condition initiale
// SCL low
// SDA x
static void i2c_send_ack_bit(){
    _set_sda_low();
    i2c_clock();
}

// retourne le ACK bit envoyé par le MCP7940N
// doit-être 0.
// condition initiale
// SCL low
// sda x
static BOOL i2c_receive_ack_bit(){
    BOOL ack;

    ack=(BOOL)i2c_receive_bit();
    if (ack){
        longjmp(env,1);
    }
    return ack;
}

//envoie un octet au MCP7940N
// condition initiale
// la condition start est déjà initiée.
// SCL high
// SDA x
static void i2c_send_byte(uint8_t byte){
    uint8_t i;
    for (i=128;i;i>>=1){
        i2c_send_bit(byte&i);
    }
    i2c_receive_ack_bit();
}

//reçoie un octet du MCP7940N
// condition initiale
// le préambule est déjà initiée.
// SCL high
// SDA mode entrée
static uint8_t i2c_receive_byte(BOOL send_ack){
    uint8_t i, byte=0;
    
    for (i=0;i<8;i++){
        byte<<=1;
        byte+=i2c_receive_bit();
    }
    if (send_ack){
        i2c_send_bit(0);
    }else{
        i2c_stop_bit();
    }
    return byte;
}

uint8_t rtcc_read_next(){
    uint8_t byte;
    rtcc_error=FALSE;
    if (!setjmp(env)){
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_READ);
        byte=i2c_receive_byte(FALSE);
        return byte;
    }else{
        i2c_stop_bit();
        rtcc_error=TRUE;
    }
}

void rtcc_read_buf(uint8_t addr,uint8_t *buf, uint8_t size){
    uint8_t dummy;
    if (!size) return;
    rtcc_error=FALSE;
    if (!setjmp(env)){
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_WRITE);
        i2c_send_byte(addr);
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_READ);
        while (size>1){
            *buf++=i2c_receive_byte(TRUE);
            size--;
        }
        *buf=i2c_receive_byte(FALSE);
    }else{
        i2c_stop_bit();
        rtcc_error=TRUE;
    }
}

uint8_t rtcc_read_byte(uint8_t addr){
    uint8_t byte;
    
    rtcc_error=FALSE;
    if (!setjmp(env)){
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_WRITE);
        i2c_send_byte(addr);
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_READ);
        byte=i2c_receive_byte(FALSE);
        return byte;
    }else{
        i2c_stop_bit();
        rtcc_error=TRUE;
    }
}

void rtcc_write_buf(uint8_t addr,uint8_t *buf, uint8_t size){
    rtcc_error=FALSE;
    if (!setjmp(env)){
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_WRITE);
        i2c_send_byte(addr);
        while (size){
            i2c_send_byte(*buf++);
            size--;
        }
        i2c_stop_bit();
    }else{
        i2c_clock();
        i2c_stop_bit();
        rtcc_error=TRUE;
    }
}

void rtcc_write_byte(uint8_t addr, uint8_t byte){
    rtcc_error=FALSE;
    if (!setjmp(env)){
        i2c_start_bit();
        i2c_send_byte(I2C_CTRL_BYTE|RTCC_WRITE);
        i2c_send_byte(addr);
        i2c_send_byte(byte);
        i2c_stop_bit();
    }else{
        i2c_clock();
        i2c_stop_bit();
        rtcc_error=TRUE;
    }
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
    _set_sda_as_output();
    _usec_delay(5);
    byte=rtcc_read_byte(RTC_WKDAY);
    if (!rtcc_error){
        if (!(byte&(1<<5))){ // oscillateur inactif
            rtcc_write_byte(RTC_SEC,128); //activation oscillateur
        }
        if (!(byte&(1<<3))){// support pile désactivé.
            rtcc_write_byte(RTC_WKDAY,(1<<3)); // activation support pile.
        };
    }
    // interruption lorsque la broche alarm descend à zéro.
    CNCONBbits.ON=1;
    CNENBbits.CNIEB1=1;
    IPC8bits.CNIP=1;
    IPC8bits.CNIS=1;
    byte=rtcc_read_byte(RTC_CONTROL);
    if (byte&(ALM0EN_MSK|ALM1EN_MSK)){
        IFS1bits.CNBIF=0;
        IEC1bits.CNBIE=1;
    }
}




/////////////////////////////////////////////////////

void rtcc_calibration(uint8_t trim){
    rtcc_write_byte(RTC_OSCTRIM,trim);
}


static uint8_t bcd2dec(uint8_t hb){
    return (hb>>4)*10+(hb&0xf);
}

static uint8_t dec2bcd(uint8_t dec){
    return dec%10+((dec/10)<<4);
}


void get_time(stime_t* time){
    uint8_t buf[3];
    rtcc_read_buf(RTC_SEC,buf,3);
    time->sec=bcd2dec(buf[0]&0x7f);
    time->min=bcd2dec(buf[1]&0x7f);
    time->hour=bcd2dec(buf[2]&0x3f);

}

void set_time(stime_t time){
    uint8_t byte[3];
    byte[0]=128+dec2bcd(time.sec);
    byte[1]=dec2bcd(time.min);
    byte[2]=dec2bcd(time.hour);
    rtcc_write_buf(RTC_SEC,byte,3);
}

void get_date(sdate_t* date){
    uint8_t buf[4];
    rtcc_read_buf(RTC_WKDAY,buf,4);
    date->wkday=buf[0]&7;
    date->day=bcd2dec(buf[1]&0x3f);
    date->month=bcd2dec(buf[2]&0x1f);
    date->year=2000+bcd2dec(buf[3]);
}


void set_date(sdate_t date){
    uint8_t byte[4];
    byte[0]=8+dec2bcd(date.wkday);
    byte[1]=dec2bcd(date.day);
    byte[2]=dec2bcd(date.month);
    byte[3]=dec2bcd(date.year-2000);
    rtcc_write_buf(RTC_WKDAY,byte,4);
}

const char *weekdays[7]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

void get_date_str(char *date_str){
    sdate_t d;
    get_date(&d);
    sprintf(date_str,"%s %4d/%02d/%02d ",weekdays[d.wkday-1],d.year,d.month,d.day);
}

void  get_time_str(char *time_str){
    stime_t t;
    get_time(&t);
    sprintf(time_str,"%02d:%02d:%02d ",t.hour,t.min,t.sec);
}

// retourne le jour de la semaine à partir de la date
// REF: https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Implementation-dependent_methods
// méthode de Sakamoto
// dimanche=1,samedi=7
uint8_t day_of_week(sdate_t *date){
const   static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y;
    uint8_t dow;
    y=date->month < 3?date->year-1:date->year;
    dow=1+((y + y/4 - y/100 + y/400 + t[date->month-1] + date->day) % 7);
    return dow;
}

// détermine si l'année est bisextile
// REF: https://fr.wikipedia.org/wiki/Ann%C3%A9e_bissextile#R%C3%A8gle_actuelle
BOOL leap_year(unsigned short year){
    uint8_t byte;
    if (!year){
        byte=rtcc_read_byte(RTC_MTH);
        return byte&(1<<5);
    }else{
        return (!(year/4) && (year/100)) || !(year/400); 
    }
}

BOOL set_alarm(sdate_t date, stime_t time, uint8_t *msg){
    uint8_t byte, buf[6];
    
    byte=rtcc_read_byte(RTC_CONTROL);
    if ((byte&(ALM0EN_MSK|ALM1EN_MSK))==(ALM0EN_MSK|ALM1EN_MSK)){ return FALSE;} // pas d'alarme disponible.
    buf[0]=dec2bcd(time.sec);
    buf[1]=dec2bcd(time.min);
    buf[2]=dec2bcd(time.hour);
    buf[3]=dec2bcd(date.wkday)|(MODE_ALLFIELDS);
    buf[4]=dec2bcd(date.day);
    buf[5]=dec2bcd(date.month);
    if (!(byte & (ALM0EN_MSK))){ // alarme 0 disponible
        rtcc_write_buf(0x20,(uint8_t*)msg,32);
        rtcc_write_buf(RTC_ALM0SEC,buf,6);
        byte|=(1<<4);
    }else{ // alarme 1 disponible
        rtcc_write_buf(0x40,(uint8_t*)msg,32);
        rtcc_write_buf(RTC_ALM1SEC,buf,6);
        byte|=(1<<5);
    }
    rtcc_write_byte(RTC_CONTROL,byte);
    if (byte&(ALM0EN_MSK|ALM1EN_MSK)){
        IFS1bits.CNBIF=0;
        IEC1bits.CNBIE=1;
    }
}

// rapporte l'état des 2 alarmes
void get_alarms(alm_state_t *alm_st){
static const int alarm[2]={RTC_ALM0SEC,RTC_ALM1SEC};
static const int enable[2]={ALM0EN_MSK,ALM1EN_MSK};
    uint8_t ctrl_byte, alm_regs[6];
    int i;
    ctrl_byte=rtcc_read_byte(RTC_CONTROL);
    for (i=0;i<2;i++){
        rtcc_read_buf(alarm[i],alm_regs,6);
        alm_st[i].enabled=ctrl_byte&enable[i]?1:0;
        alm_st[i].sec=bcd2dec(alm_regs[0]);
        alm_st[i].min=bcd2dec(alm_regs[1]);
        alm_st[i].hour=bcd2dec(alm_regs[2]&0x3f);
        alm_st[i].wkday=alm_regs[3]&7;
        alm_st[i].day=bcd2dec(alm_regs[4]);
        alm_st[i].month=bcd2dec(alm_regs[5]);
        rtcc_read_buf(0x20*(i+1),(uint8_t*)&alm_st[i].msg,32);
    }
}

void cancel_alarm(uint8_t n){
    uint8_t ctrl_byte;
    ctrl_byte=rtcc_read_byte(RTC_CONTROL);
    if (!n){
        ctrl_byte&=~ALM0EN_MSK;
    }else{
        ctrl_byte&=~ALM1EN_MSK;
    }
    rtcc_write_byte(RTC_CONTROL,ctrl_byte);
}

void power_down_stamp(alm_state_t *pdown){
    uint8_t wkday, buf[4];
    rtcc_read_buf(RTC_PWRDNMIN,buf,4); 
    pdown->min=bcd2dec(buf[0]);
    pdown->hour=bcd2dec(buf[1]);
    pdown->day=bcd2dec(buf[2]);
    pdown->month=bcd2dec(buf[3]&0x1f);
    wkday=buf[3]>>5;
    if (wkday){ wkday--;}
    pdown->wkday=wkday;
    wkday=rtcc_read_byte(RTC_WKDAY);
    wkday&=~PWRFAIL_MSK;
    rtcc_write_byte(RTC_WKDAY,wkday);
}

static void alarm_msg(char *msg){
static  const unsigned int ring_tone[8]={329,250,523,250,329,250,0,0};
    text_coord_t cpos;
    uint8_t scr_save[HRES];
    BOOL active,invert;
 
    if ((active=is_cursor_active())){
        show_cursor(FALSE);
    }
    invert=is_invert_video();
    invert_video(TRUE);
    tune(ring_tone);
    msg[31]=0;
    memcpy((void*)scr_save,video_bmp,HRES);
    cpos=get_curpos();
    set_curpos(0,0);
    clear_eol();
    print(comm_channel,"ALARM: ");
    print(comm_channel,msg);
    print(comm_channel,"  <any key> to exit");
    wait_key(comm_channel);
    memcpy(video_bmp,(void*)scr_save,HRES);
    if (!invert){
        invert_video(FALSE);
    }
    set_curpos(cpos.x,cpos.y);
    if (active){
        show_cursor(TRUE);
    }else{
        show_cursor(FALSE);
    }
    
}


__ISR (_CHANGE_NOTICE_VECTOR,IPL1SOFT) alarm(){
   uint8_t byte,wkday, msg[32];

   if (CNSTATBbits.CNSTATB1 && !(RTCC_ALRM_PORT&RTCC_ALRM_PIN)){
        byte=rtcc_read_byte(RTC_CONTROL);
        if (byte & ALM0EN_MSK){ // alarme 0 active?
            wkday=rtcc_read_byte(RTC_ALM0WKDAY);
            if (wkday&ALMIF){
                byte&=~ALM0EN_MSK;
                rtcc_read_buf(0x20,msg,32);
                alarm_msg(msg);
            }
        }
        if (byte & ALM1EN_MSK){ // alarme 1 active?
            wkday=rtcc_read_byte(RTC_ALM1WKDAY);
            if (wkday&ALMIF){
                byte&=~ALM1EN_MSK;
                rtcc_read_buf(0x40,msg,32);
                alarm_msg(msg);
            }
        }
        rtcc_write_byte(RTC_CONTROL,byte);
        if (!(byte&(ALM0EN_MSK|ALM1EN_MSK))){
            IEC1bits.CNBIE=0;
        }
   }
   IFS1bits.CNBIF=0;
}

