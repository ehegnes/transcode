
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ioaux.h"

/* Some VNC constants */
#define VNCREC_MAGIC_STRING	"vncLog0.0"
#define VNCREC_MAGIC_SIZE	(9)
#define VNC_RFB_PROTOCOL_SCANF_FORMAT "RFB %03d.%03d\n"
#define VNC_RFB_PROTO_VERSION_SIZE	(12)

#define VNC33_CHALLENGESIZE	(16)

#define VNC33_rfbConnFailed	(0)
#define VNC33_rfbNoAuth		(1)
#define VNC33_rfbVncAuth	(2)

#define VNC33_rfbVncAuthOK	(0)
#define VNC33_rfbVncAuthFailed	(1)
#define VNC33_rfbVncAuthTooMany	(2)

/*
   RFB 3.X protocol is as follows (from vncrec source code):
    * server send 12-byte (SIZE_SIZE_RFB_PROTO_VERSION) header
      encoding the RFB version as an ASCII string
    * server sends 4-byte number (big-endian) to alert auth
      requirements
    * if requiring auth, server then sends 16-byte (VNC33_CHALLENGESIZE)
      packet, which is to be encrypted and sent back (same size). 
      The server then sends 32-bit word result on pass-fail.  Entire
      thing aborted if not passed.
    * client sends 1-byte message
    * server then sends a display-paramters message, containing
      (in order) the width (2-byte), height (2-byte), preferred pixel
      format (1-byte), and desktop name (1-byte with length, n bytes).
 */

void probe_vnc(info_t *ipipe)
{
    unsigned char buf[100];
    unsigned char matchingBuffer[100];
    int index = 0, i, major, minor, authReqs;
    int width, height;

    if(p_read(ipipe->fd_in, buf, sizeof(buf)) != sizeof(buf)) {
	fprintf(stderr, "(%s) end of stream\n", __FILE__);
	ipipe->error=1;
	return;
    }

    /* Check VNCREC magic */
    memcpy(matchingBuffer, &buf[index], VNCREC_MAGIC_SIZE);
    matchingBuffer[VNCREC_MAGIC_SIZE] = 0;
    if(strcmp(matchingBuffer, VNCREC_MAGIC_STRING)) { /* NOT EQUAL */
	fprintf(stderr, "(%s) unsupported version of vncrec (\"%s\")\n",
	    __FILE__, matchingBuffer);
	ipipe->error=1;
	return;
    }
    index += VNCREC_MAGIC_SIZE;


    /* Ensure RFB protocol is valid */
    memcpy(matchingBuffer, &buf[index], VNC_RFB_PROTO_VERSION_SIZE);
    matchingBuffer[VNC_RFB_PROTO_VERSION_SIZE] = 0;
    if(sscanf(matchingBuffer, VNC_RFB_PROTOCOL_SCANF_FORMAT, &major, &minor) != 2) {
	fprintf(stderr, "(%s) unknown RFB protocol (\"%s\")\n", __FILE__,
	    matchingBuffer);
	ipipe->error=1;
	return;
    }
    if (ipipe->verbose & TC_DEBUG) printf("File recorded as RFB Protocol v%d.%d\n", major, minor);
    if(major != 3) {
	fprintf(stderr, "(%s) unsupported RFB protocol (only support v3)\n",
	    __FILE__);
	ipipe->error=1;
	return;
    }
    index += VNC_RFB_PROTO_VERSION_SIZE;

    /* Check authentication requirements */
    authReqs = (buf[index] << 24) | (buf[index+1] << 16)
		| (buf[index+2] << 8) | buf[index+3];
    index += 4;
    switch(authReqs) {
      case VNC33_rfbNoAuth:
	if (ipipe->verbose & TC_DEBUG) printf("No authorization required.\n");
	break;

      case VNC33_rfbVncAuth: {
	int authResp = 
	index += VNC33_CHALLENGESIZE;
	authResp = (buf[index] << 24) | (buf[index+1] << 16)
		    | (buf[index+2] << 8) | buf[index+3];
	/* switch(authResp) { ... } */
	index += 4;
	break;
	}

      case VNC33_rfbConnFailed:
      default:
	fprintf(stderr, "(%s) apparently connection failed?\n", __FILE__);
	ipipe->error=1;
	return;
    }

    /* Receive display parameters */
    width = (buf[index] << 8) | buf[index+1];
    height = (buf[index+2] << 8) | buf[index+3];

    ipipe->probe_info->width  = width;
    ipipe->probe_info->height = height;
    ipipe->probe_info->fps = 25.;
    ipipe->probe_info->frc = 3;
    ipipe->probe_info->codec = TC_CODEC_RGB;
    ipipe->probe_info->magic = ipipe->magic;

}
