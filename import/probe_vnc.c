
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ioaux.h"

void probe_vnc(info_t *ipipe)
{
    char buf[100];
    if(p_read(ipipe->fd_in, buf, sizeof(buf)) != sizeof(buf)) {
	fprintf(stderr, "(%s) end of stream\n", __FILE__);
	ipipe->error=1;
	return;
    }
    ipipe->probe_info->width  = ( ((buf[45]<<8)&0xff00) | (buf[46]&0xff) ) & 0xffff;
    ipipe->probe_info->height = ( ((buf[47]<<8)&0xff00) | (buf[48]&0xff) ) & 0xffff;
    ipipe->probe_info->fps = 25.;
    ipipe->probe_info->frc = 3;
    ipipe->probe_info->codec = TC_CODEC_RGB;
    ipipe->probe_info->magic = ipipe->magic;

}
