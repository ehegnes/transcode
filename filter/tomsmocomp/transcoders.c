/*
 *  transcoders.c
 *
 *  Several parts of transcode for (trivially) converting color spaces.
 *  See vid_aux.* and decode_dv.* for licensing details.
 */

void uyvytoyuy2(char *input, char *output, int width, int height)
{

    int i;
    
    for (i=0; i<width*height*2; i+=4) {

      /* packed YUV 4:2:2 is Y[i] U[i] Y[i+1] V[i] (YUY2)*/
      /* packed YUV 4:2:2 is U[i] Y[i] V[i] Y[i+1] (UYVY)*/
        output[i] = input[i+1];
        output[i+1] = input[i];
        output[i+2] = input[i+3];
        output[i+3] = input[i+2];
    }


}

void yv12toyuy2(char *_y, char *_u, char *_v, char *output, int width, int height)
{

    int i,j;
    char *y, *u, *v;

    y = _y;
    v = _v;
    u = _u;
    
    for (i=0; i<height; i+=2) {

      /* packed YUV 4:2:2 is Y[i] U[i] Y[i+1] V[i] */

      for (j=0; j<width/2; j++) {
        *(output++) = *(y++);
        *(output++) = *(u++);
        *(output++) = *(y++);
        *(output++) = *(v++);
      }
      
      //upsampling requires doubling chroma compoments (simple method)
      
      u-=width/2;
      v-=width/2;
      
      for (j=0; j<width/2; j++) {
        *(output++) = *(y++);
        *(output++) = *(u++);
        *(output++) = *(y++);
        *(output++) = *(v++);
      }
    }      
}

void yuy2toyv12(unsigned char *_y, unsigned char *_u, unsigned char *_v, unsigned char *input, int width, int height)
{

    int i,j,w2;
    unsigned char *y, *u, *v;

    w2 = width/2;

    //I420
    y = _y;
    v = _v;
    u = _u;
    
    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
        
        /* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
        *y++ = *input++;
        *u++ = *input++;
        *y++ = *input++;
        *v++ = *input++;
   

      }
      
      //down sampling
#if 1    /* box filter */
      u -= w2;
      v -= w2;
      
      for (j=0; j<w2; j++) {
	  /* average U and V */
	  *y++ = *input++;
	  *u = (((unsigned int) *u) + *input++) >>1;
	  *y++ = *input++;
	  *v = (((unsigned int) *v) + *input++) >>1;
	  u++;  v++;
      }
#else   /* nearest neighbor filter */
      for (j=0; j<w2; j++) {
	  /* skip U and V */
	  *y++ = *input++;
	  input++;
	  *y++ = *input++;
	  input++;
      }
#endif
    }
}

void yuy2touyvy(char *dest, char *src, int width, int height)
{

    int i;

    for (i=0; i<width*height*2; i+=4) {

        /* packed YUV 4:2:2 is Y[i] U[i] Y[i+1] V[i] (YUY2)*/
        /* packed YUV 4:2:2 is U[i] Y[i] V[i] Y[i+1] (UYVY)*/

        dest[i] = src[i+1];
        dest[i+1] = src[i];
        dest[i+2] = src[i+3];
        dest[i+3] = src[i+2];
    }
}

