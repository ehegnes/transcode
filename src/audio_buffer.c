/*
 *  audio_buffer.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "transcode.h"
#include "framebuffer.h"
#include "frame_threads.h"

pthread_mutex_t aframe_list_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t aframe_list_full_cv=PTHREAD_COND_INITIALIZER;

aframe_list_t *aframe_list_head;
aframe_list_t *aframe_list_tail;

static int aud_buf_max = 0;
static int aud_buf_next = 0;

static int aud_buf_fill  =0;
static int aud_buf_ready =0;
static int aud_buf_locked=0;
static int aud_buf_empty =0;
static int aud_buf_wait  =0;

static aframe_list_t **aud_buf_ptr; char *aud_buf_mem, **aud_buf_vid;

/* ------------------------------------------------------------------ */

static unsigned char *bufalloc(int n, size_t size)
{

#ifdef HAVE_GETPAGESIZE
   unsigned long buffer_align=getpagesize();
#else
   unsigned long buffer_align=0;
#endif

   char *buf = malloc(size + buffer_align);

   unsigned long adjust;

   if (buf == NULL) {
       fprintf(stderr, "(%s) out of memory", __FILE__);
   }
   
   adjust = buffer_align - ((unsigned long) buf) % buffer_align;

   if (adjust == buffer_align)
      adjust = 0;

   aud_buf_vid[n] = buf;

   return (unsigned char *) (buf + adjust);
}


/* ------------------------------------------------------------------ */

static int aud_buf_alloc(int ex_num)
{
    
    /* objectives: 
       ===========
       
       allocate memory for ringbuffer structure
       return -1 on failure, 0 on success
       
    */
    
    int n, num;

    if(ex_num < 0) return(-1);
    
    num = ex_num + 1; //alloc at least one buffer
    
    if((aud_buf_ptr = (aframe_list_t **) calloc(num, sizeof(aframe_list_t *)))==NULL) {
      perror("out of memory");
      return(-1);
    }
    
    if((aud_buf_mem = (char *) calloc(num, sizeof(aframe_list_t)))==NULL) {
      perror("out of memory");
      return(-1);
    }
    
    if((aud_buf_vid = (char **) calloc(num, sizeof(char *)))==NULL) {
      perror("out of memory");
      return(-1);
    }
    
    // init ringbuffer
    for (n=0; n<num; ++n) {
	aud_buf_ptr[n] = (aframe_list_t *) (aud_buf_mem + n * sizeof(aframe_list_t));
	
	aud_buf_ptr[n]->status = FRAME_NULL;
	aud_buf_ptr[n]->bufid = n;

	//allocate extra video memory:
	if((aud_buf_ptr[n]->audio_buf=bufalloc(n, (SIZE_PCM_FRAME)))==NULL) {
	  perror("out of memory");
	  return(-1);
	}
	
    }
    
    // assign to static
    aud_buf_max = num;
    
    return(0);
}

    
    
    
/* ------------------------------------------------------------------ */

static void aud_buf_free()
{
    
    /* objectives: 
       ===========
       
       free memory for ringbuffer structure
       
    */

  int n;
  
  if(aud_buf_max > 0) {

    for (n=0; n<aud_buf_max; ++n) {
      free(aud_buf_vid[n]);
    }
    free(aud_buf_mem);
    free(aud_buf_ptr);
  }
}
    
/* ------------------------------------------------------------------ */

static aframe_list_t *aud_buf_retrieve()
{
    
    /* objectives: 
       ===========

       retrieve a valid pointer to a aframe_list_t structure
       return NULL on failure, valid pointer on success

       thread safe
       
    */
    
    aframe_list_t *ptr;
    int i=0;

    ptr = aud_buf_ptr[aud_buf_next];
   
    // find an unused ptr, the next one may already be busy
    while (ptr->status != FRAME_NULL && i < aud_buf_max) {
	++i;
	++aud_buf_next;
	aud_buf_next %= aud_buf_max;
	ptr = aud_buf_ptr[aud_buf_next];
    }

    // check, if this structure is really free to reuse

    if(ptr->status != FRAME_NULL) {
      if(verbose & TC_FLIST) fprintf(stderr, "(%s) A buffer=%d not empty\n", __FILE__, ptr->status);
      return(NULL);
    }
    
    // ok
    if(verbose & TC_FLIST) printf("A alloc  =%d [%d]\n", aud_buf_next, ptr->bufid);

    ++aud_buf_next;
    aud_buf_next %= aud_buf_max;
    
    return(ptr);
}



/* ------------------------------------------------------------------ */

static int aud_buf_release(aframe_list_t *ptr)
{
  
  /* objectives: 
     ===========
     
     release a valid pointer to a aframe_list_t structure
     return -1 on failure, 0 on success
     
     thread safe
     
  */
  
  // instead of freeing the memory and setting the pointer
  // to NULL we only change a flag
  
  if(ptr == NULL) return(-1);
  
  if(ptr->status != FRAME_EMPTY) {
    fprintf(stderr, "A (%s) internal error (%d)\n", __FILE__, ptr->status);
    return(-1);
  } else {
    
    if(verbose & TC_FLIST) printf("A release=%d [%d]\n", aud_buf_next, ptr->bufid);
    ptr->status = FRAME_NULL;
  }
  
  return(0);
}


/* ------------------------------------------------------------------ */

int aframe_alloc(int ex_num)
{
  return(aud_buf_alloc(ex_num));
}

void aframe_free()
{
  aud_buf_free();
}

/* ------------------------------------------------------------------ */

aframe_list_t *aframe_register(int id)

{
  
  /* objectives: 
     ===========

     register new frame

     allocate space for frame buffer and establish backward reference
     
     requirements:
     =============

     thread-safe

     global mutex: aframe_list_lock
     
  */

  aframe_list_t *ptr;

  pthread_mutex_lock(&aframe_list_lock);

  // retrive a valid pointer from the pool
  
#ifdef STATBUFFER
  if(verbose & TC_FLIST) printf("A frameid=%d\n", id);
  if((ptr = aud_buf_retrieve()) == NULL) {
    pthread_mutex_unlock(&aframe_list_lock);
    return(NULL);
  }
#else 
  if((ptr = malloc(sizeof(aframe_list_t))) == NULL) {
    pthread_mutex_unlock(&aframe_list_lock);
    return(NULL);
  }
#endif
    
  ptr->status = FRAME_EMPTY;
  ++aud_buf_empty;
  
  ptr->next = NULL;
  ptr->prev = NULL;
  
  ptr->id  = id;

 if(aframe_list_tail != NULL)
    {
      aframe_list_tail->next = ptr;
      ptr->prev = aframe_list_tail;
    }
  
  aframe_list_tail = ptr;

  /* first frame registered must set aframe_list_head */

  if(aframe_list_head == NULL) aframe_list_head = ptr;

  // adjust fill level
  ++aud_buf_fill;
  
  if(verbose & TC_FLIST) fprintf(stderr, "A+  f=%d e=%d w=%d l=%d r=%d \n", aud_buf_fill, aud_buf_empty, aud_buf_wait, aud_buf_locked, aud_buf_ready);
 
  pthread_mutex_unlock(&aframe_list_lock);
  
  return(ptr);

}

/* ------------------------------------------------------------------ */

void aframe_copy_payload(aframe_list_t *dst, aframe_list_t *src)
{
    if (!dst || !src)
	return;

    // we can't use memcpy here because we don't want
    // to overwrite the pointers to alloc'ed mem

    dst->bufid = src->bufid;
    dst->tag = src->tag;
    dst->filter_id = src->filter_id;
    dst->a_codec = src->a_codec;
    dst->id = src->id;
    dst->status = src->status;
    dst->attributes = src->attributes;
    dst->thread_id = src->thread_id;
    dst->a_rate = src->a_rate;
    dst->a_bits = src->a_bits;
    dst->a_chan = src->a_chan;
    dst->audio_size = src->audio_size;

    // copy video data
    memcpy(dst->audio_buf, src->audio_buf, dst->audio_size);
}

/* ------------------------------------------------------------------ */

aframe_list_t *aframe_dup(aframe_list_t *f)

{
  
  /* objectives: 
     ===========

     duplicate a frame (for cloning) 

     insert a ptr after f;
     
     requirements:
     =============

     thread-safe

     global mutex: vframe_list_lock
     
  */

  aframe_list_t *ptr;


  pthread_mutex_lock(&aframe_list_lock);

  //fprintf(stderr, "Duplicating (%d)\n", f->id);
  if (!f) {
      fprintf(stderr, "Hmm, 1 cannot find a free slot (%d)\n", f->id);
      pthread_mutex_unlock(&aframe_list_lock);
      return (NULL);
  }

  // retrive a valid pointer from the pool
  
#ifdef STATBUFFER
  if((ptr = aud_buf_retrieve()) == NULL) {
    pthread_mutex_unlock(&aframe_list_lock);
    fprintf(stderr, "(%s) cannot find a free slot (%d)\n", __FILE__, f->id);
    return(NULL);
  }
#else 
  if((ptr = malloc(sizeof(aframe_list_t))) == NULL) {
    pthread_mutex_unlock(&aframe_list_lock);
    return(NULL);
  }
#endif
  
  aframe_copy_payload (ptr, f);

  ptr->status = FRAME_WAIT;
  ++aud_buf_wait;

  ptr->next = NULL;
  ptr->prev = NULL;
  
  // insert after ptr
  ptr->next = f->next;
  f->next = ptr;
  ptr->prev = f;

 if(!ptr->next) {
     // must be last ptr in the list
     aframe_list_tail = ptr;
 }
  
  // adjust fill level
  ++aud_buf_fill;
  
  pthread_mutex_unlock(&aframe_list_lock);
  
  return(ptr);

}

/* ------------------------------------------------------------------ */
 
void aframe_remove(aframe_list_t *ptr)

{
  
  /* objectives: 
     ===========

     remove frame from chained list

     requirements:
     =============

     thread-safe
     
  */

  
  if(ptr == NULL) return;         // do nothing if null pointer

  pthread_mutex_lock(&aframe_list_lock);
  
  if(ptr->prev != NULL) (ptr->prev)->next = ptr->next;
  if(ptr->next != NULL) (ptr->next)->prev = ptr->prev;
  
  if(ptr == aframe_list_tail) aframe_list_tail = ptr->prev;
  if(ptr == aframe_list_head) aframe_list_head = ptr->next;

  if(ptr->status == FRAME_READY)  --aud_buf_ready;
  if(ptr->status == FRAME_LOCKED) --aud_buf_locked;

  // release valid pointer to pool
  ptr->status = FRAME_EMPTY;
  ++aud_buf_empty;

#ifdef STATBUFFER
  aud_buf_release(ptr);
#else
  free(ptr);
#endif
  
  // adjust fill level
  --aud_buf_empty;
  --aud_buf_fill;
  
  if(verbose & TC_FLIST) fprintf(stderr, "A-  f=%d e=%d w=%d l=%d r=%d \n", aud_buf_fill, aud_buf_empty, aud_buf_wait, aud_buf_locked, aud_buf_ready);
  
  pthread_mutex_unlock(&aframe_list_lock); 
  
}

/* ------------------------------------------------------------------ */

 
void aframe_flush()

{
  
  /* objectives: 
     ===========

     remove all frame from chained list

     requirements:
     =============

     thread-safe
     
  */

  aframe_list_t *ptr;
  
  int cc=0;
  
  while((ptr=aframe_retrieve())!=NULL) {
    if(verbose & TC_STATS) fprintf(stderr, "flushing audio buffers\n"); 
    aframe_remove(ptr);
    ++cc;
  }
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) flushing %d audio buffer\n", __FILE__, cc); 
  
  pthread_mutex_lock(&abuffer_im_fill_lock);
  abuffer_im_fill_ctr=0;
  pthread_mutex_unlock(&abuffer_im_fill_lock);  
  
  pthread_mutex_lock(&abuffer_ex_fill_lock);
  abuffer_ex_fill_ctr=0;
  pthread_mutex_unlock(&abuffer_ex_fill_lock);
  
  pthread_mutex_lock(&abuffer_xx_fill_lock);
  abuffer_xx_fill_ctr=0;
  pthread_mutex_unlock(&abuffer_xx_fill_lock);
  
  return;
}


/* ------------------------------------------------------------------ */


aframe_list_t *aframe_retrieve()

{

  /* objectives: 
     ===========

     get pointer to next frame for rendering
     
     requirements:
     =============
     
     thread-safe
     
  */
  
  aframe_list_t *ptr;

  pthread_mutex_lock(&aframe_list_lock);

  ptr = aframe_list_head;

  /* move along the chain and check for status */

  while(ptr != NULL)
  {
      // we cannot skip a locked frame, since
      // we have to preserve order in which frames are encoded
      if(ptr->status == FRAME_LOCKED)
      {
	  pthread_mutex_unlock(&aframe_list_lock);
	  return(NULL);
      }
  
      //this frame is ready to go
      if(ptr->status == FRAME_READY) 
      {
	pthread_mutex_unlock(&aframe_list_lock);
	  return(ptr);
      }
      ptr = ptr->next;
  }
  
  pthread_mutex_unlock(&aframe_list_lock);
  
  return(NULL);
}

/* ------------------------------------------------------------------ */


aframe_list_t *aframe_retrieve_status(int old_status, int new_status)

{

  /* objectives: 
     ===========

     get pointer to next frame for rendering
     
     requirements:
     =============
     
     thread-safe
     
  */
  
  aframe_list_t *ptr;

  pthread_mutex_lock(&aframe_list_lock);
  
  ptr = aframe_list_head;
  
  /* move along the chain and check for status */
  
  while(ptr != NULL)
    {
      if(ptr->status == old_status) 
	{

	  // found matching frame

	  if(ptr->status==FRAME_WAIT)   --aud_buf_wait;
	  if(ptr->status==FRAME_READY)  --aud_buf_ready;
	  if(ptr->status==FRAME_LOCKED) --aud_buf_locked;	  
	  
	  ptr->status = new_status;
	  
	  if(ptr->status==FRAME_WAIT)   ++aud_buf_wait;
	  if(ptr->status==FRAME_READY)  ++aud_buf_ready;
	  if(ptr->status==FRAME_LOCKED) ++aud_buf_locked;	  

	  pthread_mutex_unlock(&aframe_list_lock);
	  
	  return(ptr);
	}
      ptr = ptr->next;
    }
  
  pthread_mutex_unlock(&aframe_list_lock);
  
  return(NULL);
}


/* ------------------------------------------------------------------ */


void aframe_set_status(aframe_list_t *ptr, int status)

{

  /* objectives: 
     ===========

     get pointer to next frame for rendering
     
     requirements:
     =============

  */

    if(ptr == NULL) return;
  
    pthread_mutex_lock(&aframe_list_lock);
    
    if(ptr->status==FRAME_READY)  --aud_buf_ready;
    if(ptr->status==FRAME_EMPTY)  --aud_buf_empty;
    if(ptr->status==FRAME_LOCKED) --aud_buf_locked;
    if(ptr->status==FRAME_WAIT)   --aud_buf_wait;

    ptr->status = status;

    if(ptr->status==FRAME_WAIT)   ++aud_buf_wait;
    if(ptr->status==FRAME_EMPTY)  ++aud_buf_empty;
    if(ptr->status==FRAME_READY)  ++aud_buf_ready;
    if(ptr->status==FRAME_LOCKED) ++aud_buf_locked;

    pthread_mutex_unlock(&aframe_list_lock);
	
    return;
}

/* ------------------------------------------------------------------ */

void aframe_fill_print(int r)
{
    
  fprintf(stderr, "(A) fill=%d/%d, empty=%d wait=%d locked=%d, ready=%d, tag=%d\n", aud_buf_fill, aud_buf_max, aud_buf_empty, aud_buf_wait, aud_buf_locked, aud_buf_ready, r);
}     

   
/* ------------------------------------------------------------------ */


int aframe_fill_level(int status)
{

  if(verbose & TC_STATS) aframe_fill_print(status);
  
  //user has to lock aframe_list_lock to obtain a proper result
  
  if(status==TC_BUFFER_FULL  && aud_buf_fill >= aud_buf_max-1) return(1);
  if(status==TC_BUFFER_READY && aud_buf_ready>0) return(1);
  if(status==TC_BUFFER_EMPTY && aud_buf_fill==0) return(1);
  if(status==TC_BUFFER_LOCKED && aud_buf_locked>0) return(1);

  return(0);
}
