/*
 *  decoder.c
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

// stream handle
static FILE *fd_ppm = NULL, *fd_pcm = NULL;

// import module handle
static void *import_ahandle = NULL, *import_vhandle = NULL;

// import flags (1=import active | 0=import closed)
static volatile int aimport = 0;
static volatile int vimport = 0;
static pthread_mutex_t import_v_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t import_a_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t athread = 0, vthread = 0;

// threads
static void aimport_thread(vob_t *vob);
static void vimport_thread(vob_t *vob);

static pthread_cond_t aframe_list_full_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t vframe_list_full_cv = PTHREAD_COND_INITIALIZER;

//-------------------------------------------------------------------------
//
// signal handler callback
//
//--------------------------------------------------------------------------

void tc_import_stop_nolock()
{
    vimport = 0;
    aimport = 0;
    return;
}

//-------------------------------------------------------------------------
//
// callback for external import threads shutdown request
//
//--------------------------------------------------------------------------

static void tc_import_stop(void)
{
    vimport_stop();
    aimport_stop();

    frame_threads_notify_video(TC_TRUE);
    frame_threads_notify_audio(TC_TRUE);

    if(verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "import stop requested by client=%ld"
                             " (main=%ld) import status=%d",
                             (unsigned long)pthread_self(),
                             (unsigned long)tc_pthread_main,
                             import_status());
}

//-------------------------------------------------------------------------
//
// cancel import threads
//
// called by transcode (signal handler thread)
//
//-------------------------------------------------------------------------

#ifdef HAVE_IBP
extern pthread_mutex_t xio_lock;
#endif

void import_threads_cancel()
{
    void *status = NULL;
    int vret, aret;

#ifdef HAVE_IBP
    pthread_mutex_lock(&xio_lock);
#endif

    if (tc_decoder_delay)
        tc_log_info(__FILE__, "sleeping for %d seconds to cool down", tc_decoder_delay);

    // notify import threads, if not yet done, that task is done
    tc_import_stop();

    vret = pthread_cancel(vthread);
    aret = pthread_cancel(athread);

    if (verbose & TC_DEBUG) {
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
    pthread_cond_signal(&vframe_list_full_cv);
#endif
#ifdef HAVE_IBP
    pthread_mutex_unlock(&xio_lock);
#endif
    vret = pthread_join(vthread, &status);

    if (verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "video thread exit (ret_code=%d) (status_code=%lu)",
                   vret, (unsigned long)status);

#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
    pthread_cond_signal(&aframe_list_full_cv);
#endif
#ifdef HAVE_IBP
    pthread_mutex_unlock(&xio_lock);
#endif
    aret = pthread_join(athread, &status);

    if (verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "audio thread exit (ret_code=%d) (status_code=%lu)",
                    aret, (unsigned long) status);

    vret = pthread_mutex_trylock(&vframe_list_lock);

    if (verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "vframe_list_lock=%s", (vret==EBUSY)? "BUSY":"0");
    if (vret == 0)
        pthread_mutex_unlock(&vframe_list_lock);

    aret = pthread_mutex_trylock(&aframe_list_lock);
    if (verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "aframe_list_lock=%s", (aret==EBUSY)? "BUSY":"0");
    if (aret == 0)
        pthread_mutex_unlock(&aframe_list_lock);
}

//-------------------------------------------------------------------------

void import_threads_create(vob_t *vob)
{
    //start import threads
    //flag on, in case we restart the decoder

    aimport_start();

    if (pthread_create(&athread, NULL, (void *)aimport_thread, vob) != 0)
        tc_error("failed to start audio stream import thread");

    vimport_start();

    if (pthread_create(&vthread, NULL, (void *)vimport_thread, vob) != 0)
        tc_error("failed to start video stream import thread");
}

//-------------------------------------------------------------------------
//
// initialize import by loading modules and checking capabilities
//
// called by transcode (main thread) in transcoder()
//
//-------------------------------------------------------------------------

#define FAIL_IF_NULL(HANDLE, MEDIA) do { \
    if ((HANDLE) == NULL) { \
        tc_log_error(PACKAGE, "Loading %s import module failed", (MEDIA)); \
        tc_log_error(PACKAGE, "Did you enable this module when you ran configure?"); \
        return TC_ERROR; \
    } \
} while (0)

#define FAIL_IF_NOT_SUPPORTED(CAPS, MEDIA) do { \
    if (!(CAPS)) { \
        tc_log_error(PACKAGE, "%s format not supported by import module", (MEDIA)); \
        return TC_ERROR; \
    } \
} while (0)


int import_init(vob_t *vob, char *a_mod, char *v_mod)
{
    transfer_t import_para;
    int caps;

    // load audio import module

    import_ahandle = load_module(((a_mod==NULL)? TC_DEFAULT_IMPORT_AUDIO: a_mod), TC_IMPORT+TC_AUDIO);
    FAIL_IF_NULL(import_ahandle, "audio");

    aimport_start();

    // load video import module

    import_vhandle = load_module(((v_mod==NULL)? TC_DEFAULT_IMPORT_VIDEO: v_mod), TC_IMPORT+TC_VIDEO);
    FAIL_IF_NULL(import_vhandle, "video");

    vimport_start();

    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = verbose;
    tca_import(TC_IMPORT_NAME, &import_para, NULL);

    if (import_para.flag != verbose) {
        // module returned capability flag
        if(verbose & TC_DEBUG)
            tc_log_msg(__FILE__, "Audio capability flag 0x%x | 0x%x",
                       import_para.flag, vob->im_a_codec);

        switch (vob->im_a_codec) {
          case CODEC_PCM:
            caps = (import_para.flag & TC_CAP_PCM);
            break;
          case CODEC_AC3:
            caps = (import_para.flag & TC_CAP_AC3);
            break;
          case CODEC_RAW:
            caps = (import_para.flag & TC_CAP_AUD);
            break;
          default:
            caps = 0;
        }
    } else
        caps = vob->im_a_codec == CODEC_PCM;

    FAIL_IF_NOT_SUPPORTED(caps, "audio");

    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = verbose;
    tcv_import(TC_IMPORT_NAME, &import_para, NULL);

    if (import_para.flag != verbose) {
        // module returned capability flag

        if(verbose & TC_DEBUG)
            tc_log_msg(__FILE__, "Video capability flag 0x%x | 0x%x",
                       import_para.flag, vob->im_v_codec);

        switch (vob->im_v_codec) {
          case CODEC_RGB:
            caps = (import_para.flag & TC_CAP_RGB);
            break;
          case CODEC_YUV:
            caps = (import_para.flag & TC_CAP_YUV);
            break;
          case CODEC_YUV422:
            caps = (import_para.flag & TC_CAP_YUV422);
            break;
          case CODEC_RAW_YUV: /* fallthrough */
          case CODEC_RAW:
            caps = (import_para.flag & TC_CAP_VID);
            break;
          default:
            caps = 0;
        }
    } else
        caps = vob->im_v_codec == CODEC_RGB;

    FAIL_IF_NOT_SUPPORTED(caps, "video");

    return TC_OK;
}

#undef FAIL_IF_NULL
#undef FAIL_IF_NOT_SUPPORTED


//-------------------------------------------------------------------------
//
// initialize modules for opening files and decoding etc.
//
// called by transcode (main thread)
//
//-------------------------------------------------------------------------

int import_open(vob_t *vob)
{
    transfer_t import_para;

    memset(&import_para, 0, sizeof(transfer_t));

    // start audio stream
    import_para.flag = TC_AUDIO;

    if (tca_import(TC_IMPORT_OPEN, &import_para, vob) < 0) {
        tc_log_error(PACKAGE, "audio import module error: OPEN failed");
        return TC_ERROR;
    }

    fd_pcm = import_para.fd;

    memset(&import_para, 0, sizeof(transfer_t));

    // start video stream
    import_para.flag = TC_VIDEO;

    if (tcv_import(TC_IMPORT_OPEN, &import_para, vob) < 0) {
        tc_log_error(PACKAGE, "video import module error: OPEN failed");
        return TC_ERROR;
    }

    fd_ppm = import_para.fd;

    // now we can start the import threads, the file handles are valid
    return TC_OK;
}


//-------------------------------------------------------------------------
//
// prepare modules for decoder shutdown etc.
//
// called by transcode (main thread)
//
//-------------------------------------------------------------------------

int import_close(void)
{
    int ret;
    transfer_t import_para;

    //TC_VIDEO:

    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = TC_VIDEO;
    import_para.fd   = fd_ppm;

    ret = tcv_import(TC_IMPORT_CLOSE, &import_para, NULL);
    if (ret == TC_IMPORT_ERROR) {
        tc_log_warn(PACKAGE, "video import module error: CLOSE failed");
        return TC_ERROR;
    }
    fd_ppm = NULL;

    //TC_AUDIO:

    memset(&import_para, 0, sizeof(transfer_t));

    import_para.flag = TC_AUDIO;
    import_para.fd   = fd_pcm;

    ret = tca_import(TC_IMPORT_CLOSE, &import_para, NULL);
    if (ret == TC_IMPORT_ERROR) {
        tc_log_warn(PACKAGE, "audio import module error: CLOSE failed");
        return TC_ERROR;
    }
    fd_pcm = NULL;

    return TC_OK;
}


//-------------------------------------------------------------------------
//
// check for video import flag
//
// called by video import thread, returns 1 on exit request
//
//-------------------------------------------------------------------------


static int vimport_test_shutdown(void)
{
    int ret;

    pthread_mutex_lock(&import_v_lock);
    ret = vimport;
    pthread_mutex_unlock(&import_v_lock);

    if (!vimport) {
        if(verbose & TC_DEBUG) {
            tc_log_msg(__FILE__, "video import cancelation requested");
        }
        return 1;  // notify thread to exit immediately
    }

    return 0;
}

//-------------------------------------------------------------------------
//
// optimized fread, use with care
//
//-------------------------------------------------------------------------

#ifdef PIPE_BUF
#define BLOCKSIZE PIPE_BUF /* 4096 on linux-x86 */
#else
#define BLOCKSIZE 4096
#endif

static int mfread(char *buf, int size, int nelem, FILE *f)
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
    return (nelem);
}

static void import_lock_cleanup (void *arg)
{
    pthread_mutex_unlock ((pthread_mutex_t *)arg);
}

//-------------------------------------------------------------------------
//
// video import thread
//
//-------------------------------------------------------------------------

#define MARK_TIME_RANGE(PTR, VOB) do { \
    /* Set skip attribute based on -c */ \
    if (fc_time_contains((VOB)->ttime, (PTR)->id)) \
        (PTR)->attributes &= ~TC_FRAME_IS_OUT_OF_RANGE; \
    else \
        (PTR)->attributes |= TC_FRAME_IS_OUT_OF_RANGE; \
} while (0)


void vimport_thread(vob_t *vob)
{
    long int i = 0;
    int ret = 0, vbytes = 0;
    vframe_list_t *ptr=NULL;
    transfer_t import_para;

    if (verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "video thread id=%ld", (unsigned long)pthread_self());

    // bytes per video frame
    vbytes = vob->im_v_size;

    for(;;) {
//        // init structure
//        memset(&import_para, 0, sizeof(transfer_t));

        if (verbose >= TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] V=%d bytes", "requesting", i, vbytes);

        pthread_testcancel();

        //check buffer fill level
        pthread_mutex_lock(&vframe_list_lock);
        pthread_cleanup_push(import_lock_cleanup, &vframe_list_lock);

        while (!vframe_fill_level(TC_BUFFER_NULL)) {
            pthread_cond_wait(&vframe_list_full_cv, &vframe_list_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
            pthread_testcancel();
#endif

            // check for pending shutdown via ^C
            if (vimport_test_shutdown()) {
                pthread_exit((int *)11);
            }
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&vframe_list_lock);

        ptr = vframe_register(i);
        /* ok, that's pure paranoia */
        if (ptr == NULL) {
            tc_log_error(__FILE__, "frame registration failed (V)");
            pthread_exit((int *)63);
        }

        ptr->attributes = 0;
        MARK_TIME_RANGE(ptr, vob);

        // read video frame
        // check if import module reades data

        if (fd_ppm != NULL) {
            if (vbytes && (ret = mfread(ptr->video_buf, vbytes, 1, fd_ppm)) != 1)
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

            // import module return information on true frame size
            // in import_para.size
            ptr->video_size = import_para.size;
            ptr->attributes |= import_para.attributes;
        }

        if (ret < 0) {
            if(verbose & TC_DEBUG)
                tc_log_msg(__FILE__, "video data read failed - end of stream");

            // stop decoder
            vframe_remove(ptr);

            // set flag
            vimport_stop();

            //exit
            pthread_exit( (int *) 13);
        }

        // init frame buffer structure with import frame data
        ptr->v_height   = vob->im_v_height;
        ptr->v_width    = vob->im_v_width;
        ptr->v_bpp      = BPP;
        /* XXX: according to grep -R, nobody uses it anymore */
        // ptr->thread_id  = (int) getpid(); /* XXX: WTF?!? -- fromani */

        pthread_testcancel();

        pthread_mutex_lock(&vbuffer_im_fill_lock);
        vbuffer_im_fill_ctr++;
        pthread_mutex_unlock(&vbuffer_im_fill_lock);

        if (!(ptr->attributes & TC_FRAME_IS_OUT_OF_RANGE)) {
            //first stage pre-processing - (synchronous)
            preprocess_vid_frame(vob, ptr);

            //filter pre-processing - (synchronous)
            ptr->tag = TC_VIDEO|TC_PRE_S_PROCESS;
            tc_filter_process((frame_list_t *)ptr);
        }

        if (have_vframe_threads == 0) {
            vframe_set_status(ptr, FRAME_READY);
            tc_export_video_notify();
        } else {
            // done and ready for encoder
            vframe_set_status(ptr, FRAME_WAIT);
            //notify sleeping frame processing threads
            frame_threads_notify_video(TC_FALSE);
        }

        if (verbose & TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] V=%d bytes", "received", i, ptr->video_size);

        // check for pending shutdown via ^C
        if (vimport_test_shutdown())
            pthread_exit( (int *)14);

        i++; // get next frame
    }
}

//-------------------------------------------------------------------------
//
// check for audio import status flag
//
// called by audio import thread, returns 1 on exit request
//
//-------------------------------------------------------------------------

static int aimport_test_shutdown(void)
{
    int ret;

    pthread_mutex_lock(&import_a_lock);
    ret = aimport;
    pthread_mutex_unlock(&import_a_lock);

    if(!aimport) {
        if(verbose & TC_DEBUG) {
            tc_log_msg(__FILE__, "audio import cancelation requested");
        }

        return 1;  // notify thread to exit immediately
    }
    return 0;
}

//-------------------------------------------------------------------------
//
// audio import thread
//
//-------------------------------------------------------------------------

#define GET_AUDIO_FRAME do { \
    if (fd_pcm != NULL) { \
        if (abytes && (ret = mfread(ptr->audio_buf, abytes, 1, fd_pcm)) != 1) { \
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

void aimport_thread(vob_t *vob)
{
    long int i = 0;
    int ret = 0, abytes;
    aframe_list_t *ptr = NULL;
    transfer_t import_para;

    if (verbose & TC_DEBUG)
        tc_log_msg(__FILE__, "audio thread id=%ld",
                   (unsigned long)pthread_self());

    // bytes per audio frame
    abytes = vob->im_a_size;

    for(;;) {
//        // init structure
//        memset(&import_para, 0, sizeof(transfer_t));

        // audio adjustment for non PAL frame rates:
        if (i != 0 && i % TC_LEAP_FRAME == 0) {
            abytes = vob->im_a_size + vob->a_leap_bytes;
        } else {
            abytes = vob->im_a_size;
        }

        if (verbose >= TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] A=%d bytes", "requesting", i, abytes);

        pthread_testcancel();

        //check buffer fill level
        pthread_mutex_lock(&aframe_list_lock);
        pthread_cleanup_push(import_lock_cleanup, &aframe_list_lock);

        while (!aframe_fill_level(TC_BUFFER_NULL)) {
            pthread_cond_wait(&aframe_list_full_cv, &aframe_list_lock);
#ifdef BROKEN_PTHREADS // Used to be MacOSX specific; kernel 2.6 as well?
            pthread_testcancel();
#endif

            // check for pending shutdown via ^C
            if (aimport_test_shutdown()) {
                pthread_exit((int *)11);
            }
        }

        pthread_cleanup_pop(0);
        pthread_mutex_unlock(&aframe_list_lock);

        ptr = aframe_register(i);
        /* ok, that's pure paranoia */
        if (ptr == NULL) {
            tc_log_error(__FILE__, "frame registration failed (A)");
            pthread_exit((int *)63);
        }

        ptr->attributes = 0;
        MARK_TIME_RANGE(ptr, vob);

        // read audio frame
        if(vob->sync > 0) {
            // discard vob->sync frames
            while (vob->sync--) {
                GET_AUDIO_FRAME;

                if (ret == -1)
                    break;
            }
            vob->sync++;
        }

        // default
        if (vob->sync == 0) {
            GET_AUDIO_FRAME;
        }

        // silence
        if (vob->sync < 0) {
            if (verbose & TC_DEBUG)
                tc_log_msg(__FILE__, " zero padding %d", vob->sync);
            memset(ptr->audio_buf, 0, abytes);
            ptr->audio_size = abytes;
            vob->sync++;
        }


        if (ret < 0) {
            if (verbose & TC_DEBUG)
                tc_log_msg(__FILE__, "audio data read failed - end of stream");

            // stop decoder
            aframe_remove(ptr);

            // set flag
            aimport_stop();

            //exit
            pthread_exit((int *) 13);
        }

        // init frame buffer structure with import frame data
        ptr->a_rate = vob->a_rate;
        ptr->a_bits = vob->a_bits;
        ptr->a_chan = vob->a_chan;
        /* XXX: according to grep -R, nobody uses it anymore */
        // ptr->thread_id = (int) getpid(); /* XXX WTF?! -- fromani */

        pthread_testcancel();

        pthread_mutex_lock(&abuffer_im_fill_lock);
        abuffer_im_fill_ctr++;
        pthread_mutex_unlock(&abuffer_im_fill_lock);

        if (!(ptr->attributes & TC_FRAME_IS_OUT_OF_RANGE)) {
            //first stage pre-processing - (synchronous)
            ptr->tag = TC_AUDIO|TC_PRE_S_PROCESS;
            tc_filter_process((frame_list_t *)ptr);
        }

        //no frame threads?
        if (have_aframe_threads == 0) {
            aframe_set_status(ptr, FRAME_READY);
            tc_export_audio_notify();
        } else {
            // done and ready for encoder
            aframe_set_status(ptr, FRAME_WAIT);
            //notify sleeping frame processing threads
            frame_threads_notify_audio(TC_FALSE);
        }

        if (verbose & TC_STATS)
            tc_log_msg(__FILE__, "%10s [%ld] A=%d bytes", "received", i, ptr->audio_size);

        // check for pending shutdown via ^C
        if (aimport_test_shutdown())
            pthread_exit((int *)14);

        i++; // get next frame
    }
}

//-------------------------------------------------------------------------
//
// unload import modules
//
// called by transcode (main thread) in transcoder()
//
//-------------------------------------------------------------------------

void import_shutdown()
{
    if(verbose & TC_DEBUG) {
        tc_log_msg(__FILE__, "unloading audio import module");
    }

    unload_module(import_ahandle);
    import_ahandle = NULL;

    if(verbose & TC_DEBUG) {
        tc_log_msg(__FILE__, "unloading video import module");
    }

    unload_module(import_vhandle);
    import_vhandle = NULL;
}

//-------------------------------------------------------------------------
//
// set video import status flag to OFF
//
//-------------------------------------------------------------------------

void vimport_stop()
{
    pthread_mutex_lock(&import_v_lock);
    vimport = 0;
    pthread_mutex_unlock(&import_v_lock);

    sleep(tc_decoder_delay);
}

//-------------------------------------------------------------------------
//
// set video import status flag to ON
//
//-------------------------------------------------------------------------

void vimport_start()
{
    pthread_mutex_lock(&import_v_lock);
    vimport = 1;
    pthread_mutex_unlock(&import_v_lock);
}

//-------------------------------------------------------------------------
//
// set audio import status flag to OFF
//
//-------------------------------------------------------------------------

void aimport_stop()
{
    pthread_mutex_lock(&import_a_lock);
    aimport = 0;
    pthread_mutex_unlock(&import_a_lock);

    sleep(tc_decoder_delay);
}


//-------------------------------------------------------------------------
//
// set audio import status flag to ON
//
//-------------------------------------------------------------------------

void aimport_start()
{
    pthread_mutex_lock(&import_a_lock);
    aimport = 1;
    pthread_mutex_unlock(&import_a_lock);
}

//-------------------------------------------------------------------------
//
// check audio import status flag and buffer fill level
//
//-------------------------------------------------------------------------

int aimport_status()
{
    int cc;

    pthread_mutex_lock(&import_a_lock);
    cc = aimport;
    pthread_mutex_unlock(&import_a_lock);

    return (cc || aframe_have_data());
}

//-------------------------------------------------------------------------
//
// check video import status flag and buffer fill level
//
//-------------------------------------------------------------------------

int vimport_status()
{
    int cc;

    pthread_mutex_lock(&import_v_lock);
    cc = vimport;
    pthread_mutex_unlock(&import_v_lock);

    return (cc || vframe_have_data());
}

//-------------------------------------------------------------------------
//
// major import status query:
//
// 1 = import still active OR some frames still buffered/processed
// 0 = shutdown as soon as possible
//
//--------------------------------------------------------------------------

int import_status()
{
    int vstatus = 0, astatus = 0;

    pthread_mutex_lock(&vframe_list_lock);
    vstatus = vimport_status();
    pthread_mutex_unlock(&vframe_list_lock);

    pthread_mutex_lock(&aframe_list_lock);
    astatus = aimport_status();
    pthread_mutex_unlock(&aframe_list_lock);

    return vstatus && astatus;
}

/*************************************************************************/

void tc_import_audio_notify(void)
{
    /* notify sleeping import thread */
    pthread_mutex_lock(&aframe_list_lock);
    pthread_cond_signal(&aframe_list_full_cv);
    pthread_mutex_unlock(&aframe_list_lock);
}

void tc_import_video_notify(void)
{
    /* notify sleeping import thread */
    pthread_mutex_lock(&vframe_list_lock);
    pthread_cond_signal(&vframe_list_full_cv);
    pthread_mutex_unlock(&vframe_list_lock);
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
