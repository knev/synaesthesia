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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <SDL.h>

#include "syna.h"

static SDL_Surface *screen;

/* Draw a randomly sized and colored box centered about (X,Y) */
void DrawBox(SDL_Surface *screen, int X, int Y)
{
  static unsigned int seeded = 0;
  int x, y, w, h, color;
  int i;
  Uint8 *buffer;

  /* Seed the random number generator */
  if ( seeded == 0 ) {
    srand(time(NULL));
    seeded = 1;
  }

  /* Get the bounds of the rectangle */
  w = (rand()%640);
  h = (rand()%480);
  x = X-(w/2);
  y = Y-(h/2);
  color = (rand()%256);

  /* Perform clipping */
  if ( x < 0 ) {
    w += x;
    x = 0;
  }
  if ( y < 0 ) {
    h += y;
    y = 0;
  }
  if ( x+w > 640 ) {
    w = 640-x;
  }
  if ( y+h > 480 ) {
    h = 480-y;
  }

  /* Lock (8-bit surface) and load! */
  if ( SDL_MUSTLOCK(screen) ) {
    if ( SDL_LockSurface(screen) < 0 ) {
      return;
    }
  }
  buffer = ((Uint8 *)screen->pixels)+y*screen->pitch+x;
  for ( i=0; i<h; ++i ) {
    memset(buffer, color, w);
    buffer += screen->pitch;
  }
  /* Assuming that SDL_MUSTLOCK(screen) only evaluates to true
     if we are writing directly to the video hardware framebuffer.
     This is not true of normal surfaces, just the screen surface.
   */
  if ( SDL_MUSTLOCK(screen) ) {
    SDL_UnlockSurface(screen);
  } else {
    SDL_UpdateRect(screen, x, y, w, h);
  }
}

SDL_Surface *CreateScreen(Uint16 w, Uint16 h, Uint8 bpp, Uint32 flags)
{
  SDL_Surface *screen;
  int i;
  SDL_Color palette[256];

  /* Set the video mode */
  screen = SDL_SetVideoMode(w, h, bpp, flags);
  if ( screen == NULL ) {
    //fprintf(stderr, "Couldn't set display mode: %s\n",
    //          SDL_GetError());
    return(NULL);
  }
  //fprintf(stderr, "Screen is in %s mode\n",
  //  (screen->flags & SDL_FULLSCREEN) ? "fullscreen" : "windowed");

  for ( i=0; i<256; ++i ) {
#define BOUND(x) ((x) > 255 ? 255 : (x))
#define PEAKIFY(x) BOUND((x) - (x)*(255-(x))/255/2)
    palette[i].r = PEAKIFY((i&15*16));
    palette[i].g = PEAKIFY((i&15)*16+(i&15*16)/4);
    palette[i].b = PEAKIFY((i&15)*16);
  }
  SDL_SetColors(screen, palette, 0, 256);

  return(screen);
}

void screenInit(int xHint,int yHint,int width,int height) 
{
  Uint32 videoflags;

  /* Initialize SDL */
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
    char str[1000];
    sprintf(str, "initializing SDL library: %s\n",SDL_GetError());
    error(str);
  }

  /* See if we try to get a hardware colormap */
  videoflags = SDL_SWSURFACE;
  //videoflags = SDL_HWSURFACE;
  /*while ( argc > 1 ) {
    --argc;
    if ( argv[argc] && (strcmp(argv[argc], "-hw") == 0) ) {
      videoflags |= SDL_HWSURFACE;
    } else
    if ( argv[argc] && (strcmp(argv[argc], "-warp") == 0) ) {
      videoflags |= SDL_HWPALETTE;
    } else
    if ( argv[argc] && (strcmp(argv[argc], "-fullscreen") == 0) ) {
      videoflags |= SDL_FULLSCREEN;
    } else {
      fprintf(stderr, "Usage: %s [-warp] [-fullscreen]\n",
                argv[0]);
      exit(1);
    }
  }*/

  screen = CreateScreen(width, height, 8, videoflags);
  if ( screen == NULL ) {
    error("requesting screen dimensions");
  }

  outWidth = width;
  outHeight = height;
}

void screenEnd(void) {
  SDL_Quit();
}

void inputUpdate(int &mouseX,int &mouseY,int &mouseButtons,char &keyHit) {    
  SDL_Event event;
 
  keyHit = 0;
  
  while ( SDL_PollEvent(&event) > 0 ) {
    switch (event.type) {
      case SDL_MOUSEBUTTONEVENT:
        if ( event.button.state == SDL_PRESSED ) 
          mouseButtons |= 1 << event.button.button;
        else	
          mouseButtons &= ~( 1 << event.button.button );
        mouseX = event.button.x;
        mouseY = event.button.y;
	break;
      case SDL_MOUSEMOTIONEVENT :
        mouseX = event.motion.x;
        mouseY = event.motion.y;
	break;
      case SDL_KEYEVENT:
        /* Ignore key releases */
        if ( event.key.state == SDL_RELEASED ) {
          break;
        }
        /* Ignore ALT-TAB for windows */
        if ( (event.key.keysym.sym == SDLK_LALT) ||
             (event.key.keysym.sym == SDLK_TAB) ) {
          break;
        }

	keyHit = SDL_SymToASCII(&event.key.keysym, 0);
	return;
      case SDL_QUITEVENT:
        keyHit = 'q';        
        return;
      default:
        break;
    }
  }
}

int sizeUpdate(void) { return 0; }
void sizeRequest(int w,int h) { }

void screenShow(void) { 
  attempt(SDL_LockSurface(screen),"locking screen for output.");

  register unsigned long *ptr2 = (unsigned long*)output;
  unsigned long *ptr1 = (unsigned long*)( screen->pixels );
  int i = outWidth*outHeight/4;

  do {
    // Asger Alstrup Nielsen's (alstrup@diku.dk)
    // optimized 32 bit screen loop
    register unsigned int const r1 = *(ptr2++);
    register unsigned int const r2 = *(ptr2++);
  
    if (r1 || r2) {
      register unsigned int const v = 
          ((r1 & 0x000000f0ul) >> 4)
        | ((r1 & 0x0000f000ul) >> 8)
        | ((r1 & 0x00f00000ul) >> 12)
        | ((r1 & 0xf0000000ul) >> 16);
      *(ptr1++) = v | 
        ( ((r2 & 0x000000f0ul) << 12)
        | ((r2 & 0x0000f000ul) << 8)
        | ((r2 & 0x00f00000ul) << 4)
        | ((r2 & 0xf0000000ul)));
    } else ptr1++;
  } while (--i); 

  SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen, 0, 0, 0, 0);
}
