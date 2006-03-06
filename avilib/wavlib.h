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

#ifndef _WAVLIB_H_
#define _WAVLIB_H_

#include <sys/types.h>
#include <inttypes.h>
#include <stdint.h>

/* transcode build system integration */
#ifdef WORDS_BIGENDIAN
#define WAV_BIG_ENDIAN 1
#else
#define WAV_LITTLE_ENDIAN 1
#endif


#define WAVLIB_VERSION      "0.0.9"
#define WAVLIB_MAJOR        0
#define WAVLIB_MINOR        0
#define WAVLIB_PATCH        9

typedef enum {
    WAV_READ,           /* open WAV file in read-only mode */
    WAV_WRITE,          /* open WAV file in write-only mode */
} WAVMode;

typedef enum {
    WAV_SUCCESS,        /* no error so far */
    WAV_NO_MEM,         /* can't acquire requested amount of memory */
    WAV_IO_ERROR,       /* unable to read/write (often write) data */
    WAV_BAD_FORMAT,     /* acquired data doesn't seem wav-compliant */
    WAV_BAD_PARAM,      /* bad parameter for requested operation */
    WAV_UNSUPPORTED,    /* feature not yet supported by wavlib */
} WAVError;

typedef struct wav_ *WAV;

/*
 * wav_open:
 *
 * 
 * Parameters:
 * Return Value:
 * Side effects:
 * Preconditions:
 * Postconditions:
 */
WAV wav_open(const char *filename, WAVMode mode, WAVError *err);

/*
 * wav_fdopen:
 *
 * 
 * Parameters:
 * Return Value:
 * Side effects:
 * Preconditions:
 * Postconditions:
 */
WAV wav_fdopen(int fd, WAVMode mode, WAVError *err);

/*
 * wav_close:
 *
 * 
 * Parameters:
 * Return Value:
 * Side effects:
 * Preconditions:
 * Postconditions:
 */
int wav_close(WAV handle);

/*
 * wav_read_data:
 *      read a buffer of pcm data from given wav file. Delivers data
 *      in wav-native-byte-order (little endian).
 *      This function doesn't mess with given data, it just reads
 *      data verbatim from wav file. so caller must take care to
 *      split/join channel data or do any needed operation.
 * 
 * Parameters:
 *      handle: wav handle to write data in
 *      buffer: pointer to data to store the data readed
 *      bufsize: size of given buffer.
 * Return Value:
 *      return of bytes effectively readed from wav file.
 *      -1 means an error.
 * Side effects:
 *      N/A
 * Preconditions:
 *      given wav handle is a valid one obtained as return value of
 *      wav_open/wav_fdopen; wav handle was opened in WAV_READ mode.
 *      bufsize is a multiple of 2.
 * Postconditions:
 *      N/A
 */
ssize_t wav_read_data(WAV handle, uint8_t *buffer, size_t bufsize);

/*
 * wav_write_data:
 *      write a buffer of pcm data in given wav file. Expect data
 *      in wav-native-byte-order (little endian).
 *      This function doesn't mess with given data, it just writes
 *      data verbatim on wav file. so caller must take care to
 *      split/join channel data or do any needed operation.
 * 
 * Parameters:
 *      handle: wav handle to write data in
 *      buffer: pointer to data to be written
 *      bufsize: number of bytes of data to write
 * Return Value:
 *      return of bytes effectively written on wav file.
 *      -1 means an error.
 * Side effects:
 *      N/A
 * Preconditions:
 *      given wav handle is a valid one obtained as return value of
 *      wav_open/wav_fdopen; wav handle was opened in WAV_WRITE mode.
 *      buffer contains data in wav-native-byte-order (little endian)
 *      bufsize is a multiple of 2.
 * Postconditions:
 *      N/A
 */
ssize_t wav_write_data(WAV handle, const uint8_t *buffer, size_t bufsize);

/*
 * wav_chunk_size:
 *      guess^Wcompute the appropriate buffer size for reading/writing data
 *      with given wav descriptor.
 *
 * Parameters:
 *      handle: wav descriptor to work on
 * Return Value:
 *      suggested size of buffer for R/W operations
 * Side effects:
 *      N/A
 * Preconditions:
 *      Of course given wav handle is must be a valid one obtained as return
 *      value of wav_open/wav_fdopen; additionally, wav header for given
 *      descriptor must be fully avalaible.
 *      This is always true if wav file was opened in read mode. If wav file
 *      was opened in write mode, caller must ensure to issue all stream
 *      parameters using wav_set_<someting> (and possibly to use
 *      wav_write_header) BEFORE to use this function.
 *      Otherwise, caller will get an unreliable result (aka: garbage).
 * Postconditions:
 *      N/A
 */
uint32_t wav_chunk_size(WAV handle, double fps);

/*
 * wav_last_error:
 *     get descriptor of last error related to given wav descriptor.
 * 
 * Parameters:
 *     handle: a wav descriptor.
 * Return Value:
 *     code of last error occurred.
 * Side effects:
 *     N/A
 * Preconditions:
 *     given wav descriptor was obtained as valid return value of
 *     wav_open/wav_fdopen.
 * Postconditions:
 *     N/A
 */
WAVError wav_last_error(WAV handle);

/*
 * wav_strerror:
 *     get a human-readable short description of an error code
 * 
 * Parameters:
 *     err: error code to describe
 * Return Value:
 *     a pointer to a C-string describing the given error code,
 *     or NULL if given error code isn't known.
 * Side effects:
 *     N/A
 * Preconditions:
 *     N/A
 * Postconditions:
 *     N/A
 */
const char *wav_strerror(WAVError err);

/* XXX
 * wav_write_header:
 *
 *
 * Parameters:
 * Return Value:
 * Side effects:
 * Preconditions:
 * Postconditions:
 */
int wav_write_header(WAV handle);


/*
 * wav_{get,set}_*:
 *     set or get interesting WAV parameters.
 *     wav_get_* functions can always be used if WAV descriptor is both
 *     in read or write mode, but wav_set_* functions hare honoured
 *     only if wav descriptor is in write mode.
 *     wav_set_* functions applied to a read-mode wav are silently
 *     discarderd.
 *
 *     avalaible parameters:
 *     rate (Average Samples Per Second): quantization rate of stream, Hz.
 *     channels: number of channels in stream.
 *     bits: size in bits for every sample.
 *     bitrate (derived from Average Bytes per Second):
 *         bytes needed to store a second of data.
 *         Expressed in *KILOBIT/second*.
 *
 * Parameters:
 *     handle: handle to a WAV descriptor returned by wav_open/wav_fdopen.
 *     <parameter>: (only wav_set_*) value of parameter to set in descriptor.
 * Return Value:
 *     wav_get_*: value of requested parameter.
 *     wav_set_*: None.
 * Side effects:
 *     N/A
 * Preconditions:
 *     given wav descriptor is a valid one obtained as return value of
 *     wav_open or wav_fdopen.
 * Postconditions:
 *     N/A
 */

uint16_t wav_get_rate(WAV handle);
void wav_set_rate(WAV handle, uint16_t rate);

uint8_t wav_get_channels(WAV handle);
void wav_set_channels(WAV handle, uint8_t channels);

uint8_t wav_get_bits(WAV handle);
void wav_set_bits(WAV handle, uint8_t bits);

uint32_t wav_get_bitrate(WAV handle);
void wav_set_bitrate(WAV handle, uint32_t bitrate);

#endif /* _WAVLIB_H_ */
