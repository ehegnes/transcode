/*
 * tcmodule-registry.h -- module information registry.
 * (C) 2009 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * transcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef TCMODULEREGISTRY_H
#define TCMODULEREGISTRY_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "tcmodule-core.h"
#include "tcmodule-data.h"

#include <stdint.h>

/*************************************************************************/

/* registry data type. */
typedef struct tcregistry_ *TCRegistry;


TCRegistry tc_new_module_registry(TCFactory factory,
                                  const char *regpath, int verbose);

int tc_del_module_registry(TCRegistry registry);


TCModule tc_new_module_for_format(TCRegistry registry,
                                  const char *modclass,
                                  const char *format,
                                  int media);

TCModule tc_new_module_most_fit(TCRegistry registry,
                                const char *modclass,
                                const char *fmtname, const char *modname,
                                int media);

const char *tc_get_module_name_for_format(TCRegistry registry,
                                          const char *modclass,
                                          const char *fmtname);

const char *tc_module_registry_default_path(void);


TCModule tc_new_module_from_names(TCFactory factory,
                                  const char *modclass,
                                  const char *modnames,
                                  int media);

/*************************************************************************/

#endif  /* TCMODULEREGISTRY_H */

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
