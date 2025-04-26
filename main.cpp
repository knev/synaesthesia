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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include "syna.h"

//void setupIcons();

/* Config and globals =============================================== */

volatile sampleType *data;
unsigned char *output;
int outWidth, outHeight;

static SoundSource soundSource;

const int numRows = 4, rowHeight = 50, leftColWidth = 40, rowMaxWidth = 310,
          sliderBorder = 20, 
          sliderWidth = rowMaxWidth - leftColWidth - sliderBorder*2,
          uiWidth = 320, uiHeight = 200;
static int width[numRows] = 
  {leftColWidth, leftColWidth, leftColWidth, leftColWidth}; 
static int mouseX = 0, mouseY = 0, mouseButtons = 0, mouseClicked = 0;
static char keyHit = 0;
//static int oldOutWidth, oldOutHeight, isExpanded = 0, expandCountDown = 0;
static SymbolID state = NoCD;
static int track = 1, frames = 0, wasAction = 0;
static char **playList;
static int playListLength, playListPosition;
static int countDown = 0, hideCountDown = 0;
static double volume = 0.5, bright = 0.125;
static char hint[2] = "";
static int buttonWidth = 40;
static int stopTrans[]  = { Stop,  Play, Open, Exit, -1 }; 
static int playTrans[]  = { Play,  SkipBack, Pause, Stop, SkipFwd, Open, Exit, -1 }; 
static int pauseTrans[] = { Pause, SkipBack, Play, Stop, SkipFwd, Open, Exit, -1 }; 
static int openTrans[]  = { Open,  Stop, Play, Open, Exit, -1 }; /* Hard to diagnose being open */
static int lineTrans[]  = { Exit,  Exit, -1 }; 
static int noCDTrans[]  = { NoCD,  Open,  Exit, -1 }; 
static int *transitions[] = { stopTrans, pauseTrans, playTrans, openTrans,
                              noCDTrans, lineTrans };

static struct { int symbol; char key; } keyMapping[] = {
  { Stop, 's' }, { Play, 'p' }, { Open, 'e' }, { Exit, 'q' },
  { Pause, 'p' }, { SkipBack, '[' }, { SkipFwd, ']' }, { -1, 0 }
};

void actionDone() { wasAction = 1; countDown = 0; }
void setBrightness(double bright) 
  { brightFactor = int(brightness * bright * 8.0); } 

/* Rows:
  0 State
  1 Track
  2 Volume
  3 Brightness
*/

int main(int argc, char **argv) { 
  /* Load or create a config file */
  int windX=10, windY=30, windWidth=320, windHeight=200;
  char dspName[80] = "/dev/dsp";
  char mixerName[80] = "/dev/mixer";
  char cdromName[80] = "/dev/cdrom";

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
        sscanf(line,"dsp %s",dspName);
        sscanf(line,"mixer %s",mixerName);
        sscanf(line,"cdrom %s",cdromName);
      }
      fclose(f);
    } else if ((f = fopen(fileName,"wt"))) {
      fprintf(f,"# Synaesthesia config file\n\n"
              "# Window position and size under X:\n"
              "x 10\ny 30\nwidth 320\nheight 200\n\n"
              "# Brightness:\n"
              "brightness 0.125\n\n"
              "# Volume: (uncomment to set)\n"
              "#volume 1.0\n\n"
	      "# Devices (for computers with more than one soundcard)\n"
	      "dsp /dev/dsp\n"
	      "mixer /dev/mixer\n"
	      "cdrom /dev/cdrom\n");
      fclose(f); 
    }
    delete fileName;
  }
  

  if (argc == 1) {
    printf("\nSYNAESTHESIA v1.4\n\n"
           "Usage:\n\n"
           "  " PROGNAME " cd\n    - listen to a CD\n\n"
           "  " PROGNAME " line\n    - listen to line input\n\n"
           "  " PROGNAME " <track> <track> <track>...\n"
	   "    - play these CD tracks one after the other\n\n"
           "  <another program> |" PROGNAME " pipe <frequency> <sample divider>\n"
	   "    - send output of program to sound card as well as displaying it.\n"
	   "      (must be 16-bit stereo sound)\n"
	   "    - <sample divider> is a twiddle factor, make it just high enough\n"
	   "      to avoid pops and stutters. (This may take some experimentation)\n"
	   "    example: nice mpg123 -s file.mp3 |" PROGNAME " pipe 44100 4\n\n"
	   "  Moving the mouse will reveal a control-bar that may be used to\n"
	   "  control the CD and to exit the program.\n\n"
	   "Enjoy!\n"
	   );
    return 1;
  }

  int configPlayTrack = -1;
  int inFrequency = frequency, windowSize;

  playListLength = 0;
  
  if (strcmp(argv[1],"line") == 0) soundSource = SourceLine;
  else if (strcmp(argv[1],"cd") == 0) soundSource = SourceCD;
  else if (strcmp(argv[1],"pipe") == 0) {
    if (argc < 3 || sscanf(argv[2],"%d",&inFrequency) != 1)
      error("frequency not specified");
    if (argc < 4 || sscanf(argv[3],"%d",&windowSize) != 1)
      error("divider not specified");
    soundSource = SourcePipe;
  } else
    if (sscanf(argv[1],"%d",&configPlayTrack) != 1)
      error("comprehending user's bizzare requests");
    else {
      soundSource = SourceCD;
      playList = argv+1;
      playListPosition = 0;
      playListLength = argc-1;
    }

  if (soundSource == SourceCD)
    cdOpen(cdromName);
  else
    state = Exit;

  if (configPlayTrack != -1) {
  //  cdGetStatus(track, frames, state);
  //  cdPlay(cdGetTrackFrame(configPlayTrack));    
    cdStop();
  }

  openSound(soundSource, inFrequency, windowSize, dspName, mixerName); 
  
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
  windWidth  &= ~3;

  setBrightness(bright);

  screenInit(windX,windY,windWidth,windHeight);
  output = new unsigned char[outWidth*outHeight*2];

  coreInit();
  //setupIcons();

  time_t timer = time(NULL);
  
  int frames = 0;
  do {
    showOutput();
    if (-1 == coreGo())
      break;
    frames++;
  } while(!processUserInput());

  timer = time(NULL) - timer;
  
  delete output;

//  if (configPlayTrack != -1)
//    cdStop();    
  
  if (soundSource == SourceCD) 
    cdClose();

  closeSound();
  screenEnd();

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


char symbolToKey(int symbol) {
  int i;
  for(i=0;keyMapping[i].symbol != -1;i++)
    if (keyMapping[i].symbol == symbol)
      return keyMapping[i].key;
  return 0;
}

int keyToSymbol(char key) {
  int i;

  if (key == 'p') {
    if (state == Play)
      return Pause;
    else
      return Play;
  }
  
  for(i=0;keyMapping[i].symbol != -1;i++)
    if (keyMapping[i].key == key)
      return keyMapping[i].symbol;
  return -1;
}

int changeState(int transitionSymbol) {
  int retVal = 0;
  switch(transitionSymbol) {
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
  return retVal;
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
      retVal = changeState(*tr);
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
        mouseY > 0 && mouseY < rowHeight) {
      putSymbol(x,0,*tr,MaxBlue);
      hint[0] = symbolToKey(*tr); 
    }

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

void showSlider(int row, int sliderWidth, double *value, char keyLeft, char keyRight) {
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

    double newValue;
    newValue = double(mouseX-(rowMaxWidth-sliderWidth-sliderBorder))/sliderWidth;
    if (newValue < 0.0) newValue = 0.0;
    if (newValue > 1.0) newValue = 1.0;

    if (newValue < *value) hint[0] = keyLeft; else hint[0] = keyRight;
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
      if (retVal < 9)
        hint[0] = retVal+'0';
    } else if (i+1 == track) 
      box(x+step/4,rowHeight*5/4,step/2+1,rowHeight/2, 1, 255);
    else
      box(x+step/4,rowHeight*5/4,step/2+1,rowHeight/2, 1, 128);
  }

  int interval = (cdGetTrackFrame(track+1)-cdGetTrackFrame(track));
  if (interval != 0) { 
    double v = double(frames) / interval; 
    showSlider(1,sliderWidth/4,&v,  '{', '}');
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
  /* Doesn't work well, superfluous now that keyboard supported (pfh, 7/7/98)
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
  }*/
 
  if (hideCountDown > 0) {
    hideCountDown--;

    hint[0] = 0;

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
          case 2 : showSlider(2,sliderWidth,&bright, 'z', 'x'); break;
          case 3 : showSlider(3,sliderWidth,&volume, '-', '+'); break;
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
    putString(hint,mouseX+6,mouseY+7,0,128);
  }

  screenShow(); 
}

int processUserInput() {
  int ox = mouseX, oy = mouseY, ob = mouseButtons;
  int i, retVal = 0;
  
  inputUpdate(mouseX,mouseY,mouseButtons,keyHit);

  if (outWidth > leftColWidth && outHeight > rowHeight) { 
    if ((ox != mouseX || oy != mouseY) && 
        ox >= 0 && oy >= 0 && ox < outWidth && oy < outHeight)
      hideCountDown = 200;
  
    if (ob) 
      mouseClicked = 0;
    else
      mouseClicked = mouseButtons;
  
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
  } else hideCountDown = 0; 

  if (keyHit) {
    int symbol = keyToSymbol(keyHit);
    if (symbol != -1)
      retVal = changeState(symbol);
    else switch(keyHit) {
      case '1' :
      case '2' :
      case '3' :
      case '4' :
      case '5' :
      case '6' :
      case '7' :
      case '8' :
      case '9' : {
          int track = keyHit - '0';
	  if (track <= cdGetTrackCount()) {
	    cdPlay(cdGetTrackFrame(track));
	    actionDone();
	  }
	} 
	break;
      case 'z' :
      case 'x' :
        bright += (keyHit == 'z' ? -0.1 : 0.1);
	if (bright < 0.0) bright = 0.0;
	if (bright > 1.0) bright = 1.0;
	setBrightness(bright);
	break;
      case '-' :
      case '+' :
      case '=' :
        volume += (keyHit == '-' ? -0.1 : 0.1);
	if (volume < 0.0) volume = 0.0;
	if (volume > 1.0) volume = 1.0;
	setVolume(volume);
	break;
      case '{' :
      case '}' : {
	  int interval = (cdGetTrackFrame(track+1)-cdGetTrackFrame(track));
          if (interval) {
            double v = double(frames) / interval;
	    v += (keyHit == '{' ? -0.1 : 0.1);
	    if (v < 0.0) v = 0.0;
	    if (v > 1.0) v = 1.0;
            cdPlay(cdGetTrackFrame(track)+int(v*interval));
            actionDone();
	  }
	}
	break;
    }
  }
  
  if (countDown <= 0 && state != Exit /* Line input */) {
    SymbolID oldState = state;
    cdGetStatus(track, frames, state);

    if (!wasAction && 
        (oldState == Play || oldState == Open || oldState == NoCD) && 
        state == Stop) {
      if (playListLength == 0)
        cdPlay(cdGetTrackFrame(1));
      else {
        int track = atoi(playList[playListPosition]);
	playListPosition = (playListPosition+1) % playListLength;
	cdPlay(cdGetTrackFrame(track),cdGetTrackFrame(track+1));
      }
      actionDone(); 
    } else {
      wasAction = 0;
      countDown = 100;
    } 
  } else
    countDown--;
  
  if (sizeUpdate()) {
  //  isExpanded = 0;
  //  expandCountDown = 200;
  }

  return retVal;
}

