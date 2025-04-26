/* Synaesthesia - program to display sound graphically
   Copyright (C) 1997  Paul Francis Harrison

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  675 Mass Ave, Cambridge, MA 02139, USA.

  The author may be contacted at:
    pfh@yoyo.cc.monash.edu.au
  or
    27 Bond St., Mt. Waverley, 3149, Melbourne, Australia
*/

/*
template <
  class SrcType,
  class DestType,
  DestType (*transfer)(SrcType,DestType)
>
void bitmapPut(
  SrcType *src, int srcWidth, srcHeight, 
  DestType *dest, int destWidth, int destHeight,
  int srcX,int srcY,int destX, int destY, int width,int height) {
*/

#include "bitmap.h"
#include "symbol.h"
#include "font.h"
#include "syna.h"

static int span[SymbolCount][2] = {
  {1,25},
  {32,55},
  {63,78},
  {86,109},
  {116,140},
  {147,172},
  {176,201},
  {206,216},
  {224,246},
  {266,289},
  {300,332},
  {338,373},

  {524,538},
  {386,391},
  {401,415},
  {419,430},
  {432,446},
  {447,461},
  {462,476},
  {480,492},
  {493,507},
  {508,522},

  {545,552},
  {559,568},
  {575,584}
};

struct Entry {
  unsigned char blue;
  unsigned char red;
  Entry(unsigned char b,unsigned char r) : blue(b), red(r) { }
};

Entry maxBlue(unsigned char src,Entry dest) 
  { return Entry(src > dest.blue ? src : dest.blue,dest.red); }
Entry halfBlue(unsigned char src,Entry dest) 
  { return Entry(src/2 > dest.blue ? src/2 : dest.blue,dest.red); }
Entry maxRed(unsigned char src,Entry dest) 
  { return Entry(dest.blue,src > dest.red ? src : dest.red); }
Entry halfRed(unsigned char src,Entry dest) 
  { return Entry(dest.blue,src/2 > dest.red ? src/2 : dest.red); }

template <Entry (*transfer)(unsigned char,Entry)>
struct PutSymbol {
  static void go(int x,int y,int id) {
    BitmapPut<unsigned char,Entry,transfer>
     ::go(
            Symbols,SYMBOLSWIDTH,SYMBOLSHEIGHT,
            (Entry*)output,outWidth,outHeight,
            span[id][0],0,
            x-(span[id][1]-span[id][0]>>1),y,
            span[id][1]-span[id][0],SYMBOLSHEIGHT);
  }
};

void putSymbol(int x,int y,int id,TransferType typ) {
  switch(typ) {
    case HalfBlue : PutSymbol<halfBlue>::go(x,y,id); break;
    case MaxBlue : PutSymbol<maxBlue>::go(x,y,id); break;
    case HalfRed  :  PutSymbol<halfRed>::go(x,y,id); break;
    case MaxRed  :  PutSymbol<maxRed>::go(x,y,id); break;
    default      : error("Invalid transfer operation.\n");
  }
}

void putChar(unsigned char character,int x,int y,int red,int blue) {
  Entry *ptr = ((Entry*)output) + x + y*outWidth;
  Entry put(blue,red);
  int i,j;
  for(i=0;i<8;i++,ptr += outWidth-8)
    for(j=0;j<8;j++,ptr++)
      if (font[character*8+i] & (128>>j))
        *ptr = put;
}

void putString(char *string,int x,int y,int red,int blue) {
  if (x < 0 || y < 0 || y >= outHeight-8)
    return;
  for(;*string && x <= outWidth-8;string++,x+=8)
    putChar((unsigned char)(*string),x,y,red,blue);
}

