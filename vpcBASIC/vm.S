/*  Nom: vm.S
 *  Auteur: Jacques Deschênes
 *  Description:
 *      machine viruelle à piles qui exécute du bytecode.
 *      le programme utilisant la VM doit l'appellé en utilisant le prototype de
 *      fonction suivant: void StackVM( char *ip);
 *         ip pointe vers le bytecode à exécuter
 *  rev: 2018-03-20
 */

#define _ASM_CODE_
    
#include <p32xxxx.h>

#include "BASIC.h"
    
    
// fonctions en C    
.extern sys_ticks
.extern con  
.extern uppercase
.extern clear_screen
.extern clear_line
.extern clear_eol
.extern get_key
.extern wait_key
.extern read_line
.extern get_curpos
.extern set_curpos
.extern put_char
.extern print
.extern spaces
.extern invert_video
.extern crlf
.extern print_int
.extern print_hex
.extern println
.extern scroll_down
.extern scroll_up
.extern set_tab_witdh
.extern get_tab_width
.extern alloc_var_space
.extern string_alloc
.extern string_free
.extern delay_ms
    
//math 
.extern print_float
.extern srand    
.extern rnd
.extern sinf
.extern cosf
.extern tanf
.extern acosf
.extern asinf
.extern atanf
.extern sqrtf
.extern fabsf
.extern powf
.extern expf
.extern logf
.extern log10f
.extern rintf
.extern floorf
.extern ceilf
//graphiques
.extern getPixel
.extern setPixel
.extern clearPixel
.extern line
.extern rectangle
.extern circle
.extern ellipse
.extern polygon
.extern bezier
.extern fill
// son
.extern tone
.extern tune
    

/*************** macros *************/

.macro _dpush
sw T, 0(dp)
addiu dp,dp, CELL_SIZE
.endm

.macro _dpop
addiu dp,dp,-CELL_SIZE
lw T, 0(dp)
.endm

.macro _drop_b_next
addiu dp,dp,-CELL_SIZE
b next
lw T, 0(dp)
.endm
    
.macro _rpush
sw R, 0(rp)
addiu rp,rp,CELL_SIZE
.endm

.macro _rpop
addiu rp,rp,-CELL_SIZE
lw R, 0(rp)
.endm

.macro _inc_ip
addiu ip,ip,1
.endm

.macro _back_jump
    lbu t0,0(ip)
    b next
    sub ip,ip,t0
.endm
    
.macro _fore_jump
    lbu t0,0(ip)
    b next
    add ip,ip,t0
.endm
    
.macro _console
    la a0,con
    lbu a0,0(a0)
.endm
    
    
/* utilisation des regitres MIPS */
ip=s0  /* pointeur d'instruction VM */
dp=s1  /* pointeur pile de données */
rp=s2  /* pointeur pile de contrôle */
w=s3  /* pointeur de travail */
T=s4  /* sommet de la pile des données */
R=s5  /* sommet de la pile des retours */
limit=s6  /* limite boucle FOR ... NEXT */
step=s7  /* incrément boucle FOR ... NEXT */ 
frame=s8 /* pointeur variables locales */

RAM_SEG=0xA000
CODE_SEG=0x9D00
SFR_SEG=0xBF80
CFG_SEG=0xBFC0
    
CELL_SIZE=4
    
DSTACK_SIZE=128
RSTACK_SIZE=128
TIB_SIZE=80
PAD_SIZE=80
    
SFR_CLEAR=4
SFR_SET=8
SFR_INV=12

.data
f_trace: .word 0
    
.bss
.global dstack,rstack,rstack0,dstack0
// piles
.section .bss.dstack bss    
dstack: .space DSTACK_SIZE
.section .bss.rstack bss
rstack: .space RSTACK_SIZE

.data    
dstack0: .word dstack+CELL_SIZE
rstack0: .word rstack+CELL_SIZE
    
    
.text
.align 2, 0xff
.set noreorder
.set macro


.global StackVM
.ent StackVM
StackVM:  // VM initialization
    addiu sp, sp, -40 // save registers
    sw  s0, 0(sp)
    sw  s1, 4(sp)
    sw  s2, 8(sp)
    sw  s3,12(sp)
    sw  s4,16(sp)
    sw  s5,20(sp)
    sw  s6,24(sp)
    sw  s7,28(sp)
    sw  s8,32(sp)
    sw  ra,36(sp)
    move ip, a0   // pointeur bytecode à exécuter
    lui w, RAM_SEG
    ori dp, w, dstack
    ori rp, w, rstack
    addu T,zero,zero
    addu R,zero,zero
    la frame,dstack0
    lw frame,0(frame)
    sw zero,0(dp)
    sw zero,CELL_SIZE(dp)
    
// VM instruction loop
next:
    la t0,f_trace
    lw t0,0(t0)
    beqz t0,1f
    nop
    jal dots
    nop
1:  lbu   t0, 0(ip)
    sll   t0, t0, 2   
    la t1,opcodes_table
    addu t1,t1,t0

#ifdef _CHECK_OPCODE    
    la t0, bad_op
    subu t0,t0,t1
    blez t0,vm_bad_op
#endif    
    lw    t1, 0(t1)
    jr    t1
    _inc_ip

#ifdef _CHECK_OPCODE    
vm_bad_op:
    _console
    la a1,badop_msg
    jal print
    nop
    _console
    lbu a1,0(ip)
    jal print_int
    addu a2,zero,zero
    b 1f
    addiu v0,zero,-1
#endif    
bye:   // 'bye' ( -- ) exit virtual machine
    jal depth
1:  lw  s0,0(sp)
    lw  s1,4(sp)
    lw  s2,8(sp)
    lw  s3,12(sp)
    lw  s4,16(sp)
    lw  s5,20(sp)
    lw  s6,24(sp)
    lw  s7,28(sp)
    lw  s8,32(sp)
    lw  ra,36(sp)
    jr  ra
    addiu sp,sp,40

qrx: // '?key' ( -- ch -1 | 0 )
    la a0,con
    jal get_key
    lbu a0,0(a0)
    _dpush
    beqz v0, next
    addu T,v0,zero 
    addiu t0,zero,-1
    _dpush
    b next
    move T, t0

space: // ( -- ) émet un caractère ascii 32
    _dpush
    addiu T,zero,32
emit:   // 'emit' ( ch -- ) transmet charactère au terminal
    _console
    jal put_char
    move  a1, T
    _drop_b_next

cr: // 'cr' ( -- ) émet un carriage return line feed
    la a0,con
    jal crlf
    lbu a0,0(a0)
    b next
    nop
    
    
lit:    // 'lit' ( -- n) empile un entier
    _dpush
    ulw T,0(ip)
    b next
    addiu ip,ip,CELL_SIZE

clit:   // 'clit' ( -- byte ) empile un octet signe étendu sur 32 bits
    _dpush
    lbu T, 0(ip)
    b next
    _inc_ip

//wlit:  // 'wlit' ( -- w16  ) empile un mot de 16 bits
//    _dpush
//    lwr T,0(ip)
//    andi T,T,0xffff
//    b next
//    addiu ip,ip,2

lits: // 's"' ( -- addr) empile l'adresse d'une chaîne litérale
    _dpush
    move T,ip
    lbu w,0(T)
    addu ip,ip,w
    b next
    _inc_ip
    
bra:   // 'branch' ( -- ) branchement incondionnnel (adresse relative +127,-128)
    lb t0, 0(ip)
    _inc_ip
    b next
    addu ip, ip, t0  // branchement relatif à ip, intervale -128 à +127

qbra: // '?bra' ( t|f -- ) branche si sommet pile est vrai (tos!=0)
    move t0, T
    _dpop
    beqz t0, next
    _inc_ip
    lb t0, -1(ip)
    b next
    addu ip,ip,t0 // relatif à ip {-128,127}

qbraz:  // '?braz' ( t|f -- ) branche si sommet pile est faux (tos==0)
    move t0, T
    _dpop
    bne t0, zero, next
    _inc_ip
    lb t0, -1(ip)
    b next
    addu ip,ip,t0 // relatif à ip {-128,127}

store:  // '!' ( n addr -- )  variable = n
    lw t0, -CELL_SIZE(dp)
    usw t0,0(T)
    lw T,-2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE
    
// load variable in T
fetch:  // '@' ( addr -- n ) T= *addr
    move t0, T
    ulw T,0(t0)
    b next
    nop

cstore:  // 'c!' ( char addr --  ) *addr = char
    lw t0, -CELL_SIZE(dp)
    sb t0, 0(T)
    lw T, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

cfetch:  // 'c@' ( addr -- char )  T = (char)*addr
    b next
    lb T, 0(T)

rfetch:   // 'r@'  ( -- n R: n -- n )  empile le sommet de la pile des retours
    _dpush
    b next
    move T, R

tor:  // '>r' ( n -- ) (R: -- n ) envoie T sur pile retour
    _rpush
    move R, T
    _drop_b_next
    
rfrom:  // 'r>' ( -- n ) (R: n -- )  envoie R dans T
    _dpush
    move T, R
    lw  R, 0(rp)
    b next
    addiu rp,rp, -CELL_SIZE


dcnt:  // 'dcnt' ( -- n) nombre d'éléments sur la pile de données
    _dpush
    la t0,dstack
    subu t0,dp,t0
    b next
    sra T, t0,2

rcnt:  // 'rcnt' ( -- n ) nombre d'éléments sur la pile des retours
    _dpush
    la t0,rstack
    subu T,rp,t0
    addiu t0,t0,1 // + élément dans R
    b next
    sra T,T,2

ipfetch: // 'ip@' ( -- ip ) empile la valeur courante du pointeur d''instructions
    _dpush
    b next
    move T, ip

ipstore: // 'ip!' ( ip -- ) initialise le pointeur d''insturctions
    move ip, T
    _drop_b_next
    
drop:  // 'drop' ( n -- )
    _drop_b_next
    
twodrop: // '2drop' ( n1 n2 -- )
    lw T, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

dup:  // 'dup' ( n -- n n )
    sw T, 0(dp)
    b next
    addiu dp,dp,CELL_SIZE

twodup: // (n2 n1 -- n2 n1 n2 n1)
    addiu dp,dp,2*CELL_SIZE
    sw T,-2*CELL_SIZE(dp)
    lw t0, -3*CELL_SIZE(dp)
    b next
    sw t0, -CELL_SIZE(dp)

qdup:   // ?dup ( n -- n n | n ) duplique n si n!=0
    beq T,zero, next
    nop
    sw T, 0(dp)
    b next
    addiu dp,dp,CELL_SIZE

swap: // 'swap' (n2 n1 -- n1 n2 )
    move t0, T
    lw T, -CELL_SIZE(dp)
    b next
    sw t0, -CELL_SIZE(dp)

over:   // 'over' ( n2 n1 -- n2 n1 n2 )
    _dpush
    b next
    lw T, -2*CELL_SIZE(dp)

rot: // ( n3 n2 n1 -- n2 n1 n3)
    move t0, T
    lw T,-2*CELL_SIZE(dp)
    lw t1, -CELL_SIZE(dp)
    sw t0, -CELL_SIZE(dp)
    b next
    sw t1, -2*CELL_SIZE(dp)

invert: // ( n -- -n)  
    b next
    subu T,zero,T
    
plus:  // '+'   ( n1 n2 -- n1+n2 )
    lw t0,-CELL_SIZE(dp)
    addu T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

oneplus:  // '1+'  (n -- n+1 )
    b next
    addiu T, T, 1

plusstore:  // '+!'  ( n addr -- ) *addr  = *addr+n
    ulw t0,0(T)
    lw t1,-CELL_SIZE(dp)
    add t0,t1,t0
    usw t0,0(T)
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)

minus:   //  '-' ( n1 n2 -- n1-n2 )
    lw t0,-CELL_SIZE(dp)
    subu T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

oneminus:   // '1-' ( n -- n-1 )
    b next
    addiu T, T, -1

star:   // '*' ( n1 n2 -- n1*n2 ) multiplication signé, résultat simple précision.
    lw t0,-CELL_SIZE(dp) 
    mul T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

ustar:   // 'U*' ( u1 u2 -- u3 ) multiplication non signe, résultat simple précision
    lw t0,-CELL_SIZE(dp)
    multu  t0, T
    mflo T
    b next
    addiu dp,dp,-CELL_SIZE

mstar:  // 'M*' ( n1 n2 -- d) multiplication signée avec résultat double précision.
    lw t0, -CELL_SIZE(dp)
    mult t0,T
    mflo t0
    mfhi T
    b next
    sw t0, -CELL_SIZE(dp)

umstar: // 'UM*' ( u1 u2 -- ud ) multiplication non signé avec résultat double précision.
    lw t0, -CELL_SIZE(dp)
    multu t0,T
    mflo t0
    mfhi T
    b next
    sw t0, -CELL_SIZE(dp)

slash:  // '/' ( n1 n2 -- n1/n2 )  division signée
    lw t0, -CELL_SIZE(dp)
    div t0,T
    mflo T
    b next
    addiu dp,dp,-CELL_SIZE

uslash:  // 'U/'  (u1 u2 -- u3)  division non signée
    lw t0,-CELL_SIZE(dp)
    divu t0,T
    mflo T
    b next
    addiu dp,dp,-CELL_SIZE

twostar:  # '2*'  ( n  -- 2*n ) multiply by 2 (shift left 1 bit)
    b next
    sll T,T,1

twoslash: // '2/'  ( n -- n/2 )  divide by 2 ( arithmetic shift right 1 bit)
    b next
    sra T,T,1

divmod:  // '/mod'  ( n1 n2 -- n1%n2 n1/n2 )
    lw t0, -CELL_SIZE(dp)
    div t0, T  # n1/n2
    mfhi t0    #  remainder
    sw t0, -CELL_SIZE(dp)
    b next
    mflo T     # quotient

lshift:  // '<<' ( n1 u -- n1<<u )
    lw t0, -CELL_SIZE(dp)
    sllv T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

rshift:  // '>>'  ( n1 u -- n1>>u )
    lw t0,-CELL_SIZE(dp)
    srlv T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

modulo:  // 'mod' ( n1 n2 -- n1%n2 )
    lw t0, -CELL_SIZE(dp)
    div t0,T
    mfhi T
    b next
    addiu dp,dp,-CELL_SIZE

min:  // min (n1 n2 -- min )  conserve le plus petit
    lw t0, -CELL_SIZE(dp)
    subu t1,t0,T
    bgtz t1, next
    addiu dp,dp,-CELL_SIZE
    b next
    move T,t0

max:  // max (n1 n2 -- max ) conserve le plus grand
    lw t0,-CELL_SIZE(dp)
    subu t1,t0,T
    bltz t1, next
    addiu dp,dp,-CELL_SIZE
    b next
    move T,t0

abs:  // 'abs' ( n -- abs(n) )
    bgez T, next
    nop
    b next
    negu T

bitand: // 'and' ( n1 n2 -- n1&n2)
    lw t0, -CELL_SIZE(dp)
    and T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

bitor:  // 'or' ( n1 n2 -- n1|n2)
    lw t0, -CELL_SIZE(dp)
    or T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

bitxor: // 'xor' ( n1 n2 -- n1^n2)
    lw t0, -CELL_SIZE(dp)
    xor T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

bitnot: // 'not' ( n1 -- ~n1 )
    b next
    not T,T

equal:  // '='  ( n1 n2 -- t|f ) n1==n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    beq T,t0,next
    li T, -1
    b next
    li T, 0

nequal: // '<>' ( n1 n2 -- t|f ) n1<>n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    bne t0,T, next
    li T, -1
    b next
    li T, 0

ltz:  // '0<'  ( n -- t|f )
    b next
    slt T,T,zero

zequal: // '0=' ( n -- t|f ) vrai si n == 0 sinon faux
    b next
    sltiu T,1

less:  //  '<'  ( n1 n2 -- t|f ) n1<n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bltz t0, next
    li T, -1
    b next
    li T, 0

greater:  // '>'  ( n1 n2 -- t|f ) n1>n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bgtz t0, next
    li T, -1
    b next
    li T, 0

lteq:  // '<=' ( n1 n2 -- t|f ) n1<=n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    blez t0, next
    li T,-1
    b next
    li T, 0

gteq:  // '>=' ( n1 n2 -- t|f ) n1>=n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bgez t0, next
    li T, -1
    b next
    li T, 0

key:  // ( -- char ) attend réception caractère de la console
    la a0,con
    jal wait_key
    lbu a0,0(a0)
    _dpush
    b next
    move T, v0  // T=v0


// imprime une chaîne litérale imbriquée dans le code.    
//  ip pointe vers le texte.    
prt_str: // IPSTR ( -- )
    lbu a1, 0(ip)
    _inc_ip
    beqz a1, next
    nop
    la a0,con
    jal put_char
    lbu a0,0(a0)
    b prt_str
    nop

type: // ( addr -- ) // imprime la chaîne ascciz
    _console
    jal print
    move a1,T
    _drop_b_next
    
dot: // "." ( n -- ) imprime un entier en format libre
    _console
    move a1,T
    jal print_int
    move a2,zero
    _drop_b_next
    
    
delay: // 'delay' ( msec -- ) delais en millisecondes
    jal delay_ms
    move a0,T
    _drop_b_next
    
ticks:  // 'ticks'  ( -- n ) empile le compteur sys_tick
    _dpush
    la w,sys_ticks
    b next
    lw T, 0(w)

forsave: // ISAVEFOR ( R: -- limit step ) sauvegarde limit et step sur rstack
    _rpush
    move R,limit
    _rpush
    b next
    move R,step

forrest: // IRESTFOR ( R: L S -- ) restitue les valeurs originales de limit et step après NEXT
    move step,R
    _rpop
    move limit,R
    lw R,0(rp)
    b next
    addiu rp,rp,-CELL_SIZE
    
for: // IFOR ( limit step -- ) initialise une boucle FOR...NEXT 
    lw limit, -CELL_SIZE(dp) 
    move step,T
    lw T, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-(2*CELL_SIZE)
    
// vrai si var dépasse la limite    
fortest: // IFORTEST ( n -- f|t ) n=valeur variable boucle.  
   move t0,T
   bgtz step, 1f
   subu t0,t0,limit
   bltz t0,next
   addu T,zero,zero
   b next
   addiu T,zero,-1
1: blez t0,next
   addiu T,zero,-1
   b next
   addu T,zero,zero
   
fornext: // INEXT ( adr -- ) incrémente la variable for
    lw t0,0(T)
    addu t0,t0,step
    sw t0,0(T)
    _drop_b_next
    
//sauvegarde variable local
//[IP] indique la position de la variable
local_store: // ( valeur -- )
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    addu t0,frame,t0
    sw T,0(t0)
    _drop_b_next
    
//empile variable locale
//[IP] indique la position  de la variable
local_fetch: // ( -- n )
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    addu t0,frame,t0
    _dpush
    b next
    lw T,0(t0)
    
//empile l'adresse de la variable locale
// [IP] indique la position de la variable
//sur la pile
local_addr: //( -- addr )
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    _dpush
    b next
    addu T,frame,t0

// créé un pointeur de référence vers les arguments
// et les variables locales.    
//sauvegarde frame sur R
// frame=dp+CELL_SIZE
set_frame:// (R:  -- old_frame )
    _rpush
    move R,frame //préserve la valeur actuelle de frame
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    addu dp,dp,t0
    b next
    addiu frame,dp,CELL_SIZE
    
// réserve de l'espace sur dstack pour les variables locales    
// [ip] nombre de variables
lc_space: // ( -- ) 
    lw t0,0(ip)
    _inc_ip
    sll t0,t0,2
    b next
    addu dp,dp,t0
    
    
// quitte une sous-routine ou fonction
// restore dp, ip et frame
leave: // (R: frame ip -- )
    addiu dp,frame,-CELL_SIZE 
    move ip,R
    _rpop
    move frame,R
    addiu rp,rp,-CELL_SIZE
    b next
    lw R,0(rp)
    
    
// appelle une sous-routine ou fonction
// addr->uint32_t  addresse sous-routine    
sr_call: // (addr --  R:  -- ip )
    _rpush
    move R,ip
    move ip,T
    _drop_b_next
    
pick: // ( xu...,x1,x0,u -- xu...,x1,x0,xu ) empile une copie du Uième élément de la pile
    addiu w,dp,-CELL_SIZE
    sll  T, T, 2
    subu w,w,T
    b next
    lw T, 0(w)

sine: // (angle -- sine)
    move a0, T
    jal sinf
    b next
    move T, v0

acos: // (angle -- cos)
    move a0, T
    jal cosf
    b next
    move T, v0

asin: // (sine -- angle)
    move a0, T
    jal asinf
    b next
    move T, v0

// initialize le générateur pseudo-hasard
// en utilisant sys_ticks    
randomize: // ( -- )
    la a0,sys_ticks
    jal srand
    lw a0,0(a0)
    b next
    nop

// retourne un entier pseudo aléatoire    
random: // ( -- n )
    jal rand
    _dpush
    b next
    move T, v0

trunc: // ( f -- i)
    move a0, T
    jal rintf
    b next
    move T, v0

num: //

    b next
    nop

beep: // ( freq,duration -- ) fait entendre une tonalitée
    move a1,T
    _dpop
    move a0,T
    jal tone
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
melody: // ( addr -- ) fait entendre la mélodie contenue dans le tableau
    move a0,T
    jal tune
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
 
clr_scr: // ( -- ) efface l'écran
    la a0,con
    jal clear_screen
    lbu a0,0(a0)
    b next
    nop
    
//retourne la grandeur d'une table
// cette valeur précède le premier
// élément de la table    
array_bound: // ( adr -- size )
    b next
    lw T,0(T)
 
// active/désactive le mode trace
// imprime l'état de dstack avant exécution de chaque opcode    
trace: // ( f -- )
    la t0,f_trace
    sw T,0(t0)
    _drop_b_next
  
// retourne l'adresse de la chaîne imbriquée dans le code
// ip pointe la chaîne
str_adr: // ( -- addr )    
    _dpush
    move T, ip
    lbu t0,0(ip)
1:  beqz t0,next
    _inc_ip
    b 1b
    lbu t0,0(ip)
    
// retourne la longueur de la chaîne dont l'adresse est sur dstack
str_len: // ( adr -- length )    
    move t0,T
    move T,zero
1:  lbu t1,0(t0)
    beqz t1,next
    addiu t0,t0,1
    b 1b
    addiu T,T,1
    
// lecture d'une ligne de text à partir de la consoel
// addr  adresse du tampon de réception
// n1  longueur du tampon
// n2  longueur de la chaîne lue.    
readln: // ( addr n1 -- addr n2 )
    _console
    lw a1,-CELL_SIZE(dp)
    jal read_line
    move a2,T
    b next
    move T,v0
    
// convertie la chaîne en entier    
// addr adresse de la châine asciiz
// n valeur entière extraite de la chaîne    
to_int: // addr -- n )
    jal atoi
    move a0,T
    b next
    move T,v0

// alloue de la mémoire dans l'espace progamme.
// n taille en octets
// addr adresse du bloc mémoire alloué ou 0 si manque de mémoire    
alloc: // ( n -- addr )
    jal alloc_var_space
    move a0,T
    b next
    move T,v0
 
//alloue de l'espace sur le heap pour une chaîne asciiz
// n longueur de la chaîne en octets    
str_alloc: // ( n -- addr )    
    jal string_alloc
    move a0,T
    j next
    move T,v0
    
    
// libère une chaîne préalablement allouée sur le heap.
str_free: // ( addr -- )
    jal string_free
    move a0,T
    _drop_b_next
    
    
// copie une chaîne asciiz de src vers dest
// src adresse chaîne à copier
// dest adresse destination    
str_cpy: // ( src dest -- dest )
    lw a1,-CELL_SIZE(dp)
    jal strcpy
    move a0,T
    b next
    addiu dp,dp,-CELL_SIZE
    
// retourne la ligne du curseur texte {0..29}    
curline: // ( -- line )
    _console
    jal vga_get_curpos
    _dpush
    b next
    srl T,v0,16
    
// retourne la colonne du curseur texte {0..79}    
curcolon: // ( -- colon )
    _console
    jal get_curpos
    _dpush
    b next
    andi T,v0,0xff
    
.end StackVM
    
    
//*************************************************
depth: // retourne le nombre d'éléments sur dstack
    la v0,dstack0
    lw v0,0(v0)
    subu v0,dp,v0
    sra v0,v0,2
    jr ra    
    addiu v0,v0,1
    
.global dots
.ent dots
dots:    
    addiu sp,sp,-12
    sw s6,0(sp)
    sw s7, 4(sp)
    sw ra,8(sp)
    la s7,con
    lbu s7,0(s7)
    // imprime "trace: "
    la a1,trace_msg
    jal print
    move a0,s7
    jal depth
    move a0,s7
    move a1,v0
    move s6,a1
    // imprime nombre d'éléments sur dstack
    move a0,s7
    jal print_int
    move a2,zero
    // imprime "s: "
    la a1,s_msg
    jal print
    move a0,s7
    // si dstack vide va à 3f
    beqz s6,3f
    nop
    la s6,dstack0
    lw s6,0(s6)
    // boucle d'impression des éléments de dstack
1:  subu t0,s6,dp
    bgez t0,2f
    lw a1,0(s6)
    addu a2,zero,zero
    jal print_hex
    move a0,s7
    b 1b
    addiu s6,s6,CELL_SIZE
    // imprime T
2:  move a1,T
    move a0,s7
    jal print_hex
    move a2,zero
    // imprime "next opcode: "
3:  la a1,opcode_msg
    jal print
    move a0,s7
    lbu a1,0(ip)
    addu a2,zero,zero
    jal print_int
    move a0,s7
    jal crlf
    move a0,s7
    lw s6,0(sp)
    lw s7,4(sp)
    lw ra,8(sp)
    jr ra
    addiu sp,sp,12
    
.end dots
    
// routines names must be in same order as OP_CODES enum in vm.h    
opcodes_table:
.word bye,lit,clit,bra,qbra,store,fetch,cstore
.word cfetch,rfetch,tor,rfrom,drop,dup,swap,over,pick,invert
.word plus,minus,star,ustar,mstar,umstar,slash,uslash,modulo,ltz
.word zequal, bitand,bitor
.word bitxor,bitnot,ticks, delay
.word qdup, oneplus, oneminus,plusstore, twostar, twoslash, twodrop, rot, twodup
.word  min, max, abs, lshift, rshift, divmod, qbraz, dcnt, rcnt
.word equal,nequal, less, greater,lteq,gteq,forsave,forrest,for,fortest,fornext
.word sine, cos, tan, atan, acos, asin,randomize,random,trunc,num
.word key,qrx,emit,prt_str,cr,dot,lits,type,space
.word beep,melody,clr_scr,array_bound    
.word local_store,local_fetch,local_addr,set_frame,sr_call,leave,lc_space
.word trace,str_adr,str_len,readln,to_int,alloc,str_alloc,str_free,str_cpy
.word curline,curcolon
    
    
bad_op:  
badop_msg: .asciz "bad operating code: "    
trace_msg: .asciz "\ntrace: "
opcode_msg: .asciz "\nnext opcode: "
s_msg: .asciz "s: "
    
#undef _ASM_CODE_