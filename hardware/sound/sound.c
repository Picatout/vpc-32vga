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
 * File:   sound.c
 * Author: Jacques Deschênes
 *
 * Created on 13 septembre 2013, 20:42
 * rev: 2017-07-31
 */

#include <string.h>
#include <plib.h>
#include "../HardwareProfile.h"
#include "sound.h"
#include "../../vpcBASIC/BASIC.h"

enum MUSIC_CTRL_CODE{
    ePLAY_PAUSE,   // "Pn"
    eNOTE_LENGTH,  // "Ln"
    eOCTAVE_DOWN,  // '<' 
    eOCTAVE_UP,    // '>'
    eOCTAVE,       // "On"
    eTEMPO,        // "Tn"
    eNOTE_MODE,    // "MS|ML|MN"
    ePLAY_STOP,    // fin de la mélodie
};


const float octave0[12]={
    32.70, // DO
    34.65, // DO#,RÉb
    36.71, // RÉ
    38.89, // RÉ#,MIb
    41.20, // MI
    43.65, // FA
    46.25, // FA#,SOLb
    49.0,  // SOL
    51.91, // SOL#,LAb
    55.0,  // LA
    58.27, // LA#,SIb
    61.74, // SI
};

const float tone_fraction[]={
    0.0, // eTONE_PAUSE
    0.25,
    0.375,
    0.5, 
    0.625,
    0.75, // eTONE_STACCATO
    0.875, // eTONE_NORMAL
    1.0  // eTONE_LEGATO
};


volatile unsigned int duration;
volatile unsigned int audible;
volatile unsigned char tone_play;
volatile unsigned char tune_play;


static volatile tone_fraction_t fraction=eTONE_NORMAL;

static volatile note_t *tones_list;

int sound_init(){
    OC3CONbits.OCM = 5; // PWM mode
    OC3CONbits.OCTSEL=1; // use TIMER3
    T3CON=(3<<4); // timer 3 prescale 1/8.
    IPC3bits.T3IP=2; // timer interrupt priority
    IPC3bits.T3IS=0; // sub-priority
    IEC0bits.T3IE=0;
    tone_play=0;
    tune_play=0;
    duration=0;
    audible=0;
    fraction=eTONE_LEGATO;
    tones_list=NULL;
    return 0;
}

void tone(float freq, // fréquence hertz
          uint16_t msec) // durée  millisecondes
{       float count;
      
    tone_play=0;
    //configuration OC3 pour génération tonalité.
    OC3RS=0;
    T3CONbits.ON=0;
    if (freq<100.0){
        count=(SYSCLK>>5)/freq;
        T3CONbits.TCKPS=5;
    }else{
        count=(SYSCLK>>3)/freq;
        T3CONbits.TCKPS=3;
    }
    PR3=(uint16_t)count-1;
    OC3R=(uint16_t)(count/2);
    duration=msec;
    audible=duration-msec*tone_fraction[fraction];
    mTone_on();
    T3CONbits.ON=1;
    tone_play=1;
} //tone();

// play a sequence of tones
void tune(const note_t *buffer){
    if (buffer && buffer->freq>0.0 && buffer->duration){
        tones_list=(note_t*)buffer;
        IFS0bits.T3IF=0;
        IEC0bits.T3IE=1;
        fraction=tones_list->fraction;
        tone(tones_list->freq,tones_list->duration);
        tune_play=255;
        tones_list++;
    }
}//tune()

#define FRQ_BEEP 1000.0
#define BEEP_MSEC 40
void beep(){
    tone(FRQ_BEEP,BEEP_MSEC);
}

/*********************************************
 * fonctions et variables utilisées par play()
 *********************************************/
#define WHOLE 1
#define HALF  2
#define QUARTER 4
#define HEIGHTH 8

static unsigned note_len=QUARTER; // ronde=1,blanche=2,noire=4,etc
static uint8_t tempo=120; // nombre de noires par minute
static uint8_t octave=4; // {0..6}
static bool syntax_error;
static char *play_str;
static volatile note_t *play_list=NULL;
static note_t *walking;
static bool play_background;
static volatile bool free_list;


static void set_tempo(uint8_t t){
   tempo=t; 
}

static void set_tone_fraction(tone_fraction_t f){
    fraction=f&7;
}

// définie l'octave actif {0..6}    
static void set_octave(unsigned o){
    octave=o%7;
}

//durée de la note.
static void set_note_len(unsigned t){
    note_len=t&63;
}

const char *note_name="C D EF G A B";
static int note_order(char c){
    char *adr;
    adr=strchr(note_name,c);
    if (!adr){
        syntax_error=true;
        return -1;
    }else{
        return adr-note_name;
    }
}

// calcule la durée de la note.
// la durée dépend du tempo et de la durée de la note
// temps=60000/tempo*lnote/QUARTER
static uint16_t calc_duration(int lnote,//longueur note:1=ronde,2=blanche,4=noire,etc
                              int alteration)// note pointée.
{
#define MSEC_PER_MINUTE 60000 // nombre de millisecondes dans une minute.
    unsigned time;
    time=MSEC_PER_MINUTE*QUARTER/lnote/tempo;
    switch(alteration){
        case 1:
            time=time*3/2;
            break;
        case 2:
            time=time*7/4;
            break;
    }
    return time;
}

// retourne la fréquence en fonction de la note 
// et de l'octave actif.
static float frequency(unsigned o, unsigned note){
    return (1<<o)*octave0[note];
}

static int number(){
    int n=0,d=0;
    
    while (isdigit(*play_str)){
        n=n*10+(*play_str)-'0';
        play_str++;
        d++;
    }
    if (!d) {syntax_error=true;}
    return n;
}

static bool parse_note(){
    char c;
    int o=octave,note,nlen=note_len;
    int state=0;
    int alteration=0;
   
    while (!syntax_error && (state<5) && (c=*play_str)){
        play_str++;
        switch(state){
            case 0:
                note=note_order(c);
                if (syntax_error){
                    break;
                }
                state=1;
                break;
            case 1: // modification hauteur de la note?
                switch(c){
                    case '+':
                    case '#':
                        note++;
                        if ((note>11) && o<6){
                            note=0;
                            o++;
                        }
                        state=2;
                        break;
                    case '-':
                        note--;
                        if ((note<0) && (o>0)){
                            note=11;
                            o--;
                        }
                        state=2;
                        break;
                    case '.':
                        alteration++;
                        state=3;
                        break;
                    default:
                        if (isdigit(c)){
                            play_str--;
                            nlen=number();
                            state=3;
                        }else{
                            play_str--;
                            state=5;
                        }
                }//swtich
                break;
            case 2: // modificateur longueur note?
                if (isdigit(c)){
                    nlen=number();
                    state=3;
                }else{
                    play_str--;
                    state=3;
                }
                break;
            case 3: // altération de la durée?
                if (c=='.'){
                    alteration++;
                }else{
                    play_str--;
                    state=5;
                }
                break;
        }//switch(state)              
    }//while
    if (!syntax_error && (state || !c)){
        if ((note<0) || (note>11)||(o<0)||(o>6)){
            walking->freq=0.0;
        }else{
            walking->freq=frequency(o,note);
        }
        walking->duration=calc_duration(nlen,alteration);
        walking->fraction=fraction;
        walking++;
        return true;
    }
    return false;
}//parse_note()



static void parse_options(){
    switch (*play_str++){
        case 'B': // play background
            play_background=true;
            break;
        case 'F': // play foreground
            play_background=false;
            break;
        case 'S': // play staccato
            set_tone_fraction(eTONE_STACCATO);
            break;
        case 'L': // play legato
            set_tone_fraction(eTONE_LEGATO);
            break;
        case 'N': // play normal
            set_tone_fraction(eTONE_NORMAL);
            break;
        default: // erreur de syntaxe.
            syntax_error=true;
    }
}//parse_options


// joue la mélodie représentée par la chaîne de caractère
// ref: https://en.wikibooks.org/wiki/QBasic/Full_Book_View#PLAY
int play(const char *melody){
#define SIZE_INCR 32
    int i=0,n,o;
    char c,*pstr;
    int size=SIZE_INCR;
    
    if (tune_play){
        IEC0bits.T3IE=0;
        tune_play=false;
        if (play_list){
           free((void*)play_list);
       }
    }
    play_list=malloc((size)*sizeof(note_t));
    if (!play_list){return eERR_ALLOC; }
    walking=(note_t*)play_list;
    pstr=malloc(strlen(melody)+1);
    if (!pstr){
        free((void*)play_list);
        return eERR_ALLOC;
    }
    strcpy(pstr,melody);
    play_str=pstr;
    uppercase(play_str);
    play_background=false;
    free_list=true;
    note_len=QUARTER;
    octave=4;
    tempo=120;
    fraction=eTONE_NORMAL;
    syntax_error=false;
    //compile la mélodie dans play_list
    while (!syntax_error && (c=*play_str)){
        play_str++;
        switch(c){
            case ' ': // ignore les espaces
                break;
            case 'L': // set_note_len()
                n=number();
                if (!syntax_error && n){
                    set_note_len(n);
                }
                break;
            case 'M': // options
                parse_options();
                break;
            case 'N': // joue une note selon son indice 0..84
                n=number();
                if (syntax_error){
                    break;
                }
                if (n){
                    o=(n-1)/12; // octave
                    n=(n-1)%12; // note 0..11
                }else{
                    n=12;
                }
                if (n==12){ // pause
                    walking->freq=0.0;
                }else{
                    walking->freq=frequency(o,n);
                }
                walking->duration=calc_duration(note_len,0);
                walking->fraction=fraction;
                walking++;
                i++;
                break;
            case 'O':// set_octave
                o=number();
                if (!syntax_error){
                    set_octave(o);
                }
                break;
            case 'P': // pause
                n=number();
                if (syntax_error){
                    break;
                }
                walking->freq=0.0;
                walking->duration=calc_duration(n,0);
                walking++;
                i++;
                break;
            case 'T': // set_tempo
                n=number();
                if (syntax_error){
                    break;
                }
                if (n>=32 && n<256){
                    set_tempo(n);
                }else{
                    syntax_error=true;
                }
                break;
            case '<':
                if (octave) octave--;
                break;
            case '>':
                if (octave<6) octave++;
                break;
            case 'X': // X n'est pas reconnue et mais fin à la compilation.
                syntax_error=true;
                break;
            default:
                play_str--;
                if (parse_note()){i++;}
        }//switch(c)
        if (i==size){
            size+=SIZE_INCR;
            play_list=realloc((void*)play_list,size*sizeof(note_t));
            if (!play_list){return eERR_ALLOC;}
            walking=(note_t*)play_list+i;
        }
    }//while
    if (!syntax_error){
        free(pstr);
        walking->freq=0.0;
        walking->duration=0;
        tune((note_t*)play_list);
        if (!play_background){
            while (tune_play);
        }
        return eERR_NONE;
    }else{
        free((void*)play_list);
        println(1,pstr);
        spaces(1,(int)(play_str-pstr));
        put_char(1,*play_str);
        free(pstr);
        return eERR_PLAY;
    }
}//play()



// TIMER3 interrupt service routine
// select next note to play
void __ISR(_TIMER_3_VECTOR, IPL2SOFT)  T3Handler(void){
    
       mT3ClearIntFlag();
       if (tune_play && !tone_play){ 
           set_tone_fraction(tones_list->fraction);
           if (tones_list->freq>0.0){// tonalitée
               tone(tones_list->freq,tones_list->duration);
               tones_list++;
           }else{ // pause ou fin de liste 
               if (tones_list->duration>0){  // pause
                    duration=tones_list->duration;
                    audible=duration;
                    tone_play=1;
                    tones_list++;
               }else{ // fin de la liste
                    IEC0bits.T3IE=0;
                    tune_play=0;
                    if (free_list){
                        free((void*)play_list);
                        play_list=NULL;
                        free_list=false;
                    }
                    set_tone_fraction(eTONE_LEGATO); //valeur par défaut.
               }
           }
       }// if
}// T3Handler

