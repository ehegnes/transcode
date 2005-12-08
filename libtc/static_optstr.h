
#ifndef _STATIC_OPTSTR_H_
#define _STATIC_OPTSTR_H_

#include "libtc/optstr.h"
void dummy_optstr(void);
void dummy_optstr(void) {
  optstr_lookup(NULL, NULL);
  optstr_get(NULL, NULL, NULL);
  optstr_filter_desc(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  optstr_frames_needed(NULL, NULL);
  optstr_param(NULL, NULL, NULL, NULL, NULL);
}

#endif /* _STATIC_OPTSTR_H_ */
