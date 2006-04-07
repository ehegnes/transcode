
#ifndef _STATIC_LIBIOAUX_H_
#define _STATIC_LIBIOAUX_H_

#include "libioaux/configs.h"
void dummy_libioaux(void);
void dummy_libioaux(void) {
    module_print_config(NULL, NULL);		
    append_fc_time(NULL, NULL);
}

#endif /* _STATIC_LIBIOAUX_H_ */
