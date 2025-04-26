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
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <linux/soundcard.h>
#include <linux/cdrom.h>
#include <linux/ucdrom.h>
#include <time.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "syna.h"

static int cdDevice;

static int trackCount = 0, *trackFrame = 0;

void cdOpen(void) {
  attempt(cdDevice = open("/dev/cdrom",O_RDONLY),"talking to CD");
}

void cdClose(void) {
  close(cdDevice);

  delete[] trackFrame;
}

//void cdConfigure(void) {
//#ifdef HAVE_UCDROM
//  if (!cdOpen()) return;
//  int options = (CDO_AUTO_EJECT|CDO_AUTO_CLOSE);
//  attemptNoDie(ioctl(cdDevice, CDROM_CLEAR_OPTIONS, options),
//    "configuring CD behaviour");
//  cdClose();
//#endif
//}

void getTrackInfo(void) {
  delete trackFrame;
  trackFrame = 0;
  trackCount  = 0;

  cdrom_tochdr cdTochdr;
  if (-1 == ioctl(cdDevice, CDROMREADTOCHDR, &cdTochdr))
     return;
  trackCount = cdTochdr.cdth_trk1;

  int i;
  trackFrame = new int[trackCount+1];
  for(i=trackCount;i>=0;i--) {
    cdrom_tocentry cdTocentry;
    cdTocentry.cdte_format = CDROM_MSF;
    cdTocentry.cdte_track  = (i == trackCount ? CDROM_LEADOUT : i+1);
    if (-1 == ioctl(cdDevice, CDROMREADTOCENTRY, & cdTocentry) ||
        (cdTocentry.cdte_ctrl & CDROM_DATA_TRACK))
      trackFrame[i] = (i==trackCount?0:trackFrame[i+1]);
    else
      trackFrame[i] = cdTocentry.cdte_addr.msf.minute*60*CD_FRAMES+
                      cdTocentry.cdte_addr.msf.second*CD_FRAMES+
                      cdTocentry.cdte_addr.msf.frame;
  }
}

int cdGetTrackCount(void) {
  return trackCount;
}
int cdGetTrackFrame(int track) {
  if (track <= 1 || track > trackCount+1)
    return trackFrame[0];
  else
    return trackFrame[track-1]; 
}

void cdPlay(int frame) {
  cdrom_msf msf;
  if (frame < 1) frame = 1;
  msf.cdmsf_min0 = frame / (60*CD_FRAMES);
  msf.cdmsf_sec0 = frame / CD_FRAMES % 60;
  msf.cdmsf_frame0 = frame % CD_FRAMES;
  msf.cdmsf_min1 = trackFrame[trackCount] / (60*CD_FRAMES);
  msf.cdmsf_sec1 = trackFrame[trackCount] / CD_FRAMES % 60;
  msf.cdmsf_frame1 = trackFrame[trackCount] % CD_FRAMES;
  attemptNoDie(ioctl(cdDevice, CDROMPLAYMSF, &msf),"playing CD");
}

void cdGetStatus(int &track, int &frames, SymbolID &state) {
  cdrom_subchnl subchnl;
  subchnl.cdsc_format = CDROM_MSF;
  if (-1 == ioctl(cdDevice, CDROMSUBCHNL, &subchnl)) {
    track = 1; 
    frames  = 0;
    state = (state == Open ? Open : NoCD); /* ? */
    return;
  }
  track = subchnl.cdsc_trk;
  frames  = subchnl.cdsc_reladdr.msf.minute*60*CD_FRAMES+
            subchnl.cdsc_reladdr.msf.second*CD_FRAMES+
            subchnl.cdsc_reladdr.msf.frame; 
  
  SymbolID oldState = state;
  switch(subchnl.cdsc_audiostatus) {
    case CDROM_AUDIO_PAUSED    : state = Pause; break;
    case CDROM_AUDIO_PLAY      : state = Play; break;
    case CDROM_AUDIO_COMPLETED : state = Stop; break;
    case CDROM_AUDIO_NO_STATUS : state = Stop; break;
    default : state = NoCD; break;
  }

  if ((oldState == NoCD || oldState == Open) &&
      (state == Pause || state == Play || state == Stop)) {
    getTrackInfo();
  }
  
  if (state != Play && state != Pause) {
    track = 1;
    frames = 0;
  }
}

void cdStop(void) {
  //attemptNoDie(ioctl(cdDevice, CDROMSTOP),"stopping CD");
  ioctl(cdDevice, CDROMSTOP);
}
void cdPause(void) {
  attemptNoDie(ioctl(cdDevice, CDROMPAUSE),"pausing CD");
}
void cdResume(void) {
  attemptNoDie(ioctl(cdDevice, CDROMRESUME),"resuming CD");
}
void cdEject(void) {
  attemptNoDie(ioctl(cdDevice, CDROMEJECT),"ejecting CD");
}
void cdCloseTray(void) {
  attemptNoDie(ioctl(cdDevice, CDROMCLOSETRAY),"ejecting CD");
}

/* Sound Recording ================================================= */

static int configUseCD, device;

void setupMixer(double &loudness) {
  int device = open("/dev/mixer",O_WRONLY);
  if (device == -1) 
    warning("opening /dev/mixer");
  else {
    int blah = (configUseCD ? SOUND_MASK_CD : SOUND_MASK_LINE + SOUND_MASK_MIC);
    attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_RECSRC,&blah),"writing to mixer");
    
    if (configUseCD) {
      int volume;
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_CD,&volume),"writing to mixer");
      loudness = ((volume&0xff) + volume/256) / 200.0; 
    } else {
      int volume = 100*256 + 100;
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_LINE,&volume),"writing to mixer");
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_MIC,&volume),"writing to mixer");
      loudness = 1.0;
    }
    close(device);
  }
}

void setVolume(double loudness) {
  int device = open("/dev/mixer",O_WRONLY);
  if (device == -1) 
    warning("opening /dev/mixer");
  else {
    int scaledLoudness = int(loudness * 100.0);
    if (scaledLoudness < 0) scaledLoudness = 0;
    if (scaledLoudness > 100) scaledLoudness = 100;
    scaledLoudness = scaledLoudness*256 + scaledLoudness;
    if (configUseCD) {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_CD,&scaledLoudness),"writing to mixer");
    } else {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_LINE,&scaledLoudness),"writing to mixer");
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_MIC,&scaledLoudness),"writing to mixer");
    }
    close(device);
  }
} 

void openSound(int param) {
  configUseCD = param;
  
  int format, stereo, fragment, fqc;
 
  attempt(device = open("/dev/dsp",O_RDONLY),"opening /dev/dsp");
    
  //Probably not needed
  //attemptNoDie(ioctl(device,SNDCTL_DSP_RESET,0),"reseting dsp");
  format = AFMT_S16_LE;
  fqc = frequency;
  stereo = 1;
  fragment = 0x00020000 + (m-overlap+1); //2 fragments of size 2*(2^(m-overlap+1)) bytes
  //Was 0x00010000 + m; 
    
  attemptNoDie(ioctl(device,SNDCTL_DSP_SETFRAGMENT,&fragment),"setting fragment");
  attempt(ioctl(device,SNDCTL_DSP_SETFMT,&format),"setting format");
  if (format != AFMT_S16_LE) error("setting format (2)");
  attempt(ioctl(device,SNDCTL_DSP_STEREO,&stereo),"setting stereo");
  attemptNoDie(ioctl(device,SNDCTL_DSP_SPEED,&fqc),"setting frequency");
   
  data = new short[n*2];  
}

void closeSound() {
  delete data;
  close(device);
}

void getNextFragment(void) {
  if (recSize != n)
    memmove((char*)data,(char*)data+recSize*4,(n-recSize)*4);

  read(device,(char*)data+n*4-recSize*4, recSize*4);
}


/* Old sound stuff. Unneseccary. Device reads ahead instead of blocking. */
#if 0
static int semaphore; /* readyToRead, recordNow */
#define SEMREADY 0
#define SEMRECORD 1
static int sharedMem;
static int configUseCD;

void setupMixer(double &loudness) {
  int device = open("/dev/mixer",O_WRONLY);
  if (device == -1) 
    warning("opening /dev/mixer");
  else {
    int blah = (configUseCD ? SOUND_MASK_CD : SOUND_MASK_LINE + SOUND_MASK_MIC);
    attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_RECSRC,&blah),"writing to mixer");
    
    if (configUseCD) {
      int volume;
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_CD,&volume),"writing to mixer");
      loudness = ((volume&0xff) + volume/256) / 200.0; 
    } else {
      int volume = 100*256 + 100;
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_LINE,&volume),"writing to mixer");
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_MIC,&volume),"writing to mixer");
      loudness = 1.0;
    }
    close(device);
  }
}

void setVolume(double loudness) {
  int device = open("/dev/mixer",O_WRONLY);
  if (device == -1) 
    warning("opening /dev/mixer");
  else {
    int scaledLoudness = int(loudness * 100.0);
    if (scaledLoudness < 0) scaledLoudness = 0;
    if (scaledLoudness > 100) scaledLoudness = 100;
    scaledLoudness = scaledLoudness*256 + scaledLoudness;
    if (configUseCD) {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_CD,&scaledLoudness),"writing to mixer");
    } else {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_LINE,&scaledLoudness),"writing to mixer");
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_MIC,&scaledLoudness),"writing to mixer");
    }
    close(device);
  }
} 

void killIPC() {
  union semun zero;
  zero.val = 0;
  semctl(semaphore,0,IPC_RMID,zero);
  shmdt((char*)data);
  shmctl(sharedMem,IPC_RMID,0);
}

int mySemop(int semid,struct sembuf *sops,unsigned nsops) {
  int result;
  for(;;) {
    result = semop(semid,sops,nsops);
    if (result != -1 || errno != EINTR)
      return result;
  }
  return 0; /* Should never happen. */
}

void restartSound() {
  int device, format, stereo, fragment, fqc;
  int parentPID = getpid();
 
  attempt(semaphore = semget(IPC_PRIVATE,2,0700),"creating semaphore");
  
  union semun val;
  val.val = 0;
  semctl(semaphore, SEMREADY, SETVAL, val); 
  val.val = 1;
  semctl(semaphore, SEMRECORD, SETVAL, val); 
  atexit(killIPC);
 

  attempt(sharedMem = shmget(IPC_PRIVATE,n*4,0777),"creating shared memory");
  
  if (fork() == 0) {

    attempt(device = open("/dev/dsp",O_RDONLY),"opening /dev/dsp");
    
    //Probably not needed
    //attemptNoDie(ioctl(device,SNDCTL_DSP_RESET,0),"reseting dsp");
    format = AFMT_S16_LE;
    fqc = frequency;
    stereo = 1;
    fragment = 0x00020000 + (m-overlap+1); //2 fragments of size 2*(2^(m-overlap+1)) bytes
    //Was 0x00010000 + m; 
    
    attemptNoDie(ioctl(device,SNDCTL_DSP_SETFRAGMENT,&fragment),"setting fragment");
    attempt(ioctl(device,SNDCTL_DSP_SETFMT,&format),"setting format");
    if (format != AFMT_S16_LE) error("setting format (2)");
    attempt(ioctl(device,SNDCTL_DSP_STEREO,&stereo),"setting stereo");
    attemptNoDie(ioctl(device,SNDCTL_DSP_SPEED,&fqc),"setting frequency");
    
    attempt(data = (short*)shmat(sharedMem,0,0),"attaching shared memory");
    
    unsigned char tempBuf[n*4]; 
    
    for(;;) {
      struct sembuf wait = { SEMRECORD, -1, 0 };
      if (-1 == mySemop(semaphore, &wait, 1)) {
        break; 
      }

      if (recSize != n)
        memmove((char*)data,(char*)data+recSize*4,(n-recSize)*4);

      read(device,(char*)data+n*4-recSize*4, recSize*4);
      
      struct sembuf ready = { SEMREADY, 1, 0 };
      if (-1 == mySemop(semaphore, &ready, 1)) { 
        break; 
      }
    }
    
    exit(0);
  }

  attempt(data = (short*)shmat(sharedMem,0,0),"attaching shared memory");
}

void forkOffRecorder(int param) {
  configUseCD = param;
  restartSound();
}

void getNextFragment(void) {
  struct sembuf ready = { SEMREADY, -1, 0 };
  attempt(mySemop(semaphore, &ready, 1),"talking to sound process"); 
  
  /*{
    sleep(3); //Give time for sound process to die
    restartSound();
    attempt(mySemop(semaphore, &ready, 1),"talking to sound process"); 
  }*/
}

void doneWithFragment(void) {
  struct sembuf record = { SEMRECORD, 1, 0 };
  mySemop(semaphore, &record, 1); 
}

#endif
