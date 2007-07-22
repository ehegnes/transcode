/*
 *  framebuffer.h
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updates and Enhancements
 *  (C) 2007 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <pthread.h>

#include "tc_defaults.h"

/* frame attributes */
typedef enum tcframeattributes_ TCFrameAttributes;
enum tcframeattributes_ {
    TC_FRAME_IS_KEYFRAME       =   1,
    TC_FRAME_IS_INTERLACED     =   2,
    TC_FRAME_IS_BROKEN         =   4,
    TC_FRAME_IS_SKIPPED        =   8,
    TC_FRAME_IS_CLONED         =  16,
    TC_FRAME_WAS_CLONED        =  32,
    TC_FRAME_IS_OUT_OF_RANGE   =  64,
    TC_FRAME_IS_DELAYED        = 128,
    TC_FRAME_IS_END_OF_STREAM  = 256,
};

#define TC_FRAME_NEED_PROCESSING(PTR) \
    (!((PTR)->attributes & TC_FRAME_IS_OUT_OF_RANGE) \
     && !((PTR)->attributes & TC_FRAME_IS_END_OF_STREAM))

typedef enum tcframestatus_ TCFrameStatus;
enum tcframestatus_ {
    FRAME_NULL = -1, /* not yet claimed */
    FRAME_EMPTY = 0, /* claimed, but still empty */
    FRAME_READY,
    FRAME_LOCKED,
    FRAME_WAIT,
};

/* FIXME: naming schme deserve a deep review */
typedef enum tcbufferstatus_ TCBufferStatus;
enum tcbufferstatus_ {
    TC_BUFFER_EMPTY = 0, /* claimed frame, but holds null data */
    TC_BUFFER_FULL,      
    TC_BUFFER_NULL,      /* unclaimed frame */
    TC_BUFFER_LOCKED,    /* frame is in filtering stage */
    TC_BUFFER_READY,     /* frame is ready to be encoded */
};

/*
 * NOTE: The following warning will become irrelevant once NMS is
 *       in place, and frame_list_t can go away completely.  --AC
 *       (here's a FIXME tag so we don't forget)
 *
 * BIG FAT WARNING:
 *
 * These structures must be kept in sync: meaning that if you add
 * another field to the vframe_list_t you must add it at the end
 * of the structure.
 *
 * aframe_list_t, vframe_list_t and the wrapper frame_list_t share
 * the same offsets to their elements up to the field "size". That
 * means that when a filter is called with at init time with the
 * anonymouse frame_list_t, it can already access the size.
 *
 *          -- tibit
 */

/* This macro factorizes common frame data fields.
 * Isn't possible to compeltely factor out all frame_list_t fields
 * because video and audio typess uses different names for same field,
 * and existing code relies on this situation.
 * Fixing this is stuff for 1.2.0 and beyond. -- FR.
 */
#define TC_FRAME_COMMON \
    int id;         /* FIXME: comment */ \
    int bufid;      /* buffer id */ \
    int tag;        /* init, open, close, ... */ \
    int filter_id;  /* filter instance to run */ \
    int status;     /* FIXME: comment */ \
    int attributes; /* FIXME: comment */
/* BEWARE: semicolon NOT NEEDED */

/* 
 * Size vs Length
 *
 * Size represent the effective size of audio/video buffer,
 * while length represent the amount of valid data into buffer.
 * Until 1.1.0, there isn't such distinction, and 'size'
 * have approximatively a mixed meaning of above.
 *
 * In the long shot[1] (post-1.1.0) transcode will start
 * intelligently allocate frame buffers based on highest
 * request of all modules (core included) through filter
 * mangling pipeline. This will lead on circumstances on
 * which valid data into a buffer is less than buffer size:
 * think to demuxer->decoder transition or RGB24->YUV420.
 * 
 * There also are more specific cases like a full-YUV420P
 * pipeline with final conversion to RGB24 and raw output,
 * so we can have something like
 *
 * framebuffer size = sizeof(RGB24_frame)
 * after demuxer:
 *     frame length << frame size (compressed data)
 * after decoder:
 *     frame length < frame size (YUV420P smaller than RGB 24)
 * in filtering:
 *      frame length < frame size (as above)
 * after encoding (in fact just colorspace transition):
 *     frame length == frame size (data becomes RGB24)
 * into muxer:
 *     frame length == frame size (as above)
 *
 * In all those cases having a distinct 'lenght' fields help
 * make things nicier and easier.
 *
 * +++
 *
 * [1] in 1.1.0 that not happens due to module interface constraints
 * since we're still bound to Old Module System.
 */

typedef struct frame_list frame_list_t;
struct frame_list {
    TC_FRAME_COMMON

    int codec;   /* codec identifier */

    int size;    /* buffer size avalaible */
    int len;     /* how much data is valid? */

    int param1;  /* v_width or a_rate */
    int param2;  /* v_height or a_bits */
    int param3;  /* v_bpp or a_chan */

    struct frame_list *next;
    struct frame_list *prev;
};


typedef struct vframe_list vframe_list_t;
struct vframe_list {
    TC_FRAME_COMMON
    /* frame physical parameter */
    
    int v_codec;       /* codec identifier */

    int video_size;    /* buffer size avalaible */
    int video_len;     /* how much data is valid? */

    int v_width;
    int v_height;
    int v_bpp;

    struct vframe_list *next;
    struct vframe_list *prev;

    int clone_flag;     
    /* set to N if frame needs to be processed (encoded) N+1 times. */
    int deinter_flag;
    /* set to N for internal de-interlacing with "-I N" */

    uint8_t *video_buf;  /* pointer to current buffer */
    uint8_t *video_buf2; /* pointer to backup buffer */

    int free; /* flag */

    uint8_t *video_buf_RGB[2];

    uint8_t *video_buf_Y[2];
    uint8_t *video_buf_U[2];
    uint8_t *video_buf_V[2];

#ifdef STATBUFFER
    uint8_t *internal_video_buf_0;
    uint8_t *internal_video_buf_1;
#else
    uint8_t internal_video_buf_0[SIZE_RGB_FRAME];
    uint8_t internal_video_buf_1[SIZE_RGB_FRAME];
#endif
};


typedef struct aframe_list aframe_list_t;
struct aframe_list {
    TC_FRAME_COMMON

    int a_codec;       /* codec identifier */

    int audio_size;    /* buffer size avalaible */
    int audio_len;     /* how much data is valid? */

    int a_rate;
    int a_bits;
    int a_chan;

    struct aframe_list *next;
    struct aframe_list *prev;

    uint8_t *audio_buf;

#ifdef STATBUFFER
    uint8_t *internal_audio_buf;
#else
    uint8_t internal_audio_buf[SIZE_PCM_FRAME<<2];
#endif
};

/* 
 * generic pointer type, needed at least by internal code.
 * In the long (long) shot I'd like to use a unique generic
 * data container, like AVPacket (libavcodec) or something like it.
 * -- FR
 */
typedef union tcframeptr_ TCFramePtr;
union tcframeptr_ {
    frame_list_t *generic;
    vframe_list_t *video;
    aframe_list_t *audio;
};

/* 
 * frame*buffer* specifications, needed to properly allocate
 * and initialize single frame buffers
 */
typedef struct tcframespecs_ TCFrameSpecs;
struct tcframespecs_ {
    int frc;   /* frame ratio code is more precise than value */

    /* video fields */
    int width;
    int height;
    int format; /* TC_CODEC_XXX preferred,
                 * CODEC_XXX still supported for compatibility
                 */
    /* audio fields */
    int rate;
    int channels;
    int bits;

    /* private field, used internally */
    double samples;
};

/*
 * tc_ring_framebuffer_get_specs: (NOT thread safe)
 *     Get a pointer to a TCFrameSpecs structure representing current
 *     framebuffer structure. Frame handling code will use those parameters
 *     to allocate framebuffers.
 *
 * Parameters:
 *     None
 * Return Value:
 *     Constant pointer to a TCFrameSpecs structure. There is no need
 *     to *free() this structure.
 */
const TCFrameSpecs *tc_ring_framebuffer_get_specs(void);

/*
 * tc_ring_framebuffer_set_specs: (NOT thread safe)
 *     Setup new framebuffer parameters, to be used by internal framebuffer
 *     code to properly handle frame allocation.
 *     PLEASE NOTE that only allocation performed AFTER calling this function
 *     will use those parameters.
 *     PLEASE ALSO NOTE that is HIGHLY unsafe to mix allocation by changing
 *     TCFrameSpecs in between without freeing ringbuffers. Just DO NOT.
 *
 * Parameters:
 *     Constant pointer to a TCFrameSpecs holding new framebuffer parameters.
 * Return Value:
 *     None
 */
void tc_ring_framebuffer_set_specs(const TCFrameSpecs *specs);

/*
 * vframe_alloc, aframe_alloc: (NOT thread safe)
 *     Allocate respectively a video or audio frame ringbuffer capable to hold
 *     given amount of frames, with a minimum of one.
 *     Each framebuffer is allocated using TCFrameSpecs parameters.
 *     Use vframe_free/aframe_free to release acquired ringbuffers.
 *
 * Parameters:
 *     num: size of ringbuffer to allocate (number of framebuffers holded
 *          in ringbuffer).
 * Return Value:
 *      0: succesfull
 *     !0: error, tipically this means that one (or more) frame
 *         can't be allocated.
 */
int vframe_alloc(int num);
int aframe_alloc(int num);

/*
 * vframe_alloc_single, aframe_alloc_single: (NOT thread safe)
 *     allocate a single framebuffer (respectively, video or audio)
 *     in compliacy with last TCFrameSpecs set.
 *     Those functione are mainly intended to provide a convenient
 *     way to encoder/decoder/whatelse to allocate private framebuffers
 *     without doing any size computation or waste memory.
 *     Returned value can be SAFELY deallocated using
 *     tc_del_video_frame or tc_del_audio_frame.
 *
 * Parameters:
 *      None
 * Return Value:
 *      respectively a pointer to a vframe_list_t or aframe_list_t,
 *      like, tc_new_video_frame() or tc_new_audio_frame() called
 *      with right parameters.
 *      NULL if allocation fails.
 */
vframe_list_t *vframe_alloc_single(void);
aframe_list_t *aframe_alloc_single(void);

/*
 * vframe_free, aframe_free: (NOT thread safe)
 *     release all framebuffer memory acquired respect. for video and
 *     audio frame ringbuffers.
 *     Please remember thet ffter those functions called, almost
 *     all other ringbuffer functions will fail.
 *
 * Parameters:
 *     None
 * Return Value:
 *     None
 */
void vframe_free(void);
void aframe_free(void);

/*
 * vframe_flush, aframe_flush: (NOT thread safe)
 *     flush all framebuffers still in ringbuffer, by marking those as unused.
 *     This will reset ringbuffer to an ampty state, ready to be (re)used again.
 *
 * Parameters:
 *     None
 * Return Value:
 *     None
 */
void vframe_flush(void);
void aframe_flush(void);

/*
 * vframe_register, aframe_register: (thread safe)
 *     Fetch anew empty (FRAME_EMPRTY) framebuffer from ringbuffer and
 *     assign to it the given id for later usage. Meaning of id is defined by
 *     client; common usage (as in decoder) is to use id to attach sequential
 *     number to frames.
 *     The key factor is that ringbuffer internal routines DO NOT uses at all
 *     this value internally.
 *     DO NOT *free() returned pointer! Use respectively vframe_remove
 *     or aframe_remove to dispose returned framebuffer when is no longer
 *     needed.
 *
 * Parameters:
 *     id: set framebuffer id to this value.    
 *
 * Return Value:
 *     NULL if failed (no avalaible framebuffers)
 *     a valid pointer to respectively a video or audio framebuffer
 *     if succesfull.
 */
vframe_list_t *vframe_register(int id);
aframe_list_t *aframe_register(int id);

/*
 * vframe_retrieve, aframe_retrieve: (thread safe)
 *     Fetch next ready (FRAME_READY) framebuffer, by scanning IN ORDER
 *     the frame list. This means that this function WILL FAIL if there
 *     are some locked (FRAME_LOCKED) frames before the first ready frame.
 *     That happens because frame ordering MUST be preserved in current
 *     architecture.
 *     DO NOT *free() returned pointer! Use respectively vframe_remove
 *     or aframe_remove to dispose returned framebuffer when is no longer
 *     needed.
 *
 * Parameters:
 *     None
 * Return Value:
 *     NULL if there aren't ready framebuffers avalaible, or if they
 *     are preceeded by one or more locked frmabuffer.
 *     A valid pointer to a framebuffer otherwise.
 */
vframe_list_t *vframe_retrieve(void);
aframe_list_t *aframe_retrieve(void);

/*
 * vframe_remove, aframe_remove: (thread safe)
 *      release a framebuffer obtained via vframe_register or vframe_remove
 *      by putting back it on belonging ringbuffer (respectively,
 *      video and audio ringbuffers). Released framebuffer becomes
 *      avalaible for later usage.
 *
 * Parameters:
 *      ptr: framebuffer tor release.
 * Return Value:
 *      None
 * Side Effects:
 *      ringbuffer counters are updated as well.
 */
void vframe_remove(vframe_list_t *ptr);
void aframe_remove(aframe_list_t *ptr);

/*
 * vframe_dup, aframe_dup: (thread safe)
 *      Duplicate given respectively video or audio framebuffer by
 *      using another frame from video or audio ringbuffer. New
 *      framebuffer will be a full (deep) copy of old one
 *      (see aframe_copy/vframe_copy documentation to learn about
 *      deep copy).
 *
 * Parameters:
 *      f: framebuffer to be copied
 *         (this should be maked const ASAP --  FR)
 * Return Value:
 *      NULL if error, otherwise
 *      A pointer to a new, valid, framebuffer that's a full copy of
 *      given argument. Dispose it using respectively vframe_remove
 *      or aframe_remove, just as usual.
 * Side Effects:
 *      clone_flag for copied video framebuffer is handled intelligently.
 */
vframe_list_t *vframe_dup(vframe_list_t *f);
aframe_list_t *aframe_dup(aframe_list_t *f);

/*
 * vframe_retrieve_status, aframe_retrieve_status: (thread safe)
 *      scan the claimed (!FRAME_NULL) respectively video or audio
 *      framebuffer list looking for first frame with given status
 *      (old_status); change framebuffer status to given one (new_status).
 *      update ringbuffer counters and finally returns a pointer to
 *      found and manipulated frame. Returned pointer can be disposed
 *      as usual by using respectively vframe_remove or aframe_remove.
 *
 * Parameters:
 *      old_status: status of framebuffer to look for (halt on first match).
 *      new_status: new status of found framebuffer.
 * Return Value:
 *      NULL if failed, otherwise
 *      a pointer to found framebuffer, ready for usage.
 */
vframe_list_t *vframe_retrieve_status(int old_status, int new_status);
aframe_list_t *aframe_retrieve_status(int old_status, int new_status);

/*
 * vframe_copy, aframe_copy (thread safe)
 *     perform a soft or optionally deep copy respectively of a 
 *     video or audio framebuffer. A soft copy just copies metadata;
 *     #ifdef STATBUFFER
 *     soft copy also let the buffer pointers point to original frame
 *     buffers, so data isn't really copied around.
 *     #endif
 *     A deep copy just ac_memcpy()s buffer data from a frame to other
 *     one, so new frame will be an independent copy of old one.
 *
 * Parameters:
 *           dst: framebuffer which will hold te copied (meta)data.
 *           src: framebuffer to be copied.
 *                Mind the fact that when using softcopy real buffers will
 *                remain the ones of this frame
 *     copy_data: boolean flag. If 0, do softcopy; do deepcopy otherwise.
 *         
 * Return Value:
 *     None
 */
void vframe_copy(vframe_list_t *dst, vframe_list_t *src, int copy_data);
void aframe_copy(aframe_list_t *dst, aframe_list_t *src, int copy_data);

/*
 * vframe_set_status, aframe_set_status: (thread safe)
 *     change the status of a given framebuffer and updates the counters
 *     of originating ringbuffer (respectively, video and audio).
 *
 * Parameters:
 *        ptr: framebuffer pointer to be updated.
 *     status: new framebuffer status
 * Return Value:
 *     None
 */
void vframe_set_status(vframe_list_t *ptr, int status);
void aframe_set_status(aframe_list_t *ptr, int status);

/*
 * vframe_fill_level, aframe_fill_level: (NOT thread safe)
 *     check out respectively video and audio ringbuffer, veryfing if
 *     there is some frames of given status present and usable.
 *
 *     THREAD SAFENESS WARNING: this function access data in read-only
 *     fashon, so it doesn't hurt anything to use it in a multithreaded
 *     environment, as already happens, but since it DOES NOT lock counters
 *     before to read values, it's possible that it logs outdated
 *     informations. That still happens for legacy reasons, but of course
 *     this will fixed ASAP.
 *
 *     Optionally log out ringbuffer status if 'verbose' >= TC_STATS
 *     like vframe_fill_print, aframe_fill_print.
 *
 * Parameters:
 *     status: framebuffer status to be verified
 * Return Value:
 *      0: no framebuffer avalaible.
 *     >0: at least a framebuffer avalaible.
 */
int vframe_fill_level(int status);
int aframe_fill_level(int status);

/*
 * vframe_fill_print, aframe_fill_print: (NOT thread safe)
 *      tc_log* out current framebuffer ringbuffer fill level (counters
 *      for null/ready/empty/loacked frames) respectively for video
 *      and audio ringbuffers.
 *
 *      THREAD SAFENESS WARNING: this function access data in read-only
 *      fashon, so it doesn't hurt anything to use it in a multithreaded
 *      environment, as already happens, but since it DOES NOT lock counters
 *      before to read values, it's possible that it logs outdated
 *      informations. That still happens for legacy reasons, but of course
 *      this will fixed ASAP.
 *
 * Parameters:
 *      r: tag to be logged. Client-defined meaning. (Legacy).
 * Return Value:
 *      None
 * Side effects:
 *      See THREAD SAFENESS WARNING above.
 */
void vframe_fill_print(int r);
void aframe_fill_print(int r);

/*
 * vframe_have_data, aframe_have)data: (NOT thread safe)
 *      check if video/audio frame list is empty or not.
 *
 * Parameters:
 *      None
 * Return Value:
 *      !0 if frame list has at least one frame
 *       0 otherwise
 * Precinditions:
 *      caller must hold vframe_list_lock to get valid data.
 */
int vframe_have_data(void);
int aframe_have_data(void);


/* 
 * Some legacy code still access directly those variables.
 * I'm mostly OK (at least in principles) for doing so for lock and conditions,
 * but codebase still deserve an audit. -- FR.
 */
extern pthread_mutex_t aframe_list_lock;
extern pthread_mutex_t vframe_list_lock;


#endif /* FRAMEBUFFFER_H */
