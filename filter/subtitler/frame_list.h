#ifndef _FRAME_LIST_H_
#define _FRAME_LIST_H_

#define FRAME_HASH_SIZE	MAX_FRAMES

struct frame
	{
	char *name;

	int type;

	int end_frame;

	int xsize;
	int ysize;
	int zsize;

	char *data;

	font_desc_t *pfd;

	int id;

	int status;
	
	struct frame *nxtentr;
	struct frame *prventr;
	};
struct frame *frametab[FRAME_HASH_SIZE];


#endif //_FRAME_LIST_H

