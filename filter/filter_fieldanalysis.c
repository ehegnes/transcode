/*
 *  filter_fieldanalysis.c
 *
 *  (c) by Matthias Hopf - August 2004
 *
 *  This file is part of transcode, a linux video stream processing tool
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

/*
 * This filter determines the type of video
 * (interlaced / progressive / field shifed / telecined)
 * by analysing the luminance field of the input frames.
  */

#define MOD_NAME    "filter_fieldanalysis.so"
#define MOD_VERSION "v1.0 pl1 (2004-08-13)"
#define MOD_CAP     "Field analysis for detecting interlace and telecine"
#define MOD_AUTHORS "Matthias Hopf"


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"
#if 0
#include "aclib/ac.h"
#endif


/*
 * State
 */

typedef struct {

    double interlaceDiff;
    double unknownDiff;
    double progressiveDiff;
    double progressiveChange;
    double changedIfMore;
    
    int   forceTelecineDetect;
    int   verbose;
    int   outDiff;

    float fps;
    int   codec;

    int   width;
    int   height;
    int   size;

    /* lumance fields, current (In) and previous,
     * top original T (bottom interpolated) and bottom original B */
    uint8_t *lumIn, *lumPrev, *lumInT, *lumInB, *lumPrevT, *lumPrevB;

    int   telecineState;

    int   numFrames;
    int   unknownFrames;
    int   topFirstFrames;
    int   bottomFirstFrames;
    int   interlacedFrames;
    int   progressiveFrames;
    int   fieldShiftFrames;
    int   telecineFrames;

} myfilter_t;

static myfilter_t *myf_global = NULL;

/* Internal state flag values */
enum { IS_UNKNOWN = -1, IS_FALSE = 0, IS_TRUE = 1 };


/* API */
extern int tc_filter (vframe_list_t *ptr, char *options);


/*
 * transcoders Any -> Luminance
 */

/* packed YUV 4:2:2 is Y[i] U[i] Y[i+1] V[i] (YUY2)*/
/* packed YUV 4:2:2 is U[i] Y[i] V[i] Y[i+1] (UYVY)*/
void uyvytoy (uint8_t *input, uint8_t *output, int width, int height)
{
    int i;
    for (i = width*height/2; i; i--) {
	*output++ = input[1];
	*output++ = input[3];
	input += 4;
    }
}
void yuy2toy (uint8_t *input, uint8_t *output, int width, int height)
{
    int i;
    for (i = width*height/2; i; i--) {
	*output++ = input[0];
	*output++ = input[2];
	input += 4;
    }
}
void rgbtoy (uint8_t *input, uint8_t *output, int width, int height)
{
    int i;
    for (i = width*height; i; i--) {
	/* This is a rough approximation of (0.3)(r) + (0.59)(g) + (0.11)(b) */
	*output++ = (((short)input[0]) * 5 + ((short)input[1]) * 9 + ((short)input[2]) * 2) >> 4;
	input += 3;
    }
}

/*
 * Helper functions
 */

/* bob a single field */
void bob_field (uint8_t *in, uint8_t *out, int width, int height) {
    int i, j, w2 = 2*width;
    for (i = 0; i < height; i++) {
	/* First bob (average lines) */
	for (j = 0; j < width; j++)
	    out[j] = (((short)in[j]) + ((short)in[j+w2])) >>1;
	/* Then copy original line */
	tc_memcpy (out+width, in+w2, width);
	in  += w2;
	out += w2;
    }
}

/* compare images: calc squared 2-norm of difference image
 * maximum difference is 255^2 = 65025 */
double pic_compare (uint8_t *p1, uint8_t *p2, int width, int height,
                    int modulo) {
    long long res = 0;
    int i, j;
    for (i = height; i; i--) {
	for (j = width; j; j--) {
	    int d = ((int)*p1++) - ((int)*p2++);
	    res += d*d;
	}
	p1 += modulo;
	p2 += modulo;
    }
    return ((double) res) / (width*height);
}

/* create scaled difference image (for outdiff) */
void pic_diff (uint8_t *p1, uint8_t *p2, uint8_t *dest, int size, int scale) {
    int i;
    for (i = size ; i; i--) {
	int d = scale * (((int)*p1++) - ((int)*p2++));
	d = d > 0 ? d : -d;
	*dest++ = d > 255 ? 255 : d;
    }
}

/*
 * main function: check interlace state
 */
void check_interlace (myfilter_t *myf, int id) {
    
    double pixDiff, pixShiftChangedT, pixShiftChangedB;
    double pixLastT, pixLastB, pixLast;
    int isChangedT = IS_FALSE,   isChangedB = IS_FALSE;
    int isProg     = IS_UNKNOWN;
    int isShift    = IS_UNKNOWN, isTop      = IS_UNKNOWN;
    int *counter = &myf->unknownFrames;

    /* Determine frame parmeters */
    pixDiff          = pic_compare (myf->lumInT, myf->lumInB,
				    myf->width, myf->height-2, 0);
    pixShiftChangedT = pic_compare (myf->lumInT, myf->lumPrevB,
				    myf->width, myf->height-2, 0);
    pixShiftChangedB = pic_compare (myf->lumInB, myf->lumPrevT,
				    myf->width, myf->height-2, 0);
    pixLastT         = pic_compare (myf->lumIn,  myf->lumPrev,
				    myf->width, myf->height/2, myf->width);
    pixLastB         = pic_compare (myf->lumIn   + myf->width,
				    myf->lumPrev + myf->width,
				    myf->width, myf->height/2, myf->width);
    pixLast = (pixLastT + pixLastB) / 2;
    
    /* Check for changed fields */
    if (pixLastT > myf->changedIfMore)
	isChangedT = IS_TRUE;
    if (pixLastB > myf->changedIfMore)
	isChangedB = IS_TRUE;
    
    /* Major field detection */
    if (pixShiftChangedT * myf->interlaceDiff < pixShiftChangedB)
	isTop = IS_TRUE;
    if (pixShiftChangedB * myf->interlaceDiff < pixShiftChangedT)
	isTop = IS_FALSE;

    /* Check for progessive frame */
    if (pixDiff * myf->unknownDiff > pixShiftChangedT ||
	pixDiff * myf->unknownDiff > pixShiftChangedB)
	isProg = IS_FALSE;
    if (pixDiff * myf->progressiveDiff < pixShiftChangedT &&
	pixDiff * myf->progressiveDiff < pixShiftChangedB &&
	pixDiff < pixLast * myf->progressiveChange)
	isProg = IS_TRUE;

    /* Check for shifted progressive frame */
    /* not completely equivalent to check.prog.frame. (pixLast) */
    /* TODO: this triggers too often for regular interlaced material */
    if (pixShiftChangedT * myf->progressiveDiff < pixDiff &&
	pixShiftChangedT * myf->progressiveDiff < pixShiftChangedB &&
	pixShiftChangedT < myf->progressiveChange * pixLast)
	isShift = IS_TRUE;
    if (pixShiftChangedB * myf->progressiveDiff < pixDiff &&
	pixShiftChangedB * myf->progressiveDiff < pixShiftChangedT &&
	pixShiftChangedT < myf->progressiveChange * pixLast)
	isShift = IS_TRUE;

    /* Detect telecine */
    /* telecined material will create a sequence like this (TopfieldFirst):
     * 0t1b (0)1t1b (1)2t2b (2)3t3b (3)3t4b (4)4t5b (0)5t5b ...
     * and vice versa for BottomfieldFirst.
     * We start looking for an progressive frame (1t1b) with one field
     * equal to the last frame and continue checking the frames for
     * adhering to the pattern.
     * We only count frames as valid telecine frames, if the pattern has been
     * detected twice in a row (telecineState > 10).
     * Undecidable frames (isProg == UNKNOWN && isTop == UNKNOWN) and
     * frames without changes (isChangedT != TRUE && isChangedB != TRUE)
     * are only accepted, if telecineState > 10.
     * Only happens for NTSC (29.97fps).
     * If a test fails, it reduces the current telecineState by 20.
     * Max. telecineState is 100.
     */
    /* TODO: currently no field insertion/deletion is detected,
     * requires resync right now */
    if ((myf->fps > 29.9 && myf->fps < 30.1) || myf->forceTelecineDetect) {

	if ((isChangedT == IS_TRUE || isChangedB == IS_TRUE) &&
	    (isProg != IS_UNKNOWN || isTop != IS_UNKNOWN ||
	     myf->telecineState > 10)){
	    
	    switch (myf->telecineState % 5) {
	    case 0:
/*		if (isProg == IS_FALSE)
		myf->telecineState -= 20; */ /* prog. detmination may fail */
		switch (isTop) {
		case IS_TRUE:
		    if (isChangedB == IS_TRUE)
			myf->telecineState -= 20;
		    break;
		case IS_FALSE:
		    if (isChangedT == IS_TRUE)
			myf->telecineState -= 20;
		    break;
		}
		break;
	    case 1:
	    case 2:
		if (isProg == IS_FALSE)
		    myf->telecineState -= 20;
		break;
	    case 3:
		if (isProg == IS_TRUE)
		    myf->telecineState -= 20;
		switch (isTop) {
		case IS_TRUE:
		    if (isChangedT == IS_TRUE)
			myf->telecineState -= 20;
		    break;
		case IS_FALSE:
		    if (isChangedB == IS_TRUE)
			myf->telecineState -= 20;
		    break;
		}
		break;
	    case 4:
		if (isProg == IS_TRUE)
		    myf->telecineState -= 20;
		break;
	    }
	    if (myf->telecineState < 0)
		myf->telecineState = 0;
	    if (myf->telecineState == 0) {
		/* Frame has another chance to be case 0 */
/*		if (isProg == IS_FALSE)
		myf->telecineState = -1; */ /* prog. detmination may fail */
		switch (isTop) {
		case IS_TRUE:
		    if (isChangedB == IS_TRUE)
			myf->telecineState = -1;
		    break;
		case IS_FALSE:
		    if (isChangedT == IS_TRUE)
			myf->telecineState = -1;
		    break;
		}
	    }
	    myf->telecineState++;
	}
	else if (myf->telecineState > 10)
	    myf->telecineState++;
	else
	    myf->telecineState = 0;
	if (myf->telecineState > 100)
	    myf->telecineState -= 10;
    }
    
	
    /* Detect inconstistencies */
    if (isProg == IS_FALSE && isTop == IS_UNKNOWN)
	isProg = IS_UNKNOWN;
    if (isProg != IS_FALSE && isTop != IS_UNKNOWN) {
	isTop  = IS_UNKNOWN;
	isProg = IS_UNKNOWN;
    }
    if (isChangedT == IS_FALSE || isChangedB == IS_FALSE) {
	isProg  = IS_UNKNOWN;
	isTop   = IS_UNKNOWN;
	isShift = IS_UNKNOWN;
    }

    /* verbose output */
    if (myf->verbose) {
	char verboseBuffer[64];
	char *outType = 0, *outField = " ";
	
	memset (verboseBuffer, ' ', 63);
	
	if (pixDiff * myf->unknownDiff < pixShiftChangedT)
	    memcpy (&verboseBuffer[0], "pt", 2);
	if (pixDiff * myf->progressiveDiff < pixShiftChangedT)
	    memcpy (&verboseBuffer[0], "Pt", 2);
	if (pixDiff * myf->unknownDiff < pixShiftChangedB)
	    memcpy (&verboseBuffer[2], "pb", 2);
	if (pixDiff * myf->progressiveDiff < pixShiftChangedB)
	    memcpy (&verboseBuffer[2], "Pb", 2);

	if (pixDiff < myf->progressiveChange*pixLast)
	    verboseBuffer[5] = 'c';
	if (pixShiftChangedT * myf->interlaceDiff < pixShiftChangedB)
	    verboseBuffer[7] = 't';
	if (pixShiftChangedB * myf->interlaceDiff < pixShiftChangedT)
	    verboseBuffer[7] = 'b';

	if (isChangedT == IS_FALSE)
	    memcpy (&verboseBuffer[9], "st", 2);
	if (isChangedB == IS_FALSE)
	    memcpy (&verboseBuffer[11], "sb", 2);

	verboseBuffer[13] = 0;
	
	if (myf->verbose > 1) {
	    fprintf (stderr, "[%s] frame %d: pixDiff %.3f pixShiftChanged %.3fT/%.3fB pixLast %.3fT/%.3fB telecineState %d\n",
		     MOD_NAME, id, pixDiff,
		     pixShiftChangedT, pixShiftChangedB, pixLastT, pixLastB,
		     myf->telecineState);
	}
	
	switch (isProg) {
	case IS_UNKNOWN:	outType = "unknown    ";	break;
	case IS_FALSE:		outType = "interlaced ";	break;
	case IS_TRUE:		outType = "progressive";	break;
	}
	if (isChangedT == IS_FALSE && isChangedB == IS_FALSE)
	    outType = "low change ";
	if (isShift == IS_TRUE)
	    outType = "shifted p  ";
	if (myf->telecineState > 10)
	    outType = "telecined  ";
	switch (isTop) {
	case IS_FALSE:		outField = "B";			break;
	case IS_TRUE:		outField = "T";			break;
	}
	
	fprintf (stderr, "[%s] frame %d: %s  %s   [%s]   \n",
		 MOD_NAME, id, outType, outField, verboseBuffer);
    }

    /* Count types */
    switch (isProg) {
    case IS_UNKNOWN:	counter = &myf->unknownFrames;		break;
    case IS_FALSE:	counter = &myf->interlacedFrames;	break;
    case IS_TRUE:	counter = &myf->progressiveFrames;	break;
    }
    if (isChangedT == IS_FALSE && isChangedB == IS_FALSE)
	counter = &myf->unknownFrames;
    if (isShift == IS_TRUE)
	counter = &myf->fieldShiftFrames;
    if (myf->telecineState > 10)
	counter = &myf->telecineFrames;
    switch (isTop) {
	case IS_FALSE:	myf->bottomFirstFrames++;		break;
	case IS_TRUE:	myf->topFirstFrames++;			break;
    }
    assert (counter);
    (*counter)++;
    myf->numFrames++;
    
}

	
/*
 * transcode API
 */
int tc_filter (vframe_list_t *ptr, char *options)
{
    vob_t *vob = NULL;
    myfilter_t *myf = myf_global;

    /*
     * filter init
     */
    if (ptr->tag & TC_FILTER_INIT) {

	if (! (vob = tc_get_vob ()))
	    return -1;
    
	if (! (myf = myf_global = calloc (1, sizeof (myfilter_t)))) {
	    fprintf (stderr, "calloc() failed\n");
	    return -1;
	}
    
	if (verbose)			/* global verbose */
	    printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
	/* default values */
	myf->interlaceDiff       = 1.1;
	myf->unknownDiff         = 1.5;
	myf->progressiveDiff     = 8;
	myf->progressiveChange   = 0.2;
	myf->changedIfMore       = 10;
	
	myf->forceTelecineDetect = 0;
	myf->verbose             = 0;
	myf->outDiff             = 0;

	/* video parameters */
	switch (vob->im_v_codec) {
	case CODEC_YUY2:
	case CODEC_YUV:
	case CODEC_YUV422:
	case CODEC_RGB:
	    break;
	default:
	    fprintf (stderr, "[%s]: Unsupported codec - need one of RGB YUV YUY2 YUV422\n", MOD_NAME);
	    return -1;
	}
	myf->codec     = vob->im_v_codec;
	myf->width     = vob->im_v_width;
	myf->height    = vob->im_v_height;
	myf->fps       = vob->fps;
	myf->size = myf->width * myf->height;

	if (options) {
	    optstr_get (options, "interlacediff",     "%lf",
			&myf->interlaceDiff);
	    optstr_get (options, "unknowndiff",       "%lf",
			&myf->unknownDiff);
	    optstr_get (options, "progressivediff",   "%lf",
			&myf->progressiveDiff);
	    optstr_get (options, "progressivechange", "%lf",
			&myf->progressiveChange);
	    optstr_get (options, "changedifmore",     "%lf",
			&myf->changedIfMore);
	    optstr_get (options, "forcetelecinedetect",
		        "%d", &myf->forceTelecineDetect);
	    optstr_get (options, "verbose",          "%d", &myf->verbose);
	    optstr_get (options, "outdiff",          "%d", &myf->outDiff);

	    if (optstr_get (options, "help", "") >= 0) {
		printf ("[%s] (%s) help\n\n"
			"* Overview:\n"
			"  'fieldanalysis' scans video for interlacing artifacts and\n"
			"  detects progressive / interlaced / telecined video.\n"
			"  It also determines the major field for interlaced video.\n"
			"* Verbose Output:   [PtPb c t eteb]\n"
			"  Pt, Pb:   progressivediff succeeded, per field.\n"
			"  pt, pb:   unknowndiff succeeded, progressivediff failed.\n"
			"  c:        progressivechange succeeded.\n"
			"  t:        topFieldFirst / b: bottomFieldFirst detected.\n"
			"  st, sb:   changedifmore failed (fields are similar to last frame).\n\n",
			MOD_NAME, MOD_CAP);
	    }
	}

	/* frame memory */
	if (! (myf->lumIn    = calloc (1, myf->size)) ||
	    ! (myf->lumPrev  = calloc (1, myf->size)) ||
	    ! (myf->lumInT   = calloc (1, myf->size)) ||
	    ! (myf->lumInB   = calloc (1, myf->size)) ||
	    ! (myf->lumPrevT = calloc (1, myf->size)) ||
	    ! (myf->lumPrevB = calloc (1, myf->size))) {
	    fprintf (stderr, "calloc() failed\n");
	    return -1;
	}

	if (verbose) {			/* global verbose */
	    printf("[%s] interlacediff %.2f,  unknowndiff %.2f,  progressivediff %.2f\n"
		   "[%s] progressivechange %.2f, changedifmore %.2f\n"
		   "[%s] forcetelecinedetect %s, verbose %d, outdiff %d\n",
		   MOD_NAME, myf->interlaceDiff, myf->unknownDiff,
		   myf->progressiveDiff,
		   MOD_NAME, myf->progressiveChange, myf->changedIfMore,
		   MOD_NAME, myf->forceTelecineDetect ? "True":"False",
		   myf->verbose, myf->outDiff);
	}
	
	return 0;
    }
  
    /*
     * filter close
     */

    if (ptr->tag & TC_FILTER_CLOSE) {
	
	int total = myf->numFrames - myf->unknownFrames;
	int totalfields = myf->topFirstFrames + myf->bottomFirstFrames;

	/* Cleanup */
	free (myf->lumIn);
	free (myf->lumPrev);
	free (myf->lumInT);
	free (myf->lumInB);
	free (myf->lumPrevT);
	free (myf->lumPrevB);
	myf->lumIn = myf->lumPrev = myf->lumInT = myf->lumInB =
	    myf->lumPrevT = myf->lumPrevB = NULL;
	
	/* Output results */
	if (totalfields < 1)
	    totalfields = 1;
	printf ("\n"
		"[%s] RESULTS: Frames:      %d (100%%)  Unknown:      %d (%.3g%%)\n"
		"[%s] RESULTS: Progressive: %d (%.3g%%)  Interlaced:   %d (%.3g%%)\n"
		"[%s] RESULTS: FieldShift:  %d (%.3g%%)  Telecined:    %d (%.3g%%)\n"
		"[%s] RESULTS: MajorField: TopFirst %d (%.3g%%)  BottomFirst %d (%.3g%%)\n",
		MOD_NAME, myf->numFrames,
		myf->unknownFrames,
		100.0 * myf->unknownFrames / (double)myf->numFrames,
		MOD_NAME, myf->progressiveFrames,
		100.0 * myf->progressiveFrames / (double)myf->numFrames,
		myf->interlacedFrames,
		100.0 * myf->interlacedFrames / (double)myf->numFrames,
		MOD_NAME, myf->fieldShiftFrames,
		100.0 * myf->fieldShiftFrames / (double)myf->numFrames,
		myf->telecineFrames,
		100.0 * myf->telecineFrames / (double)myf->numFrames,
		MOD_NAME, myf->topFirstFrames, 100.0 * myf->topFirstFrames / (double)totalfields,
		myf->bottomFirstFrames,        100.0 * myf->bottomFirstFrames / (double)totalfields);

	if (total < 50)
	    printf ("\n[%s] less than 50 frames analyzed correctly, no conclusion.\n\n",
		    MOD_NAME);
	else if (myf->unknownFrames * 10 > myf->numFrames * 9)
	    printf ("\n[%s] less than 10%% frames analyzed correctly, no conclusion.\n\n", MOD_NAME);
	else if (myf->progressiveFrames * 8 > total * 7)
	    printf ("\n[%s] CONCLUSION: progressive video.\n\n", MOD_NAME);
	else if (myf->topFirstFrames * 8 > myf->bottomFirstFrames &&
		 myf->bottomFirstFrames * 8 > myf->topFirstFrames)
	    printf ("\n[%s] major field unsure, no conclusion. Use deinterlacer for processing.\n\n", MOD_NAME);
	else if (myf->telecineFrames * 4 > total * 3)
	    printf ("\n[%s] CONCLUSION: telecined video, %s field first.\n\n", MOD_NAME, myf->topFirstFrames > myf->bottomFirstFrames ? "top" : "bottom");
	else if (myf->fieldShiftFrames * 4 > total * 3)
	    printf ("\n[%s] CONCLUSION: field shifted progressive video, %s field first.\n\n", MOD_NAME, myf->topFirstFrames > myf->bottomFirstFrames ? "top" : "bottom");
	else if (myf->interlacedFrames > myf->fieldShiftFrames &&
		 (myf->interlacedFrames+myf->fieldShiftFrames) * 8 > total * 7)
	    printf ("\n[%s] CONCLUSION: interlaced video, %s field first.\n\n", MOD_NAME, myf->topFirstFrames > myf->bottomFirstFrames ? "top" : "bottom");
	else
	    printf ("\n[%s] mixed video, no conclusion. Use deinterlacer for processing.\n\n", MOD_NAME);

	return 0;
    }
  
    /*
     * filter description
     */
    if (ptr->tag & TC_FILTER_GET_CONFIG) {
	char buf[255];
	optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION,
			    MOD_AUTHORS, "VRY4E", "2");
	sprintf (buf, "%g", myf->interlaceDiff);
	optstr_param (options, "interlacediff", "Minimum temporal inter-field difference for detecting interlaced video", "%f", buf, "1.0", "inf");
	sprintf (buf, "%g", myf->unknownDiff);
	optstr_param (options, "unknowndiff", "Maximum inter-frame change vs. detail differences for neglecting interlaced video", "%f", buf, "1.0", "inf");
	sprintf (buf, "%g", myf->progressiveDiff);
	optstr_param (options, "progressivediff", "Minimum inter-frame change vs. detail differences for detecting progressive video" ,"%f", buf, "unknowndiff", "inf");
	sprintf (buf, "%g", myf->progressiveChange);
	optstr_param (options, "progressivechange", "Minimum temporal change needed for detecting progressive video" ,"%f", buf, "0", "inf");
	sprintf (buf, "%g", myf->changedIfMore);
	optstr_param (options, "changedifmore", "Minimum temporal change for detecting truly changed frames" ,"%f", buf, "0", "65025");
	sprintf (buf, "%d", myf->forceTelecineDetect);
	optstr_param (options, "forcetelecinedetect", "Detect telecine even on non-NTSC (29.97fps) video", "%d", buf, "0", "1");
	sprintf (buf, "%d", myf->verbose);
	optstr_param (options, "verbose", "Output analysis for every frame", "%d", buf, "0", "2");
	sprintf (buf, "%d", myf->outDiff);
	optstr_param (options, "outdiff", "Output internal debug frames as luminance of YUV video (see source)", "%d", buf, "0", "11");
    }
    
    /*
     * filter frame routine
     */
    /* need to process frames in-order */
    if ((ptr->tag & TC_PRE_S_PROCESS) && (ptr->tag & TC_VIDEO)) {

	uint8_t *tmp;
	int i, j;

	assert (ptr->free == 0 || ptr->free == 1);
	assert (ptr->video_buf_Y[!ptr->free] == ptr->video_buf);
 
	/* Convert / Copy to luminance only */
	switch (myf->codec) {
	case CODEC_RGB:
	    rgbtoy (ptr->video_buf, myf->lumIn, myf->width, myf->height);
	    break;
	case CODEC_YUY2:
	    yuy2toy (ptr->video_buf, myf->lumIn, myf->width, myf->height);
	    break;
	case CODEC_YUV:
	    tc_memcpy (myf->lumIn, ptr->video_buf, myf->size);
	    break;
	case CODEC_YUV422:
	    uyvytoy (ptr->video_buf, myf->lumIn, myf->width, myf->height);
	    break;
	default:
	    assert (0);
	}

	/* Bob Top field */
	bob_field (myf->lumIn, myf->lumInT, myf->width, myf->height/2-1);
	/* Bob Bottom field */
	tc_memcpy (myf->lumInB, myf->lumIn + myf->width, myf->width);
	bob_field (myf->lumIn + myf->width, myf->lumInB + myf->width,
	           myf->width, myf->height/2-1);
	/* last copied line is ignored, buffer is large enough */

	if (myf->numFrames == 0)
	    myf->numFrames++;
	else if (! (ptr->tag & TC_FRAME_IS_SKIPPED)) {
	    /* check_it */
	    check_interlace (myf, ptr->id);
	}

	/* only works with YUV data correctly */
	switch (myf->outDiff) {
	case 1:				/* lumIn */
	    tc_memcpy (ptr->video_buf, myf->lumIn, myf->size);
	    break;
	case 2:				/* field shift */
	    for (i = 0 ; i < myf->height-2; i += 2)
		for (j = 0; j < myf->width; j++) {
		    ptr->video_buf [myf->width*i+j] =
			myf->lumIn [myf->width*i+j];
		    ptr->video_buf [myf->width*(i+1)+j] =
			myf->lumPrev [myf->width*(i+1)+j];
		}
	    break;
	case 3:				/* lumInT */
	    tc_memcpy (ptr->video_buf, myf->lumInT, myf->size);
	    break;
	case 4:				/* lumInB */
	    tc_memcpy (ptr->video_buf, myf->lumInB, myf->size);
	    break;
	case 5:				/* lumPrevT */
	    tc_memcpy (ptr->video_buf, myf->lumPrevT, myf->size);
	    break;
	case 6:				/* lumPrevB */
	    tc_memcpy (ptr->video_buf, myf->lumPrevB, myf->size);
	    break;
	case 7:				/* pixDiff */
	    pic_diff (myf->lumInT, myf->lumInB,   ptr->video_buf, myf->size,4);
	    break;
	case 8:				/* pixShiftChangedT */
	    pic_diff (myf->lumInT, myf->lumPrevB, ptr->video_buf, myf->size,4);
	    break;
	case 9:				/* pixShiftChangedB */
	    pic_diff (myf->lumInB, myf->lumPrevT, ptr->video_buf, myf->size,4);
	    break;
	case 10:			/* pixLastT */
	    pic_diff (myf->lumInT, myf->lumPrevT, ptr->video_buf, myf->size,4);
	    break;
	case 11:			/* pixLastB */
	    pic_diff (myf->lumInB, myf->lumPrevB, ptr->video_buf, myf->size,4);
	    break;
	}
	
	/* The current frame gets the next previous frame :-P */
	tmp = myf->lumPrev;   myf->lumPrev  = myf->lumIn;   myf->lumIn  = tmp;
	tmp = myf->lumPrevT;  myf->lumPrevT = myf->lumInT;  myf->lumInT = tmp;
	tmp = myf->lumPrevB;  myf->lumPrevB = myf->lumInB;  myf->lumInB = tmp;
    } 
  
    return 0;
}

