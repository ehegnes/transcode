#ifndef _ENCORE_ENCORE_H
#define _ENCORE_ENCORE_H
// This is the header file describing 
// the entrance function of the encoder core
// or the encore ...

#ifdef __cplusplus
extern "C" {
#endif 

/**
    Structure passed as an argument when creating encoder.
    You have to initialize at least x_dim and y_dim ( valid range:
	0<x_dim<=720, 0<y_dim<=576, both dimensions should be even ).
    You can set all other values to 0, in which case they'll be initialized
    to default values, or specify them directly.
    On success 'handle' member will contain non-zero handle to initialized 
    encoder.
**/    
typedef struct _ENC_PARAM_ 
{
	int x_dim;		// the x dimension of the frames to be encoded
	int y_dim;		// the y dimension of the frames to be encoded
	float framerate;        // the frame rate of the sequence to be encoded, in frames/second
	int bitrate;	        // the bitrate of the target encoded stream, in bits/second
	int rc_period;          // the intended rate control averaging period
	int rc_reaction_period; // the reaction period for rate control
	int rc_reaction_ratio;  // the ratio for down/up rate control
	int max_quantizer;      // the upper limit of the quantizer
	int min_quantizer;      // the lower limit of the quantizer
	int max_key_interval;   // the maximum interval between key frames
	int quality;		// the quality of compression ( 1 - fastest, 5 - best )
	
	void* handle;		// will be filled by encore
} ENC_PARAM;

/**
    Structure passed as a first argument when encoding a frame.
    Both pointers should be non-NULL. You are responsible for allocation
    of bitstream buffer, its size should be at large enough to hold a frame
    ( no checks for buffer overflow will be done ).
    On success 'length' will contain number of bytes written into the stream.
**/    
typedef struct _ENC_FRAME_ 
{
        void *image;	// the image frame to be encoded
	void *bitstream;// the buffer for encoded bitstream
	int length;	// the length of the encoded bitstream
	int colorspace;	// the format of image frame
} ENC_FRAME;

/**
    Structure passed as a second optional argument when encoding a frame.
    On successful return its members are filled with parameters of encoded
    stream.
**/    
typedef struct _ENC_RESULT_ 
{
	int is_key_frame; // the current frame is encoded as a key frame
} ENC_RESULT;

// the prototype of the encore() - main encode engine entrance
int encore(void* handle,        // handle		- the handle of the calling entity, must be unique
	int enc_opt,	        // enc_opt		- the option for encoding, see below
	void *param1,	        // param1		- the parameter 1 (its actually meaning depends on enc_opt
	void *param2);	        // param2		- the parameter 2 (its actually meaning depends on enc_opt

// encore options (the enc_opt parameter of encore())
#define ENC_OPT_INIT    0	// initialize the encoder, return a handle
#define ENC_OPT_RELEASE 1	// release all the resource associated with the handle
#define ENC_OPT_ENCODE  2       // encode a single frame
// return code of encore()
#define ENC_FAIL		-1
#define ENC_OK			0
#define	ENC_MEMORY		1
#define ENC_BAD_FORMAT		2

#define ENC_CSP_RGB24 	0
#define ENC_CSP_YV12	1

#ifdef __cplusplus
}
#endif 

#endif
