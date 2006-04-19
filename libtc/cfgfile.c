/*
 * cfgfile.c -- routines to handle external configuration files
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#define _ISOC99_SOURCE  /* needed by glibc to declare strtof() */

#include "transcode.h"
#include "libtc.h"
#include "cfgfile.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *config_dir = NULL;

static void parse_line(char *buf, TCConfigEntry *conf, const char *tag,
                       const char *filename, int line);

/*************************************************************************/

/**
 * tc_set_config_dir:  Sets the directory in which configuration files are
 * searched for.
 *
 * Parameters:
 *     dir: Directory to search for configuration files in.  If NULL, the
 *          current directory is used.
 * Return value:
 *     None.
 */

void tc_set_config_dir(const char *dir)
{
    free(config_dir);
    config_dir = dir ? tc_strdup(dir) : NULL;
}

/*************************************************************************/

/**
 * module_read_config:  Reads in configuration information from an external
 * file.
 *
 * Parameters:
 *     filename: Name of the configuration file to read.
 *      section: Section to read within the file, or NULL to read the
 *               entire file regardless of sections.
 *         conf: Array of configuration entries.
 *          tag: Tag to use in log messages.
 * Return value:
 *     Nonzero on success, zero on failure.
 */

int module_read_config(const char *filename, const char *section,
                       TCConfigEntry *conf, const char *tag)
{
    char buf[TC_BUF_MAX], path_buf[PATH_MAX+1];
    FILE *f;
    int line;

    /* Sanity checks */
    if (!tag)
        tag = __FILE__;
    if (!filename || !conf) {
        tc_log_error(tag, "module_read_config(): %s == NULL!",
                     !filename ? "filename" : !conf ? "conf" : "???");
        return 0;
    }

    /* Open the file */
    snprintf(path_buf, sizeof(path_buf), "%s/%s",
             config_dir ? config_dir : ".", filename);
    f = fopen(path_buf, "r");
    if (!f) {
        if (errno == EEXIST) {
            tc_log_warn(tag, "Configuration file %s does not exist!",
                        path_buf);
        } else if (errno == EACCES) {
            tc_log_warn(tag, "Configuration file %s cannot be read!",
                        path_buf);
        } else {
            tc_log_warn(tag, "Error opening configuration file %s: %s",
                        path_buf, strerror(errno));
        }
        return 0;
    }
    line = 0;

    if (section) {
        /* Look for the requested section */
        char expect[TC_BUF_MAX];
        tc_snprintf(expect, sizeof(expect), "[%s]", section);
        do {
            char *s;
            if (!fgets(buf, sizeof(buf), f)) {
                tc_log_warn(tag, "Section [%s] not found in configuration"
                            " file %s!", section, path_buf);
                fclose(f);
                return 0;
            }
            line++;
            s = strchr(buf, '#');
            if (s)
                *s = 0;
            s = buf;
            while (isspace(*s))
                s++;
            if (s > buf)
                memmove(buf, s, strlen(s)+1);
            s = buf + strlen(buf);
            while (s > buf && isspace(s[-1]))
                s--;
            *s = 0;
        } while (strcmp(buf, expect) != 0);
    }

    /* Read in the configuration values (up to the end of the section, if
     * a section name was given) */
    while (fgets(buf, sizeof(buf), f)) {
        char *s;

        line++;

        /* Ignore comments */
        s = strchr(buf, '#');
        if (s)
            *s = 0;

        /* Remove leading and trailing space */
        s = buf;
        while (isspace(*s))
            s++;
        if (s > buf)
            memmove(buf, s, strlen(s)+1);
        s = buf + strlen(buf);
        while (s > buf && isspace(s[-1]))
            s--;
        *s = 0;

        /* Ignore empty lines and comment lines */
        if (!*buf || *buf == '#')
            continue;

        /* If it's a section name, this is the end of the current section */
        if (*buf == '[') {
            if (section)
                break;
            else
                continue;
        }

        /* Pass it on to the parser */
        parse_line(buf, conf, tag, path_buf, line);
    }

    fclose(f);
    return 1;
}

/*************************************************************************/

/**
 * module_print_config:  Prints the given array of configuration data.
 *
 * Parameters:
 *     conf: Array of configuration data.
 *      tag: Tag to use in log messages.
 * Return value:
 *     None.
 */

void module_print_config(const TCConfigEntry *conf, const char *tag)
{
    /* Sanity checks */
    if (!tag)
        tag = __FILE__;
    if (!conf) {
        tc_log_error(tag, "module_read_config(): conf == NULL!");
        return;
    }

    while (conf->name) {
        char buf[TC_BUF_MAX];
        switch (conf->type) {
          case TCCONF_TYPE_FLAG:
            tc_snprintf(buf, sizeof(buf), "%d", *((int *)conf->ptr) ? 1 : 0);
            break;
          case TCCONF_TYPE_INT:
            tc_snprintf(buf, sizeof(buf), "%d", *((int *)conf->ptr));
            break;
          case TCCONF_TYPE_FLOAT:
            tc_snprintf(buf, sizeof(buf), "%f", *((float *)conf->ptr));
            break;
          case TCCONF_TYPE_STRING:
            tc_snprintf(buf, sizeof(buf), "%s", *((char **)conf->ptr));
            break;
        }
        tc_log_info(tag, "%s = %s", conf->name, buf);
        conf++;
    }
}

/*************************************************************************/
/*************************************************************************/

/**
 * parse_line:  Internal routine to parse a single line of a configuration
 * file and set the appropriate variable.
 *
 * Parameters:
 *          buf: Line to process.  Leading and trailing whitespace
 *               (including newlines) are assumed to have been stripped.
 *         conf: Array of configuration entries.
 *          tag: Tag to use in log messages.
 *     filename: Name of file being processed.
 *         line: Current line number in file.
 * Return value:
 *     None.
 * Preconditions:
 *     buf != NULL
 *     conf != NULL
 *     tag != NULL
 *     filename != NULL
 */

static void parse_line(char *buf, TCConfigEntry *conf, const char *tag,
                       const char *filename, int line)
{
    char *name, *value, *s;

    /* Split string into name and value */
    name = buf;
    s = strchr(buf, '=');
    if (s) {
        value = s+1;
        while (s > buf && isspace(s[-1]))
            s--;
        *s = 0;
        while (isspace(*value))
            value++;
    } else {
        value = NULL;
    }
    if (!*name) {
        tc_log_warn(tag, "%s:%d: Syntax error (missing variable name)",
                    filename, line);
        return;
    } else if (!*value) {
        tc_log_warn(tag, "%s:%d: Syntax error (missing value)",
                    filename, line);
        return;
    }

    /* Look for a matching configuration entry */
    while (conf->name) {
        if (strcmp(conf->name, name) == 0)
            break;
        conf++;
    }
    if (!conf->name) {
        tc_log_warn(tag, "%s:%d: Unknown configuration variable `%s'",
                    filename, line, name);
        return;
    }

    /* Set the value appropriately */

    switch (conf->type) {
      case TCCONF_TYPE_FLAG:
        if (strcmp(value,"1"   ) == 0
         || strcmp(value,"yes" ) == 0
         || strcmp(value,"on " ) == 0
         || strcmp(value,"true") == 0
        ) {
            *((int *)(conf->ptr)) = (int)conf->max;
        } else if (strcmp(value,"0"    ) == 0
                || strcmp(value,"no"   ) == 0
                || strcmp(value,"off"  ) == 0
                || strcmp(value,"false") == 0
        ) {
            *((int *)(conf->ptr)) = 0;
        } else {
            tc_log_warn(tag, "%s:%d: Value for variable `%s' must be"
                        " either 1 or 0", filename, line, name);
        }
        break;

      case TCCONF_TYPE_INT: {
        long lvalue;
        errno = 0;
        lvalue = strtol(value, &value, 0);
        if (*value) {
            tc_log_warn(tag, "%s:%d: Value for variable `%s' must be an"
                        " integer", filename, line, name);
        } else if (errno == ERANGE
#if LONG_MIN < INT_MIN
                || lvalue < INT_MIN
#endif
#if LONG_MAX < INT_MAX
                || lvalue > INT_MAX
#endif
                || ((conf->flags & TCCONF_FLAG_MIN) && lvalue < conf->min)
                || ((conf->flags & TCCONF_FLAG_MAX) && lvalue > conf->max)
        ) {
            tc_log_warn(tag, "%s:%d: Value for variable `%s' is out of"
                        " range", filename, line, name);
        } else {
            *((int *)(conf->ptr)) = (int)lvalue;
        }
        break;
      }

      case TCCONF_TYPE_FLOAT: {
        float fvalue;
        errno = 0;
#ifdef HAVE_STRTOF
        fvalue = strtof(value, &value);
#else
        fvalue = (float)strtod(value, &value);
#endif
        if (*value) {
            tc_log_warn(tag, "%s:%d: Value for variable `%s' must be a"
                        " number", filename, line, name);
        } else if (errno == ERANGE
                || ((conf->flags & TCCONF_FLAG_MIN) && fvalue < conf->min)
                || ((conf->flags & TCCONF_FLAG_MAX) && fvalue > conf->max)
        ) {
            tc_log_warn(tag, "%s:%d: Value for variable `%s' is out of"
                        " range", filename, line, name);
        } else {
            *((float *)(conf->ptr)) = fvalue;
        }
        break;
      }

      case TCCONF_TYPE_STRING: {
        char *newval = tc_strdup(value);
        if (!newval) {
            tc_log_warn(tag, "%s:%d: Out of memory setting variable `%s'",
                        filename, line, name);
        } else {
            *((char **)(conf->ptr)) = newval;
        }
        break;
      }

      default:
        tc_log_warn(tag, "%s:%d: Unknown type %d for variable `%s' (bug?)",
                    filename, line, conf->type, name);
        break;

    } /* switch (conf->type) */
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
