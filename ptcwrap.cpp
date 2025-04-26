/* Synaesthesia - program to display sound graphically
   Copyright (C) 1997  Paul Francis Harrison

  Windows95 port (mapping to Prometheus Truecolor library) (C) 1998 
  Nils Desle, ndesle@hotmail.com

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

#ifdef WIN32
#include <windows.h>
#endif
#include "ptc\ptc.h"
#include "syna.h"

PTC *ptc;
Palette pal;
Surface *vscreen;
int pitch;
int screenWidth, screenHeight;

#ifdef WIN32
HHOOK keyboard_hook;
HHOOK mouse_hook;

extern volatile char keyboard_buffer[128];
extern volatile int keyboard_head;
extern volatile int keyboard_tail;

extern HINSTANCE ghInstance;

extern volatile int my_mouseX;
extern volatile int my_mouseY;
extern volatile bool my_lbutton;
extern volatile bool my_rbutton;

HWND my_window;

extern volatile bool windowed;
//extern volatile bool altDn;
extern volatile bool switchModeWhenPossible;

/*
LRESULT CALLBACK KeyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
  if(code < 0)
  {
    return CallNextHookEx(keyboard_hook,code,wParam,lParam);
  }

  if(code == HC_ACTION)
  {
    unsigned char keycode = (unsigned char) wParam;

	if(keycode == 18)	// alt down
	{
		altDn = true;
	}
	else if(keycode == 116)
	{
		altDn = false;
	}
	else if((keycode == 13) && (altDn))
	{
		switchModeWhenPossible = true;
	}
	else
	{
		keyboard_buffer[keyboard_tail] = keycode;
		keyboard_tail++;
		if(keyboard_tail >= 128)
		{
		  keyboard_tail = 0;
		}
	}
  }
  
  return 0;
}
*/

bool my_kbhit()
{
  if(keyboard_tail != keyboard_head)
    return true;
  else
    return false;
}


unsigned char my_getch()
{
  unsigned char keycode;

  while(keyboard_tail == keyboard_head);	// nasty block!! Never call getch() if no char is available!
  
  keycode = keyboard_buffer[keyboard_head];
  keyboard_buffer[keyboard_head]=0;
  keyboard_head++;
  if(keyboard_head >= 128)
  {
    keyboard_head = 0;
  }

  return keycode;
}


LRESULT CALLBACK MouseHook(int code, WPARAM wParam, LPARAM lParam)
{
  if(code < 0)
  {
    return CallNextHookEx(mouse_hook,code,wParam,lParam);
  }

  if(code == HC_ACTION)
  {
    MOUSEHOOKSTRUCT *ms = (MOUSEHOOKSTRUCT*)lParam;

	if(ms->hwnd == my_window)
	{
      switch(wParam)
	  {
	    case WM_MOUSEMOVE:
			{
			  RECT r;
			  GetWindowRect(my_window,&r);

              if(windowed)
			  {
			    r.top+=GetSystemMetrics(SM_CYCAPTION);
				r.top+=GetSystemMetrics(SM_CYFRAME);
				r.bottom-=GetSystemMetrics(SM_CYFRAME);
				r.left+=GetSystemMetrics(SM_CXFRAME);
				r.right-=GetSystemMetrics(SM_CXFRAME);

				if((ms->pt.x < r.left) || (ms->pt.x > r.right)
				 || (ms->pt.y < r.top) || (ms->pt.y > r.bottom))
				{
				  break;
				}
			  }

			  my_mouseX = ms->pt.x - r.left;
              my_mouseY = ms->pt.y - r.top;

			  if(windowed)
			  {
				  // mouse coordinates are inside our window, so convert them from possible
				  // scaled window to the width synaesthesia is rendering to
				if(r.bottom-r.top != outHeight)
				{
				  my_mouseY = (my_mouseY*outHeight)/(r.bottom-r.top);
				}
				if(r.right-r.left != outWidth)
				{
				  my_mouseX = (my_mouseX*outWidth)/(r.right-r.left);
				}
			  }
			  
			  if(my_mouseX > outWidth)
				  my_mouseX = outWidth;
			  if(my_mouseX < 0)
				  my_mouseX = 0;
			  if(my_mouseY > outHeight)
				  my_mouseY = outHeight;
			  if(my_mouseY < 0)
				  my_mouseY = 0;
			}
			break;
        case WM_LBUTTONDOWN:
			{
			  my_lbutton = true;
			}
			break;
        case WM_LBUTTONUP:
			{
			  my_lbutton = false;
			}
			break;
        case WM_RBUTTONDOWN:
			{
			  my_rbutton = true;
			}
			break;
        case WM_RBUTTONUP:
			{
			  my_rbutton = false;
			}
			break;
      }
	}
  }

  return 0;
}

#endif



void modeInit(bool bWindowed, int xHint,int yHint,int widthHint,int heightHint)
{
        screenWidth = widthHint;
        screenHeight = heightHint;

	if(bWindowed)
	{
                ptc = new PTC("GDI",widthHint,heightHint,VIRTUAL8);
	}
	else
	{
                ptc = new PTC(widthHint,heightHint,VIRTUAL8);
	}

	if(!ptc->ok())
	{
		error("Could not initialise PTC");
	}

	// set title
	ptc->SetTitle("synaesthesia");

	// create main drawing surface
        vscreen = new Surface(*ptc,widthHint,heightHint,INDEX8);
	if (!vscreen->ok())
	{
		// failure
		ptc->Error("could not create surface");
		return;
	}

	outWidth = vscreen->GetWidth();
	outHeight = vscreen->GetHeight();
	pitch = vscreen->GetPitch();

	uint *data=(uint*)pal.Lock();
	if(!data)
	{
		error("Could not lock palette");
	}

	#define BOUND(x) ((x) > 255 ? 255 : (x))
	#define PEAKIFY(x) BOUND((x) - (x)*(255-(x))/255/2)
	unsigned int i;
	for(i=0;i<256;i++)
	{
		data[i] = RGB32(
						(PEAKIFY((i&15*16))),
						(PEAKIFY((i&15)*16+(i&15*16)/4)),
						(PEAKIFY((i&15)*16))
						); 
	}

	pal.Unlock();

	vscreen->SetPalette(pal);

	// set display palette if required
	FORMAT format=ptc->GetFormat();
	if (format.type==INDEXED && format.model!=GREYSCALE) ptc->SetPalette(pal);

#ifdef WIN32
	// next, set up the keyboard hook

	my_window = ptc->GetWindow();

	keyboard_head=0;
	keyboard_tail=0;
        memset((void*)keyboard_buffer,0,128);
        //keyboard_hook = SetWindowsHookEx(WH_KEYBOARD,(HOOKPROC)&KeyboardHook,ghInstance,NULL);

	my_mouseX = 0;
	my_mouseY = 0;
	my_lbutton = false;
	my_rbutton = false;
        //mouse_hook = SetWindowsHookEx(WH_MOUSE,(HOOKPROC)&MouseHook,ghInstance,NULL);

#endif
}


void screenInit(int xHint,int yHint,int widthHint,int heightHint) 
{
	windowed = true;
	modeInit(windowed,xHint,yHint,widthHint,heightHint);
}


void screenEnd() 
{
  //UnhookWindowsHookEx(keyboard_hook);
  //UnhookWindowsHookEx(mouse_hook);

  ptc->Close();
  delete vscreen;
  delete ptc;
}


int sizeUpdate() 
{
  return 0;
}


void sizeRequest(int width,int height) 
{
  if (width != 320 || height != 200)
    warning("resizing screen.");
}


void inputUpdate(int &mouseX,int &mouseY,int &mouseButtons,char &keyHit) 
{
  mouseX = my_mouseX;
  mouseY = my_mouseY;

  mouseButtons = my_lbutton || my_rbutton;

  if (!my_kbhit())
  {
    keyHit = 0;
  }
  else
  {
    keyHit = tolower(my_getch());
  }
}


void switchMode()
{
        //UnhookWindowsHookEx(keyboard_hook);
        //UnhookWindowsHookEx(mouse_hook);

	ptc->Close();
	delete vscreen;
	delete ptc;

	if(windowed)
	{
		windowed=false;
	}
	else
	{
		windowed=true;
	}
        modeInit(windowed, 0, 0, screenWidth, screenHeight);
}


void screenShow(void) 
{
  register unsigned long *ptr2 = (unsigned long*)output;
  unsigned long*ptr1 = (unsigned long*)vscreen->Lock();

  unsigned char* firstline = (unsigned char*) ptr1;

  if(!ptr1)
  {
    error("Couldn't lock screen buffer!");
    return;
  }


  // Asger Alstrup Nielsen's (alstrup@diku.dk)
  // optimized 32 bit screen loop
  // Oct 1998: Nils Desle: Modified to work with PTC's pitch field.
  // The pitch can be negative to cater for fast bottom-up virtual screens
  // with windows GDI.

  int x,y;
  
  for(y=0;y<screenHeight;y++)
  {
	ptr1 = (unsigned long*) firstline;
        for(x=0;x<(screenWidth/4);x++)
	{

      register unsigned int const r1 = *(ptr2++);
      register unsigned int const r2 = *(ptr2++);

      //Fade will continue even after value > 16
      //thus black pixel will be written when values just > 0
      //thus no need to write true black
      if(r1 || r2) 
	  {
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
	  } 
	  else
	  {
        ptr1++;
	  }  
    }

	firstline += pitch;
  }

  vscreen->Unlock();
  vscreen->Update();

  if(switchModeWhenPossible)
  {
	  switchMode();
	  switchModeWhenPossible = false;
  }
}


