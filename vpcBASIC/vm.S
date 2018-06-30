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
.extern free_not_ref    
.extern abort_signal    
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
.extern print_float    
.extern println
.extern scroll_down
.extern scroll_up
.extern set_tab_witdh
.extern get_tab_width
.extern alloc_var_space
.extern string_alloc
.extern string_free
.extern delay_ms
.extern sprite
.extern beep
.extern play
.extern sram_save
.extern sram_load
.extern var_value
    //fichiers
.extern close_all_files
.extern basic_fclose    
.extern basic_fopen
.extern basic_feof
.extern basic_fexist
.extern basic_fgetc
.extern basic_fgets
.extern basic_fputc
.extern basic_fputs    
.extern basic_fseek    
    
//math 
.extern srand    
.extern rnd
// fonctions virgule flottante    
.extern print_float
.extern sinf
.extern cosf
.extern tanf
.extern acosf
.extern asinf
.extern atanf
.extern sqrtf
.extern cbrtf    
.extern fabsf
.extern powf
.extern expf
.extern logf
.extern log10f
.extern rintf
.extern floorf
.extern ceilf
.extern strtof
.extern dtostrf
// fonctions hyperbolique
.extern coshf
.extern sinhf
.extern tanhf
.extern acoshf
.extern asinhf
.extern atanhf    
    
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
    
.macro _dict label, name  
FN_\label :    
    .asciz "\name"
    .align 2
    .word FN_\label
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
    
DSTACK_SIZE=256
RSTACK_SIZE=256
TIB_SIZE=80
PAD_SIZE=80
    
SFR_CLEAR=4
SFR_SET=8
SFR_INV=12

.global f_trace    
.data
f_trace: .byte 0
    
.bss

    // piles
.section .bss.dstack bss    
dstack: .space DSTACK_SIZE
.section .bss.rstack bss
rstack: .space RSTACK_SIZE

.global heap_size
    
.data    
dstack0: .word dstack
rstack0: .word rstack

.text
.align 2, 0xff
.set noreorder
.set macro


.global StackVM
.ent StackVM
StackVM:  // VM initialization
    addiu sp, sp, -40 // save registers
    sw  s0, 36(sp)
    sw  s1, 32(sp)
    sw  s2, 28(sp)
    sw  s3, 24(sp)
    sw  s4, 20(sp)
    sw  s5, 16(sp)
    sw  s6, 12(sp)
    sw  s7, 8(sp)
    sw  s8, 4(sp)
    sw  ra, 0(sp)
    move ip, a0   // pointeur bytecode à exécuter
    lui w, RAM_SEG
    ori dp, w, dstack
    ori rp, w, rstack
    addu T,zero,zero
    addu R,zero,zero
    addiu frame,dp,CELL_SIZE
    sw zero,0(dp)
    sw zero,CELL_SIZE(dp)
#ifdef _CHECK_USER_ABORT    
    la a0,abort_signal
    sw zero,0(a0)
#endif
    
// VM instruction loop
next:
#ifdef _CHECK_USER_ABORT
    la a0,abort_signal
    lw t0,0(a0)
    beqz t0,1f
    sw zero,0(a0)
    b abort
    addiu T,zero,ERR_USER_ABORT
1:    
#endif    
#ifdef _CHECK_STACKS
    jal depth
    nop
    addiu t0,v0,-DSTACK_SIZE
    bltz t0, 1f
    addiu v0,zero,ERR_DSTACK_OVF
    b bye2
    nop
    jal rdepth
    nop
    addiu t0,v0,-RSTACK_SIZE
    bltz t0, 1f
    addiu v0,zero,ERR_RSTACK_OVF
    b bye2
    nop
#endif    
1:  
    lbu   t0, 0(ip)
    sll   t0, t0, 2   
    la t1,opcodes_table
    addu t1,t1,t0

#ifdef _CHECK_OPCODE    
    la t0, bad_op
    subu t0,t0,t1
    blez t0,vm_bad_op
#endif    
    lw    t1, 0(t1)
    la t0,f_trace
    lw t0,0(t0)
    beqz t0,1f
    nop
    jal print_state
    nop
1:  jr    t1
    _inc_ip

#ifdef _CHECK_OPCODE    
vm_bad_op:
    b bye2
    addiu v0,zero,ERR_BAD_OPCODE
#endif   

_dict ABORT,"ABORT"    
abort:
    b bye2
    move v0,T

_dict BYE,"BYE"  
bye:   // 'bye' ( -- ) exit virtual machine
    jal depth
    nop
    beq v0,zero,bye2
    nop
    addiu v0,zero,ERR_DSTACK_NOT_EMPTY
bye2:
    lw  s0,36(sp)
    lw  s1,32(sp)
    lw  s2,28(sp)
    lw  s3,24(sp)
    lw  s4,20(sp)
    lw  s5,16(sp)
    lw  s6,12(sp)
    lw  s7,8(sp)
    lw  s8,4(sp)
    lw  ra,0(sp)
    jr  ra
    addiu sp,sp,40

_dict KEYQ,"KEY?"   
// lecture clavier sans attente
// retourne 0 si aucune touche disponible    
qrx: // '?key' ( -- ch | 0 )
    la a0,con
    jal get_key
    lbu a0,0(a0)
    _dpush
    b next
    move T, v0

_dict SPACES,"SPACES"    
vm_spaces: // ( n -- ) émet n espaces
    _console
    jal spaces
    move a1,T
    _drop_b_next

_dict EMIT,"EMIT"    
emit:   // 'emit' ( ch -- ) transmet charactère au terminal
    _console
    jal put_char
    move  a1, T
    _drop_b_next

_dict CR,"CR"    
cr: // 'cr' ( -- ) émet un carriage return line feed
    la a0,con
    jal crlf
    lbu a0,0(a0)
    b next
    nop
    
_dict LIT,"LIT"    
lit:    // 'lit' ( -- n) empile un entier
    _dpush
    ulw T,0(ip)
    b next
    addiu ip,ip,CELL_SIZE

_dict WLIT,"WLIT"    
wlit:  // 'wlit' ( -- w16  ) empile un mot de 16 bits
    _dpush
    lbu T,0(ip)
    lb t0,1(ip)
    sll t0,t0,8
    or T,T,t0
    b next
    addiu ip,ip,2

_dict CLIT,"CLIT"    
clit:   // 'clit' ( -- byte ) empile un octet non signé
    _dpush
    lbu T, 0(ip)
    b next
    _inc_ip

_dict BRA , "BRA"   
//  branchement relatif incondionnnel {-32768..32767}    
bra:   // 'branch' ( -- )
    lbu t0, 0(ip)
    lb  t1,1(ip)
    sll t1,t1,8
    or t0,t1,t0
    addiu ip,ip,2
    b next
    addu ip, ip, t0  // branchement relatif à ip,

_dict QBRA ,"?BRA"   
// branche si sommet pile est vrai (tos!=0) {-32768..32767}    
qbra: // '?bra' ( t|f -- )
    move t0, T
    _dpop
    beqz t0, next
    addiu ip,ip,2
    lbu t0, -2(ip)
    lb  t1, -1(ip)
    sll t1,t1,8
    or  t0,t0,t1
    b next
    addu ip,ip,t0 // relatif à ip {-32768..32767}

_dict QBRAZ,"0BRA"    
// branche si sommet pile est faux (tos==0)    
qbraz:  // '?braz' ( t|f -- )
    move t0, T
    _dpop
    bne t0, zero, next
    addiu ip,ip,2
    lbu t0, -2(ip)
    lb  t1, -1(ip)
    sll t1,t1,8
    or t0,t0,t1
    b next
    addu ip,ip,t0 // relatif à ip {-32768..32767}

_dict STORE,"!"    
store:  // '!' ( n addr -- )  variable = n
    lw t0, -CELL_SIZE(dp)
    usw t0,0(T)
    lw T,-2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE
    
_dict FETCH,"@"    
// load variable in T
fetch:  // '@' ( addr -- n ) T= *addr
    move t0, T
    ulw T,0(t0)
    b next
    nop

_dict CSTORE,"C!"    
cstore:  // 'c!' ( char addr --  ) *addr = char
    lw t0, -CELL_SIZE(dp)
    sb t0, 0(T)
    lw T, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

_dict CFETCH,"C@"    
cfetch:  // 'c@' ( addr -- char )  T = (char)*addr
    b next
    lbu T, 0(T)

_dict RFETCH, "R@"    
rfetch:   // 'r@'  ( -- n R: n -- n )  empile le sommet de la pile des retours
    _dpush
    b next
    move T, R

_dict TOR,">R"    
tor:  // '>r' ( n -- ) (R: -- n ) envoie T sur pile retour
    _rpush
    move R, T
    _drop_b_next
    
_dict RFROM, "R>"    
rfrom:  // 'r>' ( -- n ) (R: n -- )  envoie R dans T
    _dpush
    move T, R
    lw  R, 0(rp)
    b next
    addiu rp,rp, -CELL_SIZE

_dict DCNT,"DCNT"
dcnt:  // 'dcnt' ( -- n) nombre d'éléments sur la pile de données
    _dpush
    la t0,dstack
    subu t0,dp,t0
    b next
    sra T, t0,2

_dict IPFETCH ,"IP@"   
ipfetch: // 'ip@' ( -- ip ) empile la valeur courante du pointeur d''instructions
    _dpush
    b next
    move T, ip

_dict IPSTORE, "IP!"
ipstore: // 'ip!' ( ip -- ) initialise le pointeur d''insturctions
    move ip, T
    _drop_b_next
 
_dict DP0,"DP0"
// vide la pile des arguments
dp0: // ( x*i -- )
    lui dp,RAM_SEG
    b next
    ori dp,dp,dstack
    
    
_dict DROP  "DROP"  
drop:  // 'drop' ( n -- )
    _drop_b_next
    
_dict TWODROP, "2DROP"    
twodrop: // '2drop' ( n1 n2 -- )
//    lw T, -2*CELL_SIZE(dp)
//    b next
//    addiu dp,dp,-2*CELL_SIZE

_dict DUP,"DUP"    
dup:  // 'dup' ( n -- n n )
    sw T, 0(dp)
    b next
    addiu dp,dp,CELL_SIZE

_dict TWODUP, "2DUP"    
twodup: // (n2 n1 -- n2 n1 n2 n1)
//    addiu dp,dp,2*CELL_SIZE
//    sw T,-2*CELL_SIZE(dp)
//    lw t0, -3*CELL_SIZE(dp)
//    b next
//    sw t0, -CELL_SIZE(dp)

_dict QDUP ,"?DUP"   
qdup:   // ?dup ( n -- n n | n ) duplique n si n!=0
    beq T,zero, next
    nop
    sw T, 0(dp)
    b next
    addiu dp,dp,CELL_SIZE

_dict SWAP ,"SWAP"   
swap: // 'swap' (n2 n1 -- n1 n2 )
    move t0, T
    lw T, -CELL_SIZE(dp)
    b next
    sw t0, -CELL_SIZE(dp)

_dict OVER ,"OVER"   
over:   // 'over' ( n2 n1 -- n2 n1 n2 )
    _dpush
    b next
    lw T, -2*CELL_SIZE(dp)

_dict ROT ,"ROT"  
rot: // ( n3 n2 n1 -- n2 n1 n3)
    move t0, T
    lw T,-2*CELL_SIZE(dp)
    lw t1, -CELL_SIZE(dp)
    sw t0, -CELL_SIZE(dp)
    b next
    sw t1, -2*CELL_SIZE(dp)

_dict NROT,"-ROT"
nrot: // ( n3 n2 n1 -- n1 n3 n2 )
    move v0,T
    lw T,-CELL_SIZE(dp)
    lw v1,-2*CELL_SIZE(dp)
    sw v1, -CELL_SIZE(dp)
    b next
    sw v0,-2*CELL_SIZE(dp)
    
_dict INVERT, "INVERT"    
invert: // ( n -- -n)  
    b next
    subu T,zero,T
    
_dict PLUS ,"+"   
plus:  // '+'   ( n1 n2 -- n1+n2 )
    lw t0,-CELL_SIZE(dp)
    addu T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

_dict PLUS1, "1+"    
oneplus:  // '1+'  (n -- n+1 )
    b next
    addiu T, T, 1

_dict PLUSTORE  "+!"  
plusstore:  // '+!'  ( n addr -- ) *addr  = *addr+n
    ulw t0,0(T)
    lw t1,-CELL_SIZE(dp)
    add t0,t1,t0
    usw t0,0(T)
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)

_dict MINUS,"-" 
minus:   //  '-' ( n1 n2 -- n1-n2 )
    lw t0,-CELL_SIZE(dp)
    subu T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

_dict MINUS1,"1-"    
oneminus:   // '1-' ( n -- n-1 )
    b next
    addiu T, T, -1

_dict STAR,"*"    
star:   // '*' ( n1 n2 -- n1*n2 ) multiplication signé, résultat simple précision.
    lw t0,-CELL_SIZE(dp) 
    mul T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

_dict MDIV,"MDIV"
// multiplication avec résultat conservé sur 64 bits suivit d'une division
// retourne un entier 32 bits    
mslash: // ( n1 n2 n3 -- n4 )  n4=((long long)n1*(long long)n2)/n3    
    lw a0,-2*CELL_SIZE(dp)
    lw a1,-CELL_SIZE(dp)
    mult a0,a1
    mflo a0
    mfhi a1
    sra a3,T,31
    jal __divdi3 // division 64bits/64bits.
    move a2,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    move T,v0
    
    
ustar:   // 'U*' ( u1 u2 -- u3 ) multiplication non signe, résultat simple précision
//    lw t0,-CELL_SIZE(dp)
//    multu  t0, T
//    mflo T
//    b next
//    addiu dp,dp,-CELL_SIZE

mstar:  // 'M*' ( n1 n2 -- d) multiplication signée avec résultat double précision.
//    lw t0, -CELL_SIZE(dp)
//    mult t0,T
//    mflo t0
//    mfhi T
//    b next
//    sw t0, -CELL_SIZE(dp)

umstar: // 'UM*' ( u1 u2 -- ud ) multiplication non signé avec résultat double précision.
//    lw t0, -CELL_SIZE(dp)
//    multu t0,T
//    mflo t0
//    mfhi T
//    b next
//    sw t0, -CELL_SIZE(dp)

_dict SLASH, "/"    
slash:  // '/' ( n1 n2 -- n1/n2 )  division signée
    lw t0, -CELL_SIZE(dp)
    div t0,T
    mflo T
    b next
    addiu dp,dp,-CELL_SIZE

uslash:  // 'U/'  (u1 u2 -- u3)  division non signée
//    lw t0,-CELL_SIZE(dp)
//    divu t0,T
//    mflo T
//    b next
//    addiu dp,dp,-CELL_SIZE

twostar:  # '2*'  ( n  -- 2*n ) multiply by 2 (shift left 1 bit)
//    b next
//    sll T,T,1

twoslash: // '2/'  ( n -- n/2 )  divide by 2 ( arithmetic shift right 1 bit)
//    b next
//    sra T,T,1

divmod:  // '/mod'  ( n1 n2 -- n1%n2 n1/n2 )
//    lw t0, -CELL_SIZE(dp)
//    div t0, T  # n1/n2
//    mfhi t0    #  remainder
//    sw t0, -CELL_SIZE(dp)
//    b next
//    mflo T     # quotient

_dict LSHIFT ,"LSHIFT"   
lshift:  // '<<' ( n1 u -- n1<<u )
    lw t0, -CELL_SIZE(dp)
    sllv T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

_dict RSHIFT ,"RSHIFT"   
rshift:  // '>>'  ( n1 u -- n1>>u )
    lw t0,-CELL_SIZE(dp)
    srlv T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

_dict MOD  ,"MOD"  
modulo:  // 'mod' ( n1 n2 -- n1%n2 )
    lw t0, -CELL_SIZE(dp)
    div t0,T
    mfhi T
    b next
    addiu dp,dp,-CELL_SIZE

_dict MIN  , "MIN"  
min:  // min (n1 n2 -- min )  conserve le plus petit
    lw t0, -CELL_SIZE(dp)
    subu t1,t0,T
    bgtz t1, next
    addiu dp,dp,-CELL_SIZE
    b next
    move T,t0

_dict MAX , "MAX"   
max:  // max (n1 n2 -- max ) conserve le plus grand
    lw t0,-CELL_SIZE(dp)
    subu t1,t0,T
    bltz t1, next
    addiu dp,dp,-CELL_SIZE
    b next
    move T,t0

_dict ABS ,"ABS"    
abs:  // 'abs' ( n -- abs(n) )
    bgez T, next
    nop
    b next
    negu T

_dict BIT_AND, "&"    
bitand: // 'and' ( n1 n2 -- n1&n2)
    lw t0, -CELL_SIZE(dp)
    and T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

_dict BIT_OR , "|"   
bitor:  // 'or' ( n1 n2 -- n1|n2)
    lw t0, -CELL_SIZE(dp)
    or T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

_dict BIT_XOR, "^"    
bitxor: // 'xor' ( n1 n2 -- n1^n2)
    lw t0, -CELL_SIZE(dp)
    xor T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

_dict BIT_NOT, "!"    
bitnot: // 'not' ( n1 -- ~n1 )
    b next
    not T,T

_dict BOOL_NOT,"NOT"   
bool_not: // ( b -- ~b ) négation booléenne
    beq T,zero,next
    addiu T,zero,-1
    b next
    not T,T
    
_dict BOOL_OR,"OR"
bool_or: // ( b1 b2 -- b1||b2 ) ou booléen    
    lw t0,-CELL_SIZE(dp)
    or T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE
    
_dict BOOL_AND,"AND"
bool_and: // ( b1 b2 -- b1&&b2 )  et booléen
    lw t0,-CELL_SIZE(dp)
    beq T,zero, next
    addiu dp,dp,-CELL_SIZE
    bne t0,zero,next
    nop
    b next
    addu T,zero,zero
    
    
    
_dict EQUAL, "="    
equal:  // '='  ( n1 n2 -- t|f ) n1==n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    beq T,t0,next
    li T, -1
    b next
    li T, 0

_dict NOT_EQUAL, "<>"    
nequal: // '<>' ( n1 n2 -- t|f ) n1<>n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    bne t0,T, next
    li T, -1
    b next
    li T, 0

_dict LTZ , "0<"   
ltz:  // '0<'  ( n -- t|f )
    b next
    slt T,T,zero

_dict ZEQUAL , "0="  
zequal: // '0=' ( n -- t|f ) vrai si n == 0 sinon faux
    b next
    sltiu T,1

_dict LESS ,"<"   
less:  //  '<'  ( n1 n2 -- t|f ) n1<n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bltz t0, next
    li T, -1
    b next
    li T, 0

_dict GREATER ,">"   
greater:  // '>'  ( n1 n2 -- t|f ) n1>n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bgtz t0, next
    li T, -1
    b next
    li T, 0

_dict LTEQ ,"<="  
lteq:  // '<=' ( n1 n2 -- t|f ) n1<=n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    blez t0, next
    li T,-1
    b next
    li T, 0

_dict GTEQ ,">="   
gteq:  // '>=' ( n1 n2 -- t|f ) n1>=n2?
    lw t0,-CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bgez t0, next
    li T, -1
    b next
    li T, 0

_dict KEY  ,"KEY"  
key:  // ( -- char ) attend réception caractère de la console
    la a0,con
    jal wait_key
    lbu a0,0(a0)
    _dpush
    b next
    move T, v0  // T=v0

_dict PSTR, "PSTR"
//  imprime une chaîne litérale imbriquée dans le code.    
//  ip pointe vers le compteur de référence
//  pour les chaînes imbriquées le compteur reste à 128.    
prt_str: // IPSTR ( -- )
    _inc_ip
    move a1,ip
    la a0,con
    jal print
    lbu a0,0(a0)
    jal strlen
    move a0,ip
    addu ip,ip,v0
    b next
    _inc_ip

_dict TYPE ,"TYPE"   
type: // ( addr -- ) // imprime la chaîne ascciz
    _console
    jal print
    move a1,T
    jal free_not_ref
    move a0,T
1:  _drop_b_next
 
_dict DOT  ,"DOT"  
dot: // "." ( n -- ) imprime un entier en format libre
    _console
    move a1,T
    jal print_int
    move a2,zero
    _drop_b_next
   
_dict FDOT,"FDOT"
// imprime un nombre en virgule flottante 
fdot: // ( float -- )
    _console
    jal print_float
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
_dict SLEEP ,"SLEEP"   
delay: // 'delay' ( msec -- ) delais en millisecondes
    jal delay_ms
    move a0,T
    _drop_b_next

_dict TICKS  ,"TICKS"  
ticks:  // 'ticks'  ( -- n ) empile le compteur sys_tick
    _dpush
    la w,sys_ticks
    b next
    lw T, 0(w)

_dict FOR_SAVE ,"FOR>R"  
forsave: // ISAVEFOR ( R: -- limit step ) sauvegarde limit et step sur rstack
    _rpush
    move R,limit
    _rpush
    b next
    move R,step

_dict FOR_REST ,"R>FOR"   
forrest: // IFORREST ( R: L S -- ) restitue les valeurs originales de limit et step après NEXT
    move step,R
    _rpop
    move limit,R
    addiu rp,rp,-CELL_SIZE
    b next
    lw R,0(rp)
 
_dict FOR , "FOR"  
for: // IFOR ( limit step -- ) initialise une boucle FOR...NEXT 
    lw limit, -CELL_SIZE(dp) 
    move step,T
    lw T, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-(2*CELL_SIZE)
    
_dict FOR_TEST , "FOR?"  
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
 
_dict FOR_NEXT ,"NEXT"  
fornext: // INEXT ( adr -- ) incrémente la variable for
    lw t0,0(T)
    addu t0,t0,step
    sw t0,0(T)
    _drop_b_next
 
_dict LCSTORE  ,"LC!"  
//sauvegarde variable local
//[IP] indique la position de la variable
local_store: // ( valeur -- )
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    addu t0,frame,t0
    sw T,0(t0)
    _drop_b_next
  
_dict LCFETCH ,"LC@"   
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
   
_dict LCADR  ,"LCADR" 
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

_dict FRAME ,"FRAME"   
// créé un pointeur de référence vers les arguments
// et les variables locales.    
//sauvegarde frame sur R
// [ip] indique le nombre de cellules réservés sur la pile pour les arguments
set_frame:// (R:  -- old_frame )
    _rpush
    move R,frame //préserve la valeur actuelle de frame
//    addiu frame,dp,CELL_SIZE
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    b next
    subu frame,dp,t0
 
_dict LOCAL , "LOCAL"   
// réserve de l'espace sur dstack pour les variables locales    
// [ip] nombre de variables
lc_space: // ( -- ) 
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    b next
    addu dp,dp,t0
    
_dict LEAVE ,"LEAVE"   
// quitte une sous-routine ou fonction
// restore dp, ip et frame
leave: // (R: ip frame -- )
    move dp,frame
    move frame,R
    _rpop
    move ip,R
    _rpop
    b next
    lw T,0(dp)
    
  
_dict CALL ,"CALL"   
// appelle une sous-routine ou fonction
// addr->uint32_t  addresse sous-routine    
sr_call: // (addr --  R:  -- ip )
    _rpush
    move R,ip
    move ip,T
    _drop_b_next

_dict PICK ,"PICK"   
pick: // ( xu...,x1,x0,u -- xu...,x1,x0,xu ) empile une copie du Uième élément de la pile
    addiu w,dp,-CELL_SIZE
    sll  T, T, 2
    subu w,w,T
    b next
    lw T, 0(w)

_dict RANDOMIZE ,"RANDOMIZE"   
// initialize le générateur pseudo-hasard
// en utilisant sys_ticks    
randomize: // ( -- )
    la a0,sys_ticks
    jal srand
    lw a0,0(a0)
    b next
    nop

_dict RND ,"RND"   
// retourne un entier pseudo aléatoire    
random: // ( -- n )
    jal rand
    _dpush
    b next
    move T, v0

_dict TRUNC ,"TRUNC"   
trunc: // ( float -- i)
    move a0, T
    jal rintf
    b next
    move T, v0

//  fait entendre une tonalitée, attend la fin.    
_dict SOUND, "SOUND"
vm_sound: // ( freq,duration -- )
    lw a0,-CELL_SIZE(dp)
    jal __floatunsisf
    nop
    move a0,v0
    jal tone
    move a1,T
    la T,tone_play
1:  lbu v0,0(T)
    bne v0,zero,1b
    nop
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T, 0(dp)

//produit un son de 1000khz d'une durée de 40msec.    
_dict BEEP,"BEEP"
vm_beep: // ( -- )     
    jal beep
    nop
    b next
    nop
    
_dict TUNE ,"TUNE"   
melody: // ( addr -- ) fait entendre la mélodie contenue dans le tableau
    move a0,T
    jal tune
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
 
_dict PLAY,"PLAY"
// joue une mélodie codée dans une chaîne de caractère.
// si f est vrai joue en arrière plan.
vm_play: // ( saddr -- )
    jal play
    move a0,T
    beq v0,zero,1f
    nop
    b abort
    move T, v0
1:  addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
_dict CLS ,"CLS"   
clr_scr: // ( -- ) efface l'écran
    la a0,con
    jal clear_screen
    lbu a0,0(a0)
    b next
    nop
  
_dict UBOUND ,"UBOUND"   
//retourne la grandeur d'une table
// cette valeur précède le premier
// élément de la table    
array_bound: // ( adr -- size )
    b next
    lw T,0(T)
 
_dict TRACE ,"TRACE"   
// active/désactive le mode trace
// imprime l'état de dstack avant exécution de chaque opcode    
trace: // ( f -- )
    la t0,f_trace
    sw T,0(t0)
    _drop_b_next
  
_dict STRADR , "$ADR"   
// retourne l'adresse de la chaîne imbriquée dans le code
// ip pointe le compteur de référence
str_adr: // ( -- addr )    
    _dpush
    _inc_ip
    move T,ip
    lbu t0,0(ip)
    // avance ip après la chaîne
1:  beqz t0,next
    _inc_ip
    b 1b
    lbu t0,0(ip)

_dict STRLEN ,"LEN"   
// retourne la longueur de la chaîne dont l'adresse est sur dstack
    str=a0
    count=a1
    chr=a2
str_len: // ( adr -- length )    
    beq T,zero,next
    nop
    move str,T
    addu count,zero,zero
1:  lbu chr,0(str)
    beqz chr,2f
    nop
    addiu count,count,1
    b 1b
    addiu str,str,1
2:  move a0,T
    move T,count
    jal free_not_ref
    nop
    b next
    nop
    
    
_dict LCASE,"LCASE$"
// converti les caractères de la chaîne en miniscule
// retourne un pointeur sur la nouvelle chaîne.
    dest=v0
    chr=t0
    cmp=t1
    src=a3
    lbound=a0
    ubound=a1
    incr=a2
lcase: // ( s1 -- s2 )
    // vérifie le champ rcount
    addiu t0,T,-1
    lbu t0,0(t0)
    //Si cette chaîne n'est pas
    //référencée conversion in situ
    beqz t0, 1f
    nop
    jal strlen
    move a0,T
    jal string_alloc
    move a0,v0
    move src,T
    b 2f
    move T,v0
1:  move src,T
2:  move dest,T
    addiu lbound,zero,'A'
    addiu ubound,zero,'Z'
    addiu incr,zero,32
case_invert:    
    lbu chr,0(src)
    beqz chr, next
    sb zero,0(dest)
    subu cmp,lbound,chr
    bgtz cmp, 2f
    subu cmp,ubound,chr
    bltz cmp,2f
    nop
    addu chr,chr,incr
2:  sb chr,0(dest)
    addiu dest,dest,1
    b case_invert
    addiu src,src,1
    
_dict UCASE,"UCASE$"
// converti la chaîne en majuscules retourne un pointeur sur la nouvelle chaîne.    
ucase: // ( s1 -- s2 ) 
    lbu t0,-1(T)
    beqz t0,1f
    nop
    jal strlen
    move a0,T
    jal string_alloc
    move a0,v0
    move src,T
    b 2f
    move T,v0
1:  move src,T
2:  move dest,T    
    addiu lbound,zero,'a'
    addiu ubound,zero,'z'
    b case_invert
    addiu incr,zero,-32
   
    
_dict READLN,"READLN"   
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
    
_dict ATOI ,"VAL"  
// convertie la chaîne en entier    
// addr adresse de la châine asciiz
// n valeur entière extraite de la chaîne    
to_int: // addr -- n )
    jal atoi
    move a0,T
    move a0,T
    move T,v0
    jal free_not_ref
    nop
 1: b next
    nop

_dict HEX,"HEX$"
//convertie l'arguement entier en chaîne hexadécimal.
to_hex: // ( n pad  -- s )
    addiu t0,zero,'$'
    sb t0,0(T)
    b str16
    addiu a2,zero,16
   
_dict STR,"STR$"
// convertie un entier en chaîne
// la chaîne est alloué sur le heap   
to_str: // ( n pad -- str* )
    addiu t0,zero, ' '
    sb t0,0(T)
    addiu a2,zero,10  // base
str16:
    lw a1,-CELL_SIZE(dp) // value
    addiu T,T,1
    jal itoa  // buf,val,base
    move a0,T // buffer
    addiu T,T,-1
    jal strlen
    move a0,T
    jal string_alloc
    move a0,v0
    move a0,v0
    jal strcpy
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0

// convertie le premier caractère de la chaîne en entier ASCII    
_dict ASC,"ASC"
to_asc: // ( addr -- n )
    move a0,T
    lbu T,0(a0)
    jal free_not_ref
    nop
1:  b next
    nop


// Retourne une chaîne de longueur 1 correspondant au caractère ASCII de n.
// La chaîne est allouée sur le heap.    
_dict CHR,"CHR$"
to_char: // ( n -- addr )
    jal string_alloc
    addiu a0,zero,1
    sb T,0(v0)
    move T,v0
    b next
    sb zero,1(T)

// retourne la chaîne dans buf "nom_jour AAAA/MM/JJ"
// la mémoire pour buf est allouée sur le heap  
_dict DATESTR,"DATE$"    
datestr: //(  -- buf )
    jal string_alloc
    addiu a0,zero,32
    _dpush
    move T,v0
    jal rtcc_get_date_str
    move a0,v0
    b next
    nop
    
// retourne la chaîne heure dans buf "HH:MM:SS"
// la mémoire est allouée sur le heap.
_dict TIMESTR,"TIME$"    
timestr: //(  -- buf )
    jal string_alloc
    addiu a0,zero,10
    _dpush
    move T,v0
    jal rtcc_get_time_str
    move a0,v0
    b next
    nop
  
// attache la chaîne s2 à la fin de s1
// retourne l'adresse de la chaîne résultante.    
_dict APPEND,"APPEND$"
append: // ( s1 s2 -- s3 )
    bne T,zero, 1f
    nop
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp) // s2 NULL retourn s1
1:  lw a0,-CELL_SIZE(dp)
    bne a0,zero,1f 
    nop
    b next   // s1 NULL retourne s2
    addiu dp,dp,-CELL_SIZE
1:  jal strlen // longueur de s1
    lw a0,-CELL_SIZE(dp)
    _dpush
    move T,v0 // ( s1 s2 len_s1 -- 
    jal strlen // longueur de s2
    lw a0,-CELL_SIZE(dp)
    addu T,T,v0
    jal string_alloc
    move a0,T 
    move T, v0  // ( s1 s2 s3 --
    lw a1,-2*CELL_SIZE(dp)
    jal strcpy
    move a0,T
    lw a1,-CELL_SIZE(dp)
    jal strcat
    move a0,T
    // libère s1 et s2 si elles ne sont pas référencées
    jal free_not_ref
    lw a0,-2*CELL_SIZE(dp)
    jal free_not_ref
    lw a0,-CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

// retourne une chaîne contenant les n premiers caractères de s1
// si n==0 retourne NULL
_dict LEFT, "LEFT$"
    dest=v0
    byte=a0
    count=a1
    src=a2
left:// ( s1 n -- s2 )  
    lw a0,-CELL_SIZE(dp)
    bne a0,zero,0f
    nop
    addiu dp,dp,-CELL_SIZE // s1==NULL
    b next
    lw T,0(dp)
0:  bgtz T, 2f
    nop
    // n==0 retourne chaîne vide.
    //libère s1 si non référencé.
0:  jal free_not_ref
    lw a0,-CELL_SIZE(dp)
    b next
    addiu dp,dp,-CELL_SIZE
2:  jal string_alloc
    move a0,T
    move count,T
    move T,v0
    lw src,-CELL_SIZE(dp)
2:  beqz count, 3f
    lbu byte,0(src)
    beqz byte, 3f
    sb  byte,0(dest)
    addiu src,src,1
    addiu dest,dest,1
    b 2b
    addiu count,count,-1
3:  b 0b
    sb zero,0(dest)  
    
    
// insère la chaîne s2 dans la chaîne s1 à partir de la position n    
_dict INSERT,"INSERT$"
    dest=a0
    src=a1
    count=a2
insert: // ( s1 s2 n -- s3 )
    lw a0,-CELL_SIZE(dp)
    bne a0,zero,0f
    nop
    addiu dp,dp,-2*CELL_SIZE // s2==NULL
    b next
    lw T,0(dp)
0:  lw a0,-2*CELL_SIZE(dp)
    bne a0,zero,0f
    nop
    lw T,-CELL_SIZE(dp)  // s1==NULL
    b next
    addiu dp,dp,-2*CELL_SIZE
0:  addiu T,T,-1
    bgtz T,1f
    nop
    addiu dp,dp,-CELL_SIZE
    b prepend
    lw T,0(dp)
1:  jal strlen // len(s1)?
    lw a0,-2*CELL_SIZE(dp)
    _dpush
    move T,v0  // ( s1 s2 n len(s1) --
    // si n>len(s1) alors append(s1,s2)
    lw a0,-CELL_SIZE(dp)
    subu a0,a0,T // n-len(s1)
    bltz a0, 1f
    nop
    addiu dp,dp,-2*CELL_SIZE
    b append
    lw T,0(dp)
1:  // strlen(s2)  
    jal strlen 
    lw a0,-2*CELL_SIZE(dp)
    // string_alloc(len(s1)+len(s2))
    jal string_alloc
    addu a0,T,v0 
    move T, v0 //  s1 s2 n s3
    // strncpy(s3,s1,n)
    lw src,-3*CELL_SIZE(dp)
    lw count,-CELL_SIZE(dp)
    move dest,T
1:  beqz count,2f
    lbu v0,0(src)
    sb  v0,0(dest)
    addiu src,src,1
    addiu dest,dest,1
    b 1b
    addiu count,count,-1
2:  sb zero,0(dest)
    sw src,-3*CELL_SIZE(dp)
    lw src,-2*CELL_SIZE(dp)
3:  lbu v0,0(src)     
    beqz v0,4f
    sb v0,0(dest)
    addiu src,src,1
    b 3b
    addiu dest,dest,1
4:  
    lw src,-3*CELL_SIZE(dp)
5:  lbu v0,0(src)
    beqz v0, 6f
    sb v0,0(dest)
    addiu src,src,1
    b 5b
    addiu dest,dest,1
6:  // libère s1 et s2 si elles ne sont pas référencées.
    jal free_not_ref
    lw a0,-3*CELL_SIZE(dp)
    jal free_not_ref
    lw a0, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-3*CELL_SIZE
    

// extrait n2 caractères de la chaîne s1 à partir de la position n1    
_dict MID,"MID$"
    dest=v0
    src=t0
    from=t1
    count=t2
mid: // ( s1 n1 n2 -- s2 )
    lw a0,-2*CELL_SIZE(dp)
    bne a0,zero,0f
    nop
    addiu dp,dp,-2*CELL_SIZE
    b next // s1==NULL
    lw T,0(dp)
0:  bgtz T,2f
    nop
    //n2==0
0:  jal free_not_ref
    lw a0,-2*CELL_SIZE(dp)
1:  b next
    addiu dp,dp,-2*CELL_SIZE
2:  
    jal string_alloc
    move a0,T
    move count,T
    move T,v0 // ( s1 n1 s2 --
    lw from,-CELL_SIZE(dp)
    blez from,0b
    lw src,-2*CELL_SIZE(dp)
    addiu from,from,-1
    addu src,src,from
1:  beqz count,3f    
    lbu a0,0(src)
    beqz a0,0b
    sb a0,0(dest)
    addiu src,src,1
    addiu dest,dest,1
    b 1b
    addiu count,count,-1
3:  sb zero,0(dest)    
4:  b 0b
    nop
    
// attache s2 au début de s1 retourne l'adresse de la chaîne résultante    
_dict PREPEND,"PREPEND$"
prepend: // ( s1 s2 -- s3 )
    lw a0,-CELL_SIZE(dp)
    sw T,-CELL_SIZE(dp)
    b append
    move T,a0

// retourne n derniers caractères de s1    
_dict RIGHT,"RIGHT$"
right: // ( s1 n -- s2 )
    lw a0,-CELL_SIZE(dp)
    bne a0,zero,1f
    nop
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
1:  bgtz T,2f
    nop
    jal free_not_ref
    lw a0,-CELL_SIZE(dp)
    addu T,zero,zero
    b next
    addiu dp,dp,-CELL_SIZE
2:  jal strlen
    lw a0,-CELL_SIZE(dp)
    _dpush
    subu v0,v0,T
    addiu v0,v0,1
    b mid
    sw v0,-CELL_SIZE(dp)
    

// substitus les caractères de s1 par ceux de s2 à partir de la position n
// arrête la substition au premier caractère null rencontré soit dans s1 ou s2  
_dict SUBST,"SUBST$"
    dest=v0
    src=a0
    pos=a1
subst: // ( s1 s2 n -- s3 )  
    // alloue l'espace pour s3
    jal strlen
    lw a0,-2*CELL_SIZE(dp)
    _dpush 
    move T,v0 // ( s1 s2 n len --
    // si n<1 ou  n>strlen(s1) abandonnne.
    lw t0,-CELL_SIZE(dp)
    bgtz t0,1f
    nop
    // n<=0 abandonne retourne s1
0:  jal free_not_ref  // libère s2 si non référencé.
    lw a0,-2*CELL_SIZE(dp)
    lw T, -3*CELL_SIZE(dp)
    b next
    addiu dp,dp,-3*CELL_SIZE
1:  // si n>len abandonne et retourne s1
    sub t1,t0,T
    bgtz t1,0b
    addiu t0,t0,-1
    sw t0,-CELL_SIZE(dp)
    jal string_alloc
    move a0,T
    move T,v0 // ( s1 s2 n s3 -- 
    lw a1,-3*CELL_SIZE(dp)
    // copie s1 dans s3
    jal strcpy
    move a0,v0
    // libère s1 si par référencé
    jal free_not_ref
    lw a0,-3*CELL_SIZE(dp)
    lw t0,-CELL_SIZE(dp)
    addu dest,T,t0 
    lw src,-2*CELL_SIZE(dp)
4:  lbu t0,0(src)
    beqz t0, 6f
    lbu t1,0(dest)
    beqz t1, 6f
    sb t0,0(dest)
    addiu src,src,1
    b 4b
    addiu dest,dest,1
6:  // libère s2 si non référencé.
    jal free_not_ref
    lw a0,-2*CELL_SIZE(dp) // s2
    b next
    addiu dp,dp,-3*CELL_SIZE

// retourne la position de s2 dans s1 ou 0 si non trouvé
// la recherche se fait à partir de n0    
_dict INSTR,"INSTR"
instr: // ( n0 s1 s2 -- n )
    // si s1 est NULL retourne NULL
    lw a0,-CELL_SIZE(dp)
    bne a0,zero,1f
    nop
    move a0,T // a0=s2
    b 4f
    addu T,zero,zero // T=0
1:  // si s2 est NULL retourne 0
    bne T,zero,1f
    nop
    // libère s1 si non référencé.
    // s1 déjà dans a0
    jal free_not_ref
    addiu dp,dp,-2*CELL_SIZE
    b next
    addu T,zero,zero
1:  jal strlen
    lw a0,-CELL_SIZE(dp)
   // si n0>len(s1) retourne 0
    lw a0,-2*CELL_SIZE(dp) // a0=n0
    subu t0,v0,a0  // len(s1)-n0
    bgez t0,3f
    nop
    move a0,T // a0=s2
    b 4f  // sortie p.c.q. n0>len(s1)
    addu T,zero,zero // T=0
3:  addiu a0,a0,-1 // n0-1
    lw a1,-CELL_SIZE(dp) // search$
    addu a0,a0,a1
    jal strstr
    move a1,T   // find$
    move a0,T   // a0=s2
    move T,v0   // T=strstr(s1,s2)
    beqz T,4f
    lw t0,-CELL_SIZE(dp) // t0=s1
    subu T,T,t0  // T=target*-s1
    addiu T,T,1  // T= position de s2 dans s1
4:  // a0 contient déjà s2
    // T contient pos ou 0
    jal free_not_ref // libère s2 si non référencée
    nop
    jal free_not_ref
    lw a0,-CELL_SIZE(dp) // libère s1 si non référencée
    b next
    addiu dp,dp,-2*CELL_SIZE
    
    
_dict ALLOC, "VAR_ALLOC"    
// alloue de la mémoire dans l'espace progamme.
// n taille en octets
// addr adresse du bloc mémoire alloué ou 0 si manque de mémoire    
alloc: // ( n -- addr )
    jal alloc_var_space
    move a0,T
    b next
    move T,v0
 
_dict STRALLOC ,"STR_ALLOC"   
//alloue de l'espace sur le heap pour une chaîne asciiz
// n longueur de la chaîne en octets    
str_alloc: // ( n -- addr )    
    jal string_alloc
    move a0,T
    j next
    move T,v0
    
_dict STRFREE , "STR_FREE"   
// libère une chaîne préalablement allouée sur le heap.
str_free: // ( addr -- )
    jal string_free
    move a0,T
    _drop_b_next
  
_dict FREENOTREF,"FREENOTREF"
// libère la chaîne si elle n'est pas référencée.
freenotref: // ( addr -- )
    jal free_not_ref
    move a0,T
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
    
    
_dict STRCPY , "STRCPY$"   
// copie une chaîne asciiz de src vers dest
// src adresse chaîne à copier
// dest adresse destination    
str_cpy: // ( src dest -- dest )
    lw a1,-CELL_SIZE(dp)
    jal strcpy
    move a0,T
    b next
    addiu dp,dp,-CELL_SIZE

_dict STRSTORE,"STR!"
// store une chaîne dans une variable
// incrémente le compteur de référence
str_store: // ( str var -- )
    //incrémente le compteur de référence
    lw t0,-CELL_SIZE(dp)
    beq t0,zero,1f
    nop
    lbu t1,-1(t0)
    addiu t2,zero,255
    bne t2,t1,1f   // n'incrémente pas si ref_count==255 (chaîne imbriquée).
    nop
    jal string_alloc
    lbu a0,0(t0)
    move a0,v0
    jal strcpy
    lw a1,-CELL_SIZE(dp)
    move t0,v0
    lbu t1,-1(t0)
1:  addiu t1,t1,1
    sb t1,-1(t0)
    // enregistre le pointeur de chaîne dans la variable.
    sw t0,0(T)
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)
    
_dict STRCMP,"STRCMP"
vm_strcmp: // ( s1 s2 -- -1|0|1)
    lw a0,-CELL_SIZE(dp)
    jal strcmp
    move a1,T
    _dpush
    move T,v0
    jal free_not_ref
    lw a0,-3*CELL_SIZE(dp)
    jal free_not_ref
    lw a0,-2*CELL_SIZE(dp)
1:  b next
    addiu dp,dp,-2*CELL_SIZE
    
_dict CURLINE , "CURLINE"   
// retourne la ligne du curseur texte {0..29}    
curline: // ( -- line )
    _console
    jal vga_get_curpos
    _dpush
    b next
    srl T,v0,16
 
_dict CURCOL, "CURCOL"    
// retourne la colonne du curseur texte {0..79}    
curcolon: // ( -- colon )
    _console
    jal get_curpos
    _dpush
    b next
    andi T,v0,0xff
  
_dict SETCURSOR, "SETCURSOR"    
// positionne le curseur texte    
setcursor: // ( line, col -- )
    _console
    lw a2, -CELL_SIZE(dp)
    addiu dp,dp,-CELL_SIZE
    jal set_curpos
    move a1,T
    _drop_b_next
 
_dict EXTBIT , "BTEST"   
// extrait le bit n2 de l'entier n1    
extbit: // ( n1 n2 -- n3 ) 
    lw t0,-CELL_SIZE(dp)
    srlv T,t0,T
    andi T,T,1
    b next
    addiu dp,dp,-CELL_SIZE

_dict SETTIMER, "SETTIMER"    
// initialise la minuterie système.    
// n durée en millisecondes    
settimer: // ( n -- )
    jal set_timer
    move a0,T
    _drop_b_next
    
_dict TIMEOUT, "TIMEOUT"   
// retourne vrai si la minuterie est expirée.    
qtimeout: // ( -- t|f)
    jal timeout
    sw T,0(dp)
    move T,v0
    b next
    addiu dp,dp,CELL_SIZE
    
_dict INV_VIDEO, "INV_VIDEO"    
// contrôle sortie vidéeo N/B ou B/N
// f -> blanc/noir
// t -> noir/blanc    
inv_vid: // ( f|t -- )
    _console
    jal invert_video
    move a1,T
    _drop_b_next

_dict SCRLDN, "SCRLDN"    
//glisse l'écran texte d'une ligne vers le bas
scrldn: // ( -- )
    _console
    jal scroll_down
    nop
    b next
    nop
    
_dict SCRLUP , "SCRLUP"   
//glisse l'écran texte d'une ligne vers le haut    
scrlup: // ( -- )
    _console
    jal scroll_up
    nop
    b next
    nop
   
_dict INSRTLN , "INSRTLN" 
//insère une ligne vide à la position du curseur    
insertln: // ( -- )
    _console
    jal insert_line
    nop
    b next
    nop
   
_dict GETPIXEL ,"GETPIXEL"   
//retourne l'état d'un pixel de la mémoire vidéo    
get_pixel: // ( x y -- 1|0 )
    lw a0, -CELL_SIZE(dp)
    jal getPixel
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T, v0
    
_dict PUTPIXEL ,"PUTPIXEL"   
// met le pixel à 1.    
put_pixel: // ( x y p -- )
    lw a0, -2*CELL_SIZE(dp)
    lw a1, -CELL_SIZE(dp)
    jal putPixel
    move a2,T
    lw T,-3*CELL_SIZE(dp)
    b next
    addiu dp,dp, -3*CELL_SIZE
 
_dict XORPIXEL ,"XORPIXEL"   
// met le pixel à 0.    
xor_pixel: // ( x y -- )
    lw a0, -CELL_SIZE(dp)
    jal xorPixel
    move a1,T
    lw T, -2*CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE
 
_dict LINE ,"LINE"   
// dessine une ligne droite.    
vm_line: // (x0,y0,x1,y1 -- )
    lw a0,-3*CELL_SIZE(dp)
    lw a1,-2*CELL_SIZE(dp)
    lw a2,-CELL_SIZE(dp)
    jal line
    move a3,T
    addiu dp,dp,-4*CELL_SIZE
    b next
    lw T,0(dp)
 
_dict RECT, "RECT"    
// dessine un rectangle    
vm_rectangle: // ( x0,y0,x1,y1 -- )
    lw a0,-3*CELL_SIZE(dp)
    lw a1,-2*CELL_SIZE(dp)
    lw a2,-CELL_SIZE(dp)
    jal rectangle
    move a3,T
    addiu dp,dp,-4*CELL_SIZE
    b next
    lw T,0(dp)
 
_dict BOX ,"BOX"   
// dessine une boite.    
vm_box:  // ( x0,y0,w,h -- )
    lw a0,-3*CELL_SIZE(dp)
    lw a1,-2*CELL_SIZE(dp)
    lw a2,-CELL_SIZE(dp)
    jal box
    move a3,T
    addiu dp,dp,-4*CELL_SIZE
    b next
    lw T,0(dp)
  
_dict CIRCLE ,"CIRCLE"   
// dessine un circle    
vm_circle: // ( x,y,r -- )
    lw a0,-2*CELL_SIZE(dp)
    lw a1, -CELL_SIZE(dp)
    jal circle
    move a2,T
    addiu dp,dp,-3*CELL_SIZE
    b next
    lw T,0(dp)
 
_dict ELLIPSE ,"ELLIPSE"   
// dessine une ellipse    
vm_ellipse:
    lw a0,-3*CELL_SIZE(dp)
    lw a1,-2*CELL_SIZE(dp)
    lw a2,-CELL_SIZE(dp)
    jal ellipse
    move a3,T
    addiu dp,dp,-4*CELL_SIZE
    b next
    lw T,0(dp)
 
_dict POLYGON, "POLYGON"
// dessine un polygon    
vm_polygon: // ( addr n -- )
    lw a0,-CELL_SIZE(dp)
    jal polygon
    move a1,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)
 
_dict FILL,"FILL"
// remplissage d'une figure géométrique fermée.
// x,y point de départ du remplissage.
vm_fill: // ( x y -- )    
    lw a0,-CELL_SIZE(dp)
    jal fill
    move a1,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)
    
    
_dict SPRITE  ,"SPRITE"  
// applique un bitmap à l'écran VGA en utilisant la fonction xor_pixel 
// un sprite a un maximum de 32 pixels en largeur et est contenu
// dans un tableau d'entiers.    
put_sprite: // ( x y w h addr_sprite -- )   
    addiu sp,sp,-20
    sw T, 16(sp)
    lw a0,-4*CELL_SIZE(dp)
    lw a1,-3*CELL_SIZE(dp)
    lw a2,-2*CELL_SIZE(dp)
    jal sprite
    lw a3,-CELL_SIZE(dp)
    addiu sp,sp,20
    addiu dp,dp,-5*CELL_SIZE
    b next
    lw T, 0(dp)
    
    
_dict VGACLS,"VGACLS"
//efface l'écran VGA indépendemment de la console sélectionnée
vgacls: // ( -- )
    jal vga_clear_screen
    nop
    b next
    nop
    
    
//****************************
// fonction SPI RAM
//****************************
  
_dict SRCLEAR , "SRCLEAR"   
//met à zéro un bloc de mémoire SPI RAM    
sram_clear: // ( addr size -- )
    lw a0,-CELL_SIZE(dp)
    jal sram_clear_block
    move a1,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)
 
_dict SRWRITE ,"SRWRITE"   
//copie le contenu d'une variable dans la mémoire SPI RAM    
sram_write: // ( addr_ram addr_var  size -- )
    lw a0,-2*CELL_SIZE(dp)
    lw a1,-CELL_SIZE(dp)
    jal sram_write_block
    move a2,T
    addiu dp,dp,-3*CELL_SIZE
    b next
    lw T,0(dp)
    
_dict SRREAD, "SRREAD"    
//copie le contenue de la mémoire SPI RAM dans une variable    
sram_read: // ( addr_ram addr_var size -- )
    lw a0,-2*CELL_SIZE(dp)
    lw a1,-CELL_SIZE(dp)
    jal sram_read_block
    move a2,T
    addiu dp,dp,-3*CELL_SIZE
    b next
    lw T,0(dp)
    
_dict SAVSCR, "SAVSCR"
// sauvegarde la totalité du tampon vidéo dans la mémoire SPI RAM
save_screen: // ( addr -- )
    jal saveScreen
    move a0,T
    _drop_b_next
 
_dict RSTSCR , "RESTSCR"   
// restaure le contenu du tampon vidéo à partir de la mémoire SPI RAM
rest_screen: // ( addr -- )
    jal restoreScreen
    move a0,T
    _drop_b_next
    
_dict SRLOAD , "SRLOAD"   
// charge un fichier dans la mémoire SPI RAM    
vm_sram_load: // ( addr_ram char* -- size )
    lw a0,-CELL_SIZE(dp)
    jal sram_load
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0

_dict SRSAV, "SRSAVE"    
// sauvegarde un bloc de la mémoire SPI RAM dans un fichier    
vm_sram_save: // ( addr_ram char* size -- exit_code )
    lw a0,-2*CELL_SIZE(dp)
    lw a1,-CELL_SIZE(dp)
    jal sram_save
    move a2,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    move T,v0

 
_dict FCLOSE, "CLOSE"    
// ferme un fichier ouvert
file_close: // ( n -- )    
    jal basic_fclose
    move a0,T
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
_dict FCLOSEALL,"CLOSEALL"
// ferme tous les fichiers ouverts.
file_close_all: // ( -- )
    jal close_all_files
    nop
    b next
    nop
    
    
    
_dict FEOF, "EOF"
// vérifie si en fin de fichier.
file_eof: // ( n -- )
    jal basic_feof
    move a2,T
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
    
_dict FOPEN,"OPEN"
// ouvre un fichier
// Le programme avorte si le fichier ne peut-être ouvert    
file_open: // ( name$ mode number -- )
    lw a0,-2*CELL_SIZE(dp)
    lw a1,-CELL_SIZE(dp)
    jal basic_fopen
    move a2,T
    // libère name$ si non référencé.
    lw a0,-2*CELL_SIZE(dp)
    lbu a1,-1(a0)
    bne a1,zero, 1f
    nop
    jal string_free
    nop
1:  addiu dp,dp,-3*CELL_SIZE
    b next
    lw T,0(dp)
    
    
_dict FEXIST "EXIST"
// vérifie si un fichier de ce nom existe.
file_exist: // ( name$ -- t|f )
    jal basic_fexist
    move a0,T
    b next
    move T,v0
    
_dict SEEK,"SEEK"
// positionne le curseur du fichier
file_seek: // ( n pos -- )
    lw a0,-CELL_SIZE(dp)
    jal basic_fseek
    move a1,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)

_dict FGETC,"FGETC"
// lecture d'un caractère dans le fichier n    
file_getc: // ( n -- c)
    jal basic_fgetc
    move a0,T
    b next
    move T,v0
    
//_dict FGETS,"FGETS"
// lecture une ligne texte dans le fichier n.    
//file_gets: // ( n -- s )
//    jal basic_fgets
//    move a0,T
//    move T,v0
//    jal strlen
//    move a0,T
//    jal string_alloc
//    move a0,v0
//    move a0,v0
//    jal strcpy
//    move a1,T
//    b next
//    nop
    
    
_dict FPUTC,"FPUTC"
// écris un caractère dans le fichier n
file_putc: // ( n c -- )
    lw a0,-CELL_SIZE(dp)
    jal basic_fputc
    move a1,T
    addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)
    
_dict WRITE_FIELD,"WRITE_FIELD"
// écris une chaîne dans le fichier
write_field: // ( n s -- )
    lw a0,-CELL_SIZE(dp)
    jal basic_write_field
    move a1,T
    lbu t0,-1(T)
    bne t0,zero,1f
    nop
    jal string_free
    move a0,T
1:  addiu dp,dp,-2*CELL_SIZE
    b next
    lw T,0(dp)
    
_dict READ_FIELD,"READ_FIELD"
// lecture d'un champ à partir d'un fichier CSV
read_field: // ( n -- s )
    jal file_read_field
    move a0,T
    b next
    move T,v0
  
/**************************/
/* math virgule flottante */
/**************************/
    
_dict FTOSTR,"FTOSTR$"
// conversion float vers chaîne    
ftostr: //  ( f -- s )
    jal float_to_str
    move a0,T
    b next
    move T,v0
    
_dict STRTOF,"STRTOF"
// converstion chaîne vers float
vm_strtof: // ( s -- f )
    addu a1,zero,zero
    jal strtof
    move a0,T
    move a0,T
    move T,v0
    lbu t0,-1(a0)
    bne t0,zero, next
    nop
    jal string_free
    nop
    b next
    nop
    
 
_dict NEGFLOAT,"-FLOAT"
// négation dun float
negfloat: // ( fpt -- fpt)
    lui t0,0x8000
    b next
    xor T,T,t0
    
  
_dict ITOFLOAT,"INT>FLOAT"
// convertie un INT32 vers FLOAT32    
int_to_float: // ( float -- int )
    jal sitofp
    move a0,T
    b next
    move T,v0
    
_dict FPTOINT,"FP>INT"
// convertie un FLOAT32 vers un INT32    
float_to_int: // ( float -- int )
    jal __fixsfsi
    move a0,T
    b next
    move T,v0

_dict FPLUS,"F+"
// addition de 2 float
fplus: // ( float float -- float )
    lw a0, -CELL_SIZE(dp)
    jal fpadd
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0
    
_dict FMINUS, "F-"
// soustraction de 2 float
fminus: // ( float1 float2 -- float1-float2 )
    lw a0,-CELL_SIZE(dp)
    jal __subsf3
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0
    
_dict FMUL,"F*"
// multiplication de 2 flottants    
fmul: // ( float1 float2 -- float1*float2 )
    lw a0,-CELL_SIZE(dp)
    jal __mulsf3
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0

_dict FDIV, "F/"    
// division de 2 flottant
fdiv: // ( float1 float2 -- float1/float2 ) 
    lw a0,-CELL_SIZE(dp)
    jal __divsf3
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0

/*********************************/
/*  fonctions trigonométriques   */
/*********************************/
// Les angles sont en radians.
_dict SINE,"SINUS"
sine: // ( float -- float )
    jal sinf
    move a0,T
    b next
    move T,v0

_dict COSINUS,"COS"    
cos: // ( float -- float )
    jal cosf
    move a0,T
    b next
    move T,v0

_dict TANGEANT, "TAN"
tan: // ( float -- float )
    jal tanf
    move a0,T
    b next
    move T,v0
    
_dict ATAN,"ATAN"    
atan: // ( float -- float )
    jal atanf
    move a0,T
    b next
    move T,v0
  
_dict ACOS,"ACOS"    
acos: // ( float -- float )
    jal acosf
    move a0,T
    b next
    move T,v0

_dict ASINE,"ASINE"    
asine: // ( float -- float )
    jal asinf
    move a0,T
    b next
    move T,v0
    
_dict FSQRT,"SQRT"    
fsqrt: // ( float -- float )
    jal sqrtf
    move a0,T
    b next
    move T,v0

_dict EXP,"EXP"
// base e^f    
fexp: // ( float -- float )
    jal expf
    move a0,T
    b next
    move T,v0
    
_dict POWER,"POWER"    
// puissance fb^fn    
fpower: // ( fb fn -- float )
    lw a0,-CELL_SIZE(dp)
    jal powf
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0
    
_dict LOGN,"LOGN" 
// logarithme base e    
flog: // ( float -- float )
    jal logf
    move a0,T
    b next
    move T,v0
    
_dict LOG10,"LOG10"
// logarithme base 10    
flog10: // ( float -- float )
    jal log10f
    move a0,T
    b next
    move T,v0
    
_dict FABS,"FABS"    
// valeur absolue d'un float
vm_fabs: // ( float -- float )   
    lui t0,0x7fff
    ori t0,t0,0xffff
    b next
    and T,T,t0

_dict FLOOR,"FLOOR"
// réduit la vers zéro    
floor: // ( float -- float )
    jal floorf
    move a0,T
    b next
    move T, v0
    
_dict CEIL,"CEIL"    
fceil: // ( float -- float )
    jal ceilf
    move a0,T
    b next
    move T,v0
    
_dict FMOD,"FMOD"
// opération modulo sur nombre float
fmod: // ( float1 float2 -- float )    
    lw a0,-CELL_SIZE(dp)
    jal fmodf
    move a1,T
    addiu dp,dp,-CELL_SIZE
    b next
    move T,v0

_dict CON,"CON"
// sélectionne la console SERIAL|LOCAL
select_console: //( n -- )
    la a0, con
    sb T,0(a0)
    addiu dp,dp,-CELL_SIZE
    b next
    lw T,0(dp)
    
_dict ENV,"ENV"
// obtient la chaîne valeur d'une variable d'environnement.
get_env: // ( char* -- char*|NULL )    
    jal var_value
    move a0,T
    // préserve *nom_var
    move t0,T
    // push *valeur_var
    move T,v0
    // libère *nom_var
    jal free_not_ref
    move a0,t0
    beq zero, T, next
    nop
    jal strlen
    move a0,T
    jal string_alloc
    move a0,v0
    move a1,T
    jal strcpy
    move a0,v0
    b next
    move T,v0
    
.end StackVM
    
//*************************************************
depth: // retourne le nombre d'éléments sur dstack
    la v0,dstack0
    lw v0,0(v0)
    subu v0,dp,v0
    jr ra    
    sra v0,v0,2
    
  
rdepth: // retourne le nombre d'élément sur rstack
    la v0,rstack0
    lw v0,0(v0)
    subu v0,rp,v0
    jr ra
    sra v0,v0,2
    

print_state:    
    addiu sp,sp,-16
    sw s5,0(sp)
    sw s6,4(sp)
    sw s7, 8(sp)
    sw ra,12(sp)
    move s5,t1
    la s7,con
    lbu s7,0(s7)
    // imprime "trace: "
    la a1,trace_msg
    jal print
    move a0,s7
    // imprime nombre d'éléments sur R
    la a1,r_msg
    jal print
    move a0,s7
    jal rdepth
    nop
    move a1,v0
    move a2,zero
    jal print_int
    move a0,s7
    addiu a1,zero,')'
    jal put_char
    move a0,s7
    // imprime nombre d'éléments sur dstack
    la a1,s_msg
    jal print
    move a0,s7
    jal depth
    nop
    move a0,s7
    move a1,v0
    move s6,a1
    jal print_int
    move a2,zero
    // imprime ")"
    addiu a1,zero,')'
    jal put_char
    move a0,s7
    // si dstack vide va à 3f
    beqz s6,3f
    nop
    la s6,dstack0
    lw s6,0(s6)
    addiu s6,s6,CELL_SIZE
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
    //imprime ip
3:  la a1,ip_msg
    jal print
    move a0,s7
    move a1,ip
    move a2,zero
    jal print_hex
    move a0,s7
    //imprime dp
    la a1, dp_msg
    jal print
    move a0, s7
    move a1,dp
    move a2,zero
    jal print_hex
    move a0,s7
    // imprime "next opcode: "
    la a1,opcode_msg
    jal print
    move a0,s7
    // imprime nom opcode
    lw a1,-4(s5)
    jal print
    move a0,s7
    jal crlf
    move a0,s7
    la a0,f_trace
    lw a0,0(a0)
    andi a0,a0,2
    beqz a0,2f
    nop
    // wait key
    la a1,step_prompt
    jal println
    move a0,s7
    jal wait_key
    move a0,s7
    addiu a0,v0,-'q'
    beqz a0,1f
    nop
    addiu a0,v0,-'Q'
    beqz a0,1f
    nop
    b 2f
    nop
    // désactive trace
1:  la a0,f_trace
    sw zero,0(a0) 
2:  move t1,s5
    lw s5,0(sp)
    lw s6,4(sp)
    lw s7,8(sp)
    lw ra,12(sp)
    jr ra
    addiu sp,sp,16
    
  
    
// routines names must be in same order as OP_CODES enum in vm.h    
opcodes_table:
.word bye,abort,lit,wlit,clit,bra,qbra,store,fetch,cstore
.word cfetch,drop,dup,swap,over,pick,invert
.word plus,minus,star,slash,modulo,ltz
.word zequal,bitand,bitor,bitxor,bitnot,bool_not,bool_or,bool_and
.word ticks, delay
.word qdup,oneplus, oneminus,plusstore, rot,nrot
.word  min, max, abs, lshift, rshift, qbraz, dcnt
.word equal,nequal, less, greater,lteq,gteq,forsave,forrest,for,fortest,fornext
.word randomize,random
.word key,qrx,emit,prt_str,cr,dot,type,vm_spaces
.word vm_sound,vm_beep,melody,vm_play,clr_scr,array_bound    
.word local_store,local_fetch,local_addr,set_frame,sr_call,leave,lc_space
.word trace,str_adr,str_len,readln,to_int,alloc,str_alloc,str_free,freenotref,str_cpy
.word curline,curcolon,setcursor
.word extbit,settimer,qtimeout,inv_vid,scrldn,scrlup,insertln    
.word get_pixel,put_pixel,xor_pixel,vm_line,vm_rectangle,vm_box,vm_circle,vm_ellipse
.word vm_polygon,vm_fill,put_sprite,vgacls    
.word sram_clear,sram_read,sram_write,save_screen,rest_screen,vm_sram_load,vm_sram_save
.word mslash,to_str,to_asc,to_char,datestr,timestr
.word append,insert,left,mid,prepend,right,subst,instr,vm_strcmp    
.word ucase,lcase,str_store
.word to_hex
.word dp0
.word file_close,file_close_all,file_eof,file_open, file_exist, file_seek
.word file_getc,file_putc,write_field,read_field
.word ftostr, vm_strtof, negfloat,float_to_int,int_to_float
.word fplus,fminus,fmul,fdiv
.word sine, cos, tan, atan, acos, asine,fsqrt,fexp,fpower,flog,flog10
.word vm_fabs,floor,fceil,fmod,fdot
.word select_console,get_env
    
bad_op: 
.word rfetch,tor,rfrom    
.word ustar,mstar,umstar,uslash, twostar, twoslash, twodrop, twodup,divmod

    
//badop_msg: .asciz "bad operating code: "    
trace_msg: .asciz "\rtrace: "
opcode_msg: .asciz "next opcode: "
s_msg: .asciz " S("
dp_msg: .asciz "dp: "
ip_msg: .asciz "\rip: "
r_msg: .asciz "R("
step_prompt: .asciz "(Q) leave trace, other step\r"    
//float_fmt: .asciz "%G"
    
#undef _ASM_CODE_