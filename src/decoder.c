/*
 *  decoder.c
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - July 2007
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

#include "transcode.h"
#include "dl_loader.h"
#include "filter.h"
#include "framebuffer.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "decoder.h"
#include "encoder.h"
#include "frame_threads.h"
#include "cmdline.h"
#include "probe.h"


/*************************************************************************/

enum {
    TC_IM_THREAD_DONE = 0,     /* import ends as expected          */
    TC_IM_THREAD_INTERRUPT,    /* external event interrupts import */
    TC_IM_THREAD_EXT_ERROR,    /* external (I/O) error             */
    TC_IM_THREAD_INT_ERROR,    /* internal (core) error            */
    TC_IM_THREAD_PROBE_ERROR,  /* source is incompatible           */
};

/*************************************************************************/

typedef struct tcdecoderdata_ TCDecoderData;
struct tcdecoderdata_ {
    const char *tag;

    FILE *fd;

    void *im_handle;

    volatile int active_flag;

    pthread_t thread_id;
    pthread_cond_t list_full_cv;
    pthread_mutex_t lock;
};

static int audio_decode_loop(vob_t *vob);
static int video_decode_loop(vob_t *vob);

/* thread core */
static void *audio_import_thread(void *_vob);
static void *video_import_thread(void *_vob);

static void *seq_import_thread(void *_sid);

static void tc_import_video_start(void);
static void tc_import_audio_start(void);

/*************************************************************************/

static TCDecoderData video_decdata = {
    .tag          = "video",
    .fd           = NULL,
    .im_handle    = NULL,
    .active_flag  = 0,
    .thread_id    = (pthread_t)0,
    .list_full_cv = PTHREAD_COND_INITIALIZER,
    .lock         = PTHREAD_MUTEX_INITIALIZER,
};

static TCDecoderData audio_decdata = {
    .tag          = "audio",
    .fd           = NULL,
    .im_handle    = NULL,
    .active_flag  = 0,
    .thread_id    = (pthread_t)0,
    .list_full_cv = PTHREAD_COND_INITIALIZER,
    .lock         = PTHREAD_MUTEX_INITIALIZER,
};

static pthread_t tc_pthread_main = (pthread_t)0;

/*************************************************************************/
/*************************************************************************/

struct modpair {
    int codec;
    int caps;
};

static const struct modpair audpairs[] = {
    { CODEC_PCM,     TC_CAP_PCM    },
    { CODEC_AC3,     TC_CAP_AC3    },
    { CODEC_RAW,     TC_CAP_AUD    },
    { CODEC_NULL,    TC_CAP_NONE   }
};

static const struct modpair vidpairs[] = {
    { CODEC_RGB,     TC_CAP_RGB    },
    { CODEC_YUV,     TC_CAP_YUV    },
    { CODEC_YUV422,  TC_CAP_YUV422 },
    { CODEC_RAW_YUV, TC_CAP_VID    },
    { CODEC_RAW,     TC_CAP_VID    },
    { CODEC_NULL,    TC_CAP_NONE   }
};


static int check_module_caps(const transfer_t *param, int codec,
                             const struct modpair *mpairs)
{
    int caps = 0;

    if (param->flag == verbose) {
        caps = (codec == mpairs[0].codec);
        /* legacy: grab the first and stay */
    } else {
        int i = 0;

        // module returned capability flag
        if (verbose >= TC_DEBUG) {
            tc_log_msg(__FILE__, "Capability flag 0x%x | 0x%x",
                       param->flag, codec);
        }

        for (i = 0; mpairs[i].codec != CODEC_NULL; i++) {
            if (codec == mpairs[i].codec) {
                caps = (param->flag & mpairs[i].caps);
                break;
            }
        }
    }
    return caps;
}

static void import_lock_cleanup(void *arg)
{
    pthread_mutex_unlock((pthread_mutex_t *)arg);
}

/* optimized fread, use with care */
#ifdef PIPE_BUF
#define BLOCKSIZE PIPE_BUF /* 4096 on linux-x86 */
#else
#define BLOCKSIZE 4096
#endif

static int mfread(uint8_t *buf, int size, int nelem, FILE *f)
{
    int fd = fileno(f);
    int n = 0, r1 = 0, r2 = 0;
    while (n < size*nelem-BLOCKSIZE) {
        if ( !(r1 = read (fd, &buf[n], BLOCKSIZE))) return 0;
        n += r1;
    }
    while (size*nelem-n) {
        if ( !(r2 = read (fd, &buf[n], size*nelem-n)))return 0;
        n += r2;
    }
    return nelem;
}


/*************************************************************************/
/*                           generics                                    */
/*************************************************************************/


/*************************************************************************/
/*               some macro goodies                                      */
/*************************************************************************/

#define RETURN_IF_NULL(HANDLE, MEDIA) do { \
    if ((HANDLE) == NULL) { \
        tc_log_error(PACKAGE, "Loading %s import module failed", (MEDIA)); \
        tc_log_error(PACKAGE, \
                     "Did you enable this module when you ran configure?"); \
        return TC_ERROR; \
    } \
} while (0)

#define RETURN_IF_NOT_SUPPORTED(CAPS, MEDIA) do { \
    if (!(CAPS)) { \
        tc_log_error(PACKAGE, "%s format not supported by import module", \
                     (MEDIA)); \
        return TC_ERROR; \
    } \
} while (0)

#define RETURN_IF_FUNCTION_FAILED(func, ...) do { \
    int ret = func(__VA_ARGS__); \
    if (ret != TC_OK) { \
        return TC_ERROR; \
    } \
} while (0)

#define RETURN_IF_REGISTRATION_FAILED(PTR, MEDIA) do { \
    /* ok, that's pure paranoia */ \
    if ((PTR) == NULL) { \
        tc_log_error(__FILE__, "frame registration failed (%s)", (MEDIA)); \
        return TC_IM_THREAD_INT_ERROR; \
    } \
} while (0)

/*************************************************************************/
/*               stream-specific functions                               */
/*************************************************************************/

static void tc_import_video_stop(void)
{
    pthread_mutex_lock(&video_decdata.lock);
    video_decdata.active_flag = 0;
    pthread_mutex_unlock(&video_decdata.lock);

    sleep(tc_decoder_delay);
}

static void tc_import_audio_stop(void)
{
    pthread_mutex_lock(&audio_decdata.lock);
    audio_decdata.active_flag = 0;
    pthread_mutex_unlock(&audio_decdata.lock);

    sleep(tc_decoder_delay);
}

static int tc_import_audio_open(vob_t *vob)
{
    int ret;
    transfer_t import_para;

    memset(&import_para, 0, sizeof(transfer_t));

    // start audio stream
    import_para.flag = TC_AUDIO;

    ret = tca_import(TC_IMPORT_OPEN, &import_para, vob);
    if (ret < 0) {
        tc_log_error(PACKAGE, "audio import module error: OPEN failed");
        return TC_ERROR;
    }

    audio_decdata.fd = import_para.fd;

    return TC_OK;
}


static int tc_import_video_open(vob_t *vob)
{
    int ret;
    transfer_t import_para;

    memset(&import_para, 0, sizeof(transfer_t));

    // start video stream
    import_para.flag = TC_VIDEO;

    ret = tcv_import(TC_IMPORT_OPEN, &import_para, vob);
    if (ret < 0) {
        tc_log_error(PACKAGE, "video import module error: OPEN failed");
        return TC_ERROR;
    }

    video_decdata.fd = import_para.fd;

    return TC_OK;

}


static int tc_import_audio_close(void)
{
    int ret;
    transfer_t import_para;

    //TC_AUDIO:

    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = TC_AUDIO;
    import_para.fd   = audio_decdata.fd;

    ret = tca_import(TC_IMPORT_CLOSE, &import_para, NULL);
    if (ret == TC_IMPORT_ERROR) {
        tc_log_warn(PACKAGE, "audio import module error: CLOSE failed");
        return TC_ERROR;
    }
    audio_decdata.fd = NULL;

    return TC_OK;
}

static int tc_import_video_close(void)
{
    int ret;
    transfer_t import_para;

    //TC_VIDEO:

    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = TC_VIDEO;
    import_para.fd   = video_decdata.fd;

    ret = tcv_import(TC_IMPORT_CLOSE, &import_para, NULL);
    if (ret == TC_IMPORT_ERROR) {
        tc_log_warn(PACKAGE, "video import module error: CLOSE failed");
        return TC_ERROR;
    }
    video_decdata.fd = NULL;

    return TC_OK;
}

/* video chunk decode loop */

#define MARK_TIME_RANGE(PTR, VOB) do { \
    /* Set skip attribute based on -c */ \
    if (fc_time_contains((VOB)->ttime, (PTR)->id)) \
        (PTR)->attributes &= ~TC_FRAME_IS_OUT_OF_RANGE; \
    else \
        (PTR)->attributes |= TC_FRAME_IS_OUT_OF_RANGE; \
} while (0)


static int video_decode_loop(vob_t *vob)
{
    long int i = 0;
    int ret = 0, vbytes = 0;
    vframe_list_t *ptr = NULL;
    transfer_t import_para;

    int have_vframe_threads = tc_frame_threads_have_video_workers();

    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "video thread id=%ld", (unsigned long)pthread_self());

    vbytes = vob->im_v_size;

    for (; TC_TRUE; i++) {
        if (verbose >= TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] V=%d bytes", "requesting", i, vbytes);

        pthread_testcancel();

        /* stage 1: get new blank frame */
        pthread_mutex_lock(&vframe_list_lock);
        pthread_cleanup_push(import_lock_cleanup, &vframe_list_lock);

        while (!vframe_fill_level(TC_BUFFER_NULL)) {
            pthread_cond_wait(&video_decdata.list_full_cv, &vframe_list_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
            pthread_testcancel();
#endif
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&vframe_list_lock);

        /* stage 2: register acquired frame */
        ptr = vframe_register(i);
        /* ok, that's pure paranoia */
        RETURN_IF_REGISTRATION_FAILED(ptr, "video");

        /* stage 3: fill the frame with data */
        ptr->attributes = 0;
        MARK_TIME_RANGE(ptr, vob);

        if (video_decdata.fd != NULL) {
            if (vbytes && (ret = mfread(ptr->video_buf, vbytes, 1, video_decdata.fd)) != 1)
                ret = -1;
            ptr->video_size = vbytes;
        } else {
            import_para.fd         = NULL;
            import_para.buffer     = ptr->video_buf;
            import_para.buffer2    = ptr->video_buf2;
            import_para.size       = vbytes;
            import_para.flag       = TC_VIDEO;
            import_para.attributes = ptr->attributes;

            ret = tcv_import(TC_IMPORT_DECODE, &import_para, vob);

            ptr->video_size = import_para.size;
            ptr->attributes |= import_para.attributes;
        }

        if (ret < 0) {
            if (verbose >= TC_DEBUG)
                tc_log_msg(__FILE__, "video data read failed - end of stream");

            ptr->video_size = 0;
            ptr->attributes = TC_FRAME_IS_END_OF_STREAM;
        }

        ptr->v_height   = vob->im_v_height;
        ptr->v_width    = vob->im_v_width;
        ptr->v_bpp      = BPP;

        pthread_testcancel();

        /* stage 4: account filled frame and process it if needed */
        pthread_mutex_lock(&vbuffer_im_fill_lock);
        vbuffer_im_fill_ctr++;
        pthread_mutex_unlock(&vbuffer_im_fill_lock);

        if (TC_FRAME_NEED_PROCESSING(ptr)) {
            //first stage pre-processing - (synchronous)
            preprocess_vid_frame(vob, ptr);

            //filter pre-processing - (synchronous)
            ptr->tag = TC_VIDEO|TC_PRE_S_PROCESS;
            tc_filter_process((frame_list_t *)ptr);
        }

        /* stage 5: push frame to next transcoding layer */
        if (!have_vframe_threads) {
            vframe_set_status(ptr, FRAME_READY);
            tc_export_video_notify();
        } else {
            vframe_set_status(ptr, FRAME_WAIT);
            tc_frame_threads_notify_video(TC_FALSE);
        }

        if (verbose >= TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] V=%d bytes", "received", i, ptr->video_size);

        if (ret < 0) {
            tc_import_video_stop();
            return TC_IM_THREAD_DONE;
        }
        if (tc_interrupted()) {
            return TC_IM_THREAD_INTERRUPT;
        }
    }
}

/* audio chunk decode loop */

#define GET_AUDIO_FRAME do { \
    if (audio_decdata.fd != NULL) { \
        if (abytes && (ret = mfread(ptr->audio_buf, abytes, 1, audio_decdata.fd)) != 1) { \
            ret = -1; \
        } \
        ptr->audio_size = abytes; \
    } else { \
        import_para.fd         = NULL; \
        import_para.buffer     = ptr->audio_buf; \
        import_para.size       = abytes; \
        import_para.flag       = TC_AUDIO; \
        import_para.attributes = ptr->attributes; \
        \
        ret = tca_import(TC_IMPORT_DECODE, &import_para, vob); \
        \
        ptr->audio_size = import_para.size; \
    } \
} while (0)

static int audio_decode_loop(vob_t *vob)
{
    long int i = 0;
    int ret = 0, abytes;
    aframe_list_t *ptr = NULL;
    transfer_t import_para;

    int have_aframe_threads = tc_frame_threads_have_audio_workers();

    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "audio thread id=%ld",
                   (unsigned long)pthread_self());

    abytes = vob->im_a_size;

    for (; TC_TRUE; i++) {
        /* tage 0: udio adjustment for non PAL frame rates: */
        if (i != 0 && i % TC_LEAP_FRAME == 0) {
            abytes = vob->im_a_size + vob->a_leap_bytes;
        } else {
            abytes = vob->im_a_size;
        }

        if (verbose >= TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] A=%d bytes", "requesting", i, abytes);

        pthread_testcancel();

        /* stage 1: get new blank frame */
        pthread_mutex_lock(&aframe_list_lock);
        pthread_cleanup_push(import_lock_cleanup, &aframe_list_lock);

        while (!aframe_fill_level(TC_BUFFER_NULL)) {
            pthread_cond_wait(&audio_decdata.list_full_cv, &aframe_list_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
            pthread_testcancel();
#endif
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&aframe_list_lock);

        /* stage 2: register acquired frame */
        ptr = aframe_register(i);
        /* ok, that's pure paranoia */
        RETURN_IF_REGISTRATION_FAILED(ptr, "audio");

        ptr->attributes = 0;
        MARK_TIME_RANGE(ptr, vob);

        /* stage 3: fill the frame with data */
        /* stage 3.1: resync audio by discarding frames, if needed */
        if (vob->sync > 0) {
            // discard vob->sync frames
            while (vob->sync--) {
                GET_AUDIO_FRAME;

                if (ret == -1)
                    break;
            }
            vob->sync++;
        }

        /* stage 3.2: grab effective audio data */
        if (vob->sync == 0) {
            GET_AUDIO_FRAME;
        }

        /* stage 3.3: silence at last */
        if (vob->sync < 0) {
            if (verbose >= TC_DEBUG)
                tc_log_msg(__FILE__, " zero padding %d", vob->sync);
            memset(ptr->audio_buf, 0, abytes);
            ptr->audio_size = abytes;
            vob->sync++;
        }
        /* stage 3.x: all this stuff can be done in a cleaner way... */


        if (ret < 0) {
            if (verbose >= TC_DEBUG)
                tc_log_msg(__FILE__, "audio data read failed - end of stream");

            ptr->audio_size = 0;
            ptr->attributes = TC_FRAME_IS_END_OF_STREAM;
        }

        // init frame buffer structure with import frame data
        ptr->a_rate = vob->a_rate;
        ptr->a_bits = vob->a_bits;
        ptr->a_chan = vob->a_chan;

        pthread_testcancel();

        /* stage 4: account filled frame and process it if needed */
        pthread_mutex_lock(&abuffer_im_fill_lock);
        abuffer_im_fill_ctr++;
        pthread_mutex_unlock(&abuffer_im_fill_lock);

        if (TC_FRAME_NEED_PROCESSING(ptr)) {
            ptr->tag = TC_AUDIO|TC_PRE_S_PROCESS;
            tc_filter_process((frame_list_t *)ptr);
        }

        /* stage 5: push frame to next transcoding layer */
        if (!have_aframe_threads) {
            aframe_set_status(ptr, FRAME_READY);
            tc_export_audio_notify();
        } else {
            aframe_set_status(ptr, FRAME_WAIT);
            tc_frame_threads_notify_audio(TC_FALSE);
        }

        if (verbose >= TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] A=%d bytes", "received",
                                 i, ptr->audio_size);

        if (ret < 0) {
            tc_import_audio_stop();
            return TC_IM_THREAD_DONE;
        }
        if (tc_interrupted()) {
            return TC_IM_THREAD_INTERRUPT;
        }
    }
}

#undef GET_AUDIO_FRAME
#undef MARK_TIME_RANGE

static void tc_import_video_start(void)
{
    pthread_mutex_lock(&video_decdata.lock);
    video_decdata.active_flag = 1;
    pthread_mutex_unlock(&video_decdata.lock);
}

static void tc_import_audio_start(void)
{
    pthread_mutex_lock(&audio_decdata.lock);
    audio_decdata.active_flag = 1;
    pthread_mutex_unlock(&audio_decdata.lock);
}

int tc_import_audio_status(void)
{
    int flag;

    pthread_mutex_lock(&audio_decdata.lock);
    flag = audio_decdata.active_flag;
    pthread_mutex_unlock(&audio_decdata.lock);

    return (flag || aframe_have_data());
}

int tc_import_video_status(void)
{
    int flag;

    pthread_mutex_lock(&video_decdata.lock);
    flag = video_decdata.active_flag;
    pthread_mutex_unlock(&video_decdata.lock);

    return (flag || vframe_have_data());
}

/*************************************************************************/

void tc_import_audio_notify(void)
{
    /* notify sleeping import thread */
    pthread_mutex_lock(&aframe_list_lock);
    pthread_cond_signal(&audio_decdata.list_full_cv);
    pthread_mutex_unlock(&aframe_list_lock);
}

void tc_import_video_notify(void)
{
    /* notify sleeping import thread */
    pthread_mutex_lock(&vframe_list_lock);
    pthread_cond_signal(&video_decdata.list_full_cv);
    pthread_mutex_unlock(&vframe_list_lock);
}


/*************************************************************************/
/*        ladies and gentlemens, the thread routines                     */
/*************************************************************************/

/* audio decode thread wrapper */
static void *audio_import_thread(void *_vob)
{
    static int ret = 0;
    ret = audio_decode_loop(_vob);
    pthread_exit(&ret);
}

/* video decode thread wrapper */
static void *video_import_thread(void *_vob)
{
    static int ret = 0;
    ret = video_decode_loop(_vob);
    pthread_exit(&ret);
}


/*************************************************************************/
/*               main API functions                                      */
/*************************************************************************/

#ifdef HAVE_IBP
extern pthread_mutex_t xio_lock;
#endif

void tc_import_threads_cancel(void)
{
    void *status = NULL;
    int vret, aret;

#ifdef HAVE_IBP
    pthread_mutex_lock(&xio_lock);
#endif

    if (tc_decoder_delay)
        tc_log_info(__FILE__, "sleeping for %d seconds to cool down", tc_decoder_delay);

    // notify import threads, if not yet done, that task is done
    tc_import_video_stop();
    tc_import_audio_stop();

    tc_frame_threads_notify_video(TC_TRUE);
    tc_frame_threads_notify_audio(TC_TRUE);

    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "import stop requested by client=%ld"
                             " (main=%ld) import status=%d",
                             (unsigned long)pthread_self(),
                             (unsigned long)tc_pthread_main,
                             tc_import_status());

    vret = pthread_cancel(video_decdata.thread_id);
    aret = pthread_cancel(audio_decdata.thread_id);

    if (verbose >= TC_DEBUG) {
        if (vret == ESRCH)
            tc_log_msg(__FILE__, "video thread already terminated");
        if (aret == ESRCH)
            tc_log_msg(__FILE__, "audio thread already terminated");

        tc_log_msg(__FILE__, "A/V import canceled (%ld) (%ld)",
                   (unsigned long)pthread_self(),
                   (unsigned long)tc_pthread_main);

    }

    //wait for threads to terminate
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
    /* in facts our threading code is broken in more than one sense... FR */
    pthread_cond_signal(&video_decdata.list_full_cv);
#endif
#ifdef HAVE_IBP
    pthread_mutex_unlock(&xio_lock);
#endif
    vret = pthread_join(video_decdata.thread_id, &status);

    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "video thread exit (ret_code=%d) (status_code=%lu)",
                   vret, (unsigned long)status);

#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
    pthread_cond_signal(&audio_decdata.list_full_cv);
#endif
#ifdef HAVE_IBP
    pthread_mutex_unlock(&xio_lock);
#endif
    aret = pthread_join(audio_decdata.thread_id, &status);

    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "audio thread exit (ret_code=%d) (status_code=%lu)",
                    aret, (unsigned long) status);

    vret = pthread_mutex_trylock(&vframe_list_lock);

    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "vframe_list_lock=%s", (vret==EBUSY)? "BUSY":"0");
    if (vret == 0)
        pthread_mutex_unlock(&vframe_list_lock);

    aret = pthread_mutex_trylock(&aframe_list_lock);
    if (verbose >= TC_DEBUG)
        tc_log_msg(__FILE__, "aframe_list_lock=%s", (aret==EBUSY)? "BUSY":"0");
    if (aret == 0)
        pthread_mutex_unlock(&aframe_list_lock);
}

/*************************************************************************/

void tc_import_threads_create(vob_t *vob)
{
    int ret;

    tc_import_audio_start();
    ret = pthread_create(&audio_decdata.thread_id, NULL,
                         audio_import_thread, vob);
    if (ret != 0)
        tc_error("failed to start audio stream import thread");

    tc_import_video_start();
    ret = pthread_create(&video_decdata.thread_id, NULL,
                         video_import_thread, vob);
    if (ret != 0)
        tc_error("failed to start video stream import thread");
}


int tc_import_init(vob_t *vob, const char *a_mod, const char *v_mod)
{
    transfer_t import_para;
    int caps;

    a_mod = (a_mod == NULL) ?TC_DEFAULT_IMPORT_AUDIO :a_mod;
    audio_decdata.im_handle = load_module(a_mod, TC_IMPORT+TC_AUDIO);
    RETURN_IF_NULL(audio_decdata.im_handle, "audio");

    v_mod = (v_mod == NULL) ?TC_DEFAULT_IMPORT_VIDEO :v_mod;
    video_decdata.im_handle = load_module(v_mod, TC_IMPORT+TC_VIDEO);
    RETURN_IF_NULL(video_decdata.im_handle, "video");

    memset(&import_para, 0, sizeof(transfer_t));
    import_para.flag = verbose;
    tca_import(TC_IMPORT_NAME, &import_para, NULL);

    caps = check_module_caps(&import_para, vob->im_a_codec, audpairs);
    RETURN_IF_NOT_SUPPORTED(caps, "audio");
    
    memset(&import_para, 0, sizeof(transfer_t));
    import_para.flag = verbose;
    tcv_import(TC_IMPORT_NAME, &import_para, NULL);

    caps = check_module_caps(&import_para, vob->im_v_codec, vidpairs);
    RETURN_IF_NOT_SUPPORTED(caps, "video");

    tc_pthread_main = pthread_self();

    return TC_OK;
}


int tc_import_open(vob_t *vob)
{
    RETURN_IF_FUNCTION_FAILED(tc_import_audio_open, vob);
    RETURN_IF_FUNCTION_FAILED(tc_import_video_open, vob);

    return TC_OK;
}

int tc_import_close(void)
{
    RETURN_IF_FUNCTION_FAILED(tc_import_audio_close);
    RETURN_IF_FUNCTION_FAILED(tc_import_video_close);

    return TC_OK;
}


void tc_import_shutdown(void)
{
    if (verbose >= TC_DEBUG) {
        tc_log_msg(__FILE__, "unloading audio import module");
    }

    unload_module(audio_decdata.im_handle);
    audio_decdata.im_handle = NULL;

    if (verbose >= TC_DEBUG) {
        tc_log_msg(__FILE__, "unloading video import module");
    }

    unload_module(video_decdata.im_handle);
    video_decdata.im_handle = NULL;
}

/**************************************************************************/
/* major import status query:                                             */
/* 1 = import still active OR some frames still buffered/processed        */
/* 0 = shutdown as soon as possible                                       */
/**************************************************************************/

int tc_import_status()
{
    int vstatus = 0, astatus = 0;

    pthread_mutex_lock(&vframe_list_lock);
    vstatus = tc_import_video_status();
    pthread_mutex_unlock(&vframe_list_lock);

    pthread_mutex_lock(&aframe_list_lock);
    astatus = tc_import_audio_status();
    pthread_mutex_unlock(&aframe_list_lock);

    return vstatus && astatus;
}

/*************************************************************************/
/*        the new API                                                    */
/*************************************************************************/


static int probe_im_stream(const char *src, ProbeInfo *info)
{
    static pthread_mutex_t probe_mutex = PTHREAD_MUTEX_INITIALIZER; /* XXX */
    /* UGLY! */
    int ret = 1; /* be optimistic! */

    pthread_mutex_lock(&probe_mutex);
    ret = probe_stream_data(src, seek_range, info);
    pthread_mutex_unlock(&probe_mutex);

    return ret;
}

static int probe_matches(const ProbeInfo *ref, const ProbeInfo *cand, int i)
{
    if (ref->width  != cand->width || ref->height != cand->height
     || ref->frc    != cand->frc   || ref->asr    != cand->asr
     || ref->codec  != cand->codec) {
        return 0;
    }

    if (i >= ref->num_tracks || i >= cand->num_tracks) {
        return 0;
    }
    if (ref->track[i].samplerate != cand->track[i].samplerate
     || ref->track[i].chan       != cand->track[i].chan    
     || ref->track[i].bits       != cand->track[i].bits    
     || ref->track[i].format     != cand->track[i].format    ) {
        return 0;
    }       

    return 1;
}

static void probe_from_vob(ProbeInfo *info, const vob_t *vob)
{
    /* copy only interesting fields */
    if (info != NULL && vob != NULL) {
        int i = 0;

        info->width    = vob->im_v_width;
        info->height   = vob->im_v_height;
        info->codec    = vob->v_codec_flag;
        info->asr      = vob->im_asr;
        info->frc      = vob->im_frc;

        for (i = 0; i < TC_MAX_AUD_TRACKS; i++) {
            memset(&(info->track[i]), 0, sizeof(ProbeTrackInfo));
        }
        i = vob->a_track;

        info->track[i].samplerate = vob->a_rate;
        info->track[i].chan       = vob->a_chan;
        info->track[i].bits       = vob->a_bits;
        info->track[i].format     = vob->a_codec_flag;
    }
}

/* ok, that sucks. I know. I can't do any better now. */
static const char *current_in_file(vob_t *vob, int kind)
{
    if (kind == TC_VIDEO)
    	return vob->video_in_file;
    if (kind == TC_AUDIO)
    	return vob->video_in_file;
    return NULL; /* cannot happen */
}

#define RETURN_IF_PROBE_FAILED(ret, src) do {                        \
    if (ret == 0) {                                                  \
        tc_log_error(PACKAGE, "probing of source '%s' failed", src); \
        status = TC_IM_THREAD_PROBE_ERROR;                           \
        pthread_exit(&status);                                       \
    }                                                                \
} while (0)

/* black magic in here? Am I looking for troubles? */
#define SWAP(type, a, b) do { \
    type tmp = a;             \
    a = b;                    \
    b = tmp;                  \
} while (0)


/*************************************************************************/

typedef struct tcseqimportdata_ TCSeqImportData;
struct tcseqimportdata_ {
    int kind;

    TCDecoderData *decdata;
    vob_t *vob;

    ProbeInfo infos;

    int (*open)(vob_t *vob);
    int (*decode_loop)(vob_t *vob);
    int (*close)(void);

    int (*next)(vob_t *vob);
};
/* FIXME: explain a such horrible thing */


#define SEQDATA_INIT(MEDIA, VOB, KIND) do {                         \
    memset(&(MEDIA ## _seqdata.infos), 0, sizeof(ProbeInfo));       \
                                                                    \
    MEDIA ## _seqdata.kind         = KIND;                          \
                                                                    \
    MEDIA ## _seqdata.vob          = VOB;                           \
    MEDIA ## _seqdata.decdata      = &(MEDIA ## _decdata);          \
                                                                    \
    MEDIA ## _seqdata.open         = tc_import_ ## MEDIA ## _open;  \
    MEDIA ## _seqdata.decode_loop  = MEDIA ## _decode_loop;         \
    MEDIA ## _seqdata.close        = tc_import_ ## MEDIA ## _close; \
    MEDIA ## _seqdata.next         = tc_next_ ## MEDIA ## _in_file; \
} while (0)

#define SEQDATA_FINI(MEDIA) do { \
    ; /* nothing */ \
} while (0)



static TCSeqImportData audio_seqdata;
static TCSeqImportData video_seqdata;

/*************************************************************************/

static void *seq_import_thread(void *_sid)
{
    static int status = TC_IM_THREAD_DONE;
    TCSeqImportData *sid = _sid;
    int i, ret = TC_OK, track_id = sid->vob->a_track;
    ProbeInfo infos;
    ProbeInfo *old = &(sid->infos), *new = &infos;
    const char *fname = NULL;

    for (i = 0; TC_TRUE; i++) {
        /* shutdown test */
        if (tc_interrupted()) {
            status = TC_IM_THREAD_INTERRUPT;
            break;
        }

        ret = sid->open(sid->vob);
        if (ret == TC_ERROR) {
            status = TC_IM_THREAD_EXT_ERROR;
            break;
        }

        status = sid->decode_loop(sid->vob);
        /* source should always be closed */

        ret = sid->close();
        if (ret == TC_ERROR) {
            status = TC_IM_THREAD_EXT_ERROR;
        }

        if (status != TC_IM_THREAD_DONE) {
            break;
        }

        ret = sid->next(sid->vob);
        if (ret == TC_ERROR) {
            status = TC_IM_THREAD_DONE;
            break;
        }

	fname = current_in_file(sid->vob, sid->kind);
        /* probing coherency check */
        ret = probe_im_stream(fname, new);
        RETURN_IF_PROBE_FAILED(ret, fname);

        if (probe_matches(old, new, track_id)) {
            if (verbose) {
                tc_log_info(__FILE__, "switching to %s source #%i: %s",
                            sid->decdata->tag, i, fname);
            }
        } else {
            tc_log_error(PACKAGE, "source '%s' in directory"
                                  " not compatible with former", fname);
            status = TC_IM_THREAD_PROBE_ERROR;
            break;
        }
        /* now prepare for next probing round by swapping pointers */
        SWAP(ProbeInfo*, old, new);
    }
    pthread_exit(&status);
}

/*************************************************************************/

void tc_seq_import_threads_create(vob_t *vob)
{
    int ret;

    probe_from_vob(&(audio_seqdata.infos), vob);
    SEQDATA_INIT(audio, vob, TC_AUDIO);
    tc_import_audio_start();
    ret = pthread_create(&audio_decdata.thread_id, NULL,
                         seq_import_thread, &audio_seqdata);
    if (ret != 0) {
        tc_error("failed to start sequential audio stream import thread");
    }

    probe_from_vob(&(video_seqdata.infos), vob);
    SEQDATA_INIT(video, vob, TC_VIDEO);
    tc_import_video_start();
    ret = pthread_create(&video_decdata.thread_id, NULL,
                         seq_import_thread, &video_seqdata);
    if (ret != 0) {
        tc_error("failed to start sequential video stream import thread");
    }

    tc_info("sequential streams import threads started");
}


void tc_seq_import_threads_cancel(void)
{
    SEQDATA_FINI(audio);
    SEQDATA_FINI(video);

    tc_import_threads_cancel();
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
