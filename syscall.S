/*
Description:  Interface pour les appels des fonctions système.    
    le registre $v0 contient le no. de la fonction
    les registre $a0..$a3 contient les arguments de la fonctions.
     
     instruction machine SYSCALL déclenche une exception qui est gérée par
     _general_exception_handler  On comence par sauvegarder le contexte sur la pile
     avant d'appeller la fonction système concernée.
     
*/
    
    
    
    
    .text
    .set noreorder
    .set macro
    
// syscall est la fonction appellée par les applications
// pour faire un appel système.
    
    .global syscall
    .type syscall, @function
    .ent syscall
    
syscall:
    addiu $sp,$sp,24
    mfhi $k0
    mflo $k1
    sw $k0,20($sp)
    sw $k1,16($sp)
    
    syscall
    
    
    .end syscall
    

// routine de gestion des exceptions
    
    .global _general_exception_handler
    .type _general_exception_handler, @function
    .ent _general_exception_handler
    
_general_exception_handler:
    addiu $sp,$sp,8
    mfc0 $k0, $13  // cause
    mfc0 $k1, $14  // EPC
    sw $k1,4($sp)
    srl $k0,2
    andi $k0,0x1f
    bne $k0,8, inf_loop
    nop
system_call:
    la $v1,function_table
    addu $v1,$v1,$v0
    jr $v1
    nop
    lw $k1,4($sp)
    addiu $k1,$k1,4
    jr $k1
    addiu $sp,$sp,-8
inf_loop:    
    j .
    nop
    
    .end _general_exception_handler
    
.section .rodata
    
function_table:
    
    