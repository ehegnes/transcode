#include <stdio.h>

#ifdef RTJPEG_INTERNAL

typedef struct region {
                 int start;
                 int end;
               } region;

#define        MAXREGIONS 2000
int            rtjpeg_aud_file=0;
int            rtjpeg_aud_eof=0;
int            rtjpeg_aud_video_width;
int            rtjpeg_aud_video_height;
double         rtjpeg_aud_video_frame_rate;
unsigned char *rtjpeg_aud_rgb=0;
unsigned char *rtjpeg_aud_buf=0;
int            rtjpeg_aud_keyframedist;
int            rtjpeg_aud_effdsp;
int            rtjpeg_aud_framescount;
int            rtjpeg_aud_fakeframescount;
off_t          rtjpeg_aud_filesize;
off_t          rtjpeg_aud_startpos;
int            rtjpeg_aud_audiodelay;
int            rtjpeg_aud_resample;
struct rtfileheader rtjpeg_aud_fileheader;
#else
extern int            rtjpeg_aud_file;
extern int            rtjpeg_aud_eof;
extern int            rtjpeg_aud_video_width;
extern int            rtjpeg_aud_video_height;
extern double         rtjpeg_aud_video_frame_rate;
extern int            rtjpeg_aud_effdsp;
extern int            rtjpeg_aud_framescount;
extern int            rtjpeg_aud_fakeframescount;
extern int            rtjpeg_aud_audiodelay;
extern int            rtjpeg_aud_resample;
extern struct rtfileheader rtjpeg_aud_fileheader;
#endif


int            rtjpeg_aud_open(const char *tplorg);
int            rtjpeg_aud_close(void);
int            rtjpeg_aud_get_video_width(void);
int            rtjpeg_aud_get_video_height(void);
double         rtjpeg_aud_get_video_frame_rate(void);
// unsigned char *rtjpeg_aud_get_frame(void);
unsigned char *rtjpeg_aud_get_frame(int fakenumber, int *timecode, int onlyvideo,
                                unsigned char **audiodata, int *alen);

//unsigned char *rtjpeg_aud_get_frame(int fakenumber, struct rtframeheader **fhp,
//                                int onlyvideo, int *audiolen);

int            rtjpeg_aud_end_of_video(void);
int            rtjpeg_aud_check_sig(char *fname);

