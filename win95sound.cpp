/* Synaesthesia - program to display sound graphically
   Copyright (C) 1997  Paul Francis Harrison

   Windows95/NT sound and CD-ROM routines, 
   (C) 1998 Nils Desle (ndesle@yahoo.com)

   Mixer code (C) Paul Francis Harrison

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
#include <windows.h>
#include <mmsystem.h>
#include "syna.h"



/* Sound Recording ================================================= */


#define NUMBUFFERS 5

static SoundSource source;
static int inFrequency, downFactor, windowSize, pipeIn, device;
static unsigned short *dataIn;
int mixerno;
HWAVEIN hwi;
WAVEHDR buffer[NUMBUFFERS];
int bufcount;

sampleType* lastBlock;


void CALLBACK SoundCallback(HWAVEIN lhwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
  MMRESULT result;

  if(uMsg == WIM_DATA)
  {
	lastBlock = (sampleType*) ((LPWAVEHDR)dwParam1)->lpData;

	waveInUnprepareHeader(lhwi,(LPWAVEHDR)dwParam1,sizeof(WAVEHDR));
    buffer[bufcount].dwBufferLength = n*2*2;
    buffer[bufcount].dwFlags = 0;
    result = waveInPrepareHeader(hwi,&(buffer[bufcount]),sizeof(WAVEHDR));
    if(result != MMSYSERR_NOERROR)
    {
      error("Couldn't prepare buffer");
      return;
    }

    result = waveInAddBuffer(hwi,&(buffer[bufcount]),sizeof(WAVEHDR));
    if(result != MMSYSERR_NOERROR)
    {
      error("Couldn't add buffer");
      return;
    }

	bufcount++;
	if(bufcount == NUMBUFFERS)
	{
	  bufcount = 0;
	}
  }
}


void openSound(SoundSource source, int inFrequency, int windowSize, char *dspName,
               char *mixerName) 
{
  ::source = source;
  ::inFrequency = inFrequency;
  ::windowSize = windowSize;
  mixerno = atoi(mixerName);

	int soundcard_nr = atoi(dspName);
	if(soundcard_nr < 0)
	{
		error("Invalid soundcard no. specified in .cfg file");
	}


  WAVEFORMATEX format;

  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = frequency;
  format.nAvgBytesPerSec = frequency*4;
  format.nBlockAlign = 4;
  format.wBitsPerSample = 16;
  format.cbSize = 0;


  MMRESULT result = waveInOpen(&hwi, soundcard_nr, &format, (DWORD) &SoundCallback, NULL, CALLBACK_FUNCTION);
  if(result != MMSYSERR_NOERROR)
  {
	if(result == MMSYSERR_ALLOCATED)
      error("Error opening wave device for recording (device already in use)");
	if(result == MMSYSERR_BADDEVICEID)
      error("Error opening wave device for recording (bad device ID)");
	if(result == MMSYSERR_NODRIVER)
      error("Error opening wave device for recording (no driver)");
	if(result == MMSYSERR_NOMEM)
      error("Error opening wave device for recording (not enough memory)");
	if(result == WAVERR_BADFORMAT)
      error("Error opening wave device for recording (bad wave format)");

   
    return;
  }

  int i;
  for(i=0;i<NUMBUFFERS;i++)
  {
	unsigned short* tmpdata = new unsigned short[n*2];
	buffer[i].lpData = (LPSTR) tmpdata;
    buffer[i].dwBufferLength = n*2*2;	// number of BYTES
    buffer[i].dwFlags = 0;
  }

  for(i=0;i<NUMBUFFERS-1;i++)
  {
    result = waveInPrepareHeader(hwi,&(buffer[i]),sizeof(WAVEHDR));
    if(result != MMSYSERR_NOERROR)
    {
      error("Error preparing buffer for recording");
      return;
    }

    result = waveInAddBuffer(hwi,&(buffer[i]),sizeof(WAVEHDR));
    if(result != MMSYSERR_NOERROR)
    {
      error("Error preparing buffer for recording");
      return;
    }
  }

  data = (sampleType*) buffer[0].lpData;
  lastBlock = data;
  bufcount=NUMBUFFERS-1;

  result = waveInStart(hwi);
  if(result != MMSYSERR_NOERROR)
  {
  	error("Couldn't start recording");
  }
}


void closeSound()
{
  waveInReset(hwi);
  waveInStop(hwi);

  delete[] data;
}


int getNextFragment(void)
{
  data = lastBlock;
  return 0;
}


void setupMixer(double &loudness) {
#ifndef WIN32
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
#endif
}

void setVolume(double loudness) 
{
  MIXERLINE ml;
  ml.cbStruct = sizeof(ml);
  ml.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;

  
  MMRESULT result;
  result = mixerGetLineInfo((void*)mixerno, &ml,
       MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
  if (result != MMSYSERR_NOERROR)
  {
    switch(result)
	{
	case MIXERR_INVALLINE:
		error("mixer: The audio line reference is invalid");
		break;
	case MMSYSERR_BADDEVICEID:
		error("mixer: Bad device ID");
		break;
	case MMSYSERR_INVALFLAG:
		error("mixer: invalid flag");
		break;
	case MMSYSERR_INVALHANDLE:
		error("mixer: invalid handle");
		break;
	case MMSYSERR_INVALPARAM:
		error("mixer: invalid parameter");
		break;
	case MMSYSERR_NODRIVER:
		error("mixer: no valid driver");
		break;
	default:
		error("mixer: unknown error");
		break;
	}
  }

  MIXERLINECONTROLS mlc;
  MIXERCONTROL mc;
  mlc.cbStruct = sizeof(mlc);
  mlc.dwLineID = ml.dwLineID;
  mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
  mc.cbStruct = sizeof(mc);
  mlc.cControls = 1;
  mlc.cbmxctrl = sizeof(mc);
  mlc.pamxctrl = &mc;

  result = mixerGetLineControls((void*)mixerno, &mlc,
    MIXER_OBJECTF_MIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE);
  if(result != MMSYSERR_NOERROR)
  {
    switch(result)
	{
	case MIXERR_INVALLINE:
		error("mixer: The audio line reference is invalid");
		break;
	case MMSYSERR_BADDEVICEID:
		error("mixer: Bad device ID");
		break;
	case MMSYSERR_INVALFLAG:
		error("mixer: invalid flag");
		break;
	case MMSYSERR_INVALHANDLE:
		error("mixer: invalid handle");
		break;
	case MMSYSERR_INVALPARAM:
		error("mixer: invalid parameter");
		break;
	case MMSYSERR_NODRIVER:
		error("mixer: no valid driver");
		break;
	default:
		error("mixer: unknown error");
		break;
	}
  }
    


  MIXERCONTROLDETAILS d;
  d.cbStruct = sizeof(d);
  d.dwControlID = mc.dwControlID;
  d.cChannels = 1;
  d.cMultipleItems = 0;
  d.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  MIXERCONTROLDETAILS_UNSIGNED value;
  value.dwValue = mc.Bounds.dwMinimum +
            (DWORD)(loudness * (mc.Bounds.dwMaximum-mc.Bounds.dwMinimum));
  d.paDetails = &value;

  result = mixerSetControlDetails((void*)mixerno, &d, MIXER_OBJECTF_MIXER);
  if(result != MMSYSERR_NOERROR)
  {
    switch(result)
	{
	case MIXERR_INVALLINE:
		error("mixer: The audio line reference is invalid");
		break;
	case MMSYSERR_BADDEVICEID:
		error("mixer: Bad device ID");
		break;
	case MMSYSERR_INVALFLAG:
		error("mixer: invalid flag");
		break;
	case MMSYSERR_INVALHANDLE:
		error("mixer: invalid handle");
		break;
	case MMSYSERR_INVALPARAM:
		error("mixer: invalid parameter");
		break;
	case MMSYSERR_NODRIVER:
		error("mixer: no valid driver");
		break;
	default:
		error("mixer: unknown error");
		break;
	}
  }
    
}



/* CDROM stuff ============================================== */


#define CD_FRAMES 75	/* trial & error guess, but it seems to work... */

static int trackCount=0, *trackFrame = 0;
MCIDEVICEID cd_device;

bool bugfixed_pause = false;

int lastEndFrame=0;
int pausedPosition=0;

unsigned int WinMSFtoSynMSF(unsigned int WinMSF)
{
	return ((MCI_MSF_MINUTE(WinMSF) * 60 * CD_FRAMES) +
		   (MCI_MSF_SECOND(WinMSF) * CD_FRAMES) +
		   MCI_MSF_FRAME(WinMSF));
}


unsigned int SynMSFtoWinMSF(unsigned int SynMSF)
{
	return (SynMSF / (60*CD_FRAMES))
		+  ((SynMSF / CD_FRAMES % 60) << 8)
		+ ((SynMSF % CD_FRAMES) << 16);
}


void cdOpen(char* cdromName)
{
	int cdrom_nr = atoi(cdromName);
	if(cdrom_nr < 0)
	{
		error("Invalid CD-ROM device number specified in .cfg file");
	}

  MCIERROR retval;
  MCI_OPEN_PARMS openstruct;

  bugfixed_pause = false;
  
  openstruct.dwCallback = NULL;
  openstruct.wDeviceID = NULL;
  openstruct.lpstrDeviceType = (LPSTR) MCI_DEVTYPE_CD_AUDIO;
  openstruct.lpstrDeviceType = (LPSTR)((DWORD)(openstruct.lpstrDeviceType) | (DWORD)(cdrom_nr << 16));	// ordinal number of device in high order word
  openstruct.lpstrElementName = NULL;
  openstruct.lpstrAlias = NULL;

  retval = mciSendCommand(cd_device,MCI_OPEN,MCI_WAIT|MCI_OPEN_TYPE|MCI_OPEN_TYPE_ID|MCI_OPEN_SHAREABLE,(DWORD)&openstruct);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdOpen) - Please make sure you have no other CD-players active!");
	error(errorstr);
  }

  cd_device = openstruct.wDeviceID;


  MCI_SET_PARMS mciSetParms;
  mciSetParms.dwTimeFormat = MCI_FORMAT_MSF;
  
  retval = mciSendCommand(cd_device,MCI_SET,MCI_WAIT|MCI_SET_TIME_FORMAT,(DWORD)(LPVOID) &mciSetParms);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdOpen)");
	error(errorstr);
  }
}


void cdClose(void)
{
  MCIERROR retval;
  MCI_GENERIC_PARMS closestruct;

  bugfixed_pause = false;

  retval = mciSendCommand(cd_device,MCI_CLOSE,MCI_WAIT,(DWORD)&closestruct);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdClose)");
	error(errorstr);
  }

  delete[] trackFrame;
}


void getTrackInfo(void)
{
	MCIERROR retval;
	MCI_STATUS_PARMS parms;

	delete trackFrame;
	trackFrame = 0;
	trackCount  = 0;

	parms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (getTrackInfo)");
		error(errorstr);
	}
	trackCount = parms.dwReturn;

	int i;
	trackFrame = new int[trackCount+1];

	DWORD startpos;
	DWORD length;

	parms.dwItem = MCI_STATUS_POSITION;
	parms.dwTrack = trackCount;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_TRACK|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (getTrackInfo)");
		error(errorstr);
	}
	startpos = WinMSFtoSynMSF(parms.dwReturn);

	parms.dwItem = MCI_STATUS_LENGTH;
	parms.dwTrack = trackCount;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_TRACK|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (getTrackInfo)");
		error(errorstr);
	}
	length = WinMSFtoSynMSF(parms.dwReturn);

	trackFrame[trackCount] = startpos+length-1;

	for(i=trackCount-1;i>=0;i--)
	{
		// determine if track is a data or audio track
		parms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
		parms.dwTrack = i+1;
		retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM|MCI_TRACK,(DWORD)&parms);
		if(retval)
		{
			char errorstr[256];
			mciGetErrorString(retval,errorstr,256);
			strcat(errorstr," (getTrackInfo)");
			error(errorstr);
		}
		DWORD tracktype = parms.dwReturn;

		if(tracktype != MCI_CDA_TRACK_AUDIO)
		{
			trackFrame[i] = trackFrame[i+1];
		}
		else
		{
			// get start position
			parms.dwItem = MCI_STATUS_POSITION;
			parms.dwTrack = i+1;
			retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_TRACK|MCI_STATUS_ITEM,(DWORD)&parms);
			if(retval)
			{
				char errorstr[256];
				mciGetErrorString(retval,errorstr,256);
				strcat(errorstr," (getTrackInfo)");
				error(errorstr);
			}
			startpos = WinMSFtoSynMSF(parms.dwReturn);
			trackFrame[i] = startpos;
		}
	}
}


int cdGetTrackCount(void)
{
  return trackCount;
}


int cdGetTrackFrame(int track)
{
  if (!trackFrame)
    return 0;
  else if (track <= 1 || track > trackCount+1)
    return trackFrame[0];
  else
    return trackFrame[track-1];
}


void cdPlay(int frame, int endFrame)
{
  MCIERROR retval;
  MCI_PLAY_PARMS parms;

  bugfixed_pause = false;

  if (frame < 1) frame = 1;
  if (endFrame < 1) endFrame = trackFrame[trackCount];

  lastEndFrame = endFrame;

  cdStop();

  parms.dwCallback = NULL;
  parms.dwFrom = SynMSFtoWinMSF(frame);
  parms.dwTo = SynMSFtoWinMSF(endFrame);
  retval = mciSendCommand(cd_device,MCI_PLAY,MCI_NOTIFY|MCI_FROM|MCI_TO,(DWORD)&parms);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdPlay)");
	error(errorstr);
  }
}


void cdGetStatus(int &track, int &frames, SymbolID &state)
{
	MCI_STATUS_PARMS parms;

	MCIERROR retval;  
	
	parms.dwItem = MCI_STATUS_MODE;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (cdGetStatus)");
		error(errorstr);
	}

	DWORD cdstate = parms.dwReturn;

	SymbolID oldState = state;

	switch(cdstate)
	{
		case MCI_MODE_PAUSE:
		  state = Pause;
		  break;
		case MCI_MODE_PLAY:
		  state = Play;
		  break;
		case MCI_MODE_STOP:
		  if(bugfixed_pause == true)
		  {
			  state = Pause;
		  }
		  else
		  {
			  state = Stop;
		  }
		  break;
		case MCI_MODE_OPEN:
		  state = Open;
		  return;
		  break;
		case MCI_MODE_NOT_READY:
		case MCI_MODE_RECORD:
		case MCI_MODE_SEEK:
		default:
		  state = NoCD;
		  return;
		  break;
	}

	parms.dwItem = MCI_STATUS_CURRENT_TRACK;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (cdGetStatus)");
		error(errorstr);
	}
	track = parms.dwReturn;

	parms.dwItem = MCI_STATUS_POSITION;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (cdGetStatus)");
		error(errorstr);
	}
	frames = WinMSFtoSynMSF(parms.dwReturn);
	if(trackFrame)
		frames-=trackFrame[track-1];		// track relative frames!!!

	if ((oldState == NoCD || oldState == Open) &&
	  (state == Pause || state == Play || state == Stop))
	{
		getTrackInfo();
	}

	if (state != Play && state != Pause)
	{
		track = 1;
		frames = 0;
	}
}


void cdStop(void)
{
  MCI_GENERIC_PARMS parms;

  bugfixed_pause = false;
  
  MCIERROR retval = mciSendCommand(cd_device,MCI_STOP,MCI_WAIT,(DWORD)&parms);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdStop)");
	error(errorstr);
  }
}


void cdPause()
{
/*	turns out Windows standard CD-Rom drivers (MCICDA device) don't actually support pause 
    and resume. Sigh. I'll just fake a pause by doing a stop and remembering the current 
	position.

	MCI_GENERIC_PARMS parms;
	MCIERROR retval = mciSendCommand(cd_device,MCI_PAUSE,MCI_WAIT,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (cdPause)");
		error(errorstr);
	}
*/

	MCI_STATUS_PARMS parms;
	MCIERROR retval;

	parms.dwItem = MCI_STATUS_POSITION;
	retval = mciSendCommand(cd_device,MCI_STATUS,MCI_WAIT|MCI_STATUS_ITEM,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (cdPause)");
		error(errorstr);
	}
	pausedPosition = WinMSFtoSynMSF(parms.dwReturn);

	cdStop();
	bugfixed_pause = true;
}


void cdResume()
{
/* resume and pause are not supported by the standard MCICDA devices in Windows.

	MCI_GENERIC_PARMS parms;
	MCIERROR retval = mciSendCommand(cd_device,MCI_RESUME,MCI_WAIT,(DWORD)&parms);
	if(retval)
	{
		char errorstr[256];
		mciGetErrorString(retval,errorstr,256);
		strcat(errorstr," (cdResume)");
		error(errorstr);
	}
*/

	if(bugfixed_pause)
	{
		cdPlay(pausedPosition,lastEndFrame);
		bugfixed_pause = false;
	}
	else
	{
		error("trying to resume a non-paused state???");
	}
}


void cdEject()
{
  MCI_SET_PARMS parms;
  MCIERROR retval = mciSendCommand(cd_device,MCI_SET,MCI_WAIT|MCI_SET_DOOR_OPEN,(DWORD)&parms);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdEject)");
	error(errorstr);
  }
  bugfixed_pause = false;
}


void cdCloseTray()
{
  MCI_SET_PARMS parms;
  MCIERROR retval = mciSendCommand(cd_device,MCI_SET,MCI_WAIT|MCI_SET_DOOR_CLOSED,(DWORD)&parms);
  if(retval)
  {
    char errorstr[256];
    mciGetErrorString(retval,errorstr,256);
	strcat(errorstr," (cdCloseTray)");
	error(errorstr);
  }
  bugfixed_pause = false;
}
