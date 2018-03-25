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
//math 
.extern print_float
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

.macro _tpush
addiu dp,dp, CELL_SIZE
sw T, 0(dp)
.endm

.macro _tpop
lw T, 0(dp)
addiu dp,dp,-CELL_SIZE
.endm

.macro _rpush
addiu rp,rp,CELL_SIZE
sw R, 0(rp)
.endm

.macro _rpop
lw R, 0(rp)
addiu rp,rp,-CELL_SIZE
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

    
.bss
.global dstack,rstack,rsp,dsp,endmark,dptr
// piles
dstack: .space DSTACK_SIZE
rstack: .space RSTACK_SIZE
dsp: .space 4
rsp: .space 4
endmark: .space 4
dptr: .space 4    
    
    
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
    
// VM instruction loop
next:
    lbu   t0, 0(ip)
    sll   t0, t0, 2   
    la t1,opcodes_table
    addu t1,t1,t0
#define _CHECK_OPCODE
#ifdef _CHECK_OPCODE    
    la t0, bad_op
    subu t0,t0,t1
    blez t0,vm_bad_op
#endif    
    lw    t1, 0(t1)
//#define _PRT_XT
#ifdef _PRT_XT    
// debug code    
//    addiu sp,sp,-4
//    sw t1,0(sp)
//    addiu a0,zero,2
//    move a1,t1
//    jal print_hex
//    addu a2,zero,zero
//    lw t1,0(sp)
//    addiu sp,sp,4
// debug code 
#endif    
    jr    t1
    _inc_ip

#ifdef _CHECK_OPCODE    
vm_bad_op:
    la a0, con
    lbu a0,0(a0)
    la a1,badop_msg
    jal print
    nop
    la a0, con
    lbu a0, 0(a0)
    lbu a1,0(ip)
    jal print_int
    addu a2,zero,zero
    b 1f
    addiu t0,zero,1
#endif    
bye:   // 'bye' ( -- ) exit virtual machine
    addu t0,zero,zero
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
    move v0,t0
    jr  ra
    addiu sp,sp,40

qrx: // '?key' ( -- ch -1 | 0 )
    la a0,con
    jal get_key
    lbu a0,0(a0)
    _tpush
    beqz v0, next
    addu T,v0,zero 
    addiu t0,zero,-1
    _tpush
qrx_exit:
    b next
    move T, t0

space: // ( -- ) émet un caractère ascii 32
    _tpush
    addiu T,zero,32
emit:   // 'emit' ( ch -- ) transmet charactère au terminal
    la a0,con
    lbu a0,0(a0)
    jal put_char
    move  a1, T
    lw T, 0(dp)
    b next
    addiu dp,dp,-CELL_SIZE

cr: // 'cr' ( -- ) émet un carriage return line feed
    la a0,con
    jal crlf
    lbu a0,0(a0)
    b next
    nop
    
    
lit:    // 'lit' ( -- n) empile un entier
    _tpush
    ulw T,0(ip)
    b next
    addiu ip,ip,CELL_SIZE

clit:   // 'clit' ( -- byte ) empile un octet signe étendu sur 32 bits
    _tpush
    lbu T, 0(ip)
    b next
    _inc_ip

wlit:  // 'wlit' ( -- w16  ) empile un mot de 16 bits
    _tpush
    lwr T,0(ip)
    andi T,T,0xffff
    b next
    addiu ip,ip,2

lits: // 's"' ( -- addr) empile l'adresse d'une chaîne litérale
    _tpush
    move T,ip
    lbu w,0(T)
    addu ip,ip,w
    b next
    _inc_ip
    
    
    
docol:   // 'call' (R: -- addr_ret ) appelle mot de haut niveau
    ulw t0,0(ip)
    _rpush
    move R, ip
    b next
    move ip, t0

//rcall:  # 'rcall' (R: -- addr_ret ) appelle sous-routine VM en mémoire RAM
//    lui t2, RAM_SEG
//    b fcall_1
//    lbu t0, 0(ip)

exit:  // ';' (R: addr_ret -- ) sortie d'un mot de haut niveau
    move ip, R
    lw R, 0(rp)
    b next
    addiu rp,rp, -CELL_SIZE

    
bra:   // 'branch' ( -- ) branchement incondionnnel (adresse relative +127,-128)
    lb t0, 0(ip)
    _inc_ip
    b next
    addu ip, ip, t0  // branchement relatif à ip, intervale -128 à +127

qbra: // '?bra' ( t|f -- ) branche si sommet pile est vrai (tos!=0)
    move t0, T
    _tpop
    beqz t0, next
    _inc_ip
    lb t0, -1(ip)
    b next
    addu ip,ip,t0 // relatif à ip {-128,127}

qbraz:  // '?braz' ( t|f -- ) branche si sommet pile est faux (tos==0)
    move t0, T
    _tpop
    bne t0, zero, next
    _inc_ip
    lb t0, -1(ip)
    b next
    addu ip,ip,t0 // relatif à ip {-128,127}

exec:  // 'exec' ( addr -- ) appelle sous-routine à partir de la pile
    _rpush
    move R, ip
    move  ip, T
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE

store:  // '!' ( n addr -- )  variable = n
    lw t0, 0(dp)
    usw t0,0(T)
    lw T,-CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE
    
// load variable in T
fetch:  // '@' ( addr -- n ) T= *addr
    move t0, T
    ulw T,0(t0)
    b next
    nop

cstore:  // 'c!' ( char addr --  ) *addr = char
    lw t0, 0(dp)
    sb t0, 0(T)
    lw T, -CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

cfetch:  // 'c@' ( addr -- char )  T = (char)*addr
    b next
    lb T, 0(T)

rfetch:   // 'r@'  ( -- n R: n -- n )  empile le sommet de la pile des retours
    _tpush
    b next
    move T, R

tor:  // '>r' ( n -- ) (R: -- n ) envoie T sur pile retour
    _rpush
    move R, T
    lw T, 0(dp)
    b next
    addiu dp, dp, -CELL_SIZE

rfrom:  // 'r>' ( -- n ) (R: n -- )  envoie R dans T
    _tpush
    move T, R
    lw  R, 0(rp)
    b next
    addiu rp,rp, -CELL_SIZE


dcnt:  // 'dcnt' ( -- n) nombre d'éléments sur la pile de données
    la t0,dstack
    subu t0,dp,t0
    _tpush
    b next
    sra T, t0,2

rcnt:  // 'rcnt' ( -- n ) nombre d'éléments sur la pile des retours
    _tpush
    la t0,rstack
    subu T,rp,t0
    b next
    sra T,T,2

ipfetch: // 'ip@' ( -- ip ) empile la valeur courante du pointeur d''instructions
    _tpush
    b next
    move T, ip

ipstore: // 'ip!' ( ip -- ) initialise le pointeur d''insturctions
    move ip, T
    lw T, 0(dp)
    b next
    addiu dp,dp, -CELL_SIZE

drop:  // 'drop' ( n -- )
    lw T, 0(dp)
    b next
    addiu dp,dp,-CELL_SIZE

twodrop: // '2drop' ( n1 n2 -- )
    lw T, -CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

dup:  // 'dup' ( n -- n n )
    addiu dp,dp,CELL_SIZE
    b next
    sw T, 0(dp)

twodup: // (n2 n1 -- n2 n1 n2 n1)
    lw t0, 0(dp)
    addiu dp,dp,2*CELL_SIZE
    sw T, -CELL_SIZE(dp)
    b next
    sw t0, 0(dp)

qdup:   // ?dup ( n -- n n | n ) duplique n si n!=0
    beq T,zero, next
    nop
    addiu dp,dp,CELL_SIZE
    b next
    sw T, 0(dp)

swap: // 'swap' (n2 n1 -- n1 n2 )
    move t0, T
    lw T, 0(dp)
    b next
    sw t0, 0(dp)

over:   // 'over' ( n2 n1 -- n2 n1 n2 )
    _tpush
    b next
    lw T, -CELL_SIZE(dp)

rot: // ( n3 n2 n1 -- n2 n1 n3)
    move t0, T
    lw t1, 0(dp)
    lw T, -CELL_SIZE(dp)
    sw t0, 0(dp)
    b next
    sw t1, -CELL_SIZE(dp)

invert: // ( n -- -n)  
    b next
    subu T,zero,T
    
plus:  // '+'   ( n1 n2 -- n1+n2 )
    lw t0,0(dp)
    addu T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

oneplus:  // '1+'  (n -- n+1 )
    b next
    addiu T, T, 1

plusstore:  // '+!'  ( n addr -- ) *addr  = *addr+n
    ulw t0,0(T)
    lw t1,0(dp)
    add t0,t1,t0
    usw t0,0(T)
    lw T,-CELL_SIZE(dp)
    b next
    addiu dp,dp,-2*CELL_SIZE

minus:   //  '-' ( n1 n2 -- n1-n2 )
    lw t0,0(dp)
    subu T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

oneminus:   // '1-' ( n -- n-1 )
    b next
    addiu T, T, -1

star:   // '*' ( n1 n2 -- n1*n2 ) multiplication signé, résultat simple précision.
    lw t0,0(dp) 
    mul T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

ustar:   // 'U*' ( u1 u2 -- u3 ) multiplication non signe, résultat simple précision
    lw t0,0(dp)
    multu  t0, T
    mflo T
    b next
    addiu dp,dp,-CELL_SIZE

mstar:  // 'M*' ( n1 n2 -- d) multiplication signée avec résultat double précision.
    lw t0, 0(dp)
    mult t0,T
    mflo t0
    mfhi T
    b next
    sw t0, 0(dp)

umstar: // 'UM*' ( u1 u2 -- ud ) multiplication non signé avec résultat double précision.
    lw t0, 0(dp)
    multu t0,T
    mflo t0
    mfhi T
    b next
    sw t0, 0(dp)

slash:  // '/' ( n1 n2 -- n1/n2 )  division signée
    lw t0, 0(dp)
    div t0,T
    mflo T
    b next
    addiu dp,dp,-CELL_SIZE

uslash:  // 'U/'  (u1 u2 -- u3)  division non signée
    lw t0,0(dp)
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
    lw t0, 0(dp)
    div t0, T  # n1/n2
    mfhi t0    #  remainder
    sw t0, 0(dp)
    b next
    mflo T     # quotient

lshift:  // '<<' ( n1 u -- n1<<u )
    lw t0, 0(dp)
    sllv T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

rshift:  // '>>'  ( n1 u -- n1>>u )
    lw t0,0(dp)
    srlv T,t0,T
    b next
    addiu dp,dp,-CELL_SIZE

modulo:  // 'mod' ( n1 n2 -- n1%n2 )
    lw t0, 0(dp)
    div t0,T
    mfhi T
    b next
    addiu dp,dp,-CELL_SIZE

// division non signée d'un double précision par un simple, retourne quotient et reste
umsmod: // UM/MOD (ud1 u1 -- u2 u3) u2=reste, u3=quotient
        lw      t0, -CELL_SIZE(dp)  # 32 bits poids faible
        lw      t1, 0(dp)           # 32 bits poids fort
        bgtz    t1, long_div
        nop
        divu    t0, T
        mfhi    t0              # reste
        mflo    T               # quotient
        sw      t0, -CELL_SIZE(dp)
        b       next
        addiu   dp, dp, -CELL_SIZE
1:      li      T, -1             # quotient > 32 bits
        sw      T, -CELL_SIZE(dp)
        b       next
        addiu   dp,dp,-CELL_SIZE
long_div:
        divu    t1,T
        mflo    t3      #quotient
        bgtz    t3, 1b
        mfhi    t1      #reste
        li      t2, 32  # compteur de boucle
2:      bltz    t3, 1b  # overflow
        sll     t3,t3,1
        subu    t4,t1,T
        bltz    t4,3f
        nop
        move    t1, t4
        ori     t3,t3,1
3:      addiu   t2,t2,-1  # décrémente le compteur de boucle
        beqz    t2, 5f    # division terminée si 0
        nop
        sll     t1,t1,1   # décale à gauche t1:t0
        bgez    t0, 4f
        nop
        ori     t1,t1,1
4:      b 2b
        sll     t0,t0,1
5:      move    T, t3                 # quotient
        sw      t1, -CELL_SIZE(dp)    # reste
        b       next
        addiu   dp,dp,-CELL_SIZE


  // division signée d'un double par un simple, retourne quotient et reste
  // les 32 bits les plus significatifs du double sont ignorés.
  //  Il s'agit dans les fait d'une division simple/simple.
msmod:   # M/MOD  (d1 n1 - n2 n3 )  n2=reste, n3=quotient
        lw      t0, -CELL_SIZE(dp)
        div     t0, T
        mfhi    t0
        mflo    T
        sw      t0, -CELL_SIZE(dp)
        b       next
        addiu   dp,dp,-CELL_SIZE


min:  // min (n1 n2 -- min )  conserve le plus petit
    lw t0, 0(dp)
    subu t1,t0,T
    bltz t1, next
    addiu dp,dp,-CELL_SIZE
    move T,t0
    b next
    nop

max:  // max (n1 n2 -- max ) conserve le plus grand
    lw t0,0(dp)
    subu t1,T,t0
    bltz t1, next
    addiu dp,dp,-CELL_SIZE
    move T,t0
    b next
    nop

abs:  // 'abs' ( n -- abs(n) )
    bgez T, next
    nop
    b next
    negu T

bitand: // 'and' ( n1 n2 -- n1&n2)
    lw t0, 0(dp)
    and T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

bitor:  // 'or' ( n1 n2 -- n1|n2)
    lw t0, 0(dp)
    or T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

bitxor: // 'xor' ( n1 n2 -- n1^n2)
    lw t0, 0(dp)
    xor T,T,t0
    b next
    addiu dp,dp,-CELL_SIZE

bitnot: // 'not' ( n1 -- ~n1 )
    b next
    not T,T

equal:  // '='  ( n1 n2 -- t|f ) n1==n2?
    lw t0,0(dp)
    addiu dp,dp,-CELL_SIZE
    beq T,t0,next
    li T, -1
    b next
    li T, 0

nequal: // '<>' ( n1 n2 -- t|f ) n1<>n2?
    lw t0,0(dp)
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
    lw t0,0(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bltz t0, next
    li T, -1
    b next
    li T, 0

greater:  // '>'  ( n1 n2 -- t|f ) n1>n2?
    lw t0,0(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    bgtz t0, next
    li T, -1
    b next
    li T, 0

lteq:  // '<=' ( n1 n2 -- t|f ) n1<=n2?
    lw t0,0(dp)
    addiu dp,dp,-CELL_SIZE
    subu t0,t0,T
    blez t0, next
    li T,-1
    b next
    li T, 0

gteq:  // '>=' ( n1 n2 -- t|f ) n1>=n2?
    lw t0,0(dp)
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
    _tpush
    b next
    move T, v0  // T=v0


dotq: // '."' ( -- ) ip pointe vers le texte, le premier octet est le compte.
    lbu w, 0(ip)
1:  _inc_ip
    beqz w, next
    addiu w,w,-1
    la a0,con
    lbu a0,0(a0)
    jal put_char
    lbu a1, 0(ip)
    b 1b
    nop

type: // ( -- addr ) // imprime la chaîne ascciz
    la a0,con
    lbu a0,0(a0)
    jal print
    move a1,T
    lw T, 0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
    
    
dot: // "." ( n -- ) imprime un entier en format libre
    la a0,con
    lbu a0,0(a0)
    move a1,T
    jal print_int
    move a2,zero
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
    
    
delay: // 'delay' ( msec -- ) delais en millisecondes
    la w,sys_ticks
    lw t0, 0(w)
    addu t0, T,t0
    lw T, 0(dp)
1:  lw t1, 0(w)
    bne t1, t0, 1b
    nop
    b next
    addiu dp, dp, -CELL_SIZE

ticks:  // 'ticks'  ( -- n ) empile le compteur sys_tick
    _tpush
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
    lw limit, 0(dp) 
    move step,T
    lw T, -CELL_SIZE(dp)
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
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
    
//sauvegarde variable local
//[IP] indique la position de la variable
local_store: // ( n -- )
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    subu t0,frame,t0
    sw T,0(t0)
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
    
//empile variable locale
//[IP] indique la position  de la variable
local_fetch:
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    subu t0,frame,t0
    _tpush
    b next
    lw T,0(t0)
    
//empile l'adresse de la variable locale
// [IP] indique la position de la variable
//sur la pile
local_addr: //( -- addr )
    lbu t0,0(ip)
    _inc_ip
    sll t0,t0,2
    _tpush
    subu t0,frame,t0
    b next
    lw T,0(t0)

//ajuste le registe fp
//pour les arguments
// [IP] nombres d'arguments
set_frame:// (  --  )
    lw t0,0(ip)
    _inc_ip
    _rpush
    move R,frame //préserve la valeur actuelle de frame
    _tpush
    sll T,T,2
    addu dp,dp,T
    move frame,dp
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
    
pick: // ( xu...,x1,x0,u -- xu...,x1,x0,xu ) empile une copie du Uième élément de la pile
    sll  T, T, 2
    subu w, dp,T
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

random: //
    jal rand
    _tpush
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
    _tpop
    move a0,T
    jal tone
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
    
melody: // ( addr -- ) fait entendre la mélodie contenue dans le tableau
    move a0,T
    jal tune
    lw T,0(dp)
    b next
    addiu dp,dp,-CELL_SIZE
 
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
    
.end StackVM

//.ent fetch_int
//fetch_int:  // empile un entier little indian dans espace code VM, retourne dans v0
//    lbu v0, 0(ip)
//    lbu t0, 1(ip)
//    ins v0,t0,8,8
//    lbu t0, 2(ip)
//    ins v0,t0,16,8
//    lbu t0,3(ip)
//    ins v0,t0,24,8
//    j ra
//    addiu ip,ip,CELL_SIZE
//    
//
//.end fetch_int



// routines names must be in same order as OP_CODES enum in vm.h    
opcodes_table:
.word bye,lit,wlit,clit,docol,exit,bra,qbra,exec,store,fetch,cstore
.word cfetch,rfetch,tor,rfrom,drop,dup,swap,over,pick,invert
.word plus,minus,star,ustar,mstar,umstar,slash,uslash,modulo,umsmod,msmod,ltz
.word zequal, bitand,bitor
.word bitxor,bitnot,ticks, delay
.word qdup, oneplus, oneminus,plusstore, twostar, twoslash, twodrop, rot, twodup
.word  min, max, abs, lshift, rshift, divmod, qbraz, dcnt, rcnt
.word equal,nequal, less, greater,lteq,gteq,forsave,forrest,for,fortest,fornext
.word sine, cos, tan, atan, acos, asin,random,trunc,num
.word key,qrx,emit,dotq,cr,dot,lits,type,space
.word beep,melody,clr_scr,array_bound    
.word local_store,local_fetch,local_addr,set_frame 
    
bad_op:  
badop_msg: .asciz "bad operating code: "    
    
#undef _ASM_CODE_