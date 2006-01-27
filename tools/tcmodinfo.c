/*
 *  tcmodinfo.c
 *
 *  Copyright (C) Tilmann Bitterberg - August 2002
 *  updated and partially rewritten by
 *  Copyright (C) Francesco Romani - January 2006
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#include "framebuffer.h"
#include "transcode.h"
#include "filter.h"
#include "video_trans.h"

#include "tcmodule-core.h"

#define EXE "tcmodinfo"
#define OPTS_SIZE 8192 //Buffersize
#define NAME_LEN 256

#define STATUS_OK            0
#define STATUS_BAD_PARAM     1
#define STATUS_NO_MODULE     2
#define STATUS_MODULE_ERROR  4
#define STATUS_NO_SOCKET     8
#define STATUS_SOCKET_ERROR  16

static filter_t filter[MAX_FILTER];
static vob_t vob = {
    // some arbitrary values for the modules
    .fps = 25.0,
    .im_v_width = 32,
    .ex_v_width = 32,
    .im_v_height= 32,
    .ex_v_height= 32,
    .im_v_codec = CODEC_YUV,

    .a_rate = 44100,
    .mp3frequency = 44100,
    .a_chan = 2,
    .a_bits = 16,

    .video_in_file = "/dev/zero",
    .audio_in_file = "/dev/zero",
};

void version(void)
{
    printf("%s (%s v%s) (C) 2001-2006 Tilmann Bitterberg, "
           "transcode team\n", EXE, PACKAGE, VERSION);
}

static void usage(void)
{
    version();
    tc_log_info(EXE, "Usage: %s [options]", EXE);
    fprintf(stderr, "\t -i name           Module name information (like \'smooth\')\n");
    fprintf(stderr, "\t -p                Print the compiled-in module path\n");
    fprintf(stderr, "\t -d verbosity      Verbosity mode [1 == TC_INFO]\n");
    fprintf(stderr, "\t -D                dry run, only loads module (used for testing)\n");
    fprintf(stderr, "\t -m path           Use PATH as module path\n");
    fprintf(stderr, "\t -M element        Request to module informations about <element>\n");
    fprintf(stderr, "\t -s socket         Connect to transcode socket\n");
    fprintf(stderr, "\t -t type           Type of module (filter, encode, multiplex)\n");
    fprintf(stderr, "\n");
}

// dependencies
// Yeah, this sucks
vob_t *tc_get_vob()
{
    return &vob;
}

int plugin_get_handle(char *name)
{
    return 0;
}

/* symbols nbeeded by modules */
pthread_mutex_t init_avcodec_lock = PTHREAD_MUTEX_INITIALIZER;
int probe_export_attributes = 0;
const char *tc_config_dir = NULL;
int verbose  = TC_INFO;
int rgbswap  = 0;
int tc_accel = -1;    //acceleration code
int flip = 0;
int max_frame_buffer=0;
int tc_x_preview = 32;
int tc_y_preview = 32;
int gamma_table_flag = 0;
int tc_socket_msgchar;
int tc_socket_msg_lock;

void tc_socket_config(void);
void tc_socket_disable(void);
void tc_socket_enable(void);
void tc_socket_list(void);
void tc_socket_load(void);
void tc_socket_parameter(void);
void tc_socket_preview(void);
void tc_socket_config(void) {}
void tc_socket_disable(void) {}
void tc_socket_enable(void) {}
void tc_socket_list(void) {}
void tc_socket_load(void) {}
void tc_socket_parameter(void) {}
void tc_socket_preview(void) {}

int module_read_config(char *section, char *prefix, char *module,
                       void *conf, char *configdir) { return 0; }
int module_print_config(char *prefix, void *conf) { return 0; }

static int load_plugin(const char *path, int id, int verbose)
{
    const char *error;
    char module[TC_BUF_MAX];
    int n;

    if (!filter[id].name) {
        return -1;
    }

    filter[id].options=NULL;

    /* replace "=" by "/0" in filter name */
    for (n = 0; n < strlen(filter[id].name); ++n) {
        if (filter[id].name[n] == '=') {
            filter[id].name[n] = '\0';
            filter[id].options = filter[id].name + n + 1;
            break;
        }
    }

    tc_snprintf(module, sizeof(module), "%s/filter_%s.so", path, filter[id].name);

    /* try transcode's module directory */
    filter[id].handle = dlopen(module, RTLD_LAZY);

    if (!filter[id].handle) {
        if (verbose) {
            tc_log_error(EXE, "loading filter module '%s' failed (reason: %s)",
                         module, dlerror());
        }
        return -1;
    } else {
        filter[id].entry = dlsym(filter[id].handle, "tc_filter");
    }

    if ((error = dlerror()) != NULL)  {
        if (verbose) {
            tc_log_error(EXE, "error while loading '%s': %s\n",
                         module, error);
        }
        return -1;
    }
    return 0;
}

static int do_connect_socket(const char *socketfile)
{
#ifdef NET_STREAM
    int sock, retval;
    struct sockaddr_un server;
    char buf[OPTS_SIZE];
    fd_set rfds;
    struct timeval tv;
    ssize_t n;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        return STATUS_NO_SOCKET;
    }
    server.sun_family = AF_UNIX;
    strlcpy(server.sun_path, socketfile, sizeof(server.sun_path));

    if (connect(sock, (struct sockaddr *) &server,
                sizeof(struct sockaddr_un)) < 0) {
        close(sock);
        perror("connecting stream socket");
        return STATUS_NO_SOCKET;
    }

    while (1) {
    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds); // stdin
    FD_SET(sock, &rfds);
    /* Wait up to five seconds. */
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    retval = select(sock+1, &rfds, NULL, NULL, NULL);
    /* Don't rely on the value of tv now! */

    memset(buf, 0, sizeof (buf));

    if (retval>0) {
        if (FD_ISSET(0, &rfds)) {
        fgets(buf, OPTS_SIZE, stdin);
        }
        if (FD_ISSET(sock, &rfds)) {
        if ( (n = read(sock, buf, OPTS_SIZE)) < 0) {
            perror("reading on stream socket");
            break;
        } else if (n == 0) { // EOF
            fprintf (stderr, "Server closed connection\n");
            break;
        }
        printf("%s", buf);
        continue;
        }
    }

    if (write(sock, buf, sizeof(buf)) < 0)
        perror("writing on stream socket");

    memset(buf, 0, sizeof (buf));

    if (read(sock, buf, OPTS_SIZE) < 0)
        perror("reading on stream socket");

    printf("%s", buf);

    if (!isatty(0))
        break;
    }

    close(sock);
    return STATUS_OK;
#else
    tc_log_error(EXE, "No support for Netstreams compiled in");
    return STATUS_NO_SOCKET;
#endif
}

int main(int argc, char *argv[])
{
    int ch;
    const char *filename = NULL;
    const char *modpath = MOD_PATH;
    const char *modtype = "filter";
    const char *modarg = NULL;
    const char *socketfile = NULL;
    char options[OPTS_SIZE] = { '\0', };
    int print_mod = 0;
    int connect_socket = 0;
    int ret = 0;
    int status = STATUS_NO_MODULE;

    /* needed by filter modules */
    TCVHandle tcv_handle = tcv_init();
    TCFactory factory = NULL;
    TCModule module = NULL;

    vframe_list_t ptr;

    memset(&ptr, 0, sizeof(ptr));

    ac_init(AC_ALL);

    if (argc == 1) {
        usage();
        return STATUS_BAD_PARAM;
    }

    while(1) {
        ch = getopt(argc, argv, "d:i:?vhpm:M:s:t:");
        if (ch == -1) {
            break;
        }

        switch (ch) {
          case 'd':
            if (optarg[0] == '-') {
                usage();
                return STATUS_BAD_PARAM;
            }
            verbose = atoi(optarg);
            break;
          case 'i':
            if (optarg[0] == '-') {
                usage();
                return STATUS_BAD_PARAM;
            }
            filename = optarg;
            break;
          case 'm':
            modpath = optarg;
            break;
          case 'M':
            modarg = optarg;
            break;
          case 't':
            if (!optarg) {
                usage();
                return STATUS_BAD_PARAM;
            }

            if (!strcmp(optarg, "filter")
             || !strcmp(optarg, "encode")
             || !strcmp(optarg, "multiplex")) {
                modtype = optarg;
            } else {
                modtype = NULL;
            }
            break;
          case 's':
            if (optarg[0] == '-') {
                usage();
                return STATUS_BAD_PARAM;
            }
            connect_socket = 1;
            socketfile = optarg;
            break;
          case 'p':
            print_mod = 1;
            break;
          case 'v':
            version();
            return STATUS_OK;
          case '?': /* fallthrough */
          case 'h': /* fallthrough */
          default:
            usage();
            return STATUS_OK;
        }
    }

    if (print_mod) {
        printf("%s\n", modpath);
        return STATUS_OK;
    }

    if (connect_socket) {
        do_connect_socket(socketfile);
        return STATUS_OK;
    }

    if (!modtype || !strcmp(modtype, "import")) {
        tc_log_error(EXE, "Unknown module type (not in filter, encode, multiplex)");
        return STATUS_BAD_PARAM;
    }

    if (!filename) {
        usage();
        return STATUS_BAD_PARAM;
    }

    /* 
     * we can't distinguish from OMS and NMS modules at glance, so try
     * first using new module system
     */
    factory = tc_new_module_factory(modpath, verbose);
    module = tc_new_module(factory, modtype, filename);

    if (module != NULL) {
        if (verbose >= TC_DEBUG) {
            tc_log_info(EXE, "using new module system");
        }
        if (verbose >= TC_INFO) {
            /* overview and options */
            puts(tc_module_inspect(module, "help"));
            /* module capabilities */
            tc_module_show_info(module, verbose);
            /* current configuration */
            puts("\ndefault module configuration:");
            puts(tc_module_inspect(module, "all"));
        }
        if (modarg != NULL) {
            tc_log_info(EXE, "informations about '%s' for "
                             "module:", modarg);
            puts(tc_module_inspect(module, modarg));
        }
        tc_del_module(factory, module);
        status = STATUS_OK;
    } else if (!strcmp(modtype, "filter")) {
        char namebuf[NAME_LEN];
        /* compatibility support only for filters */
        if (verbose >= TC_DEBUG) {
            tc_log_info(EXE, "using old module system");
        }
        /* ok, fallback to old module system */
        filter[0].name = namebuf;
        tc_snprintf(filter[0].name, NAME_LEN, "%s", filename);
    
        ret = load_plugin(modpath, 0, verbose);
        if (ret == 0) {
            strlcpy(options, "help", OPTS_SIZE);
            ptr.tag = TC_FILTER_INIT;
            if ((ret = filter[0].entry(&ptr, options)) != 0) {
                status = STATUS_MODULE_ERROR;
            } else {
                memset(options, 0, OPTS_SIZE);
                ptr.tag = TC_FILTER_GET_CONFIG;
                ret = filter[0].entry(&ptr, options);

                if (ret == 0) {
                    if (verbose >= TC_INFO) {
                        fputs(options, stdout);
                    }
                    status = STATUS_OK;
                }
            }
        }
   }

   ret = tc_del_module_factory(factory);
   tcv_free(tcv_handle);
   return status;
}

#include "libtc/static_optstr.h"
#include "avilib/static_avilib.h"

/* vim: sw=4
 */
