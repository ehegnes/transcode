#ifndef _MAIN_H
#define _MAIN_H 1
//----------------------------------------------------------------------------
//   AVI2MPG main header
//----------------------------------------------------------------------------

#define STRICT

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "config.h"

#ifndef MAXPATH
#define MAXPATH 260
#endif

#ifndef MAXFILE
#define MAXFILE 256
#endif

#ifdef MPEGMAIN
#define EXTERN
#else
#define EXTERN extern
#endif

#define FALSE 0
#define TRUE  1


#define MPEG_MPEG1  0
#define MPEG_VCD    1
#define MPEG_MPEG2  2
#define MPEG_SVCD   3
#define MPEG_DVD    4

#define PRO_MPEG1      '1'
#define PRO_MPEG1_BIG  'b'
#define PRO_VCD        'v'
#define PRO_SVCD       's'
#define PRO_MPEG2      '2'
#define PRO_DVD        'd'

#define COMPBITRATE_NONE 0
#define COMPBITRATE_AVG  1
#define COMPBITRATE_MAX  2

#define PULLDOWN_NONE 0
#define PULLDOWN_23   1
#define PULLDOWN_32   2
#define PULLDOWN_AUTO 3

#define TIMESTAMPS_ALL    0
#define TIMESTAMPS_IPONLY 1
#define TIMESTAMPS_IONLY  2

#define PICTURE_START_CODE 0x100L
#define SLICE_MIN_START    0x101L
#define SLICE_MAX_START    0x1AFL
#define USER_START_CODE    0x1B2L
#define SEQ_START_CODE     0x1B3L
#define EXT_START_CODE     0x1B5L
#define SEQ_END_CODE       0x1B7L
#define GOP_START_CODE     0x1B8L
#define ISO_END_CODE       0x1B9L
#define PACK_START_CODE    0x1BAL
#define SYSTEM_START_CODE  0x1BBL

/* picture coding type */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

/* picture structure */
#define TOP_FIELD     1
#define BOTTOM_FIELD  2
#define FRAME_PICTURE 3

/* macroblock type */
#define MB_INTRA    1
#define MB_PATTERN  2
#define MB_BACKWARD 4
#define MB_FORWARD  8
#define MB_QUANT    16

/* motion_type */
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3

/* mv_format */
#define MV_FIELD 0
#define MV_FRAME 1

/* chroma_format */
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

/* extension start code IDs */
#define SEQ_ID       1
#define DISP_ID      2
#define QUANT_ID     3
#define SEQSCAL_ID   5
#define PANSCAN_ID   7
#define CODING_ID    8
#define SPATSCAL_ID  9
#define TEMPSCAL_ID 10

/* inputtype */
#define T_Y_U_V 0
#define T_YUV   1
#define T_PPM   2
#define T_AVI   3

#define MAXM 16
#define MAXN 32

/* Symbolic constants for feature flags in CPUID standard feature flags */

#define CPUID_STD_FPU          0x00000001
#define CPUID_STD_VME          0x00000002
#define CPUID_STD_DEBUGEXT     0x00000004
#define CPUID_STD_4MPAGE       0x00000008
#define CPUID_STD_TSC          0x00000010
#define CPUID_STD_MSR          0x00000020
#define CPUID_STD_PAE          0x00000040
#define CPUID_STD_MCHKXCP      0x00000080
#define CPUID_STD_CMPXCHG8B    0x00000100
#define CPUID_STD_APIC         0x00000200
#define CPUID_STD_SYSENTER     0x00000800
#define CPUID_STD_MTRR         0x00001000
#define CPUID_STD_GPE          0x00002000
#define CPUID_STD_MCHKARCH     0x00004000
#define CPUID_STD_CMOV         0x00008000
#define CPUID_STD_PAT          0x00010000
#define CPUID_STD_PSE36        0x00020000
#define CPUID_STD_MMX          0x00800000
#define CPUID_STD_FXSAVE       0x01000000
#define CPUID_STD_SSE          0x02000000

/* Symbolic constants for feature flags in CPUID extended feature flags */

#define CPUID_EXT_3DNOW        0x80000000
#define CPUID_EXT_AMD_3DNOWEXT 0x40000000
#define CPUID_EXT_AMD_MMXEXT   0x00400000

/* Symbolic constants for application specific feature flags */

#define FEATURE_CPUID          0x00000001
#define FEATURE_STD_FEATURES   0x00000002
#define FEATURE_EXT_FEATURES   0x00000004
#define FEATURE_TSC            0x00000010
#define FEATURE_MMX            0x00000020
#define FEATURE_CMOV           0x00000040
#define FEATURE_3DNOW          0x00000080
#define FEATURE_3DNOWEXT       0x00000100
#define FEATURE_MMXEXT         0x00000200
#define FEATURE_SSEFP          0x00000400
#define FEATURE_K6_MTRR        0x00000800
#define FEATURE_P6_MTRR        0x00001000

/* MMX types */
#define MODE_NONE      0
#define MODE_MMX       1
#define MODE_3DNOW     2
#define MODE_SSE       3
#define MODE_3DNOWEXT  4

/* bitstream stuff */
#define BUFFER_SIZE   256*1024

typedef struct bitstream {
  unsigned char *bfr;
  unsigned char outbyte;
  int byteidx;
  int bitidx;
  int bufcount;
  fpos_t actpos;
  double totbits;
  FILE *bitfile;
  int eobs;
  int fileOutError;
} bitstream;

/* macroblock information */
struct mbinfo {
  int mb_type; /* intra/forward/backward/interpolated */
  int motion_type; /* frame/field/16x8/dual_prime */
  int dct_type; /* field/frame DCT */
  int mquant; /* quantization parameter */
  int cbp; /* coded block pattern */
  int skipped; /* skipped macroblock */
  int MV[2][2][2]; /* motion vectors */
  int mv_field_sel[2][2]; /* motion vertical field select */
  int dmvector[2]; /* dual prime vectors */
  double act; /* activity measure */
  int var; /* for debugging */
};

/* motion data */
struct motion_data {
  int forw_hor_f_code,forw_vert_f_code; /* vector range */
  int sxf,syf; /* search range */
  int back_hor_f_code,back_vert_f_code;
  int sxb,syb;
};

/* type definitions for variable length code table entries */

typedef struct
{
  unsigned char code; /* right justified */
  char len;
} VLCtable;

/* for codes longer than 8 bits (excluding leading zeroes) */
typedef struct
{
  unsigned short code; /* right justified */
  char len;
} sVLCtable;

typedef struct mpegOutSettings
{
  /* general settings */
  int useFP;
  int verbose;
  int denoise;
  
  /* control flags */
  int MplexVideo; /* multiplex video flag */
  int MplexAudio; /* multiplex audio flag */
  int UserEncodeVideo;
  int UserEncodeAudio;
  int EncodeVideo; /* encode video flag */
  int EncodeAudio; /* encode audio flag */
  int SaveTempVideo; /* 1 = do not remove temp video file after multiplexing */
  int SaveTempAudio; /* 1 = do not remove temp audio file after multiplexing */
  int B_W; /* do black and white, i.e. do no chromanance */

  /* name strings */
  char id_string[MAXPATH];
  char iqname[MAXPATH], niqname[MAXPATH];
  char statname[MAXPATH];

  /* coding model parameters */
  int video_type; /* type of video stream, mpeg1, mpeg2, etc */
  int video_pulldown_flag; /* video is 24fps converted to 25.0 or 29.97fps */
  int constrparms;  /* constrained parameters flag (MPEG-1 only) */
  int N; /* number of frames in Group of Pictures */
  int M; /* distance between I/P frames */
  int P; /* intra slice refresh interval */
  int tc0; /* timecode of first frame */
  int hours, mins, sec, tframe;
  int fieldpic; /* use field pictures */
  int write_sec; /* write a sequence end code */
  int embed_SVCD_user_blocks;

  /* sequence specific data (sequence header) */
  int aspectratio; /* aspect ratio information (pel or display) */
  int frame_rate_code; /* coded value of frame rate */
  double frame_rate; /* frames per second */
  double bit_rate; /* bits per second */
  double max_bit_rate; /* maximum bit rate for variable bitrate mode */
  double avg_bit_rate;
  double min_bit_rate;
  int auto_bitrate; /* automatically compute bitrate based on res and frame rate */
  int vbv_buffer_size; /* size of VBV buffer (* 16 kbit) */
  int constant_bitrate; /* constant bitrate flag */
  int mquant_value; /* macroblock quantization value */
  int maxquality; //(min mquant allowed);
  int minquality; //(max mquant allowed unless ypu try to pass the max-bit-rate)

  /* sequence specific data (sequence extension) */
  int profile, level; /* syntax / parameter constraints */
  int prog_seq; /* progressive sequence */
  int chroma_format;
  int low_delay; /* no B pictures, skipped pictures */

  /* motion data */
  struct motion_data motion_data[MAXM];
  int xmotion, ymotion; /* x/y motion from frame to frame in pixels */
  int automotion; /* automatically compute motion vectors based on xmotion, ymotion */
  int maxmotion; /* max motion search windows for variable motion mode */

  /* sequence specific data (sequence display extension) */
  int write_sde; // write a Seq Dsply Ext
  int video_format; /* component, PAL, NTSC, SECAM or MAC */
  int color_primaries; /* source primary chromaticity coordinates */
  int transfer_characteristics; /* opto-electronic transfer char. (gamma) */
  int matrix_coefficients; /* Eg,Eb,Er / Y,Cb,Cr matrix coefficients */
  int display_horizontal_size, display_vertical_size; /* display size */

  /* picture specific data (picture coding extension) */
  int dc_prec; /* DC coefficient precision for intra coded blocks */
  int topfirst; /* display top field first */

  /* picture display extension */
  int write_pde;
  int frame_centre_horizontal_offset;
  int frame_centre_vertical_offset;

  /* use only frame prediction and frame DCT (I,P,B,current) */
  int frame_pred_dct_tab[3];
  int conceal_tab[3]; /* use concealment motion vectors (I,P,B) */
  int qscale_tab[3]; /* linear/non-linear quantizaton table */
  int intravlc_tab[3]; /* intra vlc format (I,P,B,current) */
  int altscan_tab[3]; /* alternate scan (I,P,B,current) */
  int repeatfirst; /* repeat first field after second field */
  int prog_frame; /* progressive frame */

  /* rate control vars */
  int Xi;
  int Xp;
  int Xb;
  int r;
  int d0i;
  int d0p;
  int d0b;
  int reset_d0pb;
  int avg_act;
  int fixed_vbv_delay; /* force a 0xffff vbv_delay */
  int min_frame_percent;
  int pad_frame_percent;

  /* audio vars */
  int audio_mode;
  int audio_layer;
  int psych_model;
  int audio_bitrate;
  int emphasis;
  int extension;
  int error_protection;
  int copyright;
  int original;

  /* multiplex vars */
  unsigned long sectors_delay;
  unsigned long video_delay_ms;
  unsigned long audio_delay_ms;
  unsigned long audio1_delay_ms;
  unsigned long sector_size;
  unsigned long packets_per_pack;
  unsigned long audio_buffer_size;
  unsigned long audio1_buffer_size;
  unsigned long video_buffer_size;
  int always_sys_header;
  int use_computed_bitrate;
  int mplex_type; /* type of program stream, mpeg1, mpeg2, etc */
  int mplex_pulldown_flag; /* video stream is 24fps converted to 25.0 or 29.97fps */
  int vcd_audio_pad; /* pad each vcd audio sector with zeros */
  int align_sequence_headers; /* align sequence headers to a sector */
  int user_mux_rate; /* force a particular mux rate */
  int put_private2; /* put private stream 2's instead of padding packets */
  int frame_timestamps; /* which frames to timestamp */
  int VBR_multiplex;
  int write_pec; /* write a program end code */
  int mux_SVCD_scan_offsets;
  unsigned int max_file_size;
  unsigned int mux_start_time;
  unsigned int mux_stop_time;
  int reset_clocks;
  int write_end_codes; /* write program end codes in split files */
  int set_broken_link;
} mpegOutSettings;

#include "common.h"
#include "mplex.h"
//#include "encaudio.h"

EXTERN int B_W;
EXTERN int PALDefaults;
EXTERN int BatchMode;
EXTERN int UseAdobeAPI;
EXTERN int MMXMode;
EXTERN unsigned int MMXAvail;
EXTERN int UseFP;
EXTERN struct mpegOutSettings tempSettings1;
EXTERN struct mpegOutSettings tempSettings2;
EXTERN int bb_verbose;
EXTERN int bb_denoise;
EXTERN int FileOutError;
EXTERN int AbortMPEG;
EXTERN int VideoAvail;
EXTERN int vVideoCDAvail;
EXTERN int vSVCDAvail;
EXTERN int aVideoCDAvail;
EXTERN int aSVCDAvail;
EXTERN int vDVDAvail;
EXTERN int aDVDAvail;
EXTERN int AudioAvail;
EXTERN int InvalidVideo;
EXTERN int InvalidAudio;
EXTERN char VideoFilename[255];
EXTERN char AudioFilename[255];
EXTERN char Audio1Filename[255];
EXTERN char ProgramFilename[255];
EXTERN struct bitstream videobs;
EXTERN struct bitstream audiobs;
EXTERN unsigned int currentFrame;
EXTERN unsigned int currentSample;
EXTERN unsigned int nSamples;
EXTERN int vbvUnderflows;
EXTERN int vbvOverflows;
EXTERN unsigned int min_bitrate, max_bitrate;
EXTERN unsigned int min_frame_bitrate, max_frame_bitrate;
EXTERN unsigned int min_mquant, max_mquant;
EXTERN double avg_mquant;
EXTERN double paddingSum;   // video padding bits (cumulative)
EXTERN unsigned int maxPadding; // max video padding bits in  frame
EXTERN double headerSum;        // cumulative sum of video header bits

/* control flags */
EXTERN int MplexVideo;
EXTERN int MplexAudio;
EXTERN int UserEncodeVideo;
EXTERN int UserEncodeAudio;
EXTERN int EncodeVideo;
EXTERN int EncodeAudio;
EXTERN int SaveTempVideo;
EXTERN int SaveTempAudio;

/* audio stuff */
EXTERN int audio_mode;
EXTERN int audio_layer;
EXTERN int psych_model;
EXTERN int audio_bitrate;
EXTERN int emphasis;
EXTERN int extension;
EXTERN int error_protection;
EXTERN int copyright;
EXTERN int original;
EXTERN unsigned long audioSampleRate;
EXTERN int audioStereo;
EXTERN int audioSampleSize;

/* multiplex stuff */
EXTERN unsigned long sectors_delay;
EXTERN unsigned long video_delay_ms;
EXTERN unsigned long audio_delay_ms;
EXTERN unsigned long audio1_delay_ms;
EXTERN unsigned long sector_size;
EXTERN unsigned long packets_per_pack;
EXTERN unsigned long init_audio_buffer_size;
EXTERN unsigned long init_audio1_buffer_size;
EXTERN unsigned long init_video_buffer_size;
EXTERN int always_sys_header;
EXTERN int use_computed_bitrate;
EXTERN int mplex_type;
EXTERN int vcd_audio_pad;
EXTERN int mplex_pulldown_flag;
EXTERN int align_sequence_headers;
EXTERN int user_mux_rate;
EXTERN int put_private2;
EXTERN int frame_timestamps; /* which frames to timestamp */
EXTERN int VBR_multiplex;
EXTERN int write_pec; /* write a program end code */
EXTERN int mux_SVCD_scan_offsets;
EXTERN unsigned int max_file_size;
EXTERN unsigned int mux_start_time;
EXTERN unsigned int mux_stop_time;
EXTERN int reset_clocks;
EXTERN int write_end_codes; /* write program end codes in split files */
EXTERN int set_broken_link;

    /* reconstructed frames */
EXTERN unsigned char *newrefframe[3], *oldrefframe[3], *auxframe[3], *unewrefframe[3], *uoldrefframe[3], *uauxframe[3];
    /* original frames */
EXTERN unsigned char *neworgframe[3], *oldorgframe[3], *auxorgframe[3], *uneworgframe[3], *uoldorgframe[3], *uauxorgframe[3];
    /* prediction of current frame */
EXTERN unsigned char *predframe[3], *upredframe[3];
    /* 8*8 block data */
EXTERN short (*blocks)[64], (*ublocks)[64];
    /* intra / non_intra quantization matrices */

// N.b. quantisation matrix elements now shorts (x86 WORD) *not* char (BYTE)
// The i_ versions are inverses * IQUANT_SCALE...

EXTERN unsigned short s_inter_q[64];
EXTERN unsigned short i_inter_q[64];


/* Scale factor for fast integer arithmetic routines */
/* Changed this and you *must* change the quantisation routines
   as they depend on its absolute value
 */

#define IQUANT_SCALE_POW2 16
#define IQUANT_SCALE (1<<IQUANT_SCALE_POW2)

EXTERN unsigned char intra_q[64], inter_q[64];
EXTERN unsigned char chrom_intra_q[64],chrom_inter_q[64];
    /* prediction values for DCT coefficient (0,0) */
EXTERN int dc_dct_pred[3], udc_dct_pred[3];
    /* macroblock side information array */
EXTERN struct mbinfo *mbinfo;
EXTERN struct mbinfo *umbinfo;

    /* motion estimation parameters */
EXTERN struct motion_data motion_data[MAXM], umotion_data[MAXM];
EXTERN int xmotion, ymotion, uxmotion, uymotion;
EXTERN int automotion, uautomotion;
EXTERN int maxmotion, umaxmotion;
EXTERN int submotiop, submotiob, Sxf, Syf, Sxb, Syb;

    /* clipping (=saturation) table */
EXTERN unsigned char *clp;
EXTERN unsigned char *orgclp;

    /* name strings */
EXTERN char id_string[256], tplorg[256], tplref[256];
EXTERN char iqname[256], niqname[256];
EXTERN char statname[256];
EXTERN int OutputStats;
EXTERN FILE *statfile; /* file descriptors */
EXTERN char errortext[256];

EXTERN int repeatCount;

    /* coding model parameters */

EXTERN int N; /* number of frames in Group of Pictures */
EXTERN int M; /* distance between I/P frames */
EXTERN int P; /* intra slice refresh interval */
EXTERN int nframes; /* total number of frames to encode */
EXTERN int frame0, tc0; /* number and timecode of first frame */
EXTERN int hours, mins, sec, tframe;
EXTERN int video_type; /* mpeg type to encode, mpeg-1, vcd, mpeg-2 etc. */
EXTERN int video_pulldown_flag; /* convert 24fps to 29.97fps */
EXTERN int fieldpic; /* use field pictures */
EXTERN int write_sec; /* write a sequence end code */
EXTERN int embed_SVCD_user_blocks;

    /* sequence specific data (sequence header) */

EXTERN int input_horizontal_size, input_vertical_size;
EXTERN int horizontal_size, vertical_size; /* frame size (pels) */
EXTERN int width, height; /* encoded frame size (pels) multiples of 16 or 32 */
EXTERN int chrom_width,chrom_height,block_count, ublock_count;
EXTERN int mb_width, mb_height; /* frame size (macroblocks) */
EXTERN int width2, height2, mb_height2, chrom_width2; /* picture size */
EXTERN int aspectratio; /* aspect ratio information (pel or display) */
EXTERN int frame_rate_code; /* coded value of frame rate */
EXTERN double frame_rate; /* frames per second */
EXTERN double bit_rate; /* bits per second */
EXTERN double max_bit_rate; /* max bitrate for variable bitrate mode */
EXTERN double avg_bit_rate;
EXTERN double min_bit_rate;
EXTERN int auto_bitrate; /* auto compute bitrate */
EXTERN int vbv_buffer_size; /* size of VBV buffer (* 16 kbit) */
EXTERN int constant_bitrate; /* constant bitrate flag */
EXTERN int mquant_value; /* macroblock quantization value */

EXTERN int constrparms; /* constrained parameters flag (MPEG-1 only) */
EXTERN int load_iquant, load_niquant; /* use non-default quant. matrices */
EXTERN int load_ciquant,load_cniquant;

    /* sequence specific data (sequence extension) */

EXTERN int profile, level; /* syntax / parameter constraints */
EXTERN int prog_seq; /* progressive sequence */
EXTERN int chroma_format;
EXTERN int low_delay; /* no B pictures, skipped pictures */


    /* sequence specific data (sequence display extension) */
EXTERN int write_sde; // write a Seq Dsply Ext
EXTERN int video_format; /* component, PAL, NTSC, SECAM or MAC */
EXTERN int color_primaries; /* source primary chromaticity coordinates */
EXTERN int transfer_characteristics; /* opto-electronic transfer char. (gamma) */
EXTERN int matrix_coefficients; /* Eg,Eb,Er / Y,Cb,Cr matrix coefficients */
EXTERN int display_horizontal_size, display_vertical_size; /* display size */


    /* picture specific data (picture header) */

EXTERN int temp_ref; /* temporal reference */
EXTERN int pict_type; /* picture coding type (I, P or B) */
EXTERN int vbv_delay; /* video buffering verifier delay (1/90000 seconds) */


    /* picture specific data (picture coding extension) */

EXTERN int forw_hor_f_code, forw_vert_f_code;
EXTERN int back_hor_f_code, back_vert_f_code; /* motion vector ranges */
EXTERN int dc_prec; /* DC coefficient precision for intra coded blocks */
EXTERN int pict_struct; /* picture structure (frame, top / bottom field) */
EXTERN int topfirst; /* display top field first */

    /* picture display extension */
EXTERN int write_pde;
EXTERN int frame_centre_horizontal_offset;
EXTERN int frame_centre_vertical_offset;

    /* use only frame prediction and frame DCT (I,P,B,current) */
EXTERN int frame_pred_dct_tab[3], frame_pred_dct;
EXTERN int conceal_tab[3]; /* use concealment motion vectors (I,P,B) */
EXTERN int qscale_tab[3], q_scale_type; /* linear/non-linear quantizaton table */
EXTERN int intravlc_tab[3], intravlc; /* intra vlc format (I,P,B,current) */
EXTERN int altscan_tab[3], altscan; /* alternate scan (I,P,B,current) */
EXTERN int repeatfirst; /* repeat first field after second field */
EXTERN int prog_frame; /* progressive frame */

/* rate control vars */

EXTERN int init_Xi;
EXTERN int init_Xp;
EXTERN int init_Xb;
EXTERN int init_r;
EXTERN int init_d0i;
EXTERN int init_d0p;
EXTERN int init_d0b;
EXTERN int reset_d0pb;
EXTERN double init_avg_act;
EXTERN int fixed_vbv_delay; /* force a 0xffff vbv_delay */
EXTERN int min_frame_percent;
EXTERN int pad_frame_percent;

EXTERN unsigned char *ubuffer;

EXTERN int tmp_prog_frame;
EXTERN int tmp_topfirst;
EXTERN int tmp_repeatfirst;
EXTERN int slice_hdr_every_MBrow;

/* general routines, these need to be provided by the application */
/* they can just be stubs                                         */
void DisplayError(char *txt);
void DisplayWarning(char *txt);
void DisplayInfo(char *txt);
void DisplayProgress(char *txt, int percent);

/* mmxasm.p - MMX stuff */
unsigned int get_feature_flags();
void GetMMXMode();
void init_rgb_to_yuv_mmx(int coeffs);
void RGBtoYUVmmx(unsigned char *src, unsigned char *desty, unsigned char *destu,
                 unsigned char *destv, int srcrowsize, int destrowsize,
                 int width, int height);
int dist1mmx(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h, int distlim);
int dist1sse(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h, int distlim);
int dist2mmx(unsigned char *blk1, unsigned char *blk2,
  int lx, int hx, int hy, int h);
int bdist1mmx(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);
int bdist1sse(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);
int bdist2mmx(unsigned char *pf, unsigned char *pb,
  unsigned char *p2, int lx, int hxf, int hyf, int hxb, int hyb, int h);
int variancemmx(unsigned char *p, int lx);
void sub_pred_mmx(unsigned char *pred, unsigned char *cur, int lx, short *blk);
void add_pred_mmx(unsigned char *pred, unsigned char *cur, int lx, short *blk);
int edist1mmx(unsigned char *blk1, unsigned char *blk2,
  int lx, int distlim);
int edist1sse(unsigned char *blk1, unsigned char *blk2,
  int lx, int distlim);

/* bitstream stuff */

/* bits */
int init_putbits(struct bitstream *bs, char *bs_filename);
void finish_putbits(struct bitstream *bs);
int init_getbits(struct bitstream *bs, char *bs_filename);
void finish_getbits(struct bitstream *bs);
int get1bit(struct bitstream *bs, unsigned int *retval);
int getbits(struct bitstream *bs, unsigned int *retval, int N);
void putbits(struct bitstream *bs, int val, int n);
void put1bit(struct bitstream *bs, int val);
void alignbits(struct bitstream *bs);
void prepareundo(struct bitstream *bs, struct bitstream *undo);
void undochanges(struct bitstream *bs, struct bitstream *old);
double bitcount(struct bitstream *bs);
int end_bs(struct bitstream *bs);
int seek_sync(struct bitstream *bs, unsigned int sync, int N);

/* verify settings routines */

/* conform.p */
int CheckVideoSettings(struct mpegOutSettings *set);
int CheckAudioSettings(struct mpegOutSettings *set);
void input_range_checks();
int range_checks(struct mpegOutSettings *set);
int profile_and_level_checks(struct mpegOutSettings *set);

/* video stuff */

/* fdct_mmx.s */
void bb_fdct_mmx(short *block);

/* idct_mmx.s */
void bb_idct_mmx(short *block);

/* fdctref.p */
void bb_init_fdct();
void bb_fdct(short *block);
void bb_intfdct(short *block);

/* idct.p */
void bb_idct(short *block);
void bb_init_idct();

/* motion.c */
void init_motion_est(int sh_info);
int init_motion_est2(int sh_info);
void finish_motion_est();
int motion_estimation(unsigned char *oldorg, unsigned char *neworg,
      unsigned char *oldref, unsigned char *newref, unsigned char *cur,
      unsigned char *curref, int sxf, int syf, int sxb, int syb,
      struct mbinfo *mbi, int secondfield, int ipflag);

/* predict.c */
void predict(unsigned char *reff[], unsigned char *refb[],
      unsigned char *cur[3], int secondfield, struct mbinfo *mbi);
void init_predict(int sh_info);

/* puthdr.p*/
void putseqhdr_all();
void putseqhdr();
void putseqext();
void putseqdispext();
void putuserdata(char *userdata);
void putgophdr(int frame, int closed_gop);
void putpicthdr();
void putpictcodext();
void putpictdispext();
void putseqend();

/* putmpg.p */
int putintrablk(short *blk, int cc);
int putnonintrablk(short *blk);
void putmv(int dmv, int f_code);

/* putpic.p */
int putpict(unsigned char *frame);

/* putvlc.p */
int putDClum(int val);
int putDCchrom(int val);
int putACfirst(int run, int val);
int putAC(int run, int signed_level, int vlcformat);
void putaddrinc(int addrinc);
void putmbtype(int pict_type, int mb_type);
void putmotioncode(int motion_code);
void putdmv(int dmv);
void putcbp(int cbp);

/* quant_mmx.s */
int bb_quant_ni_mmx(short *dst, short *src,
                    unsigned short *quant_mat, unsigned short *i_quant_mat,
                    int imquant, int mquant, int sat_limit);

/* quantize.p */
extern int quant_intra(short *src, short *dst, int dc_prec,
      unsigned char *quant_mat, int mquant);
extern int quant_non_intra(short *src, short *dst,
      unsigned short *quant_mat, unsigned short *i_quant_mat, int mquant);
extern void iquant_intra(short *src, short *dst, int dc_prec,
      unsigned char *quant_mat, int mquant);
extern void iquant_non_intra(short *src, short *dst,
      unsigned char *quant_mat, int mquant);

/* ratectl.p */
void calc_actj(unsigned char *frame);
void save_rc_max();
void restore_rc_max();
void rc_init_seq();
void rc_update_max();
void rc_init_GOP(int np, int nb);
void rc_init_pict(unsigned char *frame);
void rc_update_pict();
int rc_start_mb();
void update_mq(int q);
int rc_calc_mquant(int j);
void vbv_end_of_picture();
void calc_vbv_delay();

/* readpic.p */
int init_readframe();
void end_readframe();
int readframe(unsigned char *frame[], int fnum);

/* settings.p */
//int  DoSettings(HWND parent, int initPage);
//int  InitSettings(HINSTANCE instance);
void PutTempSettings(struct mpegOutSettings *set);
void GetTempSettings(struct mpegOutSettings *set);
void SetMPEG2Defaults(struct mpegOutSettings *set, int palDefaults);
void SetMPEG2Mplex(struct mpegOutSettings *set);
void SetMPEG1Defaults(struct mpegOutSettings *set, int palDefaults);
void SetMPEG1Mplex(struct mpegOutSettings *set);
void SetVCDDefaults(struct mpegOutSettings *set, int palDefaults);
void SetVCDMplex(struct mpegOutSettings *set);
void SetSVCDDefaults(struct mpegOutSettings *set, int palDefaults);
void SetSVCDMplex(struct mpegOutSettings *set);
void SetDVDDefaults(struct mpegOutSettings *set, int palDefaults);
void SetDVDMplex(struct mpegOutSettings *set);
void ChangeVideoFilename(struct mpegOutSettings *set);
void AutoSetMotionData(struct mpegOutSettings *set);
void AutoSetBitrateData(struct mpegOutSettings *set);
int  ReadSettings(char *filename, struct mpegOutSettings *set);
int  WriteSettings(char *filename, struct mpegOutSettings *set);

/* stats.p */
void calcSNR(unsigned char *org[3], unsigned char *rec[3]);
void stats();

/* transfrm.p */
void init_transform(int sh_info);
void transform(unsigned char *pred[], unsigned char *cur[],
      struct mbinfo *mbi, short blocks[][64]);
void itransform(unsigned char *pred[], unsigned char *cur[],
      struct mbinfo *mbi, short blocks[][64]);
void dct_type_estimation(unsigned char *pred, unsigned char *cur,
      struct mbinfo *mbi);

#endif

