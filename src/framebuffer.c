/*
 * framebuffer.c - audio/video frame ringbuffers, reloaded.
 * (C) 2005-2006 - Francesco Romani <fromani -at- gmail -dot- com>
 * Based on code written by Thomas Oestreich.
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "tc_defaults.h"
#include "framebuffer.h"

#include "libtc/tcframes.h"
#include "libtc/ratiocodes.h"


pthread_mutex_t aframe_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t aframe_list_full_cv = PTHREAD_COND_INITIALIZER;

aframe_list_t *aframe_list_head = NULL;
aframe_list_t *aframe_list_tail = NULL;

pthread_mutex_t vframe_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vframe_list_full_cv = PTHREAD_COND_INITIALIZER;

vframe_list_t *vframe_list_head = NULL;
vframe_list_t *vframe_list_tail = NULL;

/* ------------------------------------------------------------------ */

typedef TCFramePtr (*TCFrameAllocFn)(const TCFrameSpecs *);

typedef void (*TCFrameFreeFn)(TCFramePtr);

/* ------------------------------------------------------------------ */


typedef struct tcringframebuffer_ TCRingFrameBuffer;
struct tcringframebuffer_ {
    /* real ringbuffer */
    TCFramePtr *frames;

    int next;
    int last;

    /* counters */
    int fill;
    int ready;
    int locked;
    int empty;
    int wait;

    /* *de(allocation helpers */
    const TCFrameSpecs *specs;
    TCFrameAllocFn alloc;
    TCFrameFreeFn free;
};

static TCRingFrameBuffer tc_audio_ringbuffer;
static TCRingFrameBuffer tc_video_ringbuffer;

static TCFrameSpecs tc_specs = {
    /* PAL defaults */
    .frc = 3,
    .width = PAL_W,
    .height = PAL_H,
    .format = TC_CODEC_YUV420P,
    .rate = RATE,
    .channels = CHANNELS,
    .bits = BITS,
    .samples = 48000.0,
};

/* ------------------------------------------------------------------ */
/* Frame allocation/disposal helpers, needed by code below            */
/* thin wrappers around libtc facilities                              */
/* ------------------------------------------------------------------ */

#define TCFRAMEPTR_IS_NULL(tcf)    (tcf.generic == NULL)

static TCFramePtr tc_video_alloc(const TCFrameSpecs *specs)
{
    TCFramePtr frame;
    frame.video = tc_new_video_frame(specs->width, specs->height,
                                      specs->format, TC_FALSE);
    return frame;
}

static TCFramePtr tc_audio_alloc(const TCFrameSpecs *specs)
{
    TCFramePtr frame;
    frame.audio = tc_new_audio_frame(specs->samples, specs->channels,
                                      specs->bits);
    return frame;
}


static void tc_video_free(TCFramePtr frame)
{
    tc_del_video_frame(frame.video);
}

static void tc_audio_free(TCFramePtr frame)
{
    tc_del_audio_frame(frame.audio);
}

/* ------------------------------------------------------------------ */

/* XXX */
vframe_list_t *vframe_alloc_single(void)
{
    return tc_new_video_frame(tc_specs.width, tc_specs.height,
                              tc_specs.format, TC_TRUE);
}

/* XXX */
aframe_list_t *aframe_alloc_single(void)
{
    return tc_new_audio_frame(tc_specs.samples, tc_specs.channels,
                              tc_specs.bits);
}

/* ------------------------------------------------------------------ */

const TCFrameSpecs *tc_ring_framebuffer_get_specs(void)
{
    return &tc_specs;
}

void tc_ring_framebuffer_set_specs(const TCFrameSpecs *specs)
{
    if (specs != NULL) {
        double fps;

        /* raw copy first */
        ac_memcpy(&tc_specs, specs, sizeof(TCFrameSpecs));

        /* then deduct missing parameters */
        if (tc_frc_code_to_value(tc_specs.frc, &fps) == TC_NULL_MATCH) {
            fps = 1.0; /* sane, very worst case value */
        }
        tc_specs.samples = (double)tc_specs.rate/fps;
    }
}

/* ------------------------------------------------------------------ */
/* NEW API, yet private                                               */
/* ------------------------------------------------------------------ */

static int tc_init_ring_framebuffer(TCRingFrameBuffer *rfb,
                                    const TCFrameSpecs *specs,
                                    TCFrameAllocFn alloc,
                                    TCFrameFreeFn free,
                                    int size)
{
    if (rfb == NULL || specs == NULL || size < 0
     || alloc == NULL || free == NULL) {
        return 1;
    }
    size = (size > 0) ?size :1; /* allocate at least one frame */

    rfb->frames = tc_malloc(size * sizeof(TCFramePtr));
    if (rfb->frames == NULL) {
        return -1;
    }

    rfb->specs = specs;
    rfb->alloc = alloc;
    rfb->free = free;

    for (rfb->last = 0; rfb->last < size; rfb->last++) {
        rfb->frames[rfb->last] = rfb->alloc(rfb->specs);
        if (TCFRAMEPTR_IS_NULL(rfb->frames[rfb->last])) {
            if (verbose >= TC_DEBUG) {
                tc_log_error(__FILE__, "failed frame allocation");
            }
            return -1;
        }

        rfb->frames[rfb->last].generic->status = FRAME_NULL;
        rfb->frames[rfb->last].generic->bufid = rfb->last;
    }

    rfb->next = 0;

    rfb->fill = 0;
    rfb->ready = 0;
    rfb->locked = 0;
    rfb->empty = 0;
    rfb->wait = 0;

    if (verbose >= TC_STATS) {
        tc_log_info(__FILE__, "allocated %i frames in ringbuffer", size);
    }
    return 0;
}


static void tc_fini_ring_framebuffer(TCRingFrameBuffer *rfb)
{
    if (rfb != NULL && rfb->free != NULL) {
        int i = 0, n = rfb->last;
    
        for (i = 0; i < rfb->last; i++) {
            rfb->free(rfb->frames[i]);
        }
        tc_free(rfb->frames);
        rfb->last = 0;
    
        if (verbose >= TC_STATS) {
            tc_log_info(__FILE__, "freed %i frames in ringbuffer", n);
        }
    }
}


static TCFramePtr tc_ring_framebuffer_retrieve_frame(TCRingFrameBuffer *rfb)
{
    TCFramePtr ptr;
    ptr.generic = NULL;

    if (rfb != NULL) {
        int i = 0;

        ptr = rfb->frames[rfb->next];
        for (i = 0; i < rfb->last; i++) {
            if (ptr.generic->status == FRAME_NULL) {
                break;
            }
            rfb->next++;
            rfb->next %= rfb->last;
            ptr = rfb->frames[rfb->next];
        }

        if (ptr.generic->status != FRAME_NULL) {
            if (verbose >= TC_FLIST) {
                tc_log_warn(__FILE__, "retrieved buffer=%i, but not empty",
                                      ptr.generic->status);
            }
            ptr.generic = NULL;
            return ptr;
        }
    }

    /* ok */
    if (verbose >= TC_FLIST) {
        tc_log_info(__FILE__, "retrieved buffer = %i [%i]",
                              rfb->next, ptr.generic->bufid);
    }

    rfb->next++;
    rfb->next %= rfb->last;
    return ptr;
}


static int tc_ring_framebuffer_release_frame(TCRingFrameBuffer *rfb,
                                             TCFramePtr frame)
{
    if (rfb == NULL || TCFRAMEPTR_IS_NULL(frame)) {
        return -1;
    }
    if (frame.generic->status != FRAME_EMPTY) {
        tc_log_warn(__FILE__, "trying to release non empty frame #%i (%i)",
                    frame.generic->bufid, frame.generic->status);
        return -1;
    } else {
        if (verbose >= TC_FLIST) {
            tc_log_info(__FILE__, "releasing frame #%i [%i]",
                        frame.generic->bufid, rfb->next);
        }
        frame.generic->status = FRAME_NULL;
    }
    return 0;
}


static TCFramePtr tc_ring_framebuffer_register_frame(TCRingFrameBuffer *rfb,
                                                     int id, int status)
{
    TCFramePtr ptr;

    /* retrive a valid pointer from the pool */
#ifdef STATBUFFER
    if (verbose >= TC_FLIST) {
        tc_log_info(__FILE__, "register frame id = %i", id);
    }
    ptr = tc_ring_framebuffer_retrieve_frame(rfb);
#else
    ptr = rfb->alloc(rfb->specs);
#endif

    if (!TCFRAMEPTR_IS_NULL(ptr)) {
        rfb->fill++;

        if (status == FRAME_EMPTY) {
            rfb->empty++;
            ptr.generic->id = id;
        } else if (status == FRAME_WAIT) {
            rfb->wait++;
        }
        ptr.generic->status = status;

        ptr.generic->next = NULL;
        ptr.generic->prev = NULL;

        if (verbose >= TC_FLIST) {
            tc_log_msg(__FILE__, "registering frame:"
                                 " f=%i e=%i w=%i l=%i r=%i",
                                 rfb->fill, rfb->empty, rfb->wait,
                                 rfb->locked, rfb->ready);
        }
    }
    return ptr; 
}


static void tc_ring_framebuffer_remove_frame(TCRingFrameBuffer *rfb,
                                             TCFramePtr frame)
{
    if (rfb != NULL || !TCFRAMEPTR_IS_NULL(frame)) {
        if (frame.generic->status == FRAME_READY) {
            rfb->ready--;
        }
        if (frame.generic->status == FRAME_LOCKED) {
            rfb->locked--;
        }
        /* release valid pointer to pool */
        rfb->empty++;
        frame.generic->status = FRAME_EMPTY;

#ifdef STATBUFFER
        tc_ring_framebuffer_release_frame(rfb, frame);
#else
        rfb->free(frame);
#endif
        /* adjust fill level */
        rfb->empty--;
        rfb->fill--;

        if (verbose >= TC_FLIST) {
            tc_log_msg(__FILE__, "removing frame:"
                                 " f=%i e=%i w=%i l=%i r=%i",
                                 rfb->fill, rfb->empty, rfb->wait,
                                 rfb->locked, rfb->ready);
        }
    }
}


static int tc_ring_framebuffer_flush(TCRingFrameBuffer *rfb)
{
    TCFramePtr frame;
    int i = 0;

    do {
        frame = tc_ring_framebuffer_retrieve_frame(rfb);
        if (verbose >= TC_STATS) {
            tc_log_msg(__FILE__, "flushing frame buffer...");
        }
        tc_ring_framebuffer_remove_frame(rfb, frame);
        i++;
    } while (!TCFRAMEPTR_IS_NULL(frame));

    if (verbose >= TC_DEBUG) {
        tc_log_info(__FILE__, "flushed %i frame buffes", i);
    }
    return i;
}


static int tc_ring_framebuffer_check_status(const TCRingFrameBuffer *rfb,
                                            int status)
{
    if (status == TC_BUFFER_FULL  && rfb->fill >= rfb->last - 1)  {
        return 1;
    }
    if (status == TC_BUFFER_READY && rfb->ready > 0) {
        return 1;
    }
    if (status == TC_BUFFER_EMPTY && rfb->fill == 0) {
        return 1;
    }
    if (status == TC_BUFFER_LOCKED && rfb->locked > 0) {
        return 1;
    }
    return 0;
}


static void tc_ring_framebuffer_log_fill_level(const TCRingFrameBuffer *rfb,
                             const char *id, int tag)
{
    tc_log_msg(__FILE__, "%s: fill=%i/%i, empty=%i wait=%i"
                         " locked=%i, ready=%i tag=%i",
                         id, rfb->fill, rfb->last, rfb->empty, rfb->wait,
                         rfb->locked, rfb->ready, tag);
}



/* ------------------------------------------------------------------ */
/* Backwared-compatible API                                           */
/* ------------------------------------------------------------------ */

int aframe_alloc(int num)
{
    return tc_init_ring_framebuffer(&tc_audio_ringbuffer,
                                    &tc_specs,
                                    tc_audio_alloc,
                                    tc_audio_free,
                                    num);
}

int vframe_alloc(int num)
{
    return tc_init_ring_framebuffer(&tc_video_ringbuffer,
                                    &tc_specs,
                                    tc_video_alloc,
                                    tc_video_free,
                                    num);
}


void aframe_free(void)
{
    tc_fini_ring_framebuffer(&tc_audio_ringbuffer);
}

void vframe_free(void)
{
    tc_fini_ring_framebuffer(&tc_video_ringbuffer);
}


/* ------------------------------------------------------------------ */


#define LIST_FRAME_APPEND(ptr, tail) do { \
    if ((tail) != NULL) { \
        (tail)->next = (ptr); \
        (ptr)->prev = (tail); \
    } \
    (tail) = (ptr); \
} while (0)

#define LIST_FRAME_INSERT(ptr, head) do { \
    if ((head) == NULL) { \
        (head) = ptr; \
    } \
} while (0)

aframe_list_t *aframe_register(int id)
{
    TCFramePtr frame;

    pthread_mutex_lock(&aframe_list_lock);

    frame = tc_ring_framebuffer_register_frame(&tc_audio_ringbuffer,
                                               id, FRAME_EMPTY);
    if (!TCFRAMEPTR_IS_NULL(frame)) {
        /* 
         * complete initialization:
         * backward-compatible stuff
         */
        LIST_FRAME_APPEND(frame.audio, aframe_list_tail);
        /* first frame registered must set aframe_list_head */
        LIST_FRAME_INSERT(frame.audio, aframe_list_head);
    }
    pthread_mutex_unlock(&aframe_list_lock);
    return frame.audio;
}

vframe_list_t *vframe_register(int id)
{
    TCFramePtr frame;
    
    pthread_mutex_lock(&vframe_list_lock);

    frame = tc_ring_framebuffer_register_frame(&tc_video_ringbuffer,
                                               id, FRAME_EMPTY); 
    if (!TCFRAMEPTR_IS_NULL(frame)) {
        /* 
         * complete initialization:
         * backward-compatible stuff
         */
        LIST_FRAME_APPEND(frame.video, vframe_list_tail);
        /* first frame registered must set vframe_list_head */
        LIST_FRAME_INSERT(frame.video, vframe_list_head);
 
    }
    pthread_mutex_unlock(&vframe_list_lock);
    return frame.video;
}


/* ------------------------------------------------------------------ */


#define LIST_FRAME_LINK(ptr, f, tail) do { \
     /* insert after ptr */ \
    (ptr)->next = (f)->next; \
    (f)->next = (ptr); \
    (ptr)->prev = (f); \
    \
    if ((ptr)->next == NULL) { \
        /* must be last ptr in the list */ \
        (tail) = (ptr); \
    } \
} while (0)

aframe_list_t *aframe_dup(aframe_list_t *f)
{
    TCFramePtr frame;

    if (f == NULL) {
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "aframe_dup: empty frame");
        }
        return NULL;
    }

    pthread_mutex_lock(&aframe_list_lock);
    /* retrieve a valid pointer from the pool */
    frame = tc_ring_framebuffer_register_frame(&tc_audio_ringbuffer,
                                               0, FRAME_WAIT);
    if (!TCFRAMEPTR_IS_NULL(frame)) {
        aframe_copy(frame.audio, f, 1);

        LIST_FRAME_LINK(frame.audio, f, aframe_list_tail);
#ifdef STATBUFFER
    } else { /* ptr == NULL */
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "aframe_dup: cannot find a free slot"
                                  " (%i)", f->id);
        }
#endif
    }
    pthread_mutex_unlock(&aframe_list_lock);
    return frame.audio;
}

vframe_list_t *vframe_dup(vframe_list_t *f)
{
    TCFramePtr frame;

    if (f == NULL) {
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "vframe_dup: empty frame");
        }
        return NULL;
    }

    pthread_mutex_lock(&vframe_list_lock);
    /* retrieve a valid pointer from the pool */
    frame = tc_ring_framebuffer_register_frame(&tc_video_ringbuffer,
                                               0, FRAME_WAIT);
    if (!TCFRAMEPTR_IS_NULL(frame)) {
        vframe_copy(frame.video, f, 1);

        /* currently noone cares about this */
        frame.video->clone_flag = f->clone_flag+1;

        LIST_FRAME_LINK(frame.video, f, vframe_list_tail);
#ifdef STATBUFFER
    } else { /* ptr == NULL */
        if (verbose & TC_FLIST) {
            tc_log_warn(__FILE__, "vframe_dup: cannot find a free slot"
                                  " (%i)", f->id);
        }
#endif
    }
    pthread_mutex_unlock(&vframe_list_lock);
    return frame.video;
}


/* ------------------------------------------------------------------ */

#define LIST_FRAME_REMOVE(ptr, head, tail) do { \
    if ((ptr)->prev != NULL) { \
        ((ptr)->prev)->next = (ptr)->next; \
    } \
    if ((ptr)->next != NULL) { \
        ((ptr)->next)->prev = (ptr)->prev; \
    } \
    \
    if ((ptr) == (tail)) { \
        (tail) = (ptr)->prev; \
    } \
    if ((ptr) == (head)) { \
        (head) = (ptr)->next; \
    } \
} while (0)

void aframe_remove(aframe_list_t *ptr)
{
    if (ptr != NULL) {
        TCFramePtr frame;
        frame.audio = ptr;

        pthread_mutex_lock(&aframe_list_lock);

        LIST_FRAME_REMOVE(ptr, aframe_list_head, aframe_list_tail);

        tc_ring_framebuffer_remove_frame(&tc_audio_ringbuffer,
                                         frame);

        pthread_mutex_unlock(&aframe_list_lock);
    }
}

void vframe_remove(vframe_list_t *ptr)
{
    if (ptr != NULL) {
        TCFramePtr frame;
        frame.video = ptr;

        pthread_mutex_lock(&vframe_list_lock);
        
        LIST_FRAME_REMOVE(ptr, vframe_list_head, vframe_list_tail);
        
        tc_ring_framebuffer_remove_frame(&tc_video_ringbuffer,
                                         frame);

        pthread_mutex_unlock(&vframe_list_lock);
    }
}

/* ------------------------------------------------------------------ */

void aframe_flush(void)
{
    tc_ring_framebuffer_flush(&tc_audio_ringbuffer);
}

void vframe_flush(void)
{
    tc_ring_framebuffer_flush(&tc_video_ringbuffer);
}

/* ------------------------------------------------------------------ */
/* Macro galore section ;)                                            */
/* ------------------------------------------------------------------ */

#define LIST_FRAME_RETRIEVE(ptr) do { \
    /* move along the chain and check for status */ \
    for (; (ptr) != NULL; (ptr) = (ptr)->next) { \
        /* \
         * we cannot skip a locked frame, since \
         * we have to preserve order in which frames are encoded \
         */ \
        if ((ptr)->status == FRAME_LOCKED) { \
            (ptr) = NULL; \
            break; \
        } \
        /* this frame is ready to go */ \
        if ((ptr)->status == FRAME_READY) { \
            break; \
        } \
    } \
} while (0)

aframe_list_t *aframe_retrieve(void)
{
    aframe_list_t *ptr = NULL;

    pthread_mutex_lock(&aframe_list_lock);
    ptr = aframe_list_head;

    LIST_FRAME_RETRIEVE(ptr);

    pthread_mutex_unlock(&aframe_list_lock);
    return ptr;
}


vframe_list_t *vframe_retrieve(void)
{
    vframe_list_t *ptr = NULL;

    pthread_mutex_lock(&vframe_list_lock);
    ptr = vframe_list_head;

    LIST_FRAME_RETRIEVE(ptr);

    pthread_mutex_unlock(&vframe_list_lock);
    return ptr;
}

#undef LIST_FRAME_RETRIEVE

/* ------------------------------------------------------------------ */

#define DEC_COUNTERS(RFB, STATUS) do { \
    if ((STATUS) == FRAME_READY) { \
        (RFB)->ready--; \
    } \
    if ((STATUS) == FRAME_LOCKED) { \
        (RFB)->locked--; \
    } \
    if ((STATUS) == FRAME_WAIT) { \
       (RFB)->wait--; \
    } \
} while(0)

#define INC_COUNTERS(RFB, STATUS) do { \
    if ((STATUS) == FRAME_READY) { \
        (RFB)->ready++; \
    } \
    if ((STATUS) == FRAME_LOCKED) { \
        (RFB)->locked++; \
    } \
    if ((STATUS) == FRAME_WAIT) { \
       (RFB)->wait++; \
    } \
} while(0)

#define FRAME_SET_STATUS(RFB, PTR, NEW_STATUS) do { \
    DEC_COUNTERS((RFB), (PTR)->status); \
    (PTR)->status = (NEW_STATUS); \
    INC_COUNTERS((RFB), (PTR)->status); \
} while (0)

#define FRAME_LOOKUP(RFB, PTR, OLD_STATUS, NEW_STATUS) do { \
     /* move along the chain and check for status */ \
    for (; (PTR) != NULL; (PTR) = (PTR)->next) { \
        if ((PTR)->status == (OLD_STATUS)) { \
            /* found matching frame */ \
            FRAME_SET_STATUS(RFB, PTR, NEW_STATUS); \
            break; \
        } \
    } \
} while (0)

aframe_list_t *aframe_retrieve_status(int old_status, int new_status)
{
    aframe_list_t *ptr = NULL;

    pthread_mutex_lock(&aframe_list_lock);
    ptr = aframe_list_head;

    FRAME_LOOKUP(&tc_audio_ringbuffer, ptr,
                 old_status, new_status);

    pthread_mutex_unlock(&aframe_list_lock);
    return ptr;
}

vframe_list_t *vframe_retrieve_status(int old_status, int new_status)
{
    vframe_list_t *ptr = NULL;

    pthread_mutex_lock(&vframe_list_lock);
    ptr = vframe_list_head;

    FRAME_LOOKUP(&tc_video_ringbuffer, ptr,
                 old_status, new_status);

    pthread_mutex_unlock(&vframe_list_lock);
    return ptr;
}

#undef FRAME_LOOKUP

/* ------------------------------------------------------------------ */


#define FRAME_SET_EXT_STATUS(RFB, PTR, NEW_STATUS) do { \
    if ((PTR)->status == FRAME_EMPTY) { \
        (RFB)->empty--; \
    } \
    FRAME_SET_STATUS((RFB), (PTR), (NEW_STATUS)); \
    if ((PTR)->status == FRAME_EMPTY) { \
        (RFB)->empty++; \
    } \
} while (0)

void aframe_set_status(aframe_list_t *ptr, int status)
{
    if (ptr != NULL) {
        pthread_mutex_lock(&aframe_list_lock);
        FRAME_SET_EXT_STATUS(&tc_audio_ringbuffer, ptr, status);
        pthread_mutex_unlock(&aframe_list_lock);
    }
}


void vframe_set_status(vframe_list_t *ptr, int status)
{
    if (ptr != NULL) {
        pthread_mutex_lock(&vframe_list_lock);
        FRAME_SET_EXT_STATUS(&tc_video_ringbuffer, ptr, status);
        pthread_mutex_unlock(&vframe_list_lock);
    }
}

#undef FRAME_SET_STATUS
#undef FRAME_SET_EXT_STATUS

/* ------------------------------------------------------------------ */


int aframe_fill_level(int status)
{
    if (verbose >= TC_STATS) {
        tc_ring_framebuffer_log_fill_level(&tc_audio_ringbuffer,
                                           "audio fill level", status);
    }
    /* user has to lock aframe_list_lock to obtain a proper result */
    return tc_ring_framebuffer_check_status(&tc_audio_ringbuffer, status);
}

int vframe_fill_level(int status)
{
    if (verbose >= TC_STATS) {
        tc_ring_framebuffer_log_fill_level(&tc_video_ringbuffer,
                                           "video fill level", status);
    }
    /* user has to lock aframe_list_lock to obtain a proper result */
    return tc_ring_framebuffer_check_status(&tc_video_ringbuffer, status);
}

void aframe_fill_print(int r)
{
    tc_ring_framebuffer_log_fill_level(&tc_audio_ringbuffer,
                                       "audio fill level", r);
}

void vframe_fill_print(int r)
{
    tc_ring_framebuffer_log_fill_level(&tc_video_ringbuffer,
                                       "video fill level", r);
}

/* ------------------------------------------------------------------ */
/* Frame copying routines                                             */
/* ------------------------------------------------------------------ */

void aframe_copy(aframe_list_t *dst, aframe_list_t *src, int copy_data)
{
    if (!dst || !src) {
    	return;
    }

    /* copy all common fields with just one move */
    ac_memcpy(dst, src, sizeof(frame_list_t));
    
    if (copy_data == 1) {
        /* really copy video data */
        ac_memcpy(dst->audio_buf, src->audio_buf, dst->audio_size);
    } else {
        /* soft copy, new frame points to old audio data */
        dst->audio_buf = src->audio_buf;
    }
}

void vframe_copy(vframe_list_t *dst, vframe_list_t *src, int copy_data)
{
    if (!dst || !src) {
    	return;
    }

    /* copy all common fields with just one move */
    ac_memcpy(dst, src, sizeof(frame_list_t));
    
    dst->clone_flag = src->clone_flag;
    dst->deinter_flag = src->deinter_flag;
    dst->free = src->free;
    /* 
     * we assert that plane pointers *are already properly set*
     * we're focused on copy _content_ here.
     */

    if (copy_data == 1) {
        /* really copy video data */
        ac_memcpy(dst->video_buf, src->video_buf, dst->video_size);
        ac_memcpy(dst->video_buf2, src->video_buf2, dst->video_size);
    } else {
        /* soft copy, new frame points to old video data */
        dst->video_buf = src->video_buf;
        dst->video_buf2 = src->video_buf2;
    }
}

