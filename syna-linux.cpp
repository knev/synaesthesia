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
    phar6@student.monash.edu.au
  or
    27 Bond St., Mt. Waverley, 3149, Melbourne, Australia
*/

/* Try commenting this #define out if compile fails. */
#define HAVE_UCDROM

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <linux/soundcard.h>
#include <linux/cdrom.h>

#ifdef HAVE_UCDROM
#include <linux/ucdrom.h>
#endif

#include <stdlib.h>
#include <vga.h>
#include <vgamouse.h>
#include <math.h>
#include <string.h>
#include "syna.h"

#include "alphabet.h"


void error(char *str) { 
  printf(PROGNAME ": Error %s\n",str); 
  exit(1);
}
void warning(char *str) { printf(PROGNAME ": Non-fatal error %s\n",str); }

void putIcon(int ox,int oy,int id,int channel) {
  int i,x,y, any;
  unsigned char *data = alphabet, *dest = output+ox*2+oy*640+channel;

  for(i=0;i<id;i++) {
    do for(x=0,any=0;x<32;x++,data++) any = (any || *data > 'a'+2);
    while(!any);
    do for(x=0,any=0;x<32;x++,data++) any = (any || *data > 'a'+2);
    while(any);
  }
  do for(x=0,any=0;x<32;x++,data++) any = (any || *data > 'a'+2);
  while(!any);
  data -= 32;

  any = 1;
  for(x=0;any;x++)
    for(y=0,any=0;y<32;y++,data++)
      if (*data > 'a'+2) {
        int o = x*2+y*640;
	if (dest[o] < (*data-'a')*16)
	  dest[o] = (unsigned char)(*data-'a')*16;
        any = 1;
      }
}
  
int cdDevice;

int cdOpen(void) {
  cdDevice = open("/dev/cdrom",O_RDONLY);
  return cdDevice != -1;
}

void cdClose(void) {
  close(cdDevice);
}

void cdConfigure(void) {
#ifdef HAVE_UCDROM
  if (!cdOpen()) return;
  int options = (CDO_AUTO_EJECT|CDO_AUTO_CLOSE);
  attemptNoDie(ioctl(cdDevice, CDROM_CLEAR_OPTIONS, options),
    "configuring CD behaviour");
  cdClose();
#endif
}

int cdGetTrackCount(void) {
  if (!cdOpen()) return 0;
  cdrom_tochdr cdTochdr;
  attempt(ioctl(cdDevice, CDROMREADTOCHDR, &cdTochdr),"reading cd track info");
  cdClose();
  return cdTochdr.cdth_trk1;
}

void cdPlay(int track) {
  cdrom_ti ti;
  ti.cdti_trk0 = track;
  ti.cdti_ind0 = 1;
  ti.cdti_trk1 = cdGetTrackCount();
  ti.cdti_ind1 = 99;
  if (!cdOpen()) return;
  while(ti.cdti_trk0 <= ti.cdti_trk1 &&
        -1 == ioctl(cdDevice, CDROMPLAYTRKIND, &ti))
    ti.cdti_trk0++; //Skip over data tracks
  cdClose();
}

void cdGetStatus(int &track, int &paused, int &playing) {
  if (!cdOpen()) { track = 1; paused = 0; playing = 0; return; }
  cdrom_subchnl subchnl;
  subchnl.cdsc_format = CDROM_MSF;
  attempt(ioctl(cdDevice, CDROMSUBCHNL, &subchnl),"querying the cd");
  track = subchnl.cdsc_trk;
  paused = (subchnl.cdsc_audiostatus == CDROM_AUDIO_PAUSED);
  playing = (subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY);
  if (!paused && !playing) track = 1;
  cdClose();
}

void cdStop(void) {
  if (!cdOpen()) return;
  attemptNoDie(ioctl(cdDevice, CDROMSTOP),"stopping cd");
  cdClose();
}
void cdPause(void) {
  if (!cdOpen()) return;
  attemptNoDie(ioctl(cdDevice, CDROMPAUSE),"pausing cd");
  cdClose();
}
void cdResume(void) {
  if (!cdOpen()) return;
  attemptNoDie(ioctl(cdDevice, CDROMRESUME),"resuming cd");
  cdClose();
}


int pipes[2];
int configUseCD = 1;
int device;
int fragsLost = 0, fragsGot = 0;
unsigned char *scr;

int main(int argv, char **argc) {
  if (argv == 1) {
    printf("\nSYNAESTHESIA v1.2\n\n"
           "Usage:\n\n"
           "  " PROGNAME " cd\n    - listen to a CD\n\n"
           "  " PROGNAME " line\n    - listen to line input\n\n"
           "  " PROGNAME " <track>\n    - listen to this CD track\n\n"
	   "  Moving the mouse will reveal a control-bar that may be used to\n"
	   "  control the CD and to exit the program.\n\n"
	   "Enjoy!\n"
	   );
    return 1;
  }

  int configPlayTrack = -1;
  if (strcmp(argc[1],"line") == 0) configUseCD = 0;
  else if (strcmp(argc[1],"cd") != 0)
    if (sscanf(argc[1],"%i",&configPlayTrack) != 1)
      error("comprehending user's bizzare requests");

  attempt(pipe(pipes),"opening pipe");
  
  fcntl(0,F_SETFL,O_NONBLOCK);

  int format, stereo, fragment, fqc;
  device = open("/dev/mixer",O_WRONLY);
  if (device == -1) 
    warning("opening /dev/mixer");
  else {
    int blah = (configUseCD ? SOUND_MASK_CD : SOUND_MASK_LINE + SOUND_MASK_MIC);
    attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_RECSRC,&blah),"writing to mixer");
    blah = 100*256+100;
    if (configUseCD) {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_CD,&blah),"writing to mixer");
    } else {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_LINE,&blah),"writing to mixer");
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_MIC,&blah),"writing to mixer");
    }
    close(device);
  }
  
  if (configUseCD)
    cdConfigure();

  if (configPlayTrack != -1)
    cdPlay(configPlayTrack);    
 
  if (fork() == 0) {
    close(pipes[0]);
    
    attempt(device = open("/dev/dsp",O_RDONLY),"opening /dev/dsp");
    
    //Probably not needed
    //attemptNoDie(ioctl(device,SNDCTL_DSP_RESET,0),"reseting dsp");
    format = AFMT_S16_LE;
    fqc = frequency;
    stereo = 1;
    fragment = 0x00020000 + (m+1); //2 fragments of size 2*(2^(m+1)) bytes
    //Was 0x00010000 + m; 
    
    attemptNoDie(ioctl(device,SNDCTL_DSP_SETFRAGMENT,&fragment),"setting fragment");
    attempt(ioctl(device,SNDCTL_DSP_SETFMT,&format),"setting format");
    if (format != AFMT_S16_LE) error("setting format (2)");
    attempt(ioctl(device,SNDCTL_DSP_STEREO,&stereo),"setting stereo");
    attemptNoDie(ioctl(device,SNDCTL_DSP_SPEED,&fqc),"setting frequency");
    

    short *data = new short[n*2];
   
    for(;;) {
      attempt(read(device,data,n*4),"reading from dsp");
      if (write(pipes[1],data,n*4) != n*4) break;
    }
    
    close(pipes[1]);
    delete[] data;
    return 0;
  }
  close(pipes[1]);

  attempt(vga_setmode(G320x200x256),"entering 320x200 graphics mode");
  scr = vga_getgraphmem();
 
  attemptNoDie(mouse_init("/dev/mouse",
    vga_getmousetype(),MOUSE_DEFAULTSAMPLERATE),"initializing mouse");
  mouse_setxrange(0,320-33);
  mouse_setyrange(0,200-33);
  mouse_setposition(160,34);

  go();
  
  mouse_close();
  vga_setmode(TEXT);
  
  close(pipes[0]);
  
  if (configPlayTrack != -1)
    cdStop();    

  if (fragsLost > fragsGot)
    printf("Fragments: %i lost as compared to %i processed.\n"
           "Frame rate: %.1f frames/second\n"
           "Your computer may not be fast enough to give a good frame rate.\n",
	   fragsLost,fragsGot, 
	   (double)(frequency)/n * fragsGot/(fragsLost+fragsGot));
  return 0;
}

void setPalette(int i,int r,int g,int b) {
  vga_setpalette(i,r,g,b);
}

void realizePalette(void) { }


int oldMouseX, oldMouseY, mouseX = 0, mouseY = 0, fadeCount = 0,
    statusCheckCountdown = 1000;
int oldButtons, buttons=0;
int track=1, trackCount=1, trackStep=1, symbol=-1;
int paused=0, playing=0, showTracks=0;
#define SYMBOLCOUNT 5
#define SYMBOLSPACING 40
#define SYMBOLOFFSET 60
int symbols[SYMBOLCOUNT] = {12, 10, 13, 11, 14}; 

int processUserInput(void) {
  if (configUseCD) {
    oldMouseX = mouseX;
    oldMouseY = mouseY;
    mouse_update();
    mouseX = mouse_getx();
    mouseY = mouse_gety();
    if (mouseX < 0) mouseX = 0; else if (mouseX > 320-33) mouseX = 320-33;
    if (mouseY < 0) mouseY = 0; else if (mouseY > 200-33) mouseY = 200-33;

    if (mouseX != oldMouseX || mouseY != oldMouseY) {
      symbol = (mouseX-SYMBOLOFFSET)/SYMBOLSPACING;
      if (mouseY > 32 || mouseX < SYMBOLOFFSET) symbol = -1;
      fadeCount = 200;

      if (showTracks) {
        if (mouseX > 40)
	  showTracks = 0;
      } else if (mouseX <= 40 && mouseY <= 32) {
          showTracks = 1;
	  trackCount = cdGetTrackCount();
	  if (trackCount)
	    trackStep = (200-65)/trackCount;
          else
	    showTracks = 0;
	}
    } else 
      if (playing && fadeCount > 0) fadeCount--;
  
    if (++statusCheckCountdown > (fadeCount ? 100 : 1000)) {
      statusCheckCountdown = 0;
      cdGetStatus(track,paused,playing);

      if (!paused && !playing) { //Loop when finished, auto play on start
	cdPlay(1);               //Allows you to leave it running
				 //unattended (eg as a demo)
	cdGetStatus(track,paused,playing);
        if (!playing) fadeCount = 10;
      }
    }
  
    oldButtons = buttons;
    buttons = mouse_getbutton();
    if (buttons && !oldButtons) {
      if (showTracks) {
        int which = (mouseY-32)/trackStep+1;
	if (which > 0 && which <= trackCount) {
	  cdPlay(which);
	  showTracks = 0;
	}
      }

      fadeCount = 20;
    
      cdGetStatus(track,paused,playing);
      switch(symbol) {
	case 0 :
	  cdPlay(track>1?track-1:1);
	  break;
	case 1 :
	  if (playing)
	    cdPlay(track);
	  else
	    if (paused)
	      cdResume();
	    else cdPlay(1);
	  break;
	case 2 :
	  cdPause();
	  break;
	case 3 :
	  cdPlay(track<1?1:track+1);
	  break;
	case 4 : return 1;
      }
      cdGetStatus(track,paused,playing);
    }
  }
  
  /* Stop when user presses enter */
  char temp;
  if (read(0,&temp,1) != -1) {
    return 1;
  }
  return 0;
}

void getNextFragment(void) {
  fcntl(pipes[0],F_SETFL,0);
  while(read(pipes[0],data,4*n) != 4*n);
  fcntl(pipes[0],F_SETFL,O_NONBLOCK);
  while(read(pipes[0],data,4*n) == 4*n) fragsLost ++;
  fragsGot++;
}

void showOutput(void) {
  if (fadeCount && configUseCD) {
    int i,x,y,numberColor=0,step,number=track;
    if (showTracks) {
      for(i=0;i<trackCount;i++)
        for(y=32+i*trackStep;y<32+(i*2+1)*trackStep/2+1;y++)
	  for(x=10;x<40;x++) {
	    int o = x*2+y*640,
	        c = 127+(int)(x+15+(sin(x/4.0+y/16.0)*5))%20*(128/20)+
	           +127+(int)(x+0.5+15+(sin((x+0.5)/4.0+y/16.0)*5))%20*(128/20) >>1;
	    if (mouseY > 32 && (mouseY-32)/trackStep == i) output[o+1] = c;
	    else {
	      output[o] = c;
	      if (track == i+1) output[o+1] = c;
	    }
	  }
      i = (mouseY-32)/trackStep+1;
      if (mouseY > 32 && i > 0 && i <= trackCount) {
        number = i;
        numberColor = 1;
      }
    }
    if (number >= 10) putIcon(0,0,number/10%10,numberColor);
    putIcon(20,0,number%10,numberColor);
    
    for(int i=0;i<SYMBOLCOUNT;i++)
      putIcon(SYMBOLOFFSET+i*SYMBOLSPACING,0,symbols[i],(symbol==i?1:0));
    if (playing)
      putIcon(SYMBOLOFFSET+1*SYMBOLSPACING,0,symbols[1],1);
    else
      putIcon(SYMBOLOFFSET+2*SYMBOLSPACING,0,symbols[2],1);
    putIcon(mouseX,mouseY,15,0);
    putIcon(mouseX,mouseY,16,1);
  }
  unsigned char *ptr2 = output, *ptr1 = scr;
  int i = 320*200;
  do {
    unsigned char v = (*(ptr2++)&15*16);
    *(ptr1++) = v|(*(ptr2++)>>4);
  } while(--i > 0);
}
