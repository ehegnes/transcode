#include <stdio.h>

#ifdef RTJPEG_INTERNAL

typedef struct region {
                 int start;
                 int end;
               } region;

#define        MAXREGIONS 2000
int            rtjpeg_vid_file=0;
int            rtjpeg_vid_eof=0;
int            rtjpeg_vid_video_width;
int            rtjpeg_vid_video_height;
int            editmode;
double         rtjpeg_vid_video_frame_rate;
unsigned char *rtjpeg_vid_rgb=0;
unsigned char *rtjpeg_vid_buf=0;
struct region  regions[MAXREGIONS];
int            rtjpeg_vid_keyframedist;
int            rtjpeg_vid_effdsp;
int            rtjpeg_vid_framescount;
int            rtjpeg_vid_fakeframescount;
off_t          rtjpeg_vid_filesize;
off_t          rtjpeg_vid_startpos;
int            rtjpeg_vid_audiodelay;
int            rtjpeg_vid_resample;
struct rtfileheader rtjpeg_vid_fileheader;
#else
extern int            rtjpeg_vid_file;
extern int            rtjpeg_vid_eof;
extern int            rtjpeg_vid_video_width;
extern int            rtjpeg_vid_video_height;
extern double         rtjpeg_vid_video_frame_rate;
extern int            rtjpeg_vid_effdsp;
extern int            rtjpeg_vid_framescount;
extern int            rtjpeg_vid_fakeframescount;
extern int            rtjpeg_vid_audiodelay;
extern int            rtjpeg_vid_resample;
extern struct rtfileheader rtjpeg_vid_fileheader;
#endif


int            rtjpeg_vid_open(char *tplorg);
int            rtjpeg_vid_close();
int            rtjpeg_vid_get_video_width();
int            rtjpeg_vid_get_video_height();
double         rtjpeg_vid_get_video_frame_rate();
// unsigned char *rtjpeg_vid_get_frame();
unsigned char *rtjpeg_vid_get_frame(int fakenumber, int *timecode, int onlyvideo,
                                unsigned char **audiodata, int *alen);

//unsigned char *rtjpeg_vid_get_frame(int fakenumber, struct rtframeheader **fhp,
//                                int onlyvideo, int *audiolen);

int            rtjpeg_vid_end_of_video();
int            rtjpeg_vid_check_sig(char *fname);

