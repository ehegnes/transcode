/**********************************************************************
Copyright (c) 1991 MPEG/audio software simulation group, All Rights Reserved
common.h
**********************************************************************/
/**********************************************************************
 * MPEG/audio coding/decoding software, work in progress              *
 *   NOT for public distribution until verified and approved by the   *
 *   MPEG/audio committee.  For further information, please contact   *
 *   Davis Pan, 508-493-2241, e-mail: pan@gauss.enet.dec.com          *
 *                                                                    *
 * VERSION 4.0                                                        *
 *   changes made since last update:                                  *
 *   date   programmers         comment                               *
 * 2/25/91  Doulas Wong,        start of version 1.0 records          *
 *          Davis Pan                                                 *
 * 5/10/91  W. Joseph Carter    Reorganized & renamed all ".h" files  *
 *                              into "common.h" and "encoder.h".      *
 *                              Ported to Macintosh and Unix.         *
 *                              Added additional type definitions for *
 *                              AIFF, double/SANE and "bitstream.c".  *
 *                              Added function prototypes for more    *
 *                              rigorous type checking.               *
 * 27jun91  dpwe (Aware)        Added "alloc_*" defs & prototypes     *
 *                              Defined new struct 'frame_params'.    *
 *                              Changed info.stereo to info.mode_ext  *
 *                              #define constants for mode types      *
 *                              Prototype arguments if PROTO_ARGS     *
 * 5/28/91  Earle Jennings      added MS_DOS definition               *
 *                              MsDos function prototype declarations *
 * 7/10/91  Earle Jennings      added FLOAT definition as double      *
 *10/ 3/91  Don H. Lee          implemented CRC-16 error protection   *
 * 2/11/92  W. Joseph Carter    Ported new code to Macintosh.  Most   *
 *                              important fixes involved changing     *
 *                              16-bit ints to long or unsigned in    *
 *                              bit alloc routines for quant of 65535 *
 *                              and passing proper function args.     *
 *                              Removed "Other Joint Stereo" option   *
 *                              and made bitrate be total channel     *
 *                              bitrate, irrespective of the mode.    *
 *                              Fixed many small bugs & reorganized.  *
 *                              Modified some function prototypes.    *
 *                              Changed BUFFER_SIZE back to 4096.     *
 * 7/27/92  Michael Li          (re-)Ported to MS-DOS                 *
 * 7/27/92  Masahiro Iwadare    Ported to Convex                      *
 * 8/07/92  mc@tv.tek.com                                             *
 * 8/10/92  Amit Gulati         Ported to the AIX Platform (RS6000)   *
 *                              AIFF string constants redefined       *
 * 8/27/93 Seymour Shlien,      Fixes in Unix and MSDOS ports,        *
 *         Daniel Lauzon, and                                         *
 *         Bill Truerniet                                             *
 **********************************************************************/


/***********************************************************************
*
*  Global Definitions
*
***********************************************************************/

/* General Definitions */

#define         FALSE                   0
#define         TRUE                    1
#define         WORD                    16
#define         PI                      3.14159265358979
#define         PI64                    PI/64
#define         LN_TO_LOG10             0.2302585093
#define         MPEG_AUDIO_ID           1
#define         MONO                    1
#define         STEREO                  2
#define         SBLIMIT                 32
#define         FFT_SIZE                1024
#define         HAN_SIZE                512
#define         SCALE_BLOCK             12
#define         SCALE_RANGE             64
#define         SCALE                   32768
#define         CRC16_POLYNOMIAL        0x8005

/* MPEG Header Definitions - Mode Values */

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

/***********************************************************************
*
*  Global Type Definitions
*
***********************************************************************/

/* Structure for Reading Layer II Allocation Tables from File */

typedef struct {
    unsigned int    steps;
    unsigned int    bits;
    unsigned int    group;
    unsigned int    quant;
} sb_alloc, *alloc_ptr;

typedef sb_alloc        bb_al_table[SBLIMIT][16];

/* Header Information Structure */

typedef struct {
    int version;
    int lay;
    int error_protection;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int extension;
    int mode;
    int mode_ext;
    int copyright;
    int original;
    int emphasis;
} layer, *the_layer;

/* Parent Structure Interpreting some Frame Parameters in Header */

typedef struct {
    layer       *header;        /* raw header information */
    int         actual_mode;    /* when writing IS, may forget if 0 chs */
    bb_al_table *alloc;         /* bit allocation table read in */
    int         tab_num;        /* number of table as loaded */
    int         stereo;         /* 1 for mono, 2 for stereo */
    int         jsbound;        /* first band of joint stereo coding */
    int         sblimit;        /* total number of sub bands */
} frame_params;

/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/

/* The following functions are in the file "common.c" */

FILE           *OpenTableFile(char*);
int            read_bit_alloc(int, bb_al_table **);
int            pick_table(frame_params*);
int            js_bound(int, int);
void           hdr_to_frps(frame_params*);
void           I_CRC_calc(frame_params*, unsigned int[2][SBLIMIT],
                        unsigned int*);
void           II_CRC_calc(frame_params*, unsigned int[2][SBLIMIT],
                        unsigned int[2][SBLIMIT], unsigned int*);
void           update_CRC(unsigned int, unsigned int, unsigned int*);

