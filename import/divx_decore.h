/**
*  Copyright (C) 2001 - DivXNetworks
 *
 * Adam Li
 * Andrea Graziani
 * Jonathan White
 *
 * DivX Advanced Research Center <darc@projectmayo.com>
*
**/
// decore.h //

// This is the header file describing
// the entrance function of the encoder core
// or the encore ...

#ifndef _DECORE_H_
#define _DECORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define STDCALL _stdcall
#else
#define STDCALL
#endif // WIN32


/**
 *
**/

// decore options
#define DEC_OPT_MEMORY_REQS	0
#define DEC_OPT_INIT		1
#define DEC_OPT_RELEASE		2
#define DEC_OPT_SETPP		3 // set postprocessing mode
#define DEC_OPT_SETOUT		4 // set output mode
#define DEC_OPT_FRAME		5
#define DEC_OPT_GAMMA		7
#define DEC_OPT_VERSION		8
#define DEC_OPT_SETPP_ADV   10 // advance (personalized) postprocessing settings
#define DEC_OPT_SETDEBUG	11
#define DEC_OPT_INIT_VOL	12
#define DEC_OPT_CONVERTYUV	13
#define DEC_OPT_CONVERTYV12	14
#define DEC_OPT_GETVOLINFO	15

// decore return values
#define DEC_OK			0
#define DEC_MEMORY		1
#define DEC_BAD_FORMAT	2
#define DEC_EXIT		3

// decore YUV color format
#define DEC_YUY2		1
#define DEC_YUV2 		DEC_YUY2
#define DEC_UYVY		2
#define DEC_420			3

// decore RGB color format
#define DEC_RGB32		4
#define DEC_RGB32_INV	5
#define DEC_RGB24		6
#define DEC_RGB24_INV	7
#define DEC_RGB555		8
#define DEC_RGB555_INV	9
#define DEC_RGB565		10
#define DEC_RGB565_INV	11
#define DEC_USER		12
#define DEC_YV12		13
#define DEC_ARGB		14

#define DECORE_VERSION		20020303

#define DEC_GAMMA_BRIGHTNESS 	0
#define DEC_GAMMA_CONTRAST 		1
#define DEC_GAMMA_SATURATION 	2
/**
 *
**/

typedef struct
{
	unsigned long mp4_edged_ref_buffers_size;
	unsigned long mp4_edged_for_buffers_size;
	unsigned long mp4_edged_back_buffers_size;
	unsigned long mp4_display_buffers_size;
	unsigned long mp4_state_size;
	unsigned long mp4_tables_size;
	unsigned long mp4_stream_size;
	unsigned long mp4_reference_size;
} DEC_MEM_REQS;

typedef struct
{
	void * mp4_edged_ref_buffers;
	void * mp4_edged_for_buffers;
	void * mp4_edged_back_buffers;
	void * mp4_display_buffers;
	void * mp4_state;
	void * mp4_tables;
	void * mp4_stream;
	void * mp4_reference;
} DEC_BUFFERS;

typedef struct
{
	int x_dim; // x dimension of the frames to be decoded
	int y_dim; // y dimension of the frames to be decoded
	int output_format;	// output color format
	int time_incr;
	int codec_version;
	int build_number;
	DEC_BUFFERS buffers;
} DEC_PARAM;

typedef struct
{
	void *bmp; // decoded bitmap
	const void *bitstream; // decoder buffer
	long length; // length of the decoder stream
	int render_flag;	// 1: the frame is going to be rendered
	unsigned int stride; // decoded bitmap stride
} DEC_FRAME;

typedef struct
{
	int intra;
	const int *quant_store;
	int quant_stride;
} DEC_FRAME_INFO;

typedef struct
{
	int postproc_level; // valid interval are [0..100]

	int deblock_hor_luma;
	int deblock_ver_luma;
	int deblock_hor_chr;
	int deblock_ver_chr;
	int dering_luma;
	int dering_chr;

	int pp_semaphore;

} DEC_SET;

typedef struct
{
	void *y;
	void *u;
	void *v;
	int stride_y;
	int stride_uv;
} DEC_PICTURE;

/**
 *
**/

// the prototype of the decore() - main decore engine entrance
//
int STDCALL decore(
			unsigned long handle,	// handle	- the handle of the calling entity, must be unique
			unsigned long dec_opt, // dec_opt - the option for docoding, see below
			void *param1,	// param1	- the parameter 1 (it's actually meaning depends on dec_opt
			void *param2);	// param2	- the parameter 2 (it's actually meaning depends on dec_opt

typedef int (STDCALL *decoreFunc)(unsigned long handle,	// handle	- the handle of the calling entity, must be unique
						  unsigned long dec_opt,   // dec_opt  - the option for docoding, see below
						  void *param1,         	// param1	- the parameter 1 (it's actually meaning depends on dec_opt
						  void *param2);	        // param2	- the parameter 2 (it's actually meaning depends on dec_opt

#ifdef __cplusplus
}
#endif

#endif // _DECORE_H_

