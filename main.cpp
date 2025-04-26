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
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include "syna.h"
#include "alphabet.h"

//void setupIcons();

/* Config and globals =============================================== */

volatile short *data;
unsigned char *output;
int outWidth, outHeight;

static int configUseCD = 1;

const int numRows = 4, rowHeight = 50, leftColWidth = 40, rowMaxWidth = 310,
          sliderBorder = 20, sliderWidth = rowMaxWidth - leftColWidth - sliderBorder*2,
          numberSpacing = 15,
          uiWidth = 320, uiHeight = 200;
static int width[numRows] = 
  {leftColWidth, leftColWidth, leftColWidth, leftColWidth}; 
static int mouseX = 0, mouseY = 0, mouseButtons = 0, mouseClicked = 0;
static int oldOutWidth, oldOutHeight, isExpanded = 0, expandCountDown = 0;
static SymbolID state = NoCD;
static int track = 1, frames = 0, wasAction = 0;
static int countDown = 0, hideCountDown = 0;
static double volume = 0.5, bright = 0.125;
static int buttonWidth = 40;
static int stopTrans[]  = { Stop,  Play, Open, Exit, -1 }; 
static int playTrans[]  = { Play,  SkipBack, Pause, Stop, SkipFwd, Open, Exit, -1 }; 
static int pauseTrans[] = { Pause, SkipBack, Play, Stop, SkipFwd, Open, Exit, -1 }; 
static int openTrans[]  = { Open,  Stop, Play, Open, Exit, -1 }; /* Hard to diagnose being open */
static int lineTrans[]  = { Exit,  Exit, -1 }; 
static int noCDTrans[]  = { NoCD,  Open,  Exit, -1 }; 
static int *transitions[] = { stopTrans, pauseTrans, playTrans, openTrans,
                              noCDTrans, lineTrans };
void actionDone() { wasAction = 1; countDown = 0; };
void setBrightness(double bright) 
  { brightFactor = int(brightness * bright * 8.0); } 

/* Rows:
  0 State
  1 Track
  2 Volume
  3 Brightness
*/

int main(int argv, char **argc) { 
  /* Load or create a config file */
  int windX=10, windY=30, windWidth=320, windHeight=200;
  bool configFileVolume=false;
  struct passwd *passWord = getpwuid(getuid());
  if (passWord != 0) {
    char *fileName = new char[strlen(passWord->pw_dir) + 20];
    strcpy(fileName,passWord->pw_dir);
    strcat(fileName,"/.synaesthesia");
    FILE *f = fopen(fileName,"rt");
    if (f) {
      char line[80];
      while(0 != fgets(line,80,f)) {
        sscanf(line,"x %d",&windX);
        sscanf(line,"y %d",&windY);
        sscanf(line,"width %d",&windWidth);
        sscanf(line,"height %d",&windHeight);
        sscanf(line,"brightness %lf",&bright);
        if (1==sscanf(line,"volume %lf",&volume))
          configFileVolume = true;
      }
      fclose(f);
    } else if (f = fopen(fileName,"wt")) {
      fprintf(f,"# Synaesthesia config file\n\n"
              "# Window position and size under X:\n"
              "x 10\ny 30\nwidth 320\nheight 200\n\n"
              "# Brightness:\n"
              "brightness 0.125\n\n"
              "# Volume: (uncomment to set)\n"
              "#volume 1.0\n");
      fclose(f); 
    }
    delete fileName;
  }
  

  if (argv == 1) {
    printf("\nSYNAESTHESIA v1.3\n\n"
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
  
//Can't run in background under X with this (pfh, 31/12/97)
#ifdef PROFILE
  fcntl(0,F_SETFL,O_NONBLOCK);
#endif


  if (configUseCD)
    cdOpen();
  else
    state = Exit;

  if (configPlayTrack != -1) {
    cdGetStatus(track, frames, state);
    cdPlay(cdGetTrackFrame(configPlayTrack));    
  }

  openSound(configUseCD); 
  
  double dummy;

  if (configFileVolume) {
    setupMixer(dummy);
    setVolume(volume);
  } else 
    setupMixer(volume);

  if (volume > 0.0) {
    bright /= volume;
  }
 
  if (bright > 1.0)
    bright = 1.0;
  else if (bright < 0.0)
    bright = 0.0;
  if (volume > 1.0)
    volume = 1.0;
  else if (volume < 0.0)
    volume = 0.0;
  if (windWidth < 1)
    windWidth = 320;
  if (windHeight < 1)
    windHeight = 200;

  setBrightness(bright);

  screenInit(windX,windY,windWidth,windHeight);
  output = new unsigned char[outWidth*outHeight*2];

  coreInit();
  //setupIcons();

  time_t timer = time(NULL);
  
  int frames = 0;
  do {
    showOutput();
    coreGo();
    frames++;
  } while(!processUserInput());

  timer = time(NULL) - timer;
  
  delete output;

//  if (configPlayTrack != -1)
//    cdStop();    
  
  if (configUseCD) 
    cdClose();

  closeSound();

  if (timer > 10)
    printf("Frames per second: %f\n", double(frames) / timer);
  
  return 0;
}

void error(char *str) { 
  printf(PROGNAME ": Error %s\n",str); 
  exit(1);
}
void warning(char *str) { printf(PROGNAME ": Possible error %s\n",str); }

void box(int x,int y,int width,int height,int color,int value) {
  unsigned char *ptr = output + color + x*2 + y*outWidth*2;
  int i,j;
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x+width > outWidth)
    width = outWidth-x;
  if (y+height > outHeight)
    height = outHeight-y;

  if (width <= 0 || height <= 0) return;

  for(j=0;j<height;j++,ptr += (outWidth-width)*2)
    for(i=0;i<width;i++,ptr+=2)
      if (*ptr < value)
        *ptr = value;
}


int processSymbols() {
  int **trIter = transitions;
  while(trIter[0][0] != state) trIter++;
  int *tr = trIter[0]+1;
  int x = leftColWidth + buttonWidth / 2;

  int retVal = 0;
  while(*tr > 0) {
    if (mouseX > x-buttonWidth/2 && mouseX < x+buttonWidth/2 &&
        mouseY > 0 && mouseY < rowHeight && mouseClicked) {
      switch(*tr) {
        case Play  :
          if (state == Open) {
            cdCloseTray();
            state = NoCD;
            cdGetStatus(track, frames, state);
          } 
          if (state == Pause) cdResume(); else cdPlay(cdGetTrackFrame(1)); 
          break;
        case Pause : 
          cdPause(); break;
        case Stop  : 
          if (state == Open) {
            cdCloseTray();
            state = NoCD;
          } 
          cdStop(); 
          break;
        case Open  : 
          cdEject(); 
          state = Open;
          break;
        case SkipBack :
          cdPlay(cdGetTrackFrame(track-1));
          break;
        case SkipFwd :
          cdPlay(cdGetTrackFrame(track+1));
          break;
        case Exit  : 
          retVal = 1; break;
      }
      actionDone();
      //width[0] = leftColWidth;
    }
    tr++;
    x += buttonWidth;
  } 

  return retVal;
}

void showSymbols() {
  int **trIter = transitions;
  while(trIter[0][0] != state) trIter++;
  int *tr = trIter[0]+1;
  int x = leftColWidth + buttonWidth / 2;
  while(*tr > 0) {
    putSymbol(x,0,*tr,HalfRed);

    if (mouseX > x-buttonWidth/2 && mouseX < x+buttonWidth/2 &&
        mouseY > 0 && mouseY < rowHeight)
      putSymbol(x,0,*tr,MaxBlue);

    tr++;
    x += buttonWidth;
  } 
}

int processSlider(int row, int sliderWidth, double *value) {
  if (mouseY >= row*rowHeight && mouseY < (row+1)*rowHeight &&
      mouseX > rowMaxWidth-sliderWidth-sliderBorder*2
       && mouseX < rowMaxWidth && mouseButtons) {
    *value = double(mouseX-(rowMaxWidth-sliderWidth-sliderBorder))/sliderWidth;
    if (*value < 0.0) *value = 0.0;
    if (*value > 1.0) *value = 1.0;
    return 1;
  }
  return 0;
}
void showSlider(int row, int sliderWidth, double *value) {
  box(rowMaxWidth-sliderBorder-sliderWidth, row*rowHeight+rowHeight/2, 
      sliderWidth, 2, 1, 128);
  if (mouseY >= row*rowHeight && mouseY < (row+1)*rowHeight &&
      mouseX > rowMaxWidth-sliderBorder*2-sliderWidth
       && mouseX < rowMaxWidth) {
    int x = mouseX;
    if (x < rowMaxWidth-sliderBorder-sliderWidth)
      x = rowMaxWidth-sliderBorder-sliderWidth;
    if (x > rowMaxWidth-sliderBorder)
      x = rowMaxWidth-sliderBorder;
    putSymbol(x,row*rowHeight-2,Selector,HalfBlue);
  }

  putSymbol(int(rowMaxWidth-sliderBorder-sliderWidth+*value*sliderWidth),row*rowHeight-2,
            Slider,MaxRed);
}


int showTracks() {
  if (state == Exit || state == NoCD || state == Open) return -1;

  int count = cdGetTrackCount();
  int i, x, retVal = -1,
      step = (rowMaxWidth - leftColWidth - sliderBorder*2 - sliderWidth/4) / count;
  for(i=0;i<count;i++) {
    x = leftColWidth+step*i;
    if (mouseX >= x && mouseX < x+step &&
        mouseY >= rowHeight && mouseY < rowHeight*2) {   
      box(x+step/4,rowHeight*5/4,step/2+1,rowHeight/2, 0, 255);
      retVal = i+1;
    } else if (i+1 == track)
      box(x+step/4,rowHeight*5/4,step/2+1,rowHeight/2, 1, 255);
    else
      box(x+step/4,rowHeight*5/4,step/2+1,rowHeight/2, 1, 128);
  }

  int interval = (cdGetTrackFrame(track+1)-cdGetTrackFrame(track));
  if (interval != 0) { 
    double v = double(frames) / interval; 
    showSlider(1,sliderWidth/4,&v);
  }
  return retVal;
}

void processTracks() {
  if (state == Exit || state == NoCD || state == Open) return;

  int count = cdGetTrackCount();
  int i, x,
      step = (rowMaxWidth - leftColWidth - sliderBorder*2 - sliderWidth/4) / count;
  for(i=0;i<count;i++) {
    x = leftColWidth+step*i;
    if (mouseX >= x && mouseX < x+step &&
        mouseY >= rowHeight && mouseY < rowHeight*2 &&
        mouseButtons) {
      cdPlay(cdGetTrackFrame(i+1));
      actionDone();
    }    
  }

  int interval = (cdGetTrackFrame(track+1)-cdGetTrackFrame(track));
  if (interval == 0) return;
  double v = double(frames) / interval; 
  if (processSlider(1,sliderWidth/4,&v)) {
    cdPlay(cdGetTrackFrame(track)+
      int(v*interval));
    actionDone();
  }
}

void showOutput() {
  int inside = 
   (mouseX >= 0 && mouseY >= 0 && mouseX < outWidth && mouseY < outHeight);
  if (expandCountDown > 0 && 
     ((inside && !isExpanded) || (!inside && isExpanded)))
    expandCountDown --;
  else
    expandCountDown = 50;
    
  if (expandCountDown == 0) {
    if (isExpanded)
      sizeRequest(oldOutWidth,oldOutHeight);
    else {
      oldOutWidth = outWidth;
      oldOutHeight = outHeight;
      sizeRequest((outWidth < uiWidth ? uiWidth : outWidth), 
                  (outHeight < uiHeight ? uiHeight : outHeight));
    }
    isExpanded = !isExpanded;
    sizeUpdate();
  }
 
  if (hideCountDown > 0) {
    hideCountDown--;

    int i, trackNum = -1;
    for(i=0;i<numRows;i++) {
      box(0,rowHeight*i+1,width[i],rowHeight-2,1,92);
      box(0,rowHeight*i,width[i],1,1,64);
      box(0,rowHeight*(i+1)-1,width[i],1,1,64);
      putSymbol(width[i],rowHeight*i,Handle,MaxRed); 
      if (width[i] == rowMaxWidth)
        switch(i) {
          case 0 : showSymbols(); break;
          case 1 : trackNum = showTracks(); break;
          case 2 : showSlider(2,sliderWidth,&bright); break;
          case 3 : showSlider(3,sliderWidth,&volume); break;
        }
    }
    putSymbol(leftColWidth/2,rowHeight*0,state,HalfBlue);
    putSymbol(leftColWidth/2,rowHeight*0,state,MaxRed);
    putSymbol(leftColWidth/2,rowHeight*2,Speaker,HalfBlue);
    putSymbol(leftColWidth/2,rowHeight*2,Speaker,MaxRed);
    putSymbol(leftColWidth/2,rowHeight*3,Bulb,HalfBlue);
    putSymbol(leftColWidth/2,rowHeight*3,Bulb,MaxRed);

    if (trackNum > 0) {
      putSymbol(leftColWidth/4,rowHeight*1,Zero + trackNum/10%10, MaxBlue);
      putSymbol(leftColWidth*3/4,rowHeight*1,Zero + trackNum%10, MaxBlue);
    } else if (state == Play || state == Pause) {
      putSymbol(leftColWidth/4,rowHeight*1,Zero + track/10%10, HalfBlue);
      putSymbol(leftColWidth/4,rowHeight*1,Zero + track/10%10, MaxRed);
      putSymbol(leftColWidth*3/4,rowHeight*1,Zero + track%10, HalfBlue);
      putSymbol(leftColWidth*3/4,rowHeight*1,Zero + track%10, MaxRed);
    } 

    putSymbol(mouseX+12,mouseY,Pointer,MaxBlue);
    putSymbol(mouseX+12,mouseY,Pointer,HalfRed);
  }
 
  screenShow(); 
}

int processUserInput() {
  int ox = mouseX, oy = mouseY;
  mouseUpdate();
  mouseX = mouseGetX();
  mouseY = mouseGetY();

  if ((ox != mouseX || oy != mouseY) && 
      ox >= 0 && oy >= 0 && ox < outWidth && oy < outHeight)
    hideCountDown = 200;

  if (mouseButtons) {
    mouseClicked = 0;
    mouseButtons = mouseGetButtons();
  } else
    mouseClicked = mouseButtons = mouseGetButtons();

  int i, retVal = 0;
  for(i=0;i<numRows;i++) 
    if (mouseY >= rowHeight*i && mouseY < rowHeight*(i+1)
        && mouseX < width[i] && mouseX > 0 && mouseX < outWidth && mouseY < outHeight) {
      width[i] += 30;
      hideCountDown = 200;
      if (width[i] > rowMaxWidth)
        width[i] = rowMaxWidth;
      if (width[i] == rowMaxWidth)
        switch(i) {
          case 0 : retVal = processSymbols(); break;
          case 1 : processTracks(); break; 
          case 2 : 
            if (processSlider(2,sliderWidth,&bright))
              setBrightness(bright);
            break;
          case 3 : 
            if (processSlider(3,sliderWidth,&volume))
              setVolume(volume);
            break;
        }
    } else {
      width[i] -= 60;
      if (width[i] < leftColWidth)
        width[i] = leftColWidth;
    } 
  
  if (countDown <= 0 && state != Exit /* Line input */) {
    SymbolID oldState = state;
    cdGetStatus(track, frames, state);

    if (!wasAction && 
        (oldState == Play || oldState == Open || oldState == NoCD) && 
        state == Stop) {
      cdPlay(cdGetTrackFrame(1));
      actionDone(); 
    } else {
      wasAction = 0;
      countDown = 100;
    } 
  } else
    countDown--;
  
  if (sizeUpdate()) {
    isExpanded = 0;
    expandCountDown = 200;
  }

  return retVal;
}

