/*
 * export_profile.c -- transcode export profile support code - implementation
 * Written by Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include <unistd.h>

#include "export_profile.h"
#include "libtc/libtc.h"
#include "libtc/cfgfile.h"
#include "libtc/tccodecs.h"

/* OK, that's quite ugly but I found nothing better, yet.*/
#ifdef TCEXPORT_PROFILE
/* same value for both macros */
# define TC_EXPORT_PROFILE_OPT     "-P"
#else
# define TC_EXPORT_PROFILE_OPT     "--export_prof"
#endif

#define USER_PROF_PATH   ".transcode/profile"

/* all needed support variables/data packed in a nice structure */
typedef struct tcexportprofile_ TCExportProfile;

struct tcexportprofile_ {
    size_t profile_count;
    char  **profiles;

    TCExportInfo info;

    /* auxiliary variables */
    const char *video_codec;
    const char *audio_codec;
};

static TCExportProfile prof_data = {
    .profile_count = 0,
    .profiles = NULL,

    .video_codec = NULL,
    .audio_codec = NULL,

    /*
     * we need to take care of strings deallocating
     * them between module_read_config() calls, to
     * avoid memleaks, so we use NULL marking here.
     */
    .info.video.string = NULL,
    .info.video.module = NULL,
    .info.video.module_opts = NULL,
    .info.video.log_file = NULL,

    .info.audio.string = NULL,
    .info.audio.module = NULL,
    .info.audio.module_opts = NULL,

    .info.mplex.string = NULL,
    .info.mplex.module = NULL,
    .info.mplex.module_opts = NULL,
    .info.mplex.out_file = NULL,
    .info.mplex.out_file_aux = NULL,

    /* standard initialization */
    .info.video.width = PAL_W,
    .info.video.height = PAL_H,
    .info.video.asr = -1, // XXX
    .info.video.frc = 3, // XXX (magic number)
    .info.video.par = 0,
    .info.video.encode_fields = 0,
    .info.video.gop_size = VKEYFRAMES,
    .info.video.quantizer_min = VMINQUANTIZER,
    .info.video.quantizer_max = VMAXQUANTIZER,
    .info.video.format = CODEC_NULL,
    .info.video.quality = -1,
    .info.video.bitrate = VBITRATE,
    .info.video.bitrate_max = VBITRATE,
    .info.video.flush_flag = TC_FALSE,
    .info.video.pass_number = VMULTIPASS,

    .info.audio.format = CODEC_NULL,
    .info.audio.quality = -1,
    .info.audio.bitrate = ABITRATE,
    .info.audio.sample_rate = RATE,
    .info.audio.sample_bits = BITS,
    .info.audio.channels = CHANNELS,
    .info.audio.mode = AMODE,
    .info.audio.vbr_flag = TC_FALSE,
    .info.audio.flush_flag = TC_FALSE,
    .info.audio.bit_reservoir = TC_TRUE,
};


/* private helpers: declaration *******************************************/

/*
 * tc_load_single_export_profile:
 *      tc_load_export_profile backend.
 *      Find and load, by looking into user profile path then into system
 *      profiles path, the i-th selected profile.
 *      If profile can't be loaded, it will just skipped.
 *      If verbose >= TC_DEBUG and the profile wasn't loaded, notify
 *      the user using tc_log*.
 *      If verbose >= TC_INFO and profile was loaded, notify the user
 *      using tc_log*.
 * 
 * Parameters:
 *          i: load the i-th already parsed (see below) export profile.
 *     config: use this TCConfigEntry array, provided by frontend, to
 *             parse profile data file
 *   sys_path: system path to look for profile data
 *  user_path: user path to look for profile data
 * Return value:
 *      1: profile data succesfully loaded
 *      0: profile data not loaded for some reasons (profile file
 *         not found or not readable).
 * Side effects:
 *      Isn't a proper side effect, anyway it's worth to note that
 *      this function _will_ alter some data not explicitely provided,
 *      via config parameter. Note that this function WILL NOT alter
 *      config data, so caller providing config data will have full
 *      control of those unproper side effects by careful craft of
 *      TCConfigEntry data.
 *      Also note that this function mangle global private prof_data
 *      variable, most notably by invoking cleanup_strings on it
 *      to avoid memleaks in subsequent calls of module_read_config.
 * Preconditions:
 *      this function should be always used _after_ a succesfull
 *      call to tc_setup_export_profiles, which parse the profiles
 *      selected by user. Otherwise it's still safe to call this function,
 *      but it always fail.
 */
static int tc_load_single_export_profile(int i, TCConfigEntry *config,
                                         const char *sys_path,
                                         const char *user_path);


/* utilities used internally (yet) */

/*
 * cleanup_strings:
 *      free()s and reset to NULL every not-NULL string in a given 
 *      TCExportInfo structure
 *
 * Parameters:
 *      info: pointero to a TCExportInfo structure to cleanup.
 * Return value:
 *      None.
 */
static void cleanup_strings(TCExportInfo *info);

/*
 * tc_mangle_cmdline:
 *      parse a command line option array looking for a given option.
 *      Given option can be short or long but must be given literally.
 *      So, if you want to mangle "--foobar", give "--foobar" not
 *      "foobar". Same story for short options "-V": use "-V" not "V".
 *      If given option isn't found in string option array, do nothing
 *      and return succesfull (see below). If option is found but 
 *      it's argument isn't found, don't mangle string options array
 *      but return failure.
 *      If BOTH option and it's value its found, store a pointer to
 *      option value into "optval" parameter and remove both option
 *      and value from string options array.
 * Parameters:
 *      argc: pointer to number of values present into option string
 *            array. This parameter must be !NULL and it's updated
 *            by a succesfull call of this function.
 *      argv: pointer to array of option string items. This parameter
 *            must be !NULL and it's updated by a succesfull call of
 *            this function
 *       opt: option to look for.
 *    optval: if succesfull, pointer to option value will be stored here.
 * Return value:
 *      1: bad parameter(s) (NULL)
 *      0: succesfull
 *     -1: bad usage: given the option but not it's value
 * Postconditions:
 *      this function must operate trasparently by always leaving
 *      argc/argv in an usable and consistent state.
 */
static int tc_mangle_cmdline(int *argc, char ***argv,
                             const char *opt, const char **optval);

/*
 * tc_strsplit:
 *      split a given string into tokens using given separator character.
 *      Return NULL-terminated array of splitted tokens, and optionally
 *      return (via a out parameter) size of returned array.
 *
 * Parameters:
 *         str: string to split
 *         sep: separator CHARACTER: cut string when sep is found
 *  pieces_num: if not NULL, store here the size of returned array
 * Return value:
 *      NULL-terminated array of splitted pieces.
 *      You must explicitely free this returned array by using tc_strfreev
 *      (see below) in order to avoid memleaks.
 */
static char **tc_strsplit(const char *str, char sep,
                          size_t *pieces_num);

/*
 * tc_strfreev:
 *      return an array of strings as returned by tc_strsplit
 *
 * Parameters:
 *      pieces: return value of tc_strsplit to be freed.
 * Return value:
 *      None.
 */
static void tc_strfreev(char **pieces);

/*************************************************************************/

int tc_setup_export_profile(int *argc, char ***argv)
{
    const char *optval = NULL;
    int ret;

    if (argc == NULL || argv == NULL) {
        tc_log_warn(__FILE__, "tc_export_profile_init: bad data reference");
        return -2;
    }

    ret = tc_mangle_cmdline(argc, argv, TC_EXPORT_PROFILE_OPT,
                            &optval);
    if (ret == 0) { /* success */
        prof_data.profiles = tc_strsplit(optval, ',',
                                         &prof_data.profile_count);
        ret = (int)prof_data.profile_count;
        if (verbose >= TC_INFO) {
            tc_log_info(__FILE__, "recognized %i profiles", ret);
        }
    }
    return ret;
}

void tc_cleanup_export_profile(void)
{
    tc_strfreev(prof_data.profiles);
    prof_data.profile_count = 0;

    cleanup_strings(&prof_data.info);
}

const TCExportInfo *tc_load_export_profile(void)
{
    char home_path[PATH_MAX + 1];
    const char *home = NULL;
    int i = 0;
    /* not all setting swill be accessible from here */
    TCConfigEntry profile_conf[] = {
        /* video stuff */
        { "video_codec", &(prof_data.video_codec),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "video_module", &(prof_data.info.video.module),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "video_module_options", &(prof_data.info.video.module_opts),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "video_fourcc", &(prof_data.info.video.string),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "video_bitrate", &(prof_data.info.video.bitrate),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 12000000 },
        { "video_bitrate_max", &(prof_data.info.video.bitrate_max),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 12000000 },
        { "video_keyframes", &(prof_data.info.video.gop_size),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 2000 },
        /* audio stuff */
        { "audio_codec", &(prof_data.audio_codec),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "audio_module", &(prof_data.info.audio.module),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "audio_module_options", &(prof_data.info.audio.module_opts),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "audio_bitrate", &(prof_data.info.audio.bitrate),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 1000000 },
        { "audio_frequency", &(prof_data.info.audio.sample_rate),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 48000 }, // XXX: review min
        { "audio_bits", &(prof_data.info.audio.sample_bits),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 8, 16 }, // XXX
        { "audio_channels", &(prof_data.info.audio.channels),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 1, 2 },
        /* generic stuff */
        { "width", &(prof_data.info.video.width),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE,
                        1, TC_MAX_V_FRAME_WIDTH },
        { "height", &(prof_data.info.video.height),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE,
                        1, TC_MAX_V_FRAME_HEIGHT },
        { "mplex_module", &(prof_data.info.mplex.module),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "mplex_module_options", &(prof_data.info.mplex.module_opts),
                        TCCONF_TYPE_STRING, 0, 0, 0 },
        { "encode_fields", &(prof_data.info.video.encode_fields),
                        TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 5 }, // XXX
        { "frame_rate_code", &(prof_data.info.video.frc),
                            TCCONF_TYPE_INT, TCCONF_FLAG_RANGE, 0, 5 },
        { NULL, NULL, 0, 0, 0, 0 }
    };
    home = getenv("HOME");

    if (home != NULL) {
        tc_snprintf(home_path, sizeof(home_path), "%s/%s",
                    home, USER_PROF_PATH);
    } else {
        tc_log_warn(__FILE__, "can't determinate home directory!");
        return NULL;
    }

    for (i = 0; i < prof_data.profile_count; i++) {
        tc_load_single_export_profile(i, profile_conf,
                                      PROF_PATH, home_path);
    }
    return &prof_data.info;
}

/* it's pretty naif yet. FR */
void tc_export_profile_to_vob(const TCExportInfo *info, vob_t *vob)
{
    if (info == NULL || vob == NULL) {
        return;
    }
    vob->ex_v_string = info->video.module_opts;
    vob->ex_a_string = info->audio.module_opts;
    vob->ex_m_string = info->mplex.module_opts;
    vob->ex_v_codec = info->video.format;
    vob->ex_a_codec = info->audio.format;
    vob->ex_v_fcc = info->video.string;
    vob->ex_frc = info->video.frc;
    vob->encode_fields = info->video.encode_fields;
    vob->divxbitrate = info->video.bitrate;
    vob->mp3bitrate = info->audio.bitrate;
    vob->video_max_bitrate = info->video.bitrate_max;
    vob->divxkeyframes = info->video.gop_size;
    vob->mp3frequency = info->audio.sample_rate;
    vob->dm_bits = info->audio.sample_bits;
    vob->dm_chan = info->audio.channels;
    vob->zoom_width = info->video.width;
    vob->zoom_height = info->video.height;
}

/*************************************************************************
 * private helpers: implementation
 **************************************************************************/

static int tc_load_single_export_profile(int i, TCConfigEntry *config,
                                         const char *sys_path,
                                         const char *user_path)
{
    int found = 0, ret = 0;
    char path_buf[PATH_MAX+1];
    const char *basedir = NULL;

    if (sys_path == NULL || user_path == NULL || config == NULL
     || ((i < 0) || i >= prof_data.profile_count)) {
        /* paranoia */
        tc_log_warn(__FILE__, "tc_export_profile_load():"
                              " bad data reference");
        return -1;
    }

    tc_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                user_path, prof_data.profiles[i]);
    ret = access(path_buf, R_OK);
    if (ret == 0) {
        found = 1;
        basedir = user_path;
    } else {
        tc_snprintf(path_buf, sizeof(path_buf), "%s/%s",
                    sys_path, prof_data.profiles[i]);
        ret = access(path_buf, R_OK);
        if (ret == 0) {
            found = 1;
            basedir = PROF_PATH;
        }
    }

    if (found) {
        cleanup_strings(&prof_data.info);

        tc_set_config_dir(basedir);
        ret = module_read_config(prof_data.profiles[i], NULL,
                                 config, __FILE__);
        if (ret == 0) {
            found = 0; /* module_read_config() failed */
        } else {
            if (verbose >= TC_INFO) {
                tc_log_info(__FILE__, "loaded profile \"%s\"...", path_buf);
            }
            if (prof_data.video_codec != NULL) {
                prof_data.info.video.format = tc_codec_from_string(prof_data.video_codec);
                tc_free((char*)prof_data.video_codec); /* avoid const warning */
                prof_data.video_codec = NULL;
            }
            if (prof_data.audio_codec != NULL) {
                prof_data.info.audio.format = tc_codec_from_string(prof_data.audio_codec);
                tc_free((char*)prof_data.audio_codec); /* avoid const warning */
                prof_data.audio_codec = NULL;
            }
        }
    } else {
        if (verbose >= TC_DEBUG) {
            tc_log_warn(__FILE__, "unable to load profile \"%s\", skipped",
                        prof_data.profiles[i]);
        }
    }
    return found;
}

/*
 * module_read_config (used internally, see later)
 * allocates new strings for option values, so
 * we need to take care of them using this couple
 * of functions
 */

#define CLEANUP_STRING(field) do { \
    if (info->field != NULL) \
        tc_free(info->field); \
        info->field = NULL; \
} while (0)

static void cleanup_strings(TCExportInfo *info)
{
    if (info != NULL) {
        /* paranoia */

        CLEANUP_STRING(video.string);
        CLEANUP_STRING(video.module);
        CLEANUP_STRING(video.module_opts);
        CLEANUP_STRING(video.log_file);

        CLEANUP_STRING(audio.string);
        CLEANUP_STRING(audio.module);
        CLEANUP_STRING(audio.module_opts);

        CLEANUP_STRING(mplex.string);
        CLEANUP_STRING(mplex.module);
        CLEANUP_STRING(mplex.module_opts);
        CLEANUP_STRING(mplex.out_file);
        CLEANUP_STRING(mplex.out_file_aux);
    }
}

#undef CLEANUP_STRING

/*************************************************************************/

static int tc_mangle_cmdline(int *argc, char ***argv,
                             const char *opt, const char **optval)
{
    int i, found = TC_FALSE;

    if (argc == NULL || argv == NULL || opt == NULL || optval == NULL) {
        return 1;
    }

    /* first we looking for our option (and it's value) */
    for (i = 1; i < *argc; i++) {
        if ((*argv)[i] && strcmp((*argv)[i], opt) == 0) {
            if (i + 1 >= *argc || (*argv)[i + 1][0] == '-') {
                /* report bad usage */
                tc_log_warn(__FILE__, "you must supply a profile name");
                return -1;
            } else {
                found = TC_TRUE;
                *optval = (*argv)[i + 1];
            }
            break;
        }
    }

    /*
     * if we've found our option, now we must shift back all
     * the other options after the ours and we must also update argc.
     */
    if (found) {
        for (; i < (*argc - 2); i++) {
            (*argv)[i] = (*argv)[i + 2];
        }
        (*argc) -= 2;
    }

    return 0;
}

static char **tc_strsplit(const char *str, char sep,
                          size_t *pieces_num)
{
    const char *begin = str, *end = NULL;
    char **pieces = NULL, *pc = NULL;
    size_t i = 0, n = 2;
    int failed = TC_FALSE;

    if (!str || !strlen(str)) {
        return NULL;
    }

    while (begin != NULL) {
        begin = strchr(begin, sep);
        if (begin != NULL) {
            begin++;
            n++;
        }
    }

    pieces = tc_malloc(n * sizeof(char*));
    if (!pieces) {
        return NULL;
    }

    begin = str;
    while (begin != NULL) {
        size_t len;

        end = strchr(begin, sep);
        if (end != NULL) {
            len = (end - begin);
        } else {
            len = strlen(begin);
        }
        if (len > 0) {
            pc = tc_strndup(begin, len);
            if (pc == NULL) {
                failed = TC_TRUE;
                break;
            } else {
                pieces[i] = pc;
                i++;
            }
        }
        if (end != NULL) {
            begin = end + 1;
        } else {
            break;
        }
    }

    if (failed) {
        /* one or more copy of pieces failed */
        tc_free(pieces);
        pieces = NULL;
    } else { /* i == n - 1 -> all pieces copied */
        pieces[n - 1] = NULL; /* end marker */
        if (pieces_num != NULL) {
            *pieces_num = i;
        }
    }
    return pieces;
}

static void tc_strfreev(char **pieces)
{
    if (pieces != NULL) {
        int i = 0;
        for (i = 0; pieces[i] != NULL; i++) {
            tc_free(pieces[i]);
        }
        tc_free(pieces);
    }
}

