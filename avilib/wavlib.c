/*
 * wavlib.h - simple WAV I/O library interface
 * Copyright (C) 2006 Francesco Romani <fromani at gmail dot com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "wavlib.h"

extern int errno;

/*************************************************************************
 * utilties                                                              *
 *************************************************************************/
 
#if (!defined HAVE_BYTESWAP && defined WAV_BIG_ENDIAN)

static uint16_t bswap_16(uint16_t x)
{
    return (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
}

static uint32_t bswap_32(uint32_t x)
{
    return (((x & 0xff000000UL) >> 24) |
            ((x & 0x00ff0000UL) >> 8) |
            ((x & 0x0000ff00UL) << 8) |
            ((x & 0x000000ffUL) << 24));
}

static uint64_t bswap_64(uint64_t x)
{
    return (((x & 0xff00000000000000ULL) >> 56) |
            ((x & 0x00ff000000000000ULL) >> 40) |
            ((x & 0x0000ff0000000000ULL) >> 24) |
            ((x & 0x000000ff00000000ULL) >> 8)  |
            ((x & 0x00000000ff000000ULL) << 8)  |
            ((x & 0x0000000000ff0000ULL) << 24) |
            ((x & 0x000000000000ff00ULL) << 40) |
            ((x & 0x00000000000000ffULL) << 56));
}
#endif

#if (!defined WAV_BIG_ENDIAN && !defined WAV_LITTLE_ENDIAN)
#error "you must define either LITTLE_ENDIAN or BIG_ENDIAN"
#endif

#if (defined WAV_BIG_ENDIAN && defined WAV_LITTLE_ENDIAN)
#error "you CAN'T define BOTH LITTLE_ENDIAN and BIG_ENDIAN"
#endif

#if defined WAV_BIG_ENDIAN
#define htol_16(x) bswap_16(x)
#define htol_32(x) bswap_32(x)
#define htol_64(x) bswap_64(x)

#elif defined WAV_LITTLE_ENDIAN

#define htol_16(x) (x)
#define htol_32(x) (x)
#define htol_64(x) (x)

#endif

/* often used out-of-order */
#define make_wav_get_bits(s) \
static inline uint##s##_t wav_get_bits##s(uint8_t *d) \
{ \
    return htol_##s(*((uint##s##_t*)d)); \
}

/* often used sequentially */
#define make_wav_put_bits(s) \
static inline uint8_t *wav_put_bits##s(uint8_t *d, uint##s##_t u) \
{ \
    *((uint##s##_t*)d) = htol_##s(u); \
    return (d + (s / 8)); \
}

make_wav_get_bits(16)
make_wav_get_bits(32)
make_wav_get_bits(64)

make_wav_put_bits(16)
make_wav_put_bits(32)
make_wav_put_bits(64)

static inline uint32_t make_tag(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (a | (b << 8) | (c << 16) | (d << 24));
}

ssize_t wav_fdread(int fd, uint8_t *buf, size_t len)
{
    ssize_t n = 0;
    ssize_t r = 0;

    while (r < len) {
        n = read(fd, buf + r, len - r);

        if (n == 0) {  /* EOF */
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        r += n;
    }
    return r;
}


ssize_t wav_fdwrite(int fd, const uint8_t *buf, size_t len)
{
    ssize_t n = 0;
    ssize_t r = 0;

    while (r < len) {
        n = write(fd, buf + r, len - r);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        r += n;
    }
    return r;
}

/*************************************************************************
 * header data                                                           *
 *************************************************************************/

/*
 * WAVE header:
 * 
 * TAG: 'RIFF'  4   bytes
 * LENGTH:      4   bytes
 * TAG: 'WAVE'  4   bytes
 * 
 * TAG: 'fmt '  4   bytes
 * LENGTH:      4   bytes
 * 
 *                        +
 * FORMAT:      2   bytes |
 * CHANNELS:    2   bytes |
 * SAMPLES:     4   bytes | simple WAV format:
 * AVGBYTES:    4   bytes | 16 byte
 * BLKALIGN:    2   bytes |
 * BITS:        2   bytes |
 *                        +
 * 
 * TAG: 'data'  4   bytes
 * LENGTH:      4   bytes
 *
 * ----------------------------
 * TOTAL wav header: 44 bytes
 */

#define WAV_HEADER_LEN      (44)
#define WAV_FORMAT_LEN      (16)

#define PCM_ID              (0x1)

/*************************************************************************
 * core data/routines                                                    *
 *************************************************************************/

#define WAV_SET_ERROR(errp, code) \
        if (errp != NULL) { \
            *errp = code; \
        }

struct wav_ {
    int fd;

    int header_done;
    
    WAVMode mode;
    WAVError error;

    uint32_t len;
    uint32_t data_len;

    uint32_t bitrate;
    uint16_t bits;
    uint16_t channels;
    uint32_t rate;
    
    uint16_t block_align;
};

const char *wav_strerror(WAVError err)
{
    const char *s = NULL;

    switch (err) {
      case WAV_SUCCESS:
        s = "no error";
        break;
      case WAV_NO_MEM:
        s = "can't acquire the needed amount of memory";
        break;
      case WAV_IO_ERROR:
        s = "error while performing I/O operation";
        break;
      case WAV_BAD_FORMAT:
        s = "incorrect/unrecognized WAV data";
        break;
      case WAV_BAD_PARAM:
        s = "bad/unknown parameter for this operation";
        break;
      case WAV_UNSUPPORTED:
        s = "not yet supported by wavlib";
        break;
      default:
        s = NULL;
        break;
    }
    return s;
}

static int wav_parse_header(WAV handle, WAVError *err)
{
    uint8_t hdr[WAV_HEADER_LEN];
    ssize_t r = 0;
    uint16_t wav_fmt = 0;
    uint32_t fmt_len = 0;
    
    if (!handle || handle->fd == -1 || handle->mode != WAV_READ) {
        return -1;
    }
    
    r = wav_fdread(handle->fd, hdr, WAV_HEADER_LEN);
    if (r != WAV_HEADER_LEN) {
        WAV_SET_ERROR(err, WAV_BAD_FORMAT);
        goto bad_wav;
    }
    if ((wav_get_bits32(hdr) != make_tag('R', 'I', 'F', 'F'))
     || (wav_get_bits32(hdr + 8) != make_tag('W', 'A', 'V', 'E'))
     || (wav_get_bits32(hdr + 12) != make_tag('f', 'm', 't', ' '))) {
        WAV_SET_ERROR(err, WAV_BAD_FORMAT);
        goto bad_wav;
    }

    fmt_len = wav_get_bits32(hdr + 16);
    wav_fmt = wav_get_bits16(hdr + 20);
    if (fmt_len != WAV_FORMAT_LEN || wav_fmt != PCM_ID) {
        WAV_SET_ERROR(err, WAV_UNSUPPORTED);
        goto bad_wav;
    }
    
    handle->len = wav_get_bits32(hdr + 4);
    handle->channels = wav_get_bits16(hdr + 22);
    handle->rate = wav_get_bits32(hdr + 24);
    handle->bitrate = (wav_get_bits32(hdr + 28) * 8) / 1000;
    handle->block_align = wav_get_bits16(hdr + 32);
    handle->bits = wav_get_bits16(hdr + 34);
    /* skip 'data' tag (4 bytes) */
    handle->data_len = wav_get_bits32(hdr + 40);

    return 0;

bad_wav:    
    lseek(handle->fd, 0, SEEK_SET);
    return 1;
}

int wav_build_header(WAV handle)
{
    uint8_t hdr[WAV_HEADER_LEN];
    uint8_t *ph = hdr;
    off_t pos = 0, ret = 0;
    ssize_t w = 0;
    
    if (!handle) {
        return -1;
    }
    
    pos = lseek(handle->fd, 0, SEEK_CUR);
    ret = lseek(handle->fd, 0, SEEK_SET);
    if (ret == (off_t)-1) {
        return 1;
    }
                    
    ph = wav_put_bits32(ph, make_tag('R', 'I', 'F', 'F'));
    ph = wav_put_bits32(ph, handle->len);
    ph = wav_put_bits32(ph, make_tag('W', 'A', 'V', 'E'));
    
    ph = wav_put_bits32(ph, make_tag('f', 'm', 't', ' '));
    ph = wav_put_bits32(ph, WAV_FORMAT_LEN);

    /* format */
    ph = wav_put_bits16(ph, PCM_ID);
    /* wave format, only plain PCM supported, yet */
    ph = wav_put_bits16(ph, handle->channels);
    /* number of channels */
    ph = wav_put_bits32(ph, handle->rate);
    /* sample rate */
    ph = wav_put_bits32(ph, (handle->bitrate * 1000)/8);
    /* average bytes per second (aka bitrate) */
    ph = wav_put_bits16(ph, ((handle->channels * handle->bits) / 8));
    /* block alignment */
    ph = wav_put_bits16(ph, handle->bits);
    /* bits for sample */
    
    ph = wav_put_bits32(ph, make_tag('d', 'a', 't', 'a'));
    ph = wav_put_bits32(ph, handle->data_len);
    
    w = wav_fdwrite(handle->fd, hdr, WAV_HEADER_LEN);
    ret = lseek(handle->fd, pos, SEEK_SET);
    if (ret == (off_t)-1) {
        return 1;
    }
   
    if (w != WAV_HEADER_LEN) {
        return 2;
    }
    handle->header_done = 1;
    return 0;
}

WAV wav_open(const char *filename, WAVMode mode, WAVError *err)
{
    int oflags = (mode == WAV_READ) ?O_RDONLY :O_TRUNC|O_CREAT|O_WRONLY;
    int fd = -1;
    WAV wav = NULL;
    
    if (!filename || !strlen(filename)) {
        WAV_SET_ERROR(err, WAV_BAD_PARAM);
    } else {
        fd = open(filename, oflags,
                  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        wav = wav_fdopen(fd, mode, err);
        if (!wav) {
            close(fd);
        }
    }
    return wav;
}
   
WAV wav_fdopen(int fd, WAVMode mode, WAVError *err)
{
    int ret;
    WAV wav = calloc(1, sizeof(struct wav_));
    
    if (!wav) {
        WAV_SET_ERROR(err, WAV_NO_MEM);
    } else {
        wav->fd = fd;

        ret = wav_parse_header(wav, err);
        if (ret != 0) {
            free(wav);
            wav = NULL;
        }
        if (mode == WAV_WRITE) {
            /* reserve space for header by writing a fake one */
            wav_build_header(wav);
            /* but reset heder flag */
            wav->header_done = 0;
        }
    }
    return wav;
}


#define RETURN_IF_IOERROR(err) \
    if (err != 0) { \
        WAV_SET_ERROR(&(handle->error), WAV_IO_ERROR); \
        return -1; \
    }

int wav_close(WAV handle)
{
    int ret = 0;
    
    if (!handle) {
        return -1;
    }
    
    if (!handle->header_done && handle->mode == WAV_WRITE) {
        ret = wav_build_header(handle);
        RETURN_IF_IOERROR(ret);
    }

    ret = close(handle->fd);
    RETURN_IF_IOERROR(ret);
    free(handle);
    
    return 0;
}

#undef RETURN_IF_IOERROR

uint32_t wav_chunk_size(WAV handle, double fps)
{
    uint32_t size = 0;
    double fch;
    
    if (!handle || !fps) {
        return -1;
    }

    fch = handle->rate / fps;
    
    /* bytes per audio frame */
    size = (int)(fch * (handle->bits / 8) * handle->channels);
    size = (size>>2)<<2; /* XXX */

    return 0;
}

WAVError wav_last_error(WAV handle)
{
    return (handle) ?(handle->error) :WAV_BAD_PARAM;
}

uint32_t wav_get_bitrate(WAV handle)
{
    return (handle) ?(handle->bitrate) :0;
}

uint16_t wav_get_rate(WAV handle)
{
    return (handle) ?(handle->rate) :0;
}

uint8_t wav_get_channels(WAV handle)
{
    return (handle) ?(handle->channels) :0;
}

uint8_t wav_get_bits(WAV handle)
{
    return (handle) ?(handle->bits) :0;
}

void wav_set_rate(WAV handle, uint16_t rate)
{
    if (handle && handle->mode == WAV_WRITE) {
        handle->rate = rate;
    }
}

void wav_set_channels(WAV handle, uint8_t channels)
{
    if (handle && handle->mode == WAV_WRITE) {
        handle->channels = channels;
    }
}

void wav_set_bits(WAV handle, uint8_t bits)
{
    if (handle && handle->mode == WAV_WRITE) {
        handle->bits = bits;
    }
}

void wav_set_bitrate(WAV handle, uint32_t bitrate)
{
    if (handle && handle->mode == WAV_WRITE) {
        handle->bitrate = bitrate;
    }
}

ssize_t wav_read_data(WAV handle, uint8_t *buffer, size_t bufsize)
{
    if (!handle) {
        return -1;
    }
    if (!buffer || bufsize < 0) {
        WAV_SET_ERROR(&(handle->error), WAV_BAD_PARAM);
        return -1;
    }
    if (handle->mode != WAV_READ || (bufsize % 2 != 0)) {
        WAV_SET_ERROR(&(handle->error), WAV_UNSUPPORTED);
        return -1;
    }
    return wav_fdread(handle->fd, buffer, bufsize);
}

ssize_t wav_write_data(WAV handle, const uint8_t *buffer, size_t bufsize)
{
    if (!handle) {
        return -1;
    }
    if (!buffer || bufsize < 0) {
        WAV_SET_ERROR(&(handle->error), WAV_BAD_PARAM);
        return -1;
    }
    if (handle->mode != WAV_WRITE || (bufsize % 2 != 0)) {
        WAV_SET_ERROR(&(handle->error), WAV_UNSUPPORTED);
        return -1;
    }
    return wav_fdwrite(handle->fd, buffer, bufsize);
}