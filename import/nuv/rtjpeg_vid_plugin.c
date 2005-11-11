/********************************************
 * the nuppelvideo reader lib for           *
 *     exportvideo, nuv2divx                *
 *     nuv2yuv and nuvplay                  *
 *                                          *
 * by  Roman Hochleitner                    *
 *     roman@mars.tuwien.ac.at              *
 *                                          *
 ********************************************
 * USE AT YOUR OWN RISK         NO WARRANTY *
 * (might crash your _________!)            *
 ********************************************
 *                                          *
 * parts borrowed from Justin Schoeman      *
 *                     and others           *
 *                                          *
 * This Software is under GPL version 2     *
 *                                          *
 * http://www.gnu.org/copyleft/gpl.html     *
 * ---------------------------------------- *
 * Fri May 30 01:17:49 CEST 2001            *
 ********************************************/

#ifdef __bsdi__
#define  _LARGEFILE64_SOURCE    1
#ifndef	O_LARGEFILE
#define O_LARGEFILE 0
#endif
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef	__linux__
#include <features.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#define __USE_GNU
#include <string.h>
#include "nuppelvideo.h"
#define RTJPEG_INTERNAL 1
 #include "rtjpeg_vid_plugin.h"
#undef RTJPEG_INTERNAL

#include "transcode.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "RTjpegN.h"
#include "resample.h"
#ifdef __cplusplus
}
#endif
#include "minilzo.h"

static struct region  regions[MAXREGIONS];

/*
  int    rtjpeg_vid_file;
  int    rtjpeg_vid_video_width;
  int    rtjpeg_vid_video_height;
  double rtjpeg_vid_video_frame_rate;
*/


/* ------------------------------------------------- */

int rtjpeg_vid_open(char *tplorg)
{
  unsigned long int tbls[128];
  struct rtframeheader frameheader;
  struct stat fstatistics;
  FILE   *editfile;
  char   editfname[255];
  int    start,end,rpos;
  off_t  filesize;
  off_t  startpos;
  off_t  pos;
  //int    seekdist;
  int    foundit=0;
  int    regpos;
  char   buffer[32768];
  char   *needlepos;
  char   ctype;
  char   ftype;
  char   *space;
  int    editmode=0;

  tc_snprintf(editfname, sizeof(editfname), "%s.%s", tplorg, "edit");
  editfile = fopen(editfname, "r");

  if (editfile != NULL && editmode==0) {
    rpos=0;
    while (!feof(editfile)) {
      if (2==fscanf(editfile, "%d %d\n", &start, &end)) {
        regions[rpos].start = start;
        regions[rpos].end   = end;
        rpos++;
      }
    }
    regions[rpos].start = -1; // end of exclude list
    regions[rpos].end   = -1;
    fclose(editfile);
  } else {
    regions[0].start = -1; // end of exclude list
    regions[0].end   = -1;
  }

#ifdef SYS_BSD
  rtjpeg_vid_file=open(tplorg, O_RDONLY);
#else
  rtjpeg_vid_file=open(tplorg, O_RDONLY|O_LARGEFILE);
#endif

  if (rtjpeg_vid_file == -1) {
    fprintf(stderr, "File not found: %s\n", tplorg);
    exit(1);
  }


  fstat(rtjpeg_vid_file, &fstatistics);
  filesize = rtjpeg_vid_filesize = fstatistics.st_size;
  read(rtjpeg_vid_file, &rtjpeg_vid_fileheader, FILEHEADERSIZE);

  rtjpeg_vid_video_width      = rtjpeg_vid_fileheader.width;
  rtjpeg_vid_video_height     = rtjpeg_vid_fileheader.height;
  rtjpeg_vid_video_frame_rate = rtjpeg_vid_fileheader.fps;
  rtjpeg_vid_keyframedist     = rtjpeg_vid_fileheader.keyframedist;
  rtjpeg_vid_eof=0;

  // make sure we have enough even for a raw YUV frame
  space = (char *)malloc((int)(rtjpeg_vid_video_width*rtjpeg_vid_video_height*1.5));

  /* first frame has to be the Compression "D"ata frame */
  if (FRAMEHEADERSIZE != read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE)) {
    fprintf(stderr, "Cant read Compression (D)ata frame header\n");
    exit(1);
  }
  if (frameheader.frametype != 'D') {
    fprintf(stderr, "\nIllegal File Format, no Compression (D)ata frame\n" );
    exit(1);
  }
  if (frameheader.packetlength != read(rtjpeg_vid_file, tbls, frameheader.packetlength)) {
    fprintf(stderr, "Cant read Compression (D)ata packet, length=%d\n",
            frameheader.packetlength);
    exit(1);
  }
  RTjpeg_init_decompress(tbls, rtjpeg_vid_video_width, rtjpeg_vid_video_height);

  if ((rtjpeg_vid_video_height & 1) == 1) {
    // this won't ever happen, since RTjpeg can only handle n*8 for w and h
    rtjpeg_vid_video_height--;
    fprintf(stderr, "\nIncompatible video height, reducing height to %d\n", rtjpeg_vid_video_height);
  }

  /* init compression lzo ------------------------------ */
  if (lzo_init() != LZO_E_OK) {
    fprintf(stderr,"%s\n", "lzo_init() failed !!!");
    exit(3);
  }

  // how much video frames has the file?
  // what is the last resulting dsp frequency?

  rtjpeg_vid_startpos = startpos = lseek(rtjpeg_vid_file, 0, SEEK_CUR);
  pos = filesize-32768;

  // we have to search for a RTjjjjjjjj seekframe
  while (pos > startpos && !foundit) {
    char *p;
    lseek(rtjpeg_vid_file, pos, SEEK_SET);
    read(rtjpeg_vid_file, buffer, 32768);
    p = buffer;

    // does that work -- tibit
    needlepos = NULL;
    while (p-buffer < 32768) {
	if ( memcmp (p, "RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE) == 0) {
	    needlepos = p;
	    break;
	}
	p++;
    }

    if (needlepos != NULL) {
      lseek(rtjpeg_vid_file, pos+(needlepos - buffer) + FRAMEHEADERSIZE, SEEK_SET);
      read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE);
      // now check if RTjjjjjjjj was fake data and not a seekheader
      if (NULL == strchr("ARDVST", frameheader.frametype)) {
        pos -= 32768;
        continue;
      }
      ctype = 127 & frameheader.comptype;
      if (NULL == strchr("0123NLAV", ctype)) {
        pos -= 32768;
        continue;
      }
      if (frameheader.packetlength < 0 || frameheader.packetlength>3000000) {
        pos -= 32768;
        continue;
      }
      foundit = 1;
      pos += (needlepos - buffer) + FRAMEHEADERSIZE;
    } else {
      pos -= 32768;
    }
    //fprintf(stderr, "pos is now %d\n", pos);
  }

  if (!foundit) {
    pos = startpos;
    lseek(rtjpeg_vid_file, pos, SEEK_SET);
    read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE);
  }

  foundit = 0;
  rtjpeg_vid_effdsp=44100;
  rtjpeg_vid_framescount=0;
  rtjpeg_vid_fakeframescount=0;

  while (!foundit) {
    ftype = ' ';
    if (frameheader.frametype == 'S') {
      if (frameheader.comptype == 'V') {
        rtjpeg_vid_framescount=frameheader.timecode;  // frame number of NEXT frame == count
      }
      if (frameheader.comptype == 'A') {
        rtjpeg_vid_effdsp=frameheader.timecode;
      }
    } else {
      if (frameheader.frametype == 'V') {
        ftype = 'V';
        rtjpeg_vid_framescount++;
      }
    }
    if (frameheader.frametype != 'R' && frameheader.packetlength!=0) {
      // we have to read to be sure to count the correct amount of frames
      if (frameheader.packetlength != read(rtjpeg_vid_file, space, frameheader.packetlength)) {
        if (ftype=='V') rtjpeg_vid_framescount--; // we don't count a header of a missing body
        foundit=1;
        continue;
      }
    }

    foundit = FRAMEHEADERSIZE != read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE);
  }

  rtjpeg_vid_fakeframescount = rtjpeg_vid_framescount;

  regpos = 0;
  while (rtjpeg_vid_framescount >= regions[regpos].start &&  regions[regpos].start != -1) {
    if (rtjpeg_vid_framescount >= regions[regpos].end) {
      // we have rtjpeg_vid_framescount-frames in this excluded region
      rtjpeg_vid_fakeframescount -= (regions[regpos].end-regions[regpos].start + 1);
    } else {
      // this should normally not happen anyway
      rtjpeg_vid_fakeframescount -= rtjpeg_vid_framescount-regions[regpos].start + 1;
    }
    regpos++;
  }

  free(space); // give back memory

  lseek(rtjpeg_vid_file, startpos, SEEK_SET);

  //resample_init((rtjpeg_vid_effdsp+50)/100, 44100);

  return(0);
}



/* ------------------------------------------------- */
/* seeks to native frames only!!!!                   */
/* ------------------------------------------------- */

static int rtjpeg_vid_seekto_keyframe_before(int number)
{
  int  startpos, pos;
  int  curnum=2000000000;
  int  foundit=0;
  char buffer[32768];
  char *needlepos;
  char ctype;
  struct rtframeheader frameheader;

  if (number < 0 || number >= rtjpeg_vid_framescount) {
    return(-1);
  }

  startpos = pos =
       (int)((double)rtjpeg_vid_filesize * ((double)number/(double)rtjpeg_vid_framescount));

  while (curnum > number && pos > rtjpeg_vid_startpos) {
    // we have to search for a RTjjjjjjjj seekframe
    //fprintf(stderr, "curnum=%d  number=%d\n", curnum, number);
    foundit=0; // reset the flag
    while (pos > rtjpeg_vid_startpos && !foundit) {
      char *p;
      lseek(rtjpeg_vid_file, pos, SEEK_SET);
      read(rtjpeg_vid_file, buffer, 32768);
      //fprintf(stderr, "check for needle\n");
      p = buffer;

      // does that work -- tibit
      needlepos = NULL;
      while (p-buffer < 32768) {
	  if ( memcmp (p, "RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE) == 0) {
	      needlepos = p;
	      break;
	  }
	  p++;
      }
      if (needlepos != NULL) {
        lseek(rtjpeg_vid_file, pos+(needlepos - buffer) + FRAMEHEADERSIZE, SEEK_SET);
        read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE);
        //fprintf(stderr, "NEEDLE found\n");
        // now check if RTjjjjjjjj was fake data and not a seekheader
        if (NULL == strchr("ARDVST", frameheader.frametype)) {
          pos -= 32768;
          continue;
        }
        ctype = 127 & frameheader.comptype;
        if (NULL == strchr("0123NLAV", ctype)) {
          pos -= 32768;
          continue;
        }
        if (frameheader.packetlength < 0 || frameheader.packetlength>3000000) {
          //fprintf(stderr, "FAKE seeker frame found\n");
          pos -= 32768;
          continue;
        }
        foundit = 1;
        pos += (needlepos - buffer) + FRAMEHEADERSIZE;
      } else {
        pos -= 32768;
      }
      //fprintf(stderr, "pos is now %d\n", pos);
    }

    // we are before rtjpeg_vid_startpos, therefore the seekpoint is very near
    // to the beginning of the file
    if (!foundit) {
      continue;
    }

    foundit = 0;

    while (!foundit) {
      if (frameheader.frametype == 'S' && frameheader.comptype == 'V') {
        curnum = frameheader.timecode;  // frame number of NEXT frame == count
        foundit=1;
        continue;
      }
      if (frameheader.frametype != 'R' && frameheader.packetlength!=0) {
        pos = lseek(rtjpeg_vid_file, frameheader.packetlength, SEEK_CUR);
      }

      read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE);
    }
    pos = startpos - 32768;
    startpos = pos;
    //fprintf(stderr, "pos is now %d\n", pos);
  }

  if (pos < rtjpeg_vid_startpos) {
    pos = rtjpeg_vid_startpos;
    lseek(rtjpeg_vid_file, pos, SEEK_SET);
    return(0);
  }

  return(curnum);
}

/* ------------------------------------------------- */

static unsigned char *decode_vid_frame(struct rtframeheader *frameheader,unsigned char *strm)
{
  int r;
  int keyframe;
  unsigned int out_len;
  static unsigned char *buf2 = 0;
  static char lastct='1';
  int compoff = 0;


  if (buf2==NULL) {
    buf2       = (unsigned char *) malloc( rtjpeg_vid_video_width*rtjpeg_vid_video_height +
                        (rtjpeg_vid_video_width*rtjpeg_vid_video_height)/2);
  }

  // now everything is initialized

  /* fprintf(stderr, "%s\n", "after reading frame"); */

  // now check if we have a (N)ull or (L)ast in comprtype -- empty data packet!
  if (frameheader->frametype == 'V') {
    if (frameheader->comptype == 'N') {
      memset(rtjpeg_vid_buf,   0,  rtjpeg_vid_video_width*rtjpeg_vid_video_height);
      memset(rtjpeg_vid_buf+rtjpeg_vid_video_width*rtjpeg_vid_video_height,
                         127, (rtjpeg_vid_video_width*rtjpeg_vid_video_height)/2);
      return(rtjpeg_vid_buf);
    }
    if (frameheader->comptype == 'L') {
      switch(lastct) {
        case '0':
        case '3': return(buf2); break;
        case '1':
        case '2': return(rtjpeg_vid_buf); break;
        default: return(rtjpeg_vid_buf);
      }
    }
  }

  // look for the keyframe flag and if set clear it
  keyframe = frameheader->keyframe==0;
  if (keyframe) {
    // normally there would be nothing to do, but for testing we
    // we will reset the buffer before decompression
	  /*
    memset(rtjpeg_vid_buf,   0,  rtjpeg_vid_video_width*rtjpeg_vid_video_height);
    memset(rtjpeg_vid_buf+rtjpeg_vid_video_width*rtjpeg_vid_video_height,
                       127, (rtjpeg_vid_video_width*rtjpeg_vid_video_height)/2);
		       */
    // rtjpeg_vid_reset_old(); // this might be used if i/we make real
    // predicted frames (but with no motion compensation)
  }

  if (frameheader->comptype == '0') compoff=1;
  if (frameheader->comptype == '1') compoff=1;
  if (frameheader->comptype == '2') compoff=0;
  if (frameheader->comptype == '3') compoff=0;

  lastct = frameheader->comptype; // we need that for the 'L' ctype

  // lzo decompression ------------------
  if (!compoff) {
    r = lzo1x_decompress(strm,frameheader->packetlength,buf2,&out_len,NULL);
    if (r != LZO_E_OK) {
      // if decompression fails try raw format :-)
      fprintf(stderr,"\nminilzo: can't decompress illegal data, ft='%c' ct='%c' len=%d tc=%d\n",
                    frameheader->frametype, frameheader->comptype,
                    frameheader->packetlength, frameheader->timecode);
    }
  }

  // the raw modes

  // raw YUV420 (I420, YCrCb) uncompressed
  if (frameheader->frametype=='V' && frameheader->comptype == '0') {
    ac_memcpy(buf2, strm, (int)(rtjpeg_vid_video_width*rtjpeg_vid_video_height*1.5)); // save for 'L'
    return(buf2);
  }

  // raw YUV420 (I420, YCrCb) but compressed
  if (frameheader->frametype=='V' && frameheader->comptype == '3') {
    return(buf2);
  }

  // rtjpeg decompression

  if (compoff) {
    RTjpeg_decompressYUV420((__s8 *)strm, rtjpeg_vid_buf);
  } else {
    RTjpeg_decompressYUV420((__s8 *)buf2, rtjpeg_vid_buf);
  }

  return(rtjpeg_vid_buf);
}

/* ------------------------------------------------- */

#define MAXVBUFFER 20

unsigned char *rtjpeg_vid_get_frame(int fakenumber, int *timecode, int onlyvideo,
                                unsigned char **audiodata, int *alen)
{
  static int lastnumber=-1;
  static int lastaudiolen=0;
  static unsigned char *strm = 0;
  static struct rtframeheader frameheader;

  static int wpos = 0;
  static int rpos = 0;
  static int            bufstat[MAXVBUFFER]; // 0 .. free,  1 .. filled
  static int          timecodes[MAXVBUFFER];
  static unsigned char *vbuffer[MAXVBUFFER];
  static unsigned char audiobuffer[512000]; // big to have enough buffer for
  static unsigned char    tmpaudio[512000]; // audio delays
  static unsigned char  scaleaudio[32768];
  static int              audiolen=0;
  static int              fafterseek=0;
  static int              audiobytes=0;
  static int              audiotimecode;

  int nextfnum;
  unsigned char *ret;
  int gotvideo=0;
  int gotaudio=0;
  int regpos;
  int number;
  int seeked=0;
  int bytesperframe;
//  int calcerror;
  int tcshift;
  int shiftcorrected=0;
  int ashift;
  int i;
  //int cnt=0;

  if (rtjpeg_vid_buf==NULL) {
    rtjpeg_vid_buf = (unsigned char *) malloc( rtjpeg_vid_video_width*rtjpeg_vid_video_height +
                        (rtjpeg_vid_video_width*rtjpeg_vid_video_height)/2);
    strm       = (unsigned char *) malloc( rtjpeg_vid_video_width*rtjpeg_vid_video_height +
                        (rtjpeg_vid_video_width*rtjpeg_vid_video_height)); // a bit more

    // initialize and purge buffers
    for (i=0; i< MAXVBUFFER; i++) {
      vbuffer[i] = (unsigned char *) malloc( rtjpeg_vid_video_width*rtjpeg_vid_video_height +
                                            (rtjpeg_vid_video_width*rtjpeg_vid_video_height)/2);
      bufstat[i]=0;
      timecodes[i]=0;
    }
    wpos = 0;
    rpos = 0;
    audiolen=0;
  }

  //*fhp = &frameheader;

  // now everything is initialized

  regpos = 0;
  number = fakenumber;

  while (number >= regions[regpos].start &&  regions[regpos].start != -1) {
    number += (regions[regpos].end-regions[regpos].start + 1);
    regpos++;
  }

  if (number==0) seeked=1; // assure we normalize the begin of recording

  //fprintf(stderr, "fakenumber is %d, realnumber is %d\n", fakenumber, number);

  // now the numbers are real numbers!!

  // if it is not the next frame in line we have to seek and therefore
  // purge all audio and videobuffers

  // Warning: after every seek we will fill up exact the
  // amount of missing audiobytes with '0's so that we (hopefully) stay in sync,
  // so if somebody has the crazy idea to cut out exactly every second frame
  // (or only one frame with big audiobuffersize) it will/might jitter a lot ;-)

  // that means if you have an audioblocksize of 16384 you might experience
  // a silent passage of audio of max 0.1 seconds at the cutting position

//fprintf(stderr, "we are at %d\n", number);
  if (lastnumber+1 != number) {
    if (number<=lastnumber || (number>lastnumber+200)) {
      //fprintf(stderr, "we are at %d, must seek to %d\n", lastnumber+1, number);
      if ((nextfnum = rtjpeg_vid_seekto_keyframe_before(number)) < 0) {
        rtjpeg_vid_eof=1;
        return rtjpeg_vid_buf;
      }
    } else {
      nextfnum = lastnumber+1;
      //fprintf(stderr, "we are at %d, must decode until fnumber< %d\n", lastnumber+1, number);
    }

    while (nextfnum < number) {
      if (read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE)!=FRAMEHEADERSIZE) {
        rtjpeg_vid_eof=1;
        return(rtjpeg_vid_buf);
      }
      if (frameheader.frametype == 'R') continue;
      if(read(rtjpeg_vid_file, strm, frameheader.packetlength)!=frameheader.packetlength) {
        rtjpeg_vid_eof=1;
        return(rtjpeg_vid_buf);
      }
      if (frameheader.frametype == 'V') {
        ret = decode_vid_frame(&frameheader, strm);
        nextfnum++;
      }
    }

    // purge all buffers
    for (i=0; i<MAXVBUFFER; i++) {
      bufstat[i]=0;
      timecodes[i]=0;
    }

    wpos = 0;
    rpos = 0;
    gotvideo = 0;
    gotaudio = 0;
    audiolen = 0;
    seeked = 1;
    fafterseek=0; // should we reset this two
    audiobytes=0; // really when we seek?
    audiotimecode=0;
  }

  // now we have to read as many video frames and audio blocks
  // until we have enough audio for the current video frame
  // that is: PAL  1/25*rtjpeg_vid_effdsp*4    where rtjpeg_vid_effdsp should be 44100
  //          NTSC 1/29.97*rtjpeg_vid_effdsp*4

  bytesperframe = 4*(int)((1.0/rtjpeg_vid_video_frame_rate)*((double)rtjpeg_vid_effdsp/100.0)+0.5);

// don't know which method to use to assure we have enough audio per frame espacially
// when we cut (seek) out scenes, now i use timecode based calculation which might be best
// at least for nuv2divx but i'm not sure
// the problem is that many soundcards have temporarilly incorrect samples/sec but have
// a stable samplerate over a bigger amount of time (almost allways between 43000 - 44200)
// is there a way to adjust these dumb soundcard devices to be more exact?????

//
//  // we also have to calculate the cumulated error
//
//  calcerror = (((int)((double)audiobytes -
//                      (double)fafterseek*(4.0*((1.0/rtjpeg_vid_video_frame_rate)*
//                                              ((double)rtjpeg_vid_effdsp/100.0)+0.5))))/4)*4;
//
//  // if calcerror > 0 then subtract it from bpf
//
//  if (calcerror!=0) {
//    fprintf(stderr, "audio shift error corrected with %d bytes to %d\n", calcerror,
//            bytesperframe-calcerror);
//    bytesperframe -= calcerror;
//  } else {
//    //fprintf(stderr, "audio shift error = 0 no correction\n");
//  }

  // if seeked and the audio we get has an erlier timecode then the folowing
  // video frame we have to cut off (vid.tc-aud.tc)*rtjpeg_vid_effdsp*4 bytes
  // from the head of the audioblock

  gotvideo = 0;
  if (onlyvideo>0)  gotaudio=1;
     else           gotaudio=0;

  // if (onlyvideo<0) gotvideo=1; // do not decode

  while (!gotvideo || !gotaudio) {

    // now check if we already have a video frame in a buffer
    if (!gotvideo && bufstat[rpos]==1) {
      gotvideo = 1;
    }

    // now check if we already have enough audio for the video frame in the audiobuffer

    if (!gotaudio && (audiolen >= bytesperframe)) {
      gotaudio = 1;
    }

    // check if we have both and correct audiolen if seeked
    if (gotvideo && gotaudio) {
      if (shiftcorrected || onlyvideo>0) continue;
      //fprintf(stderr, "we have both video and audio, audiolen=%d\n", audiolen);
      if (!seeked) {
        tcshift=(int)(((double)(audiotimecode-timecodes[rpos])*(double)rtjpeg_vid_effdsp)/100000)*4;
        //fprintf(stderr, "\nbytesperframe was %d and now is %d shifted with %d\n", bytesperframe,
        //        bytesperframe-tcshift, -tcshift);
        if (tcshift> 1000) tcshift= 1000;
        if (tcshift<-1000) tcshift=-1000;
        bytesperframe -= tcshift;
        if (bytesperframe<100) {
          fprintf(stderr, "bytesperframe was %d < 100 and now is forced to 100\n", bytesperframe);
          bytesperframe=100;
        }
      } else {
        //fprintf(stderr, "we have seeked audio will be adjusted, audiolen=%d\n", audiolen);
        //fprintf(stderr, "timecodes atc=%d vtc=%d\n", audiotimecode, timecodes[rpos]);
        // we have seeked and have now to correct audiolen due to timecode differences

        // video later than audio
        // we have to cut off (vid.tc-aud.tc)*rtjpeg_vid_effdsp*4 bytes
        if (timecodes[rpos] > audiotimecode) {
          ashift = (int)(((double)(audiotimecode-timecodes[rpos])*(double)rtjpeg_vid_effdsp)/100000)*4;
          //ashift = (((timecodes[rpos]-audiotimecode)*rtjpeg_vid_effdsp)/100)*4;
          if (ashift>audiolen) {
            audiolen = 0;
          } else {
            //fprintf(stderr, "timecode shift=%d atc=%d vtc=%d\n", ashift, audiotimecode,
            //                timecodes[rpos]);
            ac_memcpy(tmpaudio, audiobuffer, audiolen);
            ac_memcpy(audiobuffer, tmpaudio+ashift, audiolen);
            audiolen -= ashift;
          }
        }

        // audio is later than video
        // we have to insert blank audio (aud.tc-vid.tc)*rtjpeg_vid_effdsp* bytes
        if (timecodes[rpos] < audiotimecode) {
          ashift = (int)(((double)(audiotimecode-timecodes[rpos])*(double)rtjpeg_vid_effdsp)/100000)*4;
          if (ashift>(30*bytesperframe)) {
            fprintf(stderr, "Warning: should never happen, huge timecode gap gap=%d atc=%d vtc=%d\n",
                    ashift, audiotimecode, timecodes[rpos]);
          } else {
            //fprintf(stderr, "timecode shift=%d atc=%d vtc=%d\n", -ashift, audiotimecode,
            //                timecodes[rpos]);
            ac_memcpy(tmpaudio, audiobuffer, audiolen);
            bzero(audiobuffer, ashift); // silence!
            ac_memcpy(audiobuffer+ashift, tmpaudio, audiolen);
            audiolen += ashift;
          }
        }
      }
      shiftcorrected=1; // mark so that we don't loop forever if audiobytes was less than
                        // bytesperframe at least once
      // now we check if we still have enough audio
      if (audiolen >= bytesperframe) {
        continue; // we are finished for this video frame + adequate audio
      } else {
        gotaudio = 0;
      }
    }

    if (read(rtjpeg_vid_file, &frameheader, FRAMEHEADERSIZE)!=FRAMEHEADERSIZE) {
      rtjpeg_vid_eof=1;
      return(rtjpeg_vid_buf);
    }

#ifdef DEBUG_FTYPE
     fprintf(stderr,"\ntype='%c' ctype='%c' length=%d  timecode=%d  f-gop=%d",
							frameheader.frametype,
                                                        frameheader.comptype,
                                                        frameheader.packetlength,
                                                        frameheader.timecode,
							frameheader.keyframe);
#endif

    if (frameheader.frametype == 'R') continue; // the R-frame has no data packet!!

    if (frameheader.packetlength!=0) {
      if(read(rtjpeg_vid_file, strm, frameheader.packetlength)!=frameheader.packetlength) {
        rtjpeg_vid_eof=1;
        return(rtjpeg_vid_buf);
      }
    }

    if (frameheader.frametype=='V') {
      // decode the videobuffer and buffer it
      if (onlyvideo>= 0) {
        ret = decode_vid_frame(&frameheader, strm);
      } else {
        ret = vbuffer[0]; // we don't decode video for exporting audio only
      }
      // now buffer it
      ac_memcpy(vbuffer[wpos], ret, (int)(rtjpeg_vid_video_width*rtjpeg_vid_video_height*1.5));
      timecodes[wpos] = frameheader.timecode;
      bufstat[wpos]=1;
      //lastwpos=wpos;
      wpos = (wpos+1) % MAXVBUFFER;
      continue;
    }

    if (frameheader.frametype=='A' && onlyvideo<=0) {
      // buffer the audio
      if (frameheader.comptype=='N' && lastaudiolen!=0) {
        // this won't happen too often, if ever!
        memset(strm,   0, lastaudiolen);
      }
      // now buffer it
      ac_memcpy(audiobuffer+audiolen, strm, frameheader.packetlength);
      audiotimecode = frameheader.timecode + rtjpeg_vid_audiodelay; // untested !!!! possible FIXME
      if (audiolen>0) {
        // now we take the new timecode and calculate the shift
        // we have to subtract from it - (audiolen*100/(effdsp*4))*1000 [msec]
        audiotimecode -= (int)(1000*(((double)audiolen*25.0)/(double)rtjpeg_vid_effdsp));
        if (audiotimecode<0) {
          //fprintf(stderr, "WARNING audiotimecode < 0, at=%d\n", audiotimecode);
          audiotimecode = 0;
        }
      }
      audiolen += frameheader.packetlength;
      lastaudiolen = audiolen;
      // we do not know now if it is enough for the current video frame
    }

  }

  lastnumber = number;
  if (onlyvideo>0) {
    *alen = 0;
  } else {
    *alen = bytesperframe;
    ac_memcpy(tmpaudio, audiobuffer, audiolen);
    ac_memcpy(audiobuffer, tmpaudio+bytesperframe, audiolen);
    audiolen -= bytesperframe;
    audiobytes += bytesperframe;
  }
  // now we actually resample audio, but only if effdsp!=44100 and resample==1
  if (!rtjpeg_vid_resample || (rtjpeg_vid_effdsp+50)/100==44100) {
    *audiodata = tmpaudio;
  } else {
    *alen = 4*resample_flow((char *)tmpaudio, bytesperframe>>2, (char *)scaleaudio);
    *audiodata = scaleaudio;
  }

  fafterseek ++;

  //fprintf(stderr, "ffn=%d rfn=%d atc=%d vtc=%d\n", fakenumber, number, audiotimecode,
  //                 timecodes[rpos]);

  // now we have to return the frame and free it!!
  ret = vbuffer[rpos];
  bufstat[rpos] = 0; // clear flag
  rpos = (rpos+1) % MAXVBUFFER;



  return(ret);
}

// banned old comments ;-)
  /* fprintf(stderr, "%s\n", "after decompressing frame"); */
  // we use yuv420 now RTjpeg_yuvrgb24(rtjpeg_vid_buf, rtjpeg_vid_rgb, 0); /* rgb24 will work */
  /* fprintf(stderr, "%s\n", "after rgb-ing frame"); */
  // cnt++; if (cnt % 10 == 0) usleep(250000); // give time to system to prevent lockups :-/
  //if (onlyvideo>5) return(rtjpeg_vid_buf); // ugly fallthrough for counting only -- never used ;]

/* ------------------------------------------------- */

int rtjpeg_vid_close()
{
  close(rtjpeg_vid_file);
  return(0);
}


/* ------------------------------------------------- */

int rtjpeg_vid_get_video_width()
{
  return(rtjpeg_vid_video_width);
}



/* ------------------------------------------------- */

int rtjpeg_vid_get_video_height()
{
  return(rtjpeg_vid_video_height);
}



/* ------------------------------------------------- */

double rtjpeg_vid_get_video_frame_rate()
{
  return(rtjpeg_vid_video_frame_rate);
}



/* ------------------------------------------------- */
/* we don't really check sig now, just the extension     */

int rtjpeg_vid_check_sig(char *fname)
{
  int len;

  len=strlen(fname);
  if (len < 4)
    return(0);
  if ((0 == strcmp(fname+len-4,".nuv")) ||
      (0 == strcmp(fname+len-4,".NUV")))   return(1);
  return(0);
}



/* ------------------------------------------------- */

int rtjpeg_vid_end_of_video()
{
   return (rtjpeg_vid_eof);
}


