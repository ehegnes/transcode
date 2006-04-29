/*
 *  tcprobe.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  updated by
 *  Francesco Romani - >pril 206
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
#include "tcinfo.h"
#include "libtc/libtc.h"
#include "libtc/iodir.h"
#include "libtc/xio.h"
#include "libtc/ratiocodes.h"
#include "ioaux.h"
#include "tc.h"
#include "demuxer.h"
#include "dvd_reader.h"

#include <math.h>

#define EXE "tcprobe"

int verbose = TC_INFO;

int bitrate = ABITRATE;
int binary_dump = 0;

void import_exit(int code)
{
    if (verbose >= TC_DEBUG) {
        tc_log_msg(EXE, "(pid=%d) exit (code %d)", (int) getpid(), code);
    }
    exit(code);
}

/*************************************************************************/

/*
 * enc_bitrate:  Print bitrate information about the source data.
 *
 * Parameters:
 *       frames: Number of frames in the source.
 *          fps: Frames per second of the source.
 *     abitrate: Audio bitrate (bits per second).
 *     discsize: User-specified disc size in bytes, or 0 for none.
 * Return value:
 *     None.
 * Notes:
 *     This function is copied from tcscan.c.  Ideally, tcprobe should
 *     only print basic source information, and this extended information
 *     should be handled by tcscan (or alternatively, tcscan should be
 *     merged into tcprobe).
 */

static void enc_bitrate(long frames, double fps, int abitrate, double discsize)
{
    static const int defsize[] = { 650, 700, 1300, 1400 };
    double audiosize, videosize, vbitrate;
    long time;

    if (frames <= 0 || fps <= 0.0) {
	    return;
    }
    time = frames / fps;
    audiosize = (double)abitrate/8 * time;

    /* Print basic source information */
    tc_log_msg(EXE, "V: %ld frames, %ld sec @ %.3f fps",
               frames, time, fps);
    tc_log_msg(EXE, "A: %.2f MB @ %d kbps",
               audiosize/(1024*1024), abitrate/1000);

    /* Print recommended bitrates for user-specified or default disc sizes */
    if (discsize) {
        videosize = discsize - audiosize;
        vbitrate = videosize / time;
        tc_log_msg(EXE, "USER CDSIZE: %4d MB | V: %6.1f MB @ %.1f kbps",
                   (int)floor(discsize/(1024*1024)), videosize/(1024*1024),
                   vbitrate);
    } else {
        int i;
        for (i = 0; i < sizeof(defsize) / sizeof(*defsize); i++) {
            videosize = defsize[i] - audiosize;
            vbitrate = videosize / time;
            tc_log_msg(EXE, "USER CDSIZE: %4d MB | V: %6.1f MB @ %.1f kbps",
                       (int)floor(discsize/(1024*1024)), videosize/(1024*1024),
                       vbitrate);
        }
    }
}

/*************************************************************************/

/* XXX; add docs */
static int fileinfo_dir(const char *dname, int *fd, long *magic)
{
    TCDirList tcdir;
    const char *name=NULL;

    //check file type - file order not important
    if (tc_dirlist_open(&tcdir, dname, 0) < 0) {
	    tc_log_error(__FILE__, "unable to open dirlist \"%s\"", dname);
        return TC_IMPORT_ERROR;
    }

    if (verbose >= TC_DEBUG) {
        tc_log_msg(__FILE__, "scanning dirlist \"%s\"", dname);
    }

    name = tc_dirlist_scan(&tcdir);
    if (name == NULL) {
        return TC_IMPORT_ERROR;
    }

    *fd = open(name, O_RDONLY);
    if (*fd < 0) {
    	tc_log_perror(__FILE__, "open file");
	    return TC_IMPORT_ERROR;
    }

    tc_dirlist_close(&tcdir);

    //first valid magic must be the same for all
    //files to follow, but is not checked here

    *magic = fileinfo(*fd, 0);
    return TC_IMPORT_OK;
}


/*
 * info_setup:
 *
 *      perform second-step initialization on a info_t  structure.
 *      While first-step setup can be done statically with simple
 *      assignements, intialization performedon this stage is based
 *      on data given previosuly (i.e.: name).
 *      This function is a catch-all for all the black magic things.
 *      Of course there is still a lot of room for cleaning up and
 *      refactoring.
 *      My thought is that doing things in a _really_ clean way means
 *      rewriting almoost from the ground up the probing infrastructure.
 *
 * Parameters:
 *         ipipe: info_t structure to (finish to) initialize
 *          skip: amount of bytes to skip when analyzing a regular file.
 *                Ignored otherwise
 * mplayer_probe: if input it's a regular file, do the real probing through
 *                mplayer (if avalaible); ignored otherwise.
 *      want_dvd: if !0 and the source looks likea DVD, handle it like a DVD.
 *                I know this is a bit obscure and maybe even sick, it's
 *                legacy and should go away in the future.
 * Return Value:
 *      TC_IMPORT_OK -> succesfull,
 *      TC_IMPORT_ERROR -> otherwise.
 *      messages are sent to user using tc_log_*() in both cases.
 * Side effects:
 *      quite a lot =)
 *      Input source is open and read to guess the source type.
 *      This function can do (and usually does) several read attempts.
 * Preconditions:
 *      given info_t structure is already basically initialized (see
 *      first-step setup above). This measn set at least:
 *          ipipe.verbose, ipipe.fd_in, ipipe.name = name;
 * Postconditions:
 *      given info_t is ready to be used in tcprobe_thread()
 *
 */
static int info_setup(info_t *ipipe, int skip, int mplayer_probe, int want_dvd)
{
    int file_kind = tc_probe_path(ipipe->name);
    int ret;

    switch (file_kind) {
      case TC_PROBE_PATH_FILE:	/* regular file */
        if (mplayer_probe) {
            ipipe->magic = TC_MAGIC_MPLAYER;
        } else if (want_dvd && dvd_is_valid(ipipe->name)) {
            ipipe->magic = TC_MAGIC_DVD;
        } else {
            ipipe->fd_in = xio_open(ipipe->name, O_RDONLY);
            if (ipipe->fd_in < 0) {
                tc_log_perror(EXE, "file open");
                return TC_IMPORT_ERROR;
            }
            ipipe->magic = fileinfo(ipipe->fd_in, skip);
            ipipe->seek_allowed = 1;
        }
        break;
      case TC_PROBE_PATH_RELDIR:        /* relative path to directory */
        ret = fileinfo_dir(ipipe->name, &ipipe->fd_in, &ipipe->magic);
        if (ret < 0) {
            return TC_IMPORT_ERROR;
        }
        break;
      case TC_PROBE_PATH_ABSPATH:       /* absolute path */
        if (dvd_is_valid(ipipe->name)) {
            /* normal directory - no DVD copy */
            ret = fileinfo_dir(ipipe->name, &ipipe->fd_in, &ipipe->magic);
            if (ret < 0) {
                return TC_IMPORT_ERROR;
            }
        } else {
            ipipe->magic = TC_MAGIC_DVD;
        }
        break;
      /* now the easy stuff */
      case TC_PROBE_PATH_NET:		/* network host */
        ipipe->magic = TC_MAGIC_SOCKET;
        break;
      case TC_PROBE_PATH_BKTR:	/* bktr device */
        ipipe->magic = TC_MAGIC_BKTR_VIDEO;
        break;
      case TC_PROBE_PATH_SUNAU:	/* sunau device */
        ipipe->magic = TC_MAGIC_SUNAU_AUDIO;
        break;
      case TC_PROBE_PATH_OSS:	/* OSS device */
        ipipe->magic = TC_MAGIC_OSS_AUDIO;
        break;
      case TC_PROBE_PATH_V4L_VIDEO:	/* v4l video device */
        ipipe->magic = TC_MAGIC_V4L_VIDEO;
        break;
      case TC_PROBE_PATH_V4L_AUDIO:	/* v4l audio device */
        ipipe->magic = TC_MAGIC_V4L_AUDIO;
        break;
      case TC_PROBE_PATH_INVALID:	/* non-existent source */
      default:                      /* fallthrough */
        tc_log_error(EXE, "can't determine the file kind");
        return TC_IMPORT_ERROR;
    } /* probe_path */
    return TC_IMPORT_OK;
}

/*
 * info_teardown:
 *
 *      reverse initialization done in info_setup
 *
 * Parameters:
 *      ipipe: info_t structure to (finish to) initialize
 * Return Value:
 *      None
 */
static void info_teardown(info_t *ipipe)
{
    if (ipipe->fd_in != STDIN_FILENO) {
        xio_close(ipipe->fd_in);
    }
}

/*************************************************************************/

/* new fancy output handlers */

/*
 * generic info dump function handler
 */
typedef void (*InfoDumpFn)(info_t *ipipe);

/*
 * dump_info_binary:
 *
 *      dump a ProbeInfo structure in binary (and platform-dependent,
 *      and probably even not fully safe) way to stdout.
 *      This dump mode is used by tcprobe to communicate with transcode.
 *      Legacy, I'd like to change this communication mode in future
 *      releases.
 *
 * Parameters:
 *      ipipe: info_t structure holding the ProbeInfo data to dump.
 * Return Value:
 *      None
 */
static void dump_info_binary(info_t *ipipe)
{
    pid_t pid = getpid();
    tc_pwrite(STDOUT_FILENO, (uint8_t *) &pid, sizeof(pid_t));
    tc_pwrite(STDOUT_FILENO, (uint8_t *) &ipipe->probe_info,
              sizeof(ProbeInfo));
}


#define PROBED_NEW  "<*>"   /* value different from tc's defaults */
#define PROBED_STD  ""      /* value equals to tc's defaults */

/*
 * user mode:
 * recommended transcode command line options:
 */
#define MARK_EXPECTED(ex) ((ex) ?(PROBED_STD) :(PROBED_NEW))
#define CHECK_MARK_EXPECTED(probed, val) \
        (((val) == (probed)) ?(PROBED_STD) :(PROBED_NEW))

/*
 * dump_info_binary:
 *
 *      dump a ProbeInfo structure in a human-readable, slightly modifed
 *      from standard pre-1,1,0 format.
 *
 * Parameters:
 *      ipipe: info_t structure holding the ProbeInfo data to dump.
 * Return Value:
 *      None
 */
static void dump_info_user(info_t *ipipe)
{
    long frame_time = 0;
    int is_std = TC_TRUE; /* flag: select PROBED_??? above */
    int nsubs = 0, n = 0;
    char extrabuf[TC_BUF_MIN];
    int extrabuf_ready = TC_FALSE;

    tc_log_msg(EXE, "summary for %s, %s = not default, 0 = not detected",
               ((ipipe->magic == TC_STYPE_STDIN) ?"-" :ipipe->name),
               PROBED_NEW);

    if (ipipe->probe_info->width != PAL_W
     || ipipe->probe_info->height != PAL_H) {
        is_std = TC_FALSE;
    }

    tc_log_msg(EXE, "%18s %s", "stream type:", filetype(ipipe->magic));
    tc_log_msg(EXE, "%18s %s", "video format:",
               tc_codec_to_string(ipipe->probe_info->codec));

    /* video first. */
    if (ipipe->probe_info->width > 0 && ipipe->probe_info->height > 0) {
        int n, d;

        extrabuf_ready = TC_FALSE;

        tc_log_msg(EXE, "%18s %dx%d [%dx%d] (-g) %s",
                   "import frame size:",
                   ipipe->probe_info->width, ipipe->probe_info->height,
                   PAL_W, PAL_H, MARK_EXPECTED(is_std));

        tc_asr_code_to_ratio(ipipe->probe_info->asr, &n, &d);
        tc_log_msg(EXE, "%18s %i:%i asr=%i (--import_asr) %s",
                   "aspect ratio:",
                   n, d, ipipe->probe_info->asr,
                   CHECK_MARK_EXPECTED(ipipe->probe_info->asr, 1));


        frame_time = (ipipe->probe_info->fps != 0) ?
                     (long)(1. / ipipe->probe_info->fps * 1000) : 0;

        tc_log_msg(EXE, "%18s %.3f [%.3f] frc=%d (-f) %s",
                   "frame rate:",
                   ipipe->probe_info->fps, PAL_FPS, ipipe->probe_info->frc,
                   CHECK_MARK_EXPECTED(ipipe->probe_info->frc, 3));

        /* video track extra info */
        if (ipipe->probe_info->pts_start) {
            tc_snprintf(extrabuf, sizeof(extrabuf),
                        "%18s PTS=%.4f, frame_time=%ld ms",
                        "", /* empty string to have a nice justification */
                        ipipe->probe_info->pts_start, frame_time);
            extrabuf_ready = TC_TRUE;
        }
        if (ipipe->probe_info->bitrate) {
            size_t len = strlen(extrabuf);
            tc_snprintf(extrabuf + len, sizeof(extrabuf) - len,
                        ", bitrate=%li kbps",
                        ipipe->probe_info->bitrate);
            extrabuf_ready = TC_TRUE;
        }
        if (extrabuf_ready) {
            tc_log_msg(EXE, extrabuf);
        }
    }

    /* audio next. */
    for (n = 0; n < TC_MAX_AUD_TRACKS; n++) {
        int D_arg = 0, D_arg_ms = 0;
        double pts_diff = 0.;

        if (ipipe->probe_info->track[n].format != 0
         && ipipe->probe_info->track[n].chan > 0) {
            extrabuf_ready = TC_FALSE;

	        if (ipipe->probe_info->track[n].samplerate != RATE
             || ipipe->probe_info->track[n].chan != CHANNELS
             || ipipe->probe_info->track[n].bits != BITS
             || ipipe->probe_info->track[n].format != CODEC_AC3) {
                is_std = TC_FALSE;
            } else {
                is_std = TC_TRUE;
            }

            tc_log_msg(EXE, "%18s id=%i [0] (-a) format=0x%x [0x%x] (-n) %s",
                       "audio track:",
                       ipipe->probe_info->track[n].tid,
                       ipipe->probe_info->track[n].format,
                       CODEC_AC3,
                       MARK_EXPECTED(is_std));
            tc_log_msg(EXE, "%18s rate,bits,channels=%d,%d,%d "
                            "[%d,%d,%d] (-e)",
                       "", /* empty string to have a nice justification */
                       ipipe->probe_info->track[n].samplerate,
                       ipipe->probe_info->track[n].bits,
                       ipipe->probe_info->track[n].chan,
                       RATE, BITS, CHANNELS);

            /* audio track extra info */
            if (ipipe->probe_info->track[n].pts_start) {
                tc_snprintf(extrabuf, sizeof(extrabuf), "PTS=%.4f",
                            ipipe->probe_info->track[n].pts_start);
                extrabuf_ready = TC_TRUE;
            }
            if (ipipe->probe_info->track[n].bitrate) {
                size_t len = strlen(extrabuf);
                tc_snprintf(extrabuf + len, sizeof(extrabuf) - len,
                            "%sbitrate=%i kbps", (len > 0) ?", " :"",
                            ipipe->probe_info->track[n].bitrate);
                extrabuf_ready = TC_TRUE;
            }
            if (extrabuf_ready) {
                tc_log_msg(EXE, "%18s %s",
                                "", /* empty string for a nice justification */
                                extrabuf);
            }

            /* audio track A/V sync suggestion */
            if (ipipe->probe_info->pts_start > 0
             && ipipe->probe_info->track[n].pts_start > 0
             && ipipe->probe_info->fps != 0) {
                pts_diff = ipipe->probe_info->pts_start \
                           - ipipe->probe_info->track[n].pts_start;
                D_arg = (int)(pts_diff * ipipe->probe_info->fps);
                D_arg_ms = (int)((pts_diff - D_arg/ipipe->probe_info->fps)*1000);

                tc_log_msg(EXE, "%18s audio delay: %i frames/%i ms "
                                "(-D/--av_fine_ms) [0/0]",
                           "", /* empty string for nice justification */
                           D_arg, D_arg_ms);
            }
        }
        /* have subtitles here? */
        if (ipipe->probe_info->track[n].attribute & PACKAGE_SUBTITLE) {
            nsubs++;
        }
    }

    /* no audio */
    if (ipipe->probe_info->num_tracks == 0) {
        tc_log_msg(EXE, "%18s %s", "no audio track:",
                  "(use \"null\" import module for audio)");
    }

    if (nsubs > 0) {
        tc_log_msg(EXE, "detected (%d) subtitle(s)", nsubs);
    }

    /* P-units */
    if (ipipe->probe_info->unit_cnt) {
        tc_log_msg(EXE, "detected (%d) presentation unit(s) (SCR reset)",
                    ipipe->probe_info->unit_cnt+1);
    }

    /* DVD only: coder bitrate infos */
    if (ipipe->magic == TC_MAGIC_DVD_PAL || ipipe->magic == TC_MAGIC_DVD_NTSC
     || ipipe->magic == TC_MAGIC_DVD) {
        enc_bitrate((long)ceil(ipipe->probe_info->fps * ipipe->probe_info->time),
                     ipipe->probe_info->fps, bitrate*1000, 0);
    } else {
        if (ipipe->probe_info->frames > 0) {
            unsigned long dur_ms;
            unsigned int dur_h, dur_min, dur_s;
            if (ipipe->probe_info->fps < 0.100) {
                dur_ms = (long)ipipe->probe_info->frames*frame_time;
            } else {
                dur_ms = (long)((float)ipipe->probe_info->frames * 1000
                                /ipipe->probe_info->fps);
            }
            dur_h = dur_ms/3600000;
            dur_min = (dur_ms %= 3600000)/60000;
            dur_s = (dur_ms %= 60000)/1000;
            dur_ms %= 1000;
            tc_log_msg(EXE, "%18s %ld frames, frame_time=%ld msec,"
                             " duration=%u:%02u:%02u.%03lu",
                             "length:",
                             ipipe->probe_info->frames, frame_time,
                             dur_h, dur_min, dur_s, dur_ms);
        }
    }
}

/*
 * dump_info_raw:
 *
 *      dump a ProbeInfo structure in a human-readable but machine-friendly
 *      format, resembling, or identical where feasible, the mplayer -identify
 *      output.
 *      Print one field at line, in the format KEY=value.
 *
 * Parameters:
 *      ipipe: info_t structure holding the ProbeInfo data to dump.
 * Return Value:
 *      None
 */
static void dump_info_raw(info_t *ipipe)
{
    double duration = 0.0; /* seconds */

    printf("ID_FILENAME=\"%s\"\n", ipipe->name);
    printf("ID_FILETYPE=\"%s\"\n", filetype(ipipe->magic));

    printf("ID_VIDEO_WIDTH=%i\n", ipipe->probe_info->width);
    printf("ID_VIDEO_HEIGHT=%i\n", ipipe->probe_info->height);
    printf("ID_VIDEO_FPS=%.3f\n", ipipe->probe_info->fps);
    printf("ID_VIDEO_FRC=%i\n", ipipe->probe_info->frc);
    printf("ID_VIDEO_ASR=%i\n", ipipe->probe_info->asr);
    printf("ID_VIDEO_FORMAT=%s\n",
              tc_codec_to_string(ipipe->probe_info->codec));
    printf("ID_VIDEO_BITRATE=%li\n", ipipe->probe_info->bitrate);

    /* only the first audio track, now */
    printf("ID_AUDIO_CODEC=%s\n",
               tc_codec_to_string(ipipe->probe_info->track[0].format));
    printf("ID_AUDIO_FORMAT=%i\n", ipipe->probe_info->track[0].format);
    printf("ID_AUDIO_BITRATE=%i\n", ipipe->probe_info->track[0].bitrate);
    printf("ID_AUDIO_RATE=%i\n", ipipe->probe_info->track[0].samplerate);
    printf("ID_AUDIO_NCH=%i\n", ipipe->probe_info->track[0].chan);
    printf("ID_AUDIO_BITS=%i\n", ipipe->probe_info->track[0].bits);
    if (ipipe->probe_info->fps != 0.0) {
        /* seconds */
        duration = ((double)ipipe->probe_info->frames/ipipe->probe_info->fps);
    }
    printf("ID_LENGTH=%.2f\n", duration);
}

/*************************************************************************/

/* ------------------------------------------------------------
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/

void version(void)
{
    /* print id string to stderr */
    tc_log_info(EXE, "%s (%s v%s) (C) 2001-2006 "
                     "Thomas Oestreich, Transcode team",
                EXE, PACKAGE, VERSION);
}


static void usage(int status)
{
    version();

    tc_log_info(EXE, "Usage: %s [options] [-]", EXE);
    tc_log_msg(EXE, "    -i name        input file/directory/device/host"
                    " name [stdin]");
    tc_log_msg(EXE, "    -B             binary output to stdout"
                    " (used by transcode) [off]");
    tc_log_msg(EXE, "    -M             use EXPERIMENTAL"
                    " mplayer probe [off]");
    tc_log_msg(EXE, "    -R             raw mode: produce machine-friendly "
                    "output [off]");
    tc_log_msg(EXE, "    -H n           probe n MB of stream [1]");
    tc_log_msg(EXE, "    -s n           skip first n bytes of stream [0]");
    tc_log_msg(EXE, "    -T title       probe for DVD title [off]");
    tc_log_msg(EXE, "    -b bitrate     audio encoder bitrate kBits/s [%d]",
               ABITRATE);
    tc_log_msg(EXE, "    -f seekfile    seek/index file [off]");
    tc_log_msg(EXE, "    -d verbosity   verbosity mode [1]");
    tc_log_msg(EXE, "    -v             print version");

    exit(status);
}

/* ------------------------------------------------------------
 * universal probing code frontend
 * ------------------------------------------------------------*/

/* very basic option sanity check */
#define VALIDATE_OPTION \
    if (optarg[0]=='-') { \
        usage(EXIT_FAILURE); \
    }

#define VALIDATE_PARAM(parm, opt, min) \
    if ((parm) < (min)) { \
        tc_log_error(EXE, "invalid parameter for option %s", (opt)); \
        exit(16); \
    }


int main(int argc, char *argv[])
{
    info_t ipipe;
    InfoDumpFn output_handler =  dump_info_user;
    /* standard old style output */

    int mplayer_probe = TC_FALSE;
    int ch, skip = 0, want_dvd = 0, ret;
    const char *name = NULL;

    /* proper initialization */
    memset(&ipipe, 0, sizeof(info_t));
    ipipe.stype = TC_STYPE_UNKNOWN;
    ipipe.seek_allowed = 0;
    ipipe.factor = 1;

    while ((ch = getopt(argc, argv, "i:vBMRd:T:f:b:s:H:?h")) != -1) {
        switch (ch) {
          case 'b':
            VALIDATE_OPTION;
            bitrate = atoi(optarg);
            VALIDATE_PARAM(bitrate, "-b", 0);
            break;
          case 'i':
            VALIDATE_OPTION;
	        name = optarg;
            break;
          case 'f':
            VALIDATE_OPTION;
	        ipipe.nav_seek_file = optarg;
            break;
          case 'd':
            VALIDATE_OPTION;
	        verbose = atoi(optarg);
            break;
          case 's':
            VALIDATE_OPTION;
	        skip = atoi(optarg);
            break;
          case 'H':
            VALIDATE_OPTION;
	        ipipe.factor = atoi(optarg); /* how much data for probing? */
            VALIDATE_PARAM(bitrate, "-H", 0);
            break;
          case 'B':
            output_handler = dump_info_binary;
            binary_dump = 1; /* XXX: compatibility with  probe_mov -- FR */
            break;
          case 'M':
            mplayer_probe = TC_TRUE;
            break;
          case 'R':
            output_handler = dump_info_raw;
            break;
          case 'T':
            VALIDATE_OPTION;
	        ipipe.dvd_title = atoi(optarg);
            want_dvd = 1;
            break;
          case 'v':
            version();
            exit(0);
            break;
          case 'h':
            usage(EXIT_SUCCESS);
            break;
          default:
            usage(EXIT_FAILURE);
        }
    }

    /* need at least a file name */
    if (argc == 1) {
        usage(EXIT_FAILURE);
    }

    if (optind < argc) {
        if (strcmp(argv[optind],"-") != 0) {
            usage(EXIT_FAILURE);
        }
        ipipe.stype = TC_STYPE_STDIN;
    }

    /* assume defaults */
    if (name == NULL) {
        ipipe.stype = TC_STYPE_STDIN;
    }
    ipipe.verbose = verbose;
    ipipe.fd_out = STDOUT_FILENO;
    ipipe.codec = TC_CODEC_UNKNOWN;
    ipipe.name = name;

    /* do not try to mess with the stream */
    if (ipipe.stype == TC_STYPE_STDIN) {
        ipipe.fd_in = STDIN_FILENO;
        ipipe.magic = streaminfo(ipipe.fd_in);
    } else {
        ret = info_setup(&ipipe, skip, mplayer_probe, want_dvd);
        if (ret != TC_IMPORT_OK) {
            /* already logged out why */
            exit(1);
        }
    }

    /* ------------------------------------------------------------
     * codec specific section
     * note: user provided values overwrite autodetection!
     * ------------------------------------------------------------*/

    tcprobe_thread(&ipipe);

    if (ipipe.error == 0) {
        output_handler(&ipipe);
    } else if (ipipe.error == 1) {
        if (verbose) {
            tc_log_error(EXE, "failed to probe source");
        }
    } else if (ipipe.error == 2) {
        if (verbose) {
            tc_log_error(EXE, "filetype/codec not yet supported by '%s'",
                         PACKAGE);
        }
    }

    info_teardown(&ipipe);
    return ipipe.error;
}

#include "libtc/static_xio.h"
