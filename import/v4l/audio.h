#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <linux/soundcard.h>
#include <linux/videodev.h>

#ifndef _AUDIO_H
#define _AUDIO_H 1

struct MOVIE_PARAMS {

  /* video */
  int video_format;
  int width, height;          /* size */
  int fps;                    /* frames per second */
  
  /* audio */
  int channels;               /* 1 = mono, 2 = stereo */
  int bits;                   /* 8/16 */
  int rate;                   /* sample rate (11025 etc) */
  
  char *adev;

};


int audio_grab_init(char *dev, int rate, int bits, int chan, int verb);
void audio_grab_close();
int audio_grab_frame(char *buffer, int bytes);

time_t tc(char * buf);

int   sound_open(struct MOVIE_PARAMS *params);
int   sound_bufsize(void);
void  sound_startrec(int on_off);
void  sound_read(char *buffer);
void  sound_close(void);

int  mixer_open(char *filename, char *device);
void mixer_close(void);
int  mixer_get_volume(void);
int  mixer_set_volume(int val);
int  mixer_mute(void);
int  mixer_unmute(void);
int  mixer_get_muted(void);

int usec2bytes(int usec);

#endif
