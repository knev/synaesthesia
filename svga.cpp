
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

#include <vga.h>
#include <vgamouse.h>
#include "syna.h"

static unsigned char *scr;

void setPalette(int i,int r,int g,int b) {
  vga_setpalette(i,r/4,g/4,b/4);
}

void screenInit(int xHint,int yHint,int widthHint,int heightHint) {
  attempt(vga_init(),"initializing svgalib");
  if (!vga_hasmode(G320x200x256)) 
    error("requesting 320x200 graphics mode");
  
  attempt(vga_setmode(G320x200x256),"entering 320x200 graphics mode");

  scr = vga_getgraphmem();
  outWidth = 320;
  outHeight = 200;
 
  attemptNoDie(mouse_init("/dev/mouse",
    vga_getmousetype(),MOUSE_DEFAULTSAMPLERATE),"initializing mouse");
  mouse_setxrange(-1,321);
  mouse_setyrange(-1,201);
  mouse_setposition(160,34);

  #define BOUND(x) ((x) > 255 ? 255 : (x))
  #define PEAKIFY(x) BOUND((x) - (x)*(255-(x))/255/2)
  int i;
  for(i=0;i<256;i++)
    setPalette(i,PEAKIFY((i&15*16)),
                 PEAKIFY((i&15)*16+(i&15*16)/4),
                 PEAKIFY((i&15)*16));
}

void screenEnd() {
  mouse_close();
  vga_setmode(TEXT);
}

int sizeUpdate() {
  return 0;
}
void sizeRequest(int width,int height) {
  if (width != 320 || height != 200)
    warning("resizing screen.");
}


void mouseUpdate() {
  mouse_update();  
}
int mouseGetX() {
  return mouse_getx();
}
int mouseGetY() {
  return mouse_gety();
}
int mouseGetButtons() {
  return mouse_getbutton();
} 

void screenShow(void) {
  register unsigned long *ptr2 = (unsigned long*)output;
  unsigned long *ptr1 = (unsigned long*)scr;
  int i = 320*200/4;
  // Asger Alstrup Nielsen's (alstrup@diku.dk)
  // optimized 32 bit screen loop
  do {
    //Original bytewize version:
    //unsigned char v = (*(ptr2++)&15*16);
    //*(ptr1++) = v|(*(ptr2++)>>4);
    register unsigned int const r1 = *(ptr2++);
    register unsigned int const r2 = *(ptr2++);

    //Fade will continue even after value > 16
    //thus black pixel will be written when values just > 0
    //thus no need to write true black
    if (r1 || r2) {
      register unsigned int const v = 
          ((r1 & 0x000000f0) >> 4)
        | ((r1 & 0x0000f000) >> 8)
        | ((r1 & 0x00f00000) >> 12)
        | ((r1 & 0xf0000000) >> 16);
      *(ptr1++) = v | 
        ( ((r2 & 0x000000f0) << 12)
        | ((r2 & 0x0000f000) << 8)
        | ((r2 & 0x00f00000) << 4)
        | ((r2 & 0xf0000000)));
    } else {
      ptr1++;
    }  
  } while (--i); 
}

