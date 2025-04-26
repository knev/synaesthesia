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

SDL_Surface *CreateScreen(Uint16 w, Uint16 h, Uint8 bpp, Uint32 flags)
{
  SDL_Surface *screen;

  /* Set the video mode */
  screen = SDL_SetVideoMode(w, h, bpp, flags);
  if ( screen == NULL ) {
    //fprintf(stderr, "Couldn't set display mode: %s\n",
    //          SDL_GetError());
    return(NULL);
  }
  //fprintf(stderr, "Screen is in %s mode\n",
  //  (screen->flags & SDL_FULLSCREEN) ? "fullscreen" : "windowed");

  return(screen);
}
  
void screenSetPalette(unsigned char *palette) {
  SDL_Color sdlPalette[256];

  for(int i=0;i<256;i++) {
    sdlPalette[i].r = palette[i*3+0];
    sdlPalette[i].g = palette[i*3+1];
    sdlPalette[i].b = palette[i*3+2];
  }
  
  SDL_SetColors(screen, sdlPalette, 0, 256);
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

  SDL_EnableUNICODE(1);
  SDL_ShowCursor(0);
}

void screenEnd(void) {
  SDL_Quit();
}

void inputUpdate(int &mouseX,int &mouseY,int &mouseButtons,char &keyHit) {    
  SDL_Event event;
 
  keyHit = 0;
  
  while ( SDL_PollEvent(&event) > 0 ) {
    switch (event.type) {
      case SDL_MOUSEBUTTONUP:
      case SDL_MOUSEBUTTONDOWN:
        if ( event.button.state == SDL_PRESSED ) 
          mouseButtons |= 1 << event.button.button;
        else	
          mouseButtons &= ~( 1 << event.button.button );
        mouseX = event.button.x;
        mouseY = event.button.y;
	break;
      case SDL_MOUSEMOTION :
        mouseX = event.motion.x;
        mouseY = event.motion.y;
	break;
      case SDL_KEYDOWN:
        ///* Ignore key releases */
        //if ( event.key.state == SDL_RELEASED ) {
        //  break;
        //}
        /* Ignore ALT-TAB for windows */
        if ( (event.key.keysym.sym == SDLK_LALT) ||
             (event.key.keysym.sym == SDLK_TAB) ) {
          break;
        }

	if (event.key.keysym.unicode > 255)
	  break;

	keyHit = event.key.keysym.unicode;
	return;
      case SDL_QUIT:
        keyHit = 'q';        
        return;
      default:
        break;
    }
  }
}

int sizeUpdate(void) { return 0; }

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
  
    //if (r1 || r2) {
#ifdef LITTLEENDIAN
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
#else
      register unsigned int const v = 
          ((r2 & 0x000000f0ul) >> 4)
        | ((r2 & 0x0000f000ul) >> 8)
        | ((r2 & 0x00f00000ul) >> 12)
        | ((r2 & 0xf0000000ul) >> 16);
      *(ptr1++) = v | 
        ( ((r1 & 0x000000f0ul) << 12)
        | ((r1 & 0x0000f000ul) << 8)
        | ((r1 & 0x00f00000ul) << 4)
        | ((r1 & 0xf0000000ul)));
#endif
    //} else ptr1++;
  } while (--i); 

  SDL_UnlockSurface(screen);
  SDL_UpdateRect(screen, 0, 0, 0, 0);
}
