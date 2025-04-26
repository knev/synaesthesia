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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#ifndef __FreeBSD__
#include <linux/soundcard.h>
#include <linux/cdrom.h>
//#include <linux/ucdrom.h>
#else
#include <machine/soundcard.h>
#include <sys/cdio.h>
#define CDROM_LEADOUT 0xAA
#define CD_FRAMES 75 /* frames per second */
#define CDROM_DATA_TRACK 0x4
#endif
#include <time.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "syna.h"

static int cdDevice;

static int trackCount = 0, *trackFrame = 0;

void cdOpen(char *cdromName) {
  attempt(cdDevice = open(cdromName,O_RDONLY),"talking to CD device");
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

#ifndef __FreeBSD__
  cdrom_tochdr cdTochdr;
  if (-1 == ioctl(cdDevice, CDROMREADTOCHDR, &cdTochdr))
#else
  ioc_toc_header cdTochdr;
  if (-1 == ioctl(cdDevice, CDIOREADTOCHEADER, (char *)&cdTochdr))
#endif
     return;
#ifndef __FreeBSD__
  trackCount = cdTochdr.cdth_trk1;
#else
  trackCount = cdTochdr.ending_track - cdTochdr.starting_track + 1;
#endif

  int i;
  trackFrame = new int[trackCount+1];
  for(i=trackCount;i>=0;i--) {
#ifndef __FreeBSD__
    cdrom_tocentry cdTocentry;
    cdTocentry.cdte_format = CDROM_MSF;
    cdTocentry.cdte_track  = (i == trackCount ? CDROM_LEADOUT : i+1);
#else
    cd_toc_entry cdTocentry;
    struct ioc_read_toc_entry t;
    t.address_format = CD_MSF_FORMAT;
    t.starting_track = (i == trackCount ? CDROM_LEADOUT : i+1);
    t.data_len = sizeof(struct cd_toc_entry);
    t.data = &cdTocentry;
#endif

    //Bug fix: thanks to Ben Gertzfield  (9/7/98)
    //Leadout track is sometimes reported as data.
    //Added check for this.
#ifndef __FreeBSD__
    if (-1 == ioctl(cdDevice, CDROMREADTOCENTRY, & cdTocentry) ||
        (i != trackCount && (cdTocentry.cdte_ctrl & CDROM_DATA_TRACK)))
      trackFrame[i] = (i==trackCount?0:trackFrame[i+1]);
    else
      trackFrame[i] = cdTocentry.cdte_addr.msf.minute*60*CD_FRAMES+
                      cdTocentry.cdte_addr.msf.second*CD_FRAMES+
                      cdTocentry.cdte_addr.msf.frame;
#else
    if ((ioctl(cdDevice, CDIOREADTOCENTRYS, (char *) &t) == -1) ||
     (i != trackCount && (cdTocentry.control & CDROM_DATA_TRACK)))
	     trackFrame[i] = (i==trackCount?0:trackFrame[i+1]);
    else
	trackFrame[i] = cdTocentry.addr.msf.minute*60*CD_FRAMES+
			    cdTocentry.addr.msf.second*CD_FRAMES+
			    cdTocentry.addr.msf.frame;
#endif
  }
}

int cdGetTrackCount(void) {
  return trackCount;
}
int cdGetTrackFrame(int track) {
  if (!trackFrame)
    return 0; 
  else if (track <= 1 || track > trackCount+1)
    return trackFrame[0];
  else
    return trackFrame[track-1]; 
}

void cdPlay(int frame, int endFrame) {
#ifndef __FreeBSD__
  cdrom_msf msf;
#else
  struct ioc_play_msf msf;
#endif
  if (frame < 1) frame = 1;
  if (endFrame < 1) endFrame = trackFrame[trackCount];

  //Some CDs can't change tracks unless not playing.
  // (Sybren Stuvel)
  cdStop();
  
#ifndef __FreeBSD__
  msf.cdmsf_min0 = frame / (60*CD_FRAMES);
  msf.cdmsf_sec0 = frame / CD_FRAMES % 60;
  msf.cdmsf_frame0 = frame % CD_FRAMES;
#else
  msf.start_m = frame / (60*CD_FRAMES);
  msf.start_s = frame / CD_FRAMES % 60;
  msf.start_f = frame % CD_FRAMES;
#endif

  //Bug fix: thanks to Martin Mitchell
  //An out by one error that affects some CD players. 
  //Have to use endFrame-1 rather than endFrame (9/7/98)
#ifndef __FreeBSD__
  msf.cdmsf_min1 = (endFrame-1) / (60*CD_FRAMES);
  msf.cdmsf_sec1 = (endFrame-1) / CD_FRAMES % 60;
  msf.cdmsf_frame1 = (endFrame-1) % CD_FRAMES;
  attemptNoDie(ioctl(cdDevice, CDROMPLAYMSF, &msf),"playing CD");
#else
  msf.end_m = (endFrame-1) / (60*CD_FRAMES);
  msf.end_s = (endFrame-1) / CD_FRAMES % 60;
  msf.end_f = (endFrame-1) % CD_FRAMES;
  attemptNoDie(ioctl(cdDevice, CDIOCPLAYMSF, (char *) &msf),"playing CD");
#endif
}

void cdGetStatus(int &track, int &frames, SymbolID &state) {
#ifndef __FreeBSD__
  cdrom_subchnl subchnl;
  subchnl.cdsc_format = CDROM_MSF;
  if (-1 == ioctl(cdDevice, CDROMSUBCHNL, &subchnl)) {
#else
  ioc_read_subchannel subchnl;
  struct cd_sub_channel_info info;

  subchnl.data = &info;
  subchnl.data_len = sizeof (info);
  subchnl.address_format = CD_MSF_FORMAT;
  subchnl.data_format = CD_CURRENT_POSITION;
  
  if (-1 == ioctl(cdDevice, CDIOCREADSUBCHANNEL, (char *) &subchnl)) {
#endif
    track = 1; 
    frames  = 0;
    state = (state == Open ? Open : NoCD); /* ? */
    return;
  }
#ifndef __FreeBSD__
  track = subchnl.cdsc_trk;
  frames  = subchnl.cdsc_reladdr.msf.minute*60*CD_FRAMES+
            subchnl.cdsc_reladdr.msf.second*CD_FRAMES+
            subchnl.cdsc_reladdr.msf.frame; 
#else
  track = subchnl.data->what.position.track_number;
  frames  = subchnl.data->what.position.reladdr.msf.minute*60*CD_FRAMES+
		subchnl.data->what.position.reladdr.msf.second*CD_FRAMES+
		subchnl.data->what.position.reladdr.msf.frame; 
#endif
  
  SymbolID oldState = state;
#ifndef __FreeBSD__
  switch(subchnl.cdsc_audiostatus) {
    case CDROM_AUDIO_PAUSED    : state = Pause; break;
    case CDROM_AUDIO_PLAY      : state = Play; break;
    case CDROM_AUDIO_COMPLETED : state = Stop; break;
    case CDROM_AUDIO_NO_STATUS : state = Stop; break;
    case CDROM_AUDIO_INVALID   : state = Stop; break;
#else
  switch(subchnl.data->header.audio_status) {
    case CD_AS_PLAY_PAUSED	     : state = Pause; break;
    case CD_AS_PLAY_IN_PROGRESS    : state = Play; break;
    case CD_AS_PLAY_COMPLETED 	     : state = Stop; break;
    case CD_AS_NO_STATUS	     : state = Stop; break;
    case CD_AS_AUDIO_INVALID       : state = Stop; break;
#endif
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
#ifndef __FreeBSD__
  ioctl(cdDevice, CDROMSTOP);
#else
  ioctl(cdDevice, CDIOCSTOP);
#endif
}
void cdPause(void) {
#ifndef __FreeBSD__
  attemptNoDie(ioctl(cdDevice, CDROMPAUSE),"pausing CD");
#else
  attemptNoDie(ioctl(cdDevice, CDIOCPAUSE),"pausing CD");
#endif
}
void cdResume(void) {
#ifndef __FreeBSD__
  attemptNoDie(ioctl(cdDevice, CDROMRESUME),"resuming CD");
#else
  attemptNoDie(ioctl(cdDevice, CDIOCRESUME),"resuming CD");
#endif
}
void cdEject(void) {
#ifndef __FreeBSD__
  attemptNoDie(ioctl(cdDevice, CDROMEJECT),"ejecting CD");
#else
  attemptNoDie(ioctl(cdDevice, CDIOCEJECT),"ejecting CD");
#endif
}
void cdCloseTray(void) {
#ifndef __FreeBSD__
  attemptNoDie(ioctl(cdDevice, CDROMCLOSETRAY),"ejecting CD");
#else
  attemptNoDie(ioctl(cdDevice, CDIOCCLOSE),"ejecting CD");
#endif
}

/* Sound Recording ================================================= */

static SoundSource source;
static int inFrequency, downFactor, windowSize, pipeIn, device;
static short *dataIn;
static char *mixer;

void setupMixer(double &loudness) {
  int device = open(mixer,O_WRONLY);
  if (device == -1) 
    warning("opening mixer device");
  else {
    if (source != SourcePipe) {
      int blah = (source == SourceCD ? SOUND_MASK_CD : SOUND_MASK_LINE + SOUND_MASK_MIC);
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_RECSRC,&blah),"writing to mixer");
    }

    if (source == SourceCD) {
      int volume;
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_CD,&volume),"writing to mixer");
      loudness = ((volume&0xff) + volume/256) / 200.0; 
    } else if (source == SourceLine) {
      int volume = 100*256 + 100;
      attemptNoDie(ioctl(device,SOUND_MIXER_READ_LINE,&volume),"writing to mixer");
      //attemptNoDie(ioctl(device,SOUND_MIXER_READ_MIC,&volume),"writing to mixer");
      loudness = ((volume&0xff) + volume/256) / 200.0; 
    } else 
      loudness = 1.0;
    close(device);
  }
}

void setVolume(double loudness) {
  int device = open(mixer,O_WRONLY);
  if (device == -1) 
    warning("opening mixer device");
  else {
    int scaledLoudness = int(loudness * 100.0);
    if (scaledLoudness < 0) scaledLoudness = 0;
    if (scaledLoudness > 100) scaledLoudness = 100;
    scaledLoudness = scaledLoudness*256 + scaledLoudness;
    if (source == SourceCD) {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_CD,&scaledLoudness),"writing to mixer");
    } else if (source == SourceLine) {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_LINE,&scaledLoudness),"writing to mixer");
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_MIC,&scaledLoudness),"writing to mixer");
    } else {
      attemptNoDie(ioctl(device,SOUND_MIXER_WRITE_PCM,&scaledLoudness),"writing to mixer");
    }
    close(device);
  }
} 

void openSound(SoundSource source, int inFrequency, int windowSize, char *dspName, 
               char *mixerName) {
  ::source = source;
  ::inFrequency = inFrequency; 
  ::windowSize = windowSize;
  mixer = mixerName;
  downFactor = inFrequency / frequency; 
  if (downFactor <= 0)
    downFactor = 1;

  int format, stereo, fragment, fqc;

#ifdef __FreeBSD__
  attempt(device = open(dspName,O_WRONLY),"opening dsp device");
  format = AFMT_S16_LE;
  attempt(ioctl(device,SNDCTL_DSP_SETFMT,&format),"setting format");
  if (format != AFMT_S16_LE) error("setting format (2)");
  close(device);
#endif
  if (source == SourcePipe)
    attempt(device = open(dspName,O_WRONLY),"opening dsp device");
  else
    attempt(device = open(dspName,O_RDONLY),"opening dsp device");
    
  //Probably not needed
  //attemptNoDie(ioctl(device,SNDCTL_DSP_RESET,0),"reseting dsp");
  format = AFMT_S16_LE;
  fqc = (source == SourcePipe ? inFrequency : frequency);
  stereo = 1;
  
  int logWindowSize = -1, tmp = windowSize*downFactor;
  while(tmp) {
    tmp /= 2;
    logWindowSize++;
  }

  if (source == SourcePipe)
    fragment = 0x00020000 + (m-overlap+1)+logWindowSize;
                                                 //Soundcard should read in windowSize 
                                                 // blocks of sound before blocking
  else
    //fragment = 0x00020000 + (m-overlap+1); //2 fragments of size 2*(2^(m-overlap+1)) bytes
  
  //Added extra fragments to allow recording overrun (9/7/98)
    fragment = 0x00080000 + (m-overlap+1); //8 fragments of size 2*(2^(m-overlap+1)) bytes
  
  
  
  
  //Was 0x00010000 + m; 

  attemptNoDie(ioctl(device,SNDCTL_DSP_SETFRAGMENT,&fragment),"setting fragment");
#ifndef __FreeBSD__
  attempt(ioctl(device,SNDCTL_DSP_SETFMT,&format),"setting format");
  if (format != AFMT_S16_LE) error("setting format (2)");
#endif
  attempt(ioctl(device,SNDCTL_DSP_STEREO,&stereo),"setting stereo");
  attemptNoDie(ioctl(device,SNDCTL_DSP_SPEED,&fqc),"setting frequency");
   
  data = new short[n*2];  

  if (source == SourcePipe) {
    dataIn = new short[n*2*downFactor*windowSize];
    memset(dataIn,0,n*4*downFactor*windowSize);
    pipeIn = dup(0);
    close(0);
  }
}

void closeSound() {
  delete data;
  if (source == SourcePipe) {
    delete dataIn;
    close(pipeIn);
  }
  close(device);
}

int getNextFragment(void) {
  if (source == SourcePipe) { 
    int i,j;
    write(device, (char*)dataIn, n*4*downFactor*windowSize); 
      
    int position = 0, remaining = n*4*downFactor*windowSize, result;
    while(remaining > 0) {
      result = read(pipeIn, ((char*)dataIn)+position, remaining);
      if (result <= 0)
        return -1;
      
      position += result;
      remaining -= result;
    }
    
    for(i=0,j=0;i<n;i++,j+=downFactor) 
      ((long*)data)[i] = ((long*)dataIn)[j]; 
  } else {
    int i;
    count_info info;
    if (-1 == ioctl(device,SNDCTL_DSP_GETIPTR,&info))
      info.blocks = 1;
    if (info.blocks > 8 || info.blocks < 1) /* Sanity check */
      info.blocks = 1;

    for(i=0;i<info.blocks;i++) {
      if (recSize != n)
        memmove((char*)data,(char*)data+recSize*4,(n-recSize)*4);

      read(device,(char*)data+n*4-recSize*4, recSize*4);
    }
  }
  return 0;
}

