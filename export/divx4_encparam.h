#ifndef _DIVX4_ENCPARAM_H
#define _DIVX4_ENCPARAM_H

#ifdef __cplusplus
extern "C"
{
#endif


/* This is the ENC_PARAM structure for the DivX 4.x libraries.  It differs *
 * from the structure used for 5.x so we need to declare it separately     *
 * (under a different name) so we can use whichever is appropriate.        */

typedef struct _DIVX4_ENC_PARAM_
{
	int x_dim;		// the x dimension of the frames to be encoded
	int y_dim;		// the y dimension of the frames to be encoded
	float framerate;	// the frame rate of the sequence to be encoded, in frames/second
	int bitrate;		// the bitrate of the target encoded stream, in bits/second
	int rc_period;		// the intended rate control averaging period
	int rc_reaction_period;	// the reaction period for rate control
	int rc_reaction_ratio;	// the ratio for down/up rate control
	int max_quantizer;	// the upper limit of the quantizer
	int min_quantizer;	// the lower limit of the quantizer
	int max_key_interval;	// the maximum interval between key frames
	int use_bidirect;	// use bidirectional coding
	int deinterlace;	// fast deinterlace
	int quality;		// the quality of compression ( 1 - fastest, 5 - best )
	int obmc;			// flag to enable overlapped block motion compensation mode
	void *handle;		// will be filled by encore
}
DIVX4_ENC_PARAM;


/* This is the DivX binary API version which this structure matches */

#define DIVX4_ENCORE_VERSION		20010807


#ifdef __cplusplus
}
#endif

#endif
