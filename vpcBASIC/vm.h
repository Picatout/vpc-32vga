/* 
 * File:   vm.h
 * Author: Jacques Deschênes
 *
 * Created on 31 mai 2013, 16:40
 * rev: 2017-08-01
 */

#ifndef OPCODES_H
#define	OPCODES_H

#define RAM_SEG  0xA000
#define CODE_SEG 0x9D00
#define SFR_SEG  0xBF80
#define CFG_SEG  0xBFC0



// op codes
enum OP_CODES{
 ILIT=0, 
 IDOCOL , 
 IEXIT , 
 IBRA , 
 IQBRA ,
 IEXEC ,
 ISTORE ,
 IFETCH ,
 ICSTORE ,
 ICFETCH ,
 IRFETCH ,
 IUFETCH ,
 IUSTORE ,
 ITOR   , 
 IRFROM , 
 IDROP , 
 IDUP , 
 ISWAP ,
 IOVER ,
 IPICK ,
 IPLUS ,
 ISUB  ,
 ISTAR  ,
 IUSTAR ,
 IMSTAR ,
 IUMSTAR ,
 ISLASH , 
 IUSLASH ,
 IMOD , 
 IUMSMOD ,
 IMSMOD , 
 ILTZ , 
 IZEQUAL ,
 IAND , 
 IOR  , 
 IXOR  , 
 INOT  , 
 ICLIT , 
 IWLIT , 
 ITICKS ,
 IDELAY ,
 IQDUP  ,
 IPLUS1 ,
 IMINUS1 ,
 IPLUSSTR ,
 I2STAR , 
 I2SLASH,
 IDDROP,
 IROT  ,
 IDDUP ,
 IMIN ,
 IMAX ,
 IABS ,
 ILSHIFT,
 IRSHIFT,
 IDIVMOD,
 IQBRAZ,
 IDCNT,
 IRCNT,
 IEQUAL,
 INEQUAL,
 ILESS,
 IGREATER ,
 ILTEZ , 
 IGTEZ , 
 IDO , 
 IUNLOOP ,
 IIFETCH  ,
 IJFETCH ,
 ILOOP , 
 IPLOOP ,
 IUSER , 
 ISINE , 
 ICOS , 
 ITAN , 
 IATAN ,
 IACOS ,
 IASIN ,
 IRND , 
 ITRUNC ,
 INUM , 
 IKEY , 
 IQRX , 
 IEMIT ,
 IDOTQ ,
 ICR , 
 IDOT,
 IBYE
};

#define LASTOP IDOT


#define TOK_COUNT (LASTOP+1)
#define IBADOP (-TOK_COUNT)



#define CELL_SIZE (4)


#endif	/* OPCODES_H */

