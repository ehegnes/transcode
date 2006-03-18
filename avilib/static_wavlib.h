
#ifndef _STATIC_WAVLIB_H_
#define _STATIC_WAVLIB_H_

#include "avilib/wav.h"
void dummy_avilib(void);
void dummy_avilib(void) {
        WAV wav = NULL;
        WAVError err;

        wav = wav_open(NULL, WAV_READ, &err);
        wav_close(wav);
}

#endif // _STATIC_WAVLIB_H_
