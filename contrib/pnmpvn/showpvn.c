/*
 * Display PVN files
 * by Jacob (Jack) Gryn
 * Small portions by Andrew Hogue
 */
#include <stdio.h>
#ifndef _MSC_VER
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
#else
  #include <windows.h>
#endif
#include "pvnglobals.h"
#include "pvn.h"
#include <GL/glut.h>

unsigned long images_read=0;
unsigned int winWidth,winHeight;
GLenum imgFormat;
GLenum imgType;
GLvoid *imgPixels=NULL;

int window_gl;
typedef void (*d_p)(void);
int pixel_zoom = 1;
int currentframe=0;
char fname[255];

  char *filename;
  FILE *in;
  long postHeaderPos;
  unsigned int frameTime;
  PVNParam inParams, tmpParams;
  unsigned int i, j, k, prec_bytes;
  unsigned long bufSize, tmpBufSize;
  unsigned long temp_val;
  unsigned char *buf, *tmpBuf;


void display(void)
{
  if(imgPixels != NULL)
  {
    glRasterPos2i(0,0);
    glViewport(0,0,winWidth, winHeight);
    glDrawPixels(winWidth,winHeight,imgFormat,imgType,imgPixels);
    glutSwapBuffers();
    glFlush();
  }
  else
  {
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
  }
}

void readFrame(int value)
{
  if (fread(buf,bufSize,1,in) != 0)
  {
    images_read++;
    if(inParams.magic[3] == 'f')
    {
      PVNParamCopy(&tmpParams, &inParams);
      tmpParams.magic[3]='a';
      tmpParams.maxcolour=8;
      tmpBufSize=calcPVNPageSize(tmpParams);
      tmpBuf=(unsigned char *)malloc(tmpBufSize);
      if(bufConvert(buf, bufSize, FORMAT_FLOAT, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(in);
        free(buf);
        free(tmpBuf);
        _exit(ERROR);
      }
    }
    else if(inParams.magic[3] == 'd')
    {
      PVNParamCopy(&tmpParams, &inParams);
      tmpParams.magic[3]='a';
      tmpParams.maxcolour=8;
      tmpBufSize=calcPVNPageSize(tmpParams);
      tmpBuf=(unsigned char *)malloc(tmpBufSize);
      if(bufConvert(buf, bufSize, FORMAT_DOUBLE, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(in);
        free(buf);
        free(tmpBuf);
        _exit(ERROR);
      }
    }
    else if(inParams.magic[3] == 'a')
    {
      if(inParams.maxcolour != 8)
      {
        PVNParamCopy(&tmpParams, &inParams);
        tmpParams.magic[3]='a';
        tmpParams.maxcolour=8;
        if (tmpParams.magic[2] == '4')
          tmpParams.magic[2]='5';
        tmpBufSize=calcPVNPageSize(tmpParams);
        tmpBuf=(unsigned char *)malloc(tmpBufSize);
        if (inParams.magic[2] == '4')
        {
          if(bufConvert(buf, bufSize, FORMAT_BIT, inParams.width, tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
          {
            fprintf(stderr, "Buffer conversion error!\n");
            fclose(in);
            free(buf);
            free(tmpBuf);
            _exit(ERROR);
          }
        }
        else
        {
          if(bufConvert(buf, bufSize, FORMAT_UINT, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
          {
            fprintf(stderr, "Buffer conversion error!\n");
            fclose(in);
            free(buf);
            free(tmpBuf);
            _exit(ERROR);
          }
        }
      }
      else // it's integer format 8bpp
      {
        tmpParams=inParams;
        tmpBuf=buf;
      }
    }
    else // inParams.magic[3] == 'b'
    {
      PVNParamCopy(&tmpParams, &inParams);
      tmpParams.magic[3]='a';
      tmpParams.maxcolour=8;
      tmpBufSize=calcPVNPageSize(tmpParams);
      tmpBuf=(unsigned char *)malloc(tmpBufSize);
      if(bufConvert(buf, bufSize, FORMAT_INT, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
      {
        fprintf(stderr, "Buffer conversion error!\n");
        fclose(in);
        free(buf);
        free(tmpBuf);
        _exit(ERROR);
      }
    }
    imgPixels=tmpBuf;
#ifdef DEBUG
    printf("Read image #%d, about to display\n", images_read);
#endif
    display();

    glutSetWindow(window_gl);
    glutPostRedisplay();
    glutTimerFunc(frameTime,readFrame,0);
  }
  else
  {
    if(tmpParams.depth != 0)
    {
      fseek(in,postHeaderPos,SEEK_SET);
      readFrame(0);
    }
    else
    {
      if(tmpBuf != buf)
        free(tmpBuf);
      free(buf);
      fclose(in);
      _exit(1);
    }
  }
}

void keyboardHit(unsigned char key, int x, int y)
{
  if(key == 'q')
    _exit(0);

  glutPostRedisplay();
}


/* this code was written by Andrew Hogue to make the video
   appear right-side-up */
void reshape(int w, int h)
{
  winWidth = w;
  winHeight = h;
  glutReshapeWindow(winWidth,winHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if(pixel_zoom)
  {
    gluOrtho2D(0,winWidth,-winHeight,0); // right side up with pixel zoom
    glMatrixMode(GL_MODELVIEW);
    glPixelZoom(1.0,-1.0);
  }
  else
  {
    gluOrtho2D(0,winWidth,0,winHeight); // inverted without pixel zoom
    glMatrixMode(GL_MODELVIEW);
  }
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
#ifdef _MSC_VER
	MessageBox(NULL,"Use PVN file as parameter", "Usage", MB_OK);
#else
    fprintf(stderr, "Usage:\n  %s filename.pvn\n", argv[0]);
#endif
    _exit(1);
  }

  filename=argv[1];

  if ((in = fopen(filename, "rb")) == NULL)
  {
#ifdef _MSC_VER
	MessageBox(NULL,"Error opening file", "ERROR", MB_OK);
#else
     fprintf(stderr, "Error opening file %s for read\n", filename);
#endif
     _exit(1);
  }

  if (readPVNHeader(in, &inParams) != VALID)
  {
    _exit(1);
  }

  postHeaderPos=ftell(in);

  if((inParams.magic[3] != 'a') && (inParams.magic[3] != 'b') && (inParams.magic[3] != 'f') && (inParams.magic[3] != 'd'))
  {
    fprintf(stderr, "For the time being, only type 'a', 'b', 'f' and 'd' PVN files are supported!\n");
    _exit(1);
  }
	
  winWidth=inParams.width;
  winHeight=inParams.height;

  if((inParams.magic[2] == '4') || (inParams.magic[2] == '5'))
    imgFormat=GL_LUMINANCE;
  else if(inParams.magic[2] == '6')
    imgFormat=GL_RGB;
  else
  {
    fprintf(stderr, "PVN Format not supported!\n");
    _exit(1);
  }

  imgType=GL_UNSIGNED_BYTE;

  if(inParams.framerate==0)
  {
    printf("Setting Frame Rate to default of 15\n");
    inParams.framerate=15;
  }

  frameTime=1000/inParams.framerate;
  bufSize=calcPVNPageSize(inParams);
  buf=(unsigned char *)malloc(bufSize);

  // set up glut
  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(winWidth,winHeight);
  glutInitWindowPosition(0,0);
  window_gl = glutCreateWindow(filename);
  glutDisplayFunc(display);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1); // align x rows to 1 byte (rather than default of 4)
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboardHit);
  glutTimerFunc(frameTime,readFrame,0); // first param is milliseconds

  glutMainLoop();
}
