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

// Noms symbolique pour les codes ASCII.
//REF: www.asciitable.com
// codes de contrôle   

#ifndef __ASCII_H
#define __ASCII_H
#ifdef	__cplusplus
extern "C" {
#endif

#define A_NUL		0   //null
#define A_SOH		1   //start of heading
#define A_STX		2   //start of text
#define A_ETX		3   //end of text
#define A_EOT		4   //end of transmission
#define A_ENQ		5   //enquiry
#define A_ACK		6   //acknowledge
#define A_BEL		7   //bell
#define A_BKSP		8   //backspace
#define A_TAB		9   //horizontal tab
#define A_LF		10  //line feed
#define A_VT		11  //vertical tab
#define A_FF		12  //form feed
#define A_CR		13  //carriage return
#define A_SO		14  //shift out
#define A_SI		15  //shift in
#define A_DLE		16  //data link escape
#define A_DC1		17  //device control 1
#define A_DC2		18  //device control 2
#define A_DC3		19  //device control 3
#define A_DC4		20  //device control 4
#define A_NAK		21  //negative ackowledge
#define A_SYN		22  //synchronous idle
#define A_ETB		23  //end of trans. block
#define A_CAN		24  //cancel
#define A_EM		25  //end of medium
#define A_SUB		26  //substitute
#define A_ESC		27  //escape
#define A_FS		28  //file separator	
#define A_GS		29  //group separator
#define A_RS		30  //record separator
#define A_US		31  //unit separator
// caractères
#define A_SPACE 	32  // ' '
#define A_EXCLA		33  // '!'
#define A_DQUOT		34  // '"'	
#define A_SHARP 	35  // '#'
#define A_DOLLR		36  // '$'
#define A_PRCNT		37  // '%'
#define A_AMP		38  // '&'
#define A_QUOT		39  // '\''
#define A_LPAR		40  // '('
#define A_RPAR  	41  // ')'
#define A_STAR  	42  // '*'
#define A_PLUS		43  // '+'
#define A_COMM		44  // ','
#define A_DASH  	45  // '-'
#define A_DOT		46  // '.'
#define A_SLASH 	47  // '/'
#define A_0		48  // '0'
#define A_1		49  // '1'
#define A_2		50  // '2'
#define A_3		51  // '3'	
#define A_4		52  // '4'
#define A_5		53  // '5'
#define A_6		54  // '6'
#define A_7		55  // '7'
#define A_8		56  // '8'
#define A_9		57  // '9'
#define A_COL		58  // ':'
#define A_SCOL		59  // '//'
#define A_LT		60  // '<'
#define A_EQU 	61  // '='
#define A_GT		62  // '>'
#define A_QUST		63  // '?'
#define A_AROB		64  // '@'
#define A_AU		65  // 'A'
#define A_BU		66  // 'B'
#define A_CU		67  // 'C'
#define A_DU		68  // 'D'
#define A_EU		69  // 'E'
#define A_FU		70  // 'F'
#define A_GU		71  // 'G'
#define A_HU		72  // 'H'
#define A_IU		73  // 'I'
#define A_JU		74  // 'J'
#define A_KU		75  // 'K'
#define A_LU		76  // 'L'
#define A_MU		77  // 'M'
#define A_NU		78  // 'N'
#define A_OU		79  // 'O'
#define A_PU		80  // 'P'
#define A_QU		81  // 'Q'
#define A_RU		82  // 'R'
#define A_SU		83  // 'S'
#define A_TU		84  // 'T'
#define A_UU		85  // 'U'
#define A_VU		86  // 'V'
#define A_WU		87  // 'W'
#define A_XU		88  // 'X'
#define A_YU		89  // 'Y'
#define A_ZU		90  // 'Z'
#define A_LBRK		91  // '['
#define A_BSLA		92  // '\\'
#define A_RBRK		93  // ']'
#define A_CIRC		94  // '^'
#define A_UNDR		95  // '_'
#define A_ACUT		96  // '`'
#define A_AL		97  // 'a'
#define A_BL		98  // 'b'
#define A_CL		99  // 'c'
#define A_DL		100 // 'd'
#define A_EL		101 // 'e'
#define A_FL		102 // 'f'
#define A_GL		103 // 'g'
#define A_HL		104 // 'h'
#define A_IL		105 // 'i'
#define A_JL		106 // 'j'
#define A_KL		107 // 'k'
#define A_LL		108 // 'l'
#define A_ML		109 // 'm'
#define A_NL		110 // 'n'
#define A_OL		111 // 'o'
#define A_PL		112 // 'p'
#define A_QL		113 // 'q'
#define A_RL		114 // 'r'
#define A_SL		115 // 's'
#define A_TL		116 // 't'
#define A_UL		117 // 'u'
#define A_VL		118 // 'v'
#define A_WL		119 // 'w'
#define A_XL		120 // 'x'
#define A_YL		121 // 'y'
#define A_ZL		122 // 'z'
#define A_LBRC		123 // '{'
#define A_PIPE		124 // '|'
#define A_RBRC		125 // '}'
#define A_TILD		126 // '~'
#define A_DEL		127 // DELETE	

#ifdef	__cplusplus
}
#endif
    
#endif // __ASCII_H

	
