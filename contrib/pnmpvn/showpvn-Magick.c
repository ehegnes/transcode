/*
  Display PVN (PVB/PVG/PVP) files on the X server using ImageMagic.

  By Jacob (Jack) Gryn
*/
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <magick/api.h>
#include "pvnglobals.h"
#include "pvn.h"

#if defined(__cplusplus) || defined(c_plusplus)
#undef class
#endif

int main(int argc,char **argv)
{
  FILE *in;
  PVNParam inParams, tmpParams;
 
  Image *images, *curImage;
  ImageInfo *image_info;
  unsigned int i=0, j=0, k=0, prec_bytes;
  unsigned long bufSize, tmpBufSize;
  unsigned long temp_val;
  unsigned char *buf, *tmpBuf;
  register PixelPacket *q, *orig_q;
  char *filename;
  double mult;
  unsigned int columns;

  if (argc != 2)
  {
    printf("Syntax:\n  %s filename.pvn\n", argv[0]);
    exit(1);
  }

  filename = argv[1];

  if ((in = fopen(argv[1], "rb")) == NULL)
  {
      fprintf(stderr, "Error opening file %s for read\n", argv[1]);
      exit(OPENERROR);
  }

  if (readPVNHeader(in, &inParams) != VALID)
  {
    return(ERROR);
  }

  if((inParams.magic[3] != 'a') && (inParams.magic[3] != 'b') && (inParams.magic[3] != 'f') && (inParams.magic[3] != 'd'))
  {
    fprintf(stderr, "For the time being, only type 'a', 'b', 'f' and 'd' PVN files are supported!\n");
    exit(1);
  }

  if(inParams.framerate==0)
  {
    printf("Setting Frame Rate to default of 15\n");
    inParams.framerate=15;
  }
  /*
    Allocate image structure.
  */
  InitializeMagick(*argv);
  image_info=CloneImageInfo((ImageInfo *) NULL);
  bufSize=calcPVNPageSize(inParams);

  buf=(unsigned char *)malloc(bufSize);

//  for(i=0; i< inParams.depth; i++)
  while(fread(buf,bufSize,1,in) != 0)
  {
    if(i==0)
    {
      images=AllocateImage(image_info);
      curImage = images;
      if(inParams.depth == 0)
        curImage->next=NULL;
    }
    else if (inParams.depth != 0)
    {
      AllocateNextImage(image_info, curImage);
      curImage = curImage->next;
    }

    if (curImage == (Image *) NULL)
      MagickError(ResourceLimitError,"Unable to display image",
        "MemoryAllocationFailed");
    /*
      Initialize image.
    */

    if ((inParams.depth != 0) || (i == 0))
    {
      curImage->columns=inParams.width;
      curImage->rows=inParams.height;
      curImage->delay=100/inParams.framerate;
      strcpy(curImage->filename, filename);
      /* read image */

      q=SetImagePixels(curImage,0,0,curImage->columns,curImage->rows);
      if (q == (PixelPacket *) NULL)
        break;
      orig_q=q;
    }
    else
      q=orig_q;

    if(inParams.magic[3] == 'f')
    {
      PVNParamCopy(&tmpParams, &inParams);
      tmpParams.magic[3]='a';
      tmpParams.maxcolour=log(MaxRGB+1)/log(2);
      tmpBufSize=calcPVNPageSize(tmpParams);
      tmpBuf=(unsigned char *)malloc(tmpBufSize);

      if(bufConvert(buf, bufSize, FORMAT_FLOAT, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(in);
        free(buf);
        free(tmpBuf);
        return(ERROR);
      }
    }
    else if(inParams.magic[3] == 'd')
    {
      PVNParamCopy(&tmpParams, &inParams);
      tmpParams.magic[3]='a';
      tmpParams.maxcolour=log(MaxRGB+1)/log(2);
      tmpBufSize=calcPVNPageSize(tmpParams);
      tmpBuf=(unsigned char *)malloc(tmpBufSize);
      if(bufConvert(buf, bufSize, FORMAT_DOUBLE, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(in);
        free(buf);
        free(tmpBuf);
        return(ERROR);
      }
    }
    else if(inParams.magic[3] == 'b')
    {
      PVNParamCopy(&tmpParams, &inParams);
      tmpParams.magic[3]='a';
      tmpBufSize=calcPVNPageSize(tmpParams);
      tmpBuf=(unsigned char *)malloc(tmpBufSize);
      if(bufConvert(buf, bufSize, FORMAT_INT, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(in);
        free(buf);
        free(tmpBuf);
        return(ERROR);
      }
    }
    else
    {
      tmpParams=inParams;
      tmpBuf=buf;
    }

    prec_bytes=(int)tmpParams.maxcolour/8;

    if(tmpParams.magic[2] == '4')
      prec_bytes=1;

    mult=(MaxRGB+1)/pow(2,tmpParams.maxcolour);

    if(tmpParams.magic[2] == '4')
    {
      columns=ceil(curImage->columns/8.0)*8;
    }
    else
      columns=curImage->columns;

    for(j=0; j < columns*curImage->rows; j++)
    {
      if(tmpParams.magic[2] == '4')
      {
        if ((j % columns) < curImage->columns)
        {
          temp_val=tmpBuf[j/8] >> (7-(j%8));
          temp_val &= 1;

          /* not sure why, but bits are NOT'ed to get correct values,
             0 = white, 1 = black?? */
          temp_val = 1 - temp_val;
          q->red=MaxRGB*temp_val;
          q->green=MaxRGB*temp_val;
          q->blue=MaxRGB*temp_val;
        }
        else
          q--;
      }
      else if(tmpParams.magic[2] == '5')
      {
        temp_val=0;
        for(k=0;k<prec_bytes;k++)
        {
          temp_val *= 256;
          temp_val+=tmpBuf[prec_bytes*j+k];
        }
        temp_val *= mult;

        q->red=temp_val;
        q->green=temp_val;
        q->blue=temp_val;
      }
      else if(tmpParams.magic[2] == '6')
      {
        temp_val=0;
        for(k=0;k<prec_bytes;k++)
        {
          temp_val *= 256;
          temp_val+=tmpBuf[prec_bytes*3*j+k];
        }
        temp_val *= mult;
        q->red=temp_val;

        temp_val=0;
        for(k=0;k<prec_bytes;k++)
        {
          temp_val *= 256;
          temp_val+=tmpBuf[prec_bytes*3*j+1*prec_bytes+k];
        }
        temp_val *= mult;
        q->green=temp_val;

        temp_val=0;
        for(k=0;k<prec_bytes;k++)
        {
          temp_val *= 256;
          temp_val+=tmpBuf[prec_bytes*3*j+2*prec_bytes+k];
        }
        temp_val *= mult;
        q->blue=temp_val;
      }
      q++;
    }

    if (!SyncImagePixels(curImage))
      break;

    if(inParams.depth == 0)
      DisplayImages(image_info,images);
    i++;
  }
  fclose(in);
  free(buf);
  if ((inParams.magic[3] == 'd') || (inParams.magic[3] == 'f'))
  {
    free(tmpBuf);
  }

  /*
    Animate images.
  */
  if(inParams.depth != 0)
    if(!AnimateImages(image_info,images))
    {
      fprintf(stderr, "%s: Unable to open X server (%s).\n", argv[0], getenv("DISPLAY"));
    }

  /*
    Free resources.
  */
  DestroyImage(images);
  DestroyImageInfo(image_info);
  DestroyMagick();
  exit(0);
}
