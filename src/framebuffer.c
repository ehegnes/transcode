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

/* Quick summary:
 * This code acts as generic ringbuffer implementation, with
 * specializations for main (auido and video) ringbufffers
 * in order to cope legacy constraints from 1.0.x series.
 * It replaces former src/{audio,video}_buffer.c in (hopefully!)
 * a more generic, clean, maintanable, compact way.
 * 
 * Please note that there is *still* some other ringbuffer
 * scatthered through codebase (subtitle buffer,d emux buffers,
 * possibly more). They will be merged lately or will be dropped
 * or reworked.
 *
 * This code can, of course, be further improved, but doing so
 * hasn't high priority on my TODO list, I've covered with this
 * piece of code most urgent todos for 1.1.0.      -- FR
 */

pthread_mutex_t aframe_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t aframe_list_full_cv = PTHREAD_COND_INITIALIZER;

aframe_list_t *aframe_list_head = NULL;
aframe_list_t *aframe_list_tail = NULL;

pthread_mutex_t vframe_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vframe_list_full_cv = PTHREAD_COND_INITIALIZER;

vframe_list_t *vframe_list_head = NULL;
vframe_list_t *vframe_list_tail = NULL;

/* ------------------------------------------------------------------ */

/*
 * Layered, custom allocator/disposer for ringbuffer structures.
 * THe idea is to semplify (from ringbufdfer viewpoint!) frame
 * allocation/disposal and to make it as much generic as is possible
 * (avoif if()s and so on).
 */

typedef TCFramePtr (*TCFrameAllocFn)(const TCFrameSpecs *);

typedef void (*TCFrameFreeFn)(TCFramePtr);

/* ------------------------------------------------------------------ */


typedef struct tcringframebuffer_ TCRingFrameBuffer;
struct tcringframebuffer_ {
    /* real ringbuffer */
    TCFramePtr *frames;

    /* frame indexes */
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

/* 
 * Specs used internally. I don't export this structure directly
 * because I want to be free to change it if needed
 */
static TCFrameSpecs tc_specs = {
    /* Largest supported values, to ensure the buffer is always big enough
     * (see FIXME in tc_ring_framebuffer_set_specs()) */
    .frc = 3,  // PAL, why not
    .width = TC_MAX_V_FRAME_WIDTH,
    .height = TC_MAX_V_FRAME_HEIGHT,
    .format = TC_CODEC_RGB,
    .rate = RATE,
    .channels = CHANNELS,
    .bits = BITS,
    .samples = 48000.0,
};

/*
 * Frame allocation/disposal helpers, needed by code below
 * thin wrappers around libtc facilities
 * I don't care about layering and performance loss, *here*, because
 * frame are supposed to be allocated/disposed ahead of time, and
 * always outside inner (performance-sensitive) loops.
 */

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

/* exported commodities :) */

vframe_list_t *vframe_alloc_single(void)
{
    return tc_new_video_frame(tc_specs.width, tc_specs.height,
                              tc_specs.format, TC_TRUE);
}

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

/* 
 * using an <OOP-ism>accessor</OOP-ism> is also justified here
 * by the fact that we compute (ahead of time) samples value for
 * later usage.
 */
void tc_ring_framebuffer_set_specs(const TCFrameSpecs *specs)
{
    /* silently ignore NULL specs */
    if (specs != NULL) {
        double fps;

        /* raw copy first */
        ac_memcpy(&tc_specs, specs, sizeof(TCFrameSpecs));

        /* restore width/height/bpp
         * (FIXME: temp until we have a way to know the max size that will
         *         be used through the decode/process/encode chain; without
         *         this, -V yuv420p -y raw -F rgb (e.g.) crashes with a
         *         buffer overrun)
         */
        tc_specs.width  = TC_MAX_V_FRAME_WIDTH;
        tc_specs.height = TC_MAX_V_FRAME_HEIGHT;
        tc_specs.format = TC_CODEC_RGB;

        /* then deduct missing parameters */
        if (tc_frc_code_to_value(tc_specs.frc, &fps) == TC_NULL_MATCH) {
            fps = 1.0; /* sane, very worst case value */
        }
/*        tc_specs.samples = (double)tc_specs.rate/fps; */
        tc_specs.samples = (double)tc_specs.rate;
        /* 
         * FIXME
         * ok, so we use a MUCH larger buffer (big enough to store 1 *second*
         * of raw audio, not 1 *frame*) than needed for reasons similar as 
         * seen for above video.
         * Most notably, this helps in keeping buffers large enough to be
         * suitable for encoder flush (see encode_lame.c first).
         */
    }
}

/* ------------------------------------------------------------------ */
/* NEW API, yet private                                               */
/* ------------------------------------------------------------------ */

/*
 * Threading notice:
 *
 * Generic code doesn't use any locking at all (yet).
 * That's was a design choice. For clarity, locking is
 * provided by back-compatibility wrapper functions.
 */


/*
 * tc_init_ring_framebuffer: (NOT thread safe)
 *     initialize a framebuffer ringbuffer by allocating needed
 *     amount of frames using given parameters.
 *
 * Parameters:
 *       rfb: ring framebuffer structure to initialize.
 *     specs: frame specifications to use for allocation.
 *     alloc: frame allocation function to use.
 *      free: frame disposal function to use.
 *      size: size of ringbuffer (number of frame to allocate)
 * Return Value:
 *      > 0: wrong (NULL) parameters
 *        0: succesfull
 *      < 0: allocation failed for one or more framesbuffers/internal error
 */
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

/*
 * tc_fini_ring_framebuffer: (NOT thread safe)
 *     finalize a framebuffer ringbuffer by freeing all acquired
 *     resources (framebuffer memory).
 *
 * Parameters:
 *       rfb: ring framebuffer structure to finalize.
 * Return Value:
 *       None.
 */
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

/*
 * tc_ring_framebuffer_retrieve_frame: (NOT thread safe)
 *      retrieve next unclaimed (FRAME_NULL) framebuffer from
 *      ringbuffer and return a pointer to it for later usage
 *      by client code.
 *
 * Parameters:
 *      rfb: ring framebuffer to use
 * Return Value:
 *      Always a framebuffer generic pointer. That can be pointing to
 *      NULL if there aren't no more unclaimed (FRAME_NULL) framebuffer
 *      avalaible; otherwise it contains
 *      a pointer to retrieved framebuffer.
 *      DO NOT *free() such pointer directly! use
 *      tc_ring_framebuffer_release_frame() instead!
 */
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
            ptr.generic = NULL; /* enforce NULL-ness */
        } else {
            if (verbose >= TC_FLIST) {
                tc_log_info(__FILE__, "retrieved buffer = %i [%i]",
                                      rfb->next, ptr.generic->bufid);
            }
            /* adjust internal pointer */
            rfb->next++;
            rfb->next %= rfb->last;
        }
    }
    return ptr;
}

/*
 * tc_ring_framebuffer_release_frame: (NOT thread safe)
 *      release a previously retrieved frame back to ringbuffer,
 *      removing claim from it and making again avalaible (FRAME_NULL).
 *
 * Parameters:
 *         rfb: ring framebuffer to use.
 *       frame: generic pointer to frame to release.
 * Return Value:
 *       > 0: wrong (NULL) parameters.
 *         0: succesfull
 *       < 0: internal error (frame to be released isn't empty).
 */
static int tc_ring_framebuffer_release_frame(TCRingFrameBuffer *rfb,
                                             TCFramePtr frame)
{
    if (rfb == NULL || TCFRAMEPTR_IS_NULL(frame)) {
        return 1;
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

/*
 * tc_ring_framebuffer_register_frame: (NOT thread safe)
 *      retrieve and register a framebuffer froma a ringbuffer by
 *      attaching an ID to it, setup properly status and updating
 *      internal ringbuffer counters.
 *
 *      That's the function that client code is supposed to use
 *      (maybe wrapped by some thin macros to save status setting troubles).
 *      In general, dont' use retrieve_frame directly, use register_frame
 *      instead.
 *
 * Parameters:
 *         rfb: ring framebuffer to use
 *          id: id to attach to registered framebuffer
 *      status: status of framebuffer to register. This was needed to
 *              make registering process multi-purpose.
 * Return Value:
 *      Always a generic framebuffer pointer. That can be pointing to NULL
 *      if there isn't no more framebuffer avalaible on given ringbuffer;
 *      otherwise, it will point to a valid framebuffer.
 */
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

/*
 * tc_ring_framebuffer_remove_frame: (NOT thread safe)
 *      De-register and release a given framebuffer;
 *      also updates internal ringbuffer counters.
 *      
 *      That's the function that client code is supposed to use.
 *      In general, dont' use release_frame directly, use remove_frame
 *      instead.
 *
 * Parameters:
 *        rfb: ring framebuffer to use.
 *      frame: generic pointer to frambuffer to remove.
 * Return Value:
 *      None.
 */
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

/*
 * tc_ring_framebuffer_flush:
 *      unclaim ALL claimed frames on given ringbuffer, maing
 *      ringbuffer ready to be used again.
 *
 * Parameters:
 *      rfb: ring framebuffer to use.
 * Return Value:
 *      amount of framebuffer unclaimed by this function.
 */
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

/*
 * tc_ring_framebuffer_chack_status:
 *      checks if there is at least one frame in a given state
 *      in given ring framebuffer.
 *
 * Parameters:
 *         rfb: ring framebuffer to use.
 *      status: framebuffer status to check on.
 * Return Value
 *      0: there aren't framebuffer of given status on given ringbuffer
 *     !0: there is at least one framebuffer of given status on given
 *         ringbuffer.
 */
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

/* 
 * Macro VS generic functions like above:
 *
 * I've used generic code and TCFramePtr in every place I was
 * capable to introduce them in a *clean* way without using any
 * casting. Of course there is still room for improvements,
 * but back compatibility is an issue too. I'd like to get rid
 * of all those macro and swtich to pure generic code of course,
 * so this will be improved in future revisions. In the
 * meantime, patches and suggestions welcome ;)             -- FR
 */

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
        tc_log_warn(__FILE__, "aframe_dup: empty frame");
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
        tc_log_warn(__FILE__, "aframe_dup: cannot find a free slot"
                              " (%i)", f->id);
#endif
    }
    pthread_mutex_unlock(&aframe_list_lock);
    return frame.audio;
}

vframe_list_t *vframe_dup(vframe_list_t *f)
{
    TCFramePtr frame;

    if (f == NULL) {
        tc_log_warn(__FILE__, "vframe_dup: empty frame");
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
        tc_log_warn(__FILE__, "vframe_dup: cannot find a free slot"
                              " (%i)", f->id);
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
    if (ptr == NULL) {
        tc_log_warn(__FILE__, "aframe_remove: given NULL frame pointer");
    } else {
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
    if (ptr == NULL) {
        tc_log_warn(__FILE__, "vframe_remove: given NULL frame pointer");
    } else {
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
    if (ptr == NULL) {
        /* a bit more of paranoia */
        tc_log_warn(__FILE__, "aframe_set_status: given NULL frame pointer");
    } else {
        pthread_mutex_lock(&aframe_list_lock);
        FRAME_SET_EXT_STATUS(&tc_audio_ringbuffer, ptr, status);
        pthread_mutex_unlock(&aframe_list_lock);
    }
}


void vframe_set_status(vframe_list_t *ptr, int status)
{
    if (ptr == NULL) {
        /* a bit more of paranoia */
        tc_log_warn(__FILE__, "vframe_set_status: given NULL frame pointer");
    } else {
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
        tc_log_warn(__FILE__, "aframe_copy: given NULL frame pointer");
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
        tc_log_warn(__FILE__, "vframe_copy: given NULL frame pointer");
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

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
