/*
* Copyright 2013,2017 Jacques Deschênes
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
 * File:   vga.h
 * Author: Jacques Deschênes
 *
 * Created on 26 août 2013, 07:48
 * rev: 2017-07-31
 */

#ifndef VGA_H
#define	VGA_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <GenericTypeDefs.h>
#include "../../ascii.h"
#include "../../graphics.h"    
#include "display.h"
#include "../../font.h"
    
extern unsigned int video_bmp[VRES][HRES/32];

typedef void (*cursor_tmr_callback_f)(void);

typedef struct cursor_timer{
    BOOL active;
    unsigned int period;
    cursor_tmr_callback_f  cb;
} cursor_timer_t;


//API vidéo
void enable_cursor_timer(BOOL enable, cursor_tmr_callback_f cb);
void vga_toggle_cursor(); // inverse l'état du curseur texte.
void vga_show_cursor(BOOL); // affiche ou masque le curseur texte
BOOL vga_is_cursor_active(); // retourne vrai si le curseur texte est actif.
void vga_set_cursor(cursor_t shape); // défini la  forme du curseur
void vga_clear_screen();
void vga_clear_eol(); // efface la fin de la ligne à partir du curseur.
void vga_scroll_up(); // fait glissé le texte une ligne vers le haut.
void vga_scroll_down(); // fait glissé le texte une ligne vers le bas.
text_coord_t vga_get_curpos(); // retourne position curseur texte.
void vga_set_curpos(unsigned short x, unsigned short y); // positionne le curseur
void vga_put_char( char c); //affiche le caractère à la position courante
void vga_print( const char *str); // imprime un chaîne à la position courante
void vga_cursor_right(); // avance le curseur, retour à la ligne si nécessaire.
void vga_cursor_left(); // recule le curseur d'une position, va à la fin de la ligne précédente si nécessaire.
void vga_cursor_down(); // descend le curseur à la ligne suivante, scroll_up() si nécessaire
void vga_cursor_up(); // monte le curseur à la ligne précédente, scroll_down() si nécessaire
void vga_set_tab_width(unsigned char width); // défini la largeur des colonnes de tabulation.
void vga_invert_char(); // inverse les pixel du caractère à la position du curseur.
void vga_toggle_cursor(); // inverse l'état du curseur texte.
void vga_show_cursor(BOOL); // affiche ou masque le curseur texte
BOOL vga_is_cursor_active(); // retourne vrai si le curseur texte est actif.
void vga_set_cursor(cursor_t shape); // défini la  forme du curseur
void vga_spaces(unsigned char count);
void vga_crlf(); // déplace le curseur à la ligne suivante
void vga_invert_video( unsigned char invert); // inverse vidéo des caractèrs noir/blanc
BOOL vga_is_invert_video(); // renvoie le mode vidéo.
void vga_println( const char *str);

// Hardware initialization
 int vga_init(void);

#ifdef	__cplusplus
}
#endif

#endif	/* VGA_H */

