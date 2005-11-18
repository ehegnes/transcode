/*
 * tcmoduleinfo.h - module data (capabilities) and helper functions
 * Written by Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef TCMODULEINFO_H
#define TCMODULEINFO_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#define TC_MODULE_NONE          0x00000000
#define TC_MODULE_ENCODE        0x00000001
#define TC_MODULE_MULTIPLEX     0x00000002

#define TC_MODULE_NOTHING       0x00000000
#define TC_MODULE_VIDEO         0x00010000
#define TC_MODULE_AUDIO         0x00020000
#define TC_MODULE_EXTRA         0x00040000

/*
 * this structure will hold all the interesting informations
 * both for user and for transcode itself of a given module.
 */
typedef struct tcmoduleinfo_ TCModuleInfo;
struct tcmoduleinfo_ {
        uint32_t flags;  /* module type (encode, multiplex) 
                                 + kind (audio, video...) */
        int api_version; /* pretty useless now */

        size_t data_size; /* size of private data */

        const char *name;
        const char *version;
	const char *description;

        /* 
         * the following two MUST point to an array of TC_CODEC_*
         * terminated by a TC_CODEC_ERROR value
         */
        const int *codecs_in;
	const int *codecs_out;
};

/*
 * tc_module_info_match: scan the given informations about two modules,
 *                       and tell if the two modules can be chained
 *                       toghether.
 *
 * Parameters: head: the first given module information structure;
 *                   'head' output is supposed to fit in 'tail' input.
 *             tail: the second  given module information structure;
 *                   'tail' input is supposed to be given by 'head' output.
 * Return value: 1 if 'head' can feed 'tail' safely,
 *               0 otherwise
 * Side effects: none
 * Preconditions: none
 * Postconditions: none
 */
int tc_module_info_match(const TCModuleInfo *head,
                         const TCModuleInfo *tail);

/*
 * tc_module_info_log: pretty-print the content of a given module 
 *                     information structure using functions of tc_log_*()
 *                     family.
 *
 * Parameters: info: module information structure to dump
 *             verbose: level of detail of description, ranging
 *                      to TC_QUIET to TC_STATS. Other values will
 *                      be ignored
 * Return value: none
 * Side effects: some informations are printed using tc_log_*()
 * Preconditions: 'verbose' must be in range TC_QUIET ... TC_STATS
 * Postconditions: none
 */
void tc_module_info_log(const TCModuleInfo *info, int verbose);

/*************************************************************************/

#endif  /* TCMODULEINFO_H */

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
