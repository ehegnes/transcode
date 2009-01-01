/*
 *  framebuffer.h -- declarations of audio/video frame ringbuffers.
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updates and Enhancements
 *  (C) 2007-2008 - Francesco Romani <fromani -at- gmail -dot- com>
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
    TC_FRAME_NULL    = -1, /* on the frame pool, not yet claimed   */
    TC_FRAME_EMPTY   = 0,  /* claimed and being filled by decoder  */
    TC_FRAME_WAIT,         /* needs further processing (filtering) */
    TC_FRAME_LOCKED,       /* being procedded by filter layer      */
    TC_FRAME_READY,        /* ready to be processed by encoder     */
};

/*
 * frame status transitions scheme (overview)
 *
 *     
 *     .-------<----- +-------<------+------<------+-------<-------.
 *     |              ^              ^             ^               ^
 *     V              |              |             |               |
 * FRAME_NULL -> FRAME_EMPTY -> FRAME_WAIT -> FRAME_LOCKED -> FRAME_READY
 * :_buffer_:    \_decoder_/    \______filter_stage______/    \encoder_%/
 * \__pool__/         |         :                                  ^    :
 *                    |         \_______________encoder $__________|____/
 *                    V                                            ^
 *                    `-------------->------------->---------------'
 *
 * Notes:
 *  % - regular case, frame (processing) threads avalaibles
 *  $ - practical (default) case, filtering is carried by encoder thread.
 */

/*************************************************************************/

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
 * Is not possible to completely factor out all frame_list_t fields
 * because video and audio typess uses different names for same fields,
 * and existing code relies on this assumption.
 * Fixing this is stuff for 1.2.0 and beyond, for which I would like
 * to introduce some generic frame structure or something like it. -- FR.
 */
#define TC_FRAME_COMMON \
    int id;                       /* frame id (sequential uint) */ \
    int bufid;                    /* buffer id                  */ \
    int tag;                      /* init, open, close, ...     */ \
    int filter_id;                /* filter instance to run     */ \
    TCFrameStatus status;         /* see enumeration above      */ \
    TCFrameAttributes attributes; /* see enumeration above      */
/* BEWARE: semicolon NOT NEEDED */

/* 
 * Size vs Length
 *
 * Size represents the effective size of audio/video buffer,
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
 *     frame length < frame size (YUV420P smaller than RGB24)
 * in filtering:
 *      frame length < frame size (as above)
 * after encoding (in fact just colorspace transition):
 *     frame length == frame size (data becomes RGB24)
 * into muxer:
 *     frame length == frame size (as above)
 *
 * In all those cases having a distinct 'lenght' fields help
 * make things nicer and easier.
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

    int param1;  /* v_width  or a_rate */
    int param2;  /* v_height or a_bits */
    int param3;  /* v_bpp    or a_chan */

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
    uint8_t internal_audio_buf[SIZE_PCM_FRAME * 2];
#endif
};

/* 
 * generic pointer type, needed at least by internal code.
 * In the long (long) shot I'd like to use a unique generic
 * data container, like AVPacket (libavcodec) or something like it.
 * (see note about TC_FRAME_COMMON above) -- FR
 */
typedef union tcframeptr_ TCFramePtr;
union tcframeptr_ {
    frame_list_t *generic;
    vframe_list_t *video;
    aframe_list_t *audio;
};

/*************************************************************************/

/*
 * Transcode Framebuffer in a Nutshell (aka: how this code works)
 * --------------------------------------------------------------
 *
 * Introduction:
 * -------------
 * This is a quick, terse overview of design principles beyond the
 * framebuffer and about the design of this code. Full-blown
 * documentation is avalaible under doc/.
 *
 * When reading framebuffer documentation/code, always take in mind
 * the thread layout of transcode:
 *
 * - import layer is supposed to run 2 threads concurrently
 * - filter layer is supposed to run 0..N threads concurrently
 * - export layer is supposed to run 1 thread
 *
 * So, in any transcode execution, framebuffer code is supposed to
 * serve from 3 to N+3 concurrent threads.
 *
 * Framebuffer entities:
 * ---------------------
 * XXX
 *
 * frame status transitions scheme (API reminder):
 * -----------------------------------------------
 *
 *       .---------<---------------<------+-------<------.
 *       V                                | 7            | 6
 * .------------.     .--------.     .--------.     .--------.
 * | frame pool | --> | import |     | filter |     | export |
 * `------------'  1  `--------'     `--------'     `--------'
 *                           |         A    |         A
 *                           |       3 |    | 4       |
 *                         2 |         |    V       5 |
 *                           V     .-------------.    |
 *                           `---->| frame chain |--->'
 *                                 `-------------'
 *
 *  In frame lifetime order:
 *   1. {a,v}frame_register   (import)
 *   2. {a,v}frame_push_next  (import)
 *   3. {a,v}frame_reserve    (filter)
 *   4. {a,v}frame_push_next  (filter)
 *   5. {a,v}frame_retrieve   (export)
 *   6. {a,v}frame_remove     (export)
 * [ 7. {a,v}frame_remove     (filter) ]
 *
 * Operating conditions:
 *
 * 1. single source, full range, no interruptions
 * 2. single source, full range, interruption
 * 3. single source, sub range, no interruptions
 * 4. single source, sub range, interruption
 * 5. single source, multi sub ranges, no interruptions
 * 5. single source, multi sub ranges, interruption
 */


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
    int format; /* TC_CODEC_reserve preferred,
                 * CODEC_reserve still supported for compatibility
                 */
    /* audio fields */
    int rate;
    int channels;
    int bits;

    /* private field, used internally */
    double samples;
};

/*
 * tc_framebuffer_get_specs: (NOT thread safe)
 *     Get a pointer to a TCFrameSpecs structure representing current
 *     framebuffer structure. Frame handling code will use those parameters
 *     to allocate framebuffers.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     Constant pointer to a TCFrameSpecs structure. There is no need
 *     to *free() this structure.
 */
const TCFrameSpecs *tc_framebuffer_get_specs(void);

/*
 * tc_framebuffer_set_specs: (NOT thread safe)
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
 *     None.
 */
void tc_framebuffer_set_specs(const TCFrameSpecs *specs);

/*
 * tc_framebuffer_interrupt: (thread safe)
 *     Interrupt the framebuffer immediately (see below for specific meaning
 *     of this act in various functions).
 *     When framebuffer is interrupted, frames belonging to any processing
 *     stage are no longer avalaible; frame unavalaibility is notified as
 *     soon as is possible.
 *     When a framebuffer is interrupted, it becomes ready to be finalized;
 *     Effectively, the only operations that make sense to be performed on
 *     an interrupted framebuffer, is to finalize it.
 *     From statements above easily descend that interruption is irreversible.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     None.
 * Side effects:
 *     Any frame-claiming function will fail after the invocation of this
 *     function (see description above).
 */
void tc_framebuffer_interrupt(void);

/*
 * tc_framebuffer_interrupt_stage: (thread safe)
 *     like tc_framebuffer_interrupt, but involves only a given processing stage.
 *
 * Parameters:
 *     S: a TCFrameStatus representing the processing stage to interrupt.
 * Return Value:
 *     None.
 * Side effects:
 *     Any function claiming frames from the specified processing stage 
 *     will fail after the invocation of this function.
 */
void tc_framebuffer_interrupt_stage(TCFrameStatus S);

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
 *      None.
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
 *     None.
 * Return Value:
 *     None.
 */
void vframe_free(void);
void aframe_free(void);

/*
 * vframe_flush, aframe_flush: (NOT thread safe)
 *     flush all framebuffers still in ringbuffer, by marking those as unused.
 *     This will reset ringbuffer to an empty state, ready to be (re)used again.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     None.
 */
void vframe_flush(void);
void aframe_flush(void);

/*
 * tc_framebuffer_flush: (NOT thread safe)
 *     flush all active ringbuffers, and mark all frames as unused.
 *     This will reset ringbuffers to an empty state, ready to be (re)used again.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     None.
 */
void tc_framebuffer_flush(void);

/*
 * vframe_register, aframe_register: (thread safe)
 *     Frame claiming functions.
 *     Respectively wait for an empty audio and video frame,
 *     then register it in frame chain, attach the given `id'
 *     and finally return the pointer to caller.
 *
 *     Those function are (and should be) used at the beginning
 *     of the frame chain. Those should are the first function
 *     that a framebuffer should see in it's lifecycle.
 *
 *     In transcode, those functions are (and should be) used
 *     only in the decoder.
 *
 *     Note:
 *     DO NOT *free() returned pointer! The memory needed for frames is
 *     handled by transcode internally.
 *
 * Parameters:
 *     id: set framebuffer id to this value.
 *         The meaning of `id' is enterely client-depended.
 * Return Value:
 *     A valid pointer to respectively an empty video or audio frame.
 *     If framebuffer is interrupted, both returns NULL.
 * Side effects:
 *     Being frame claiming functions, those functions will block
 *     calling thread until a new frame will be avalaible, OR
 *     until an interruption happens.
 */
vframe_list_t *vframe_register(int id);
aframe_list_t *aframe_register(int id);

/*
 * vframe_reserve, aframe_reserve: (thread safe)
 *     Frame claiming functions.
 *     Respectively wait for a processing-needing
 *     (`waiting' in transcode slang) audio and video frame,
 *     then reserve it, preventing other calls to those functions
 *     to claim it twice, and finally return the pointer to caller.
 *
 *     Those function are (and should be) used in the middle
 *     of the frame chain.
 *
 *     In transcode, those functions are (and should be) used
 *     only in the filter layer.
 *
 *     Note:
 *     DO NOT *free() returned pointer! The memory needed for frames is
 *     handled by transcode internally.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     A valid pointer to respectively an empty video or audio frame.
 *     If framebuffer is interrupted, both returns NULL.
 * Side effects:
 *     Being frame claiming functions, those functions will block
 *     calling thread until a new frame will be avalaible, OR
 *     until an interruption happens.
 */
vframe_list_t *vframe_reserve(void);
aframe_list_t *aframe_reserve(void);

/*
 * vframe_retrieve, aframe_retrieve: (thread safe)
 *     Frame claiming functions.
 *     Respectively wait for a audio and video frame ready to be
 *     encoded, then retrieve it, preventing other calls to those
 *     functions to claim it twice, and finally return the pointer
 *     to caller.
 *
 *     Those function are (and should be) used at the end
 *     of the frame chain.
 *
 *     In transcode, those functions are (and should be) used
 *     only in the encoder.
 *
 *     Note:
 *     DO NOT *free() returned pointer! The memory needed for frames is
 *     handled by transcode internally.
 *
 * Parameters:
 *     None.
 * Return Value:
 *     A valid pointer to respectively an empty video or audio frame.
 *     If framebuffer is interrupted, both returns NULL.
 * Side effects:
 *     Being frame claiming functions, those functions will block
 *     calling thread until a new frame will be avalaible, OR
 *     until an interruption happens.
 */
vframe_list_t *vframe_retrieve(void);
aframe_list_t *aframe_retrieve(void);

/*
 * vframe_remove, aframe_remove: (thread safe)
 *     Respectively release an audio or video frame,
 *     by marking it as unused and putting it back on the frame pool.
 *
 *     Those function are (and should be) used at the end
 *     of the frame chain. Those should are the last function
 *     that a framebuffer should see in it's lifecycle.
 *
 *     In transcode, those functions are (and should be) used
 *     only in the encoder.
 *
 * Parameters:
 *     ptr: framebuffer to release.
 * Return Value:
 *     None.
 */
void vframe_remove(vframe_list_t *ptr);
void aframe_remove(aframe_list_t *ptr);

/*
 * vframe_reinject, aframe_reinject: (thread safe)
 *     Respectively reinject an audio or video frame
 *     into the originating frame pool, so the reinjected frame will
 *     be obtained again when the frame pool is queried again.
 *
 *     Those function are (and should be) used when a processing
 *     stage needs to see again a given frame.
 *
 *     In transcode, those functions are (and should be) used
 *     only when the encoder handles a cloned frame.
 *
 * Parameters:
 *     ptr: framebuffer to reinject.
 * Return Value:
 *     None.
 */
void aframe_reinject(aframe_list_t *ptr);
void vframe_reinject(vframe_list_t *ptr);

/*
 * vframe_push_next, aframe_push_next: (thread safe)
 *     Push a frame into next processing stage, by changing
 *     its status.
 *     Those functions are used when a processing stage terminate
 *     its operations on a given frame and so it want to pass such
 *     frame to next stage.
 *
 *     In transcode, those functions are (and should be) used
 *     in the decoder and in the filter stage.
 *
 * Parameters:
 *        ptr: framebuffer pointer to be updated.
 *     status: new framebuffer status (= stage).
 * Return Value:
 *     None.
 * Side effects:
 *     A blocked thread can (and it will likely) be awaken
 *     by this operation.
 */
void vframe_push_next(vframe_list_t *ptr, TCFrameStatus status);
void aframe_push_next(aframe_list_t *ptr, TCFrameStatus status);

/*
 * vframe_dup, aframe_dup: (thread safe)
 *     Frame claiming functions.
 *     Duplicate given respectively video or audio framebuffer.
 *     New framebuffer will be a full (deep) copy of old one
 *     (see aframe_copy/vframe_copy documentation to learn about
 *     deep copy).
 *
 * Parameters:
 *     f: framebuffer to be copied.
 * Return Value:
 *     A valid pointer to respectively duplicate video or audio frame.
 *     If framebuffer is interrupted, both returns NULL.
 * Side Effects:
 *     Being frame claiming functions, those functions will block
 *     calling thread until a new frame will be avalaible, OR
 *     until an interruption happens.
 *     clone_flag for copied video framebuffer is handled intelligently.
 */
vframe_list_t *vframe_dup(vframe_list_t *f);
aframe_list_t *aframe_dup(aframe_list_t *f);

/*
 * vframe_copy, aframe_copy (thread safe)
 *     perform a soft or optionally deep copy respectively of a 
 *     video or audio framebuffer. A soft copy just copies metadata;
 * #ifdef STATBUFFER
 *     soft copy also let the buffer pointers point to original frame
 *     buffers, so data isn't really copied around.
 * #endif
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
 *     None.
 */
void vframe_copy(vframe_list_t *dst, const vframe_list_t *src, int copy_data);
void aframe_copy(aframe_list_t *dst, const aframe_list_t *src, int copy_data);

/*
 * vframe_dump_status, aframe_dump_status: (NOT thread safe)
 *      tc_log* out current framebuffer ringbuffer internal status, e.g.
 *      counters for null/ready/empty/loacked frames) respectively for
 *      video and audio ringbuffers.
 *
 *      THREAD SAFENESS WARNING:
 *      WRITEME
 *
 * Parameters:
 * 	None.
 * Return Value:
 *      None.
 * Side effects:
 *      See THREAD SAFENESS WARNING above.
 */
void vframe_dump_status(void);
void aframe_dump_status(void);

/*
 * vframe_have_more, aframe_have_more (thread safe):
 *      check if video/audio frame list is empty or not.
 *
 * Parameters:
 *      None.
 * Return Value:
 *      !0 if frame list has at least one frame
 *       0 otherwise
 */
int vframe_have_more(void);
int aframe_have_more(void);

/*
 * {v,a}frame_get_counters (thead safe):
 *     get the number of frames currently hold in the processing layers,
 *     respectively for video and audio pipelines.
 *
 * Parameters:
 *      im: if not NULL, store here the number of frames
 *          hold by import layer.
 *      fl: if not NULL, store here the number of frames
 *          hold by filter layer.
 *      ex: if not NULL, store here the number of frames
 *          hold by export layer.
 * Return Value:
 *      None.
 */
void vframe_get_counters(int *im, int *fl, int *ex);
void aframe_get_counters(int *im, int *fl, int *ex);

/*
 * tc_framebuffer_get_counters (thread safe):
 *     get the total number of frames currently hold in the processing
 *     layers, by considering both video and audio pipelines.
 *
 * Parameters:
 *      im: if not NULL, store here the number of frames
 *          hold by import layer.
 *      fl: if not NULL, store here the number of frames
 *          hold by filter layer.
 *      ex: if not NULL, store here the number of frames
 *          hold by export layer.
 * Return Value:
 *      None.
 */
void tc_framebuffer_get_counters(int *im, int *fl, int *ex);

#endif /* FRAMEBUFFER_H */
