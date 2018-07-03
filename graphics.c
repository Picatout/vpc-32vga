/*
* Copyright 2013, Jacques Deschênes
* This file is part of VPC-32.
*
*     VPC-32 is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     VPC-32 is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with VPC-32.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
 *  File: graphics.c
 *  Author: Jacques Deschênes
 *  Created on 2 novembre 2013
 *  Description: fonctions graphiques.
 *  REF: http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
 *  REF: http://members.chello.at/~easyfilter/bresenham.html
 */
#include "hardware/tvout/vga.h"
#include <math.h>

int getPixel(int x, int y){
    int h,ofs;

    if ((y>=VRES)||(y<0)||(x>=HRES)||(x<0)) return 0; // hors limites
    h=x/32;
    ofs=31-x&31;
    return (video_bmp[y][h]&(1<<ofs))>>ofs;
}//getPixel()

// fixe l'état d'un pixel, p{0,1}
void putPixel(int x, int y, unsigned c){
    int h,ofs;
    if ((y>=VRES)||(y<0)||(x>=HRES)||(x<0)) return; // hors limites
    h= x/32;
    ofs = 31 - x&31;
    if (c>7){
        video_bmp[y][h]|= (1<<ofs);
    }else{
        video_bmp[y][h]&= ~(1<<ofs);
    }
} // putPixel()

// inverse l'état du pixel
// retourne l'état du pixel avant inversion.
int xorPixel(int x, int y){
    int h,ofs, collision;
    if ((y>=VRES)||(y<0)||(x>=HRES)||(x<0)) return; // hors limites
    h= x/32;
    ofs = 31 - x&31;
    collision=(video_bmp[y][h]&(1<<ofs))>>ofs;
    video_bmp[y][h]^= (1<<ofs);
    return collision;
}//xorPixel()

//  REF: http://members.chello.at/~easyfilter/bresenham.html
void line(int x0, int y0, int x1, int y1)
{
   int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
   int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
   int err = dx+dy, e2; /* error value e_xy */

   for(;;){  /* loop */
      xorPixel(x0,y0);
      if (x0==x1 && y0==y1) break;
      e2 = 2*err;
      if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
      if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
   }
}//line()

void rectangle(int x1, int y1, int width, int height){
    int xLeft=x1,right,bottom;
    
    right=x1+width-1;
    bottom=y1+height-1;
    while (x1<=right){//lignes horizontales
        xorPixel(x1,y1);
        xorPixel(x1++,bottom);
    }
    ++y1;
    while (y1<bottom){//lignes verticales
        xorPixel(xLeft,y1);
        xorPixel(right,y1++);
    }
}//rectangle()

void box (int x0, int y0, int width, int height){
    int x;
    width+=x0;
    height+=y0;
    for (y0;y0<height;y0++){
        for (x=x0;x<width;x++){
            xorPixel(x,y0);
        }
    }
}


/* REF: http://members.chello.at/~easyfilter/bresenham.html
 * dessine une ellipse circoncrite par un rectangle
 */
void ellipse (int x0, int y0, int x1, int y1){
   int a = abs(x1-x0), b = abs(y1-y0), b1 = b&1; /* values of diameter */
   long dx = 4*(1-a)*b*b, dy = 4*(b1+1)*a*a; /* error increment */
   long err = dx+dy+b1*a*a, e2; /* error of 1.step */

   if (x0 > x1) { x0 = x1; x1 += a; } /* if called with swapped points */
   if (y0 > y1) y0 = y1; /* .. exchange them */
   y0 += (b+1)/2; y1 = y0-b1;   /* starting pixel */
   a *= 8*a; b1 = 8*b*b;

   do {
       xorPixel(x1, y0); /*   I. Quadrant */
       xorPixel(x0, y0); /*  II. Quadrant */
       xorPixel(x0, y1); /* III. Quadrant */
       xorPixel(x1, y1); /*  IV. Quadrant */
       e2 = 2*err;
       if (e2 <= dy) { y0++; y1--; err += dy += a; }  /* y step */
       if (e2 >= dx || 2*err > dy) { x0++; x1--; err += dx += b1; } /* x step */
   } while (x0 <= x1);

   while (y0-y1 < b) {  /* too early stop of flat ellipses a=1 */
       xorPixel(x0-1, y0); /* -> finish tip of ellipse */
       xorPixel(x1+1, y0++);
       xorPixel(x0-1, y1);
       xorPixel(x1+1, y1--);
   }
}//ellipse()

// circle()
// un facteur de correction doit-être appliqué à la valeur du rayon
// p.c.q. la dimension des pixels à l'écran n'est pas la même verticalement
// qu'horizontalement. Sans correction le cercle apparaît comme une ellipse.
void circle(int xc, int yc, int r){
#define CORRECTION_FACTOR 16    
    int rx,ry;
    
    rx=r+r*CORRECTION_FACTOR/200;
    ry=r-r*CORRECTION_FACTOR/200;
    ellipse(xc-rx,yc-ry,xc+rx,yc+ry);
}//circle()


/*
 * points[]={x1,y1,x2,y2,x3,y3,...}
 * vertices est le nombre de points
 */
void polygon(const int points[], int vertices){
    int i,x0=points[0],y0=points[1];
    for(i=0;i<(2*vertices-2);i+=2){
        xorPixel(x0,y0);
        line(points[i],points[i+1],points[i+2],points[i+3]);
        x0=points[i+2];
        y0=points[i+3];
    }
    line(points[0],points[1],points[i],points[i+1]);
}//polygon()

// un sprite est un bitmap appliqué à l'écran en utilisant xorPixel()
// le bitmap est un tableau d'octets. Chaque octet contient 2 pixels.
// Chaque pixel est représenté par 4 bits pour être compatible avec les sprites
// du BASIC de PV16SOG.
// x{0..HRES-1}
// y{0..VRES-1}
// width{1..32} doit-être pair.
// height{1..VRES}
// retourne:
//    1 si collision
//    0 sinon
int sprite( int x, int y, int width, int height, uint8_t* sp){
    int wx,wy,bshift;
    int xofs,yofs,collision=0;
    
    for (wx=0;wx<width;wx++){
        xofs=wx/2;
        bshift=wx&1?0:4;
        for (wy=0;wy<height;wy++){
            yofs=wy*width/2;
            if (sp[yofs+xofs]&(8<<bshift)){
                collision|=xorPixel(x+wx,y+wy);
            }//if
        }//for y
    }// for x
    return collision;
}


//sauvegarde le tampon vidéo dans la mémoire SPI RAM
void saveScreen(unsigned addr){
    sram_write_block(addr,video_bmp,BMP_SIZE);
}

//copie un bloc de la SPI RAM dans le tampon vidéo
void restoreScreen(unsigned addr){
    sram_read_block(addr,video_bmp,BMP_SIZE);
}

// remplissage d'une figure géométrique fermée
// AVERTISSEMENT: Appel récursif profond qui peut faire planter l'ordinateur
// du à la taille limité de la pile.
void fill(int x, int y){
    int x0;
    
    if ((y>=0) && (y<VRES) && (x>=0) && (x<HRES) && !getPixel(x,y)){
        xorPixel(x,y);
        fill(x-1,y);
        fill(x,y-1);
        fill(x+1,y);
        fill(x,y+1);
    }
}

