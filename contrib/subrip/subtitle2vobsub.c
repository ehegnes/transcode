/*
  
  Convert a transcode subtitle stream to vobsub format

  Author: Arne Driescher

  Copyright:

  Most of the code is stolen from
  mplayer (file vobsub.c) http://mplayer.dev.hu/homepage/news.html 
  and transcode http://www.theorie.physik.uni-goettingen.de/~ostreich/transcode/
  so that the Copyright of the respective owner should be applied.
  (That means GPL.)
  
  Version: 0.01
*/

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>     
#include "subtitle2pgm.h"
#include <string.h>
#include <unistd.h>  
#include "vobsub.h"

#ifndef MAXFLOAT
#  define MAXFLOAT      3.40282347e+38F 
#endif

#define READ_BUF_SIZE (64*1024)


int vobsub_id=-1;
int verbose=1;

// get the major version number from the version code
unsigned int major_version(unsigned int version)
{
  // bit 16-31 contain the major version number
  return version >> 16;
}

unsigned int minor_version(unsigned int version)
{
  // bit 0-15 contain the minor version number
  return version & 0xffff;
}

void usage(void)
{
  fprintf(stderr,"\n\t Convert a transcode subtitle stream to vobsub format \n\n");
  fprintf(stderr,"\t subtitle2vonsub [options]\n");
  fprintf(stderr,"\t -p input file name of transcode ps1 file\n");
  fprintf(stderr,"\t -i input file name of ifo file\n");
  fprintf(stderr,"\t -o output file base name\n");
  fprintf(stderr,"\t -s width,height set the default movie size if ifo-file is missing\n");
  fprintf(stderr,"\t -c <up to 16 hex values> set color palette if ifo-file is missing\n");
  fprintf(stderr,"\t -e start,end,new_start extract only part of file (parameters in seconds)\n");
  fprintf(stderr,"\t Version 0.1 (alpha) for >transcode-0.6.0pre4\n");
  exit(0);
}


// magic string (definition must match the one in transcode/import/extract_ac3.c)
static char *subtitle_header_str="SUBTITLE";

int main(int argc, char** argv)
{
    
  int len;
  char read_buf[READ_BUF_SIZE];
  char output_base_name[FILENAME_MAX];
  char input_file_name[FILENAME_MAX];
  char ifo_file_name[FILENAME_MAX];

  subtitle_header_v3_t subtitle_header;
  int ch,n;
  int skip_len;
  double pts_seconds=0.0;
  unsigned int show_version_mismatch=~0;
  double layer_skip_adjust=0.0;
  double layer_skip_offset=0.0;
  unsigned int discont_ctr=0;

  // default color palette
  unsigned int palette[16]={0x101010, 0x6e6e6e, 0xcbcbcb, 0x202020, 0x808080, 0x808080, 
			    0x808080, 0x808080, 0x808080, 0x808080, 0x808080, 0x808080, 
			    0xb4b4b4, 0x101010, 0xe4e4e4, 0x808080};
  // assume PAL DVD size: 720x576 if no IFO was found
  unsigned int width=720;
  unsigned int height=576;
  unsigned char lang_abrv[3] = { 'e', 'n', 0 };
  unsigned int vobsub_out_index=0;
  int dvdsub_id=0; //FIXME 
  void *vobsub_writer=NULL;
  double extract_start_pts=0.0;
  double extract_end_pts= MAXFLOAT;
  double extract_output_start_pts=0.0;
  double output_pts=0.0;

  /* initialize default values here that can be overriden by commandline arguments */

  // default filenames used for input/output
  strcpy(output_base_name,"movie_subtitle");
  strcpy(ifo_file_name,"movie.ifo");
  strcpy(input_file_name,"movie.ps1");

  /* scan command line arguments */
  opterr=0;
  while ((ch = getopt(argc, argv, "s:p:i:c:e:o:h")) != -1) {
      
      switch (ch) {

	// color palette
      case 'c': 
	n = sscanf(optarg,"%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x", 
		   &palette[0], &palette[1], &palette[2], &palette[3],
		   &palette[4], &palette[5], &palette[6], &palette[7],
		   &palette[8], &palette[9], &palette[10], &palette[11],
		   &palette[12], &palette[13], &palette[14], &palette[15]);
	if(n<1 || n>16) {
	  fprintf(stderr,"invalid argument to color palette option\n");
	  exit(-1);
	}
	break;


      case 'e':  // extract only a part 
	n = sscanf(optarg,"%lf,%lf,%lf", &extract_start_pts, &extract_end_pts,&extract_output_start_pts);
	if(n==0) {
	  fprintf(stderr,"Invalid parameter for -e option\n");
	  exit(1);
	}
	if(extract_start_pts<0){
	  fprintf(stderr,"negativ start pts not allowed for option -e\n");
	  exit(-1);
	}

	if(extract_end_pts<0){
	  fprintf(stderr,"negativ end pts not allowed for option -e\n");
	  exit(-1);
	}
	if(extract_output_start_pts<0){
	  fprintf(stderr,"negativ pts for output not allowed for option -e\n");
	  exit(-1);
	}
	
	if(verbose)
	  fprintf(stderr,"Extracting from %f. to %f and setting initial pts to %f \n",
		  extract_start_pts,
		  extract_end_pts,
		  extract_output_start_pts);
	break;

      case 's':  // set default size to -s width,height if no ifo file is avalable 
	n = sscanf(optarg,"%d,%d", &width, &height);
	if( (n<1) || (n>2) ) {
	  fprintf(stderr,"Invalid parameter for -s width,height option\n");
	  exit(1);
	}
	
      case 'o':
	n = sscanf(optarg,"%s", output_base_name);
	
	if(n!=1) {
	  fprintf(stderr,"no filename specified to option -o\n");
	  exit(1);
	}
	break;
      
      case 'i':
	n = sscanf(optarg,"%s",ifo_file_name);
	
	if(n!=1) {
	  fprintf(stderr,"no filename specified to option -i\n");
	  exit(1);
	}
	break;

      case 'p':
	n = sscanf(optarg,"%s", input_file_name);
	
	if(n!=1) {
	  fprintf(stderr,"no filename specified to option -p\n");
	  exit(1);
	} 
	// open the specified input file for reading
	if( !(freopen(input_file_name,"r",stdin)) ){
	  perror("stdin redirection");
	  fprintf(stderr,"Tried to open %s for input\n",input_file_name);
	  exit(1);
	}

	break;


      case 'h':
	usage();
	break;

      default:
	fprintf(stderr,"Unknown option. Use -h for list of valid options.\n");
	exit(1);
      }
  }

  // parse the ifo file to get subtitle picture size and color palette
  if(vobsub_parse_ifo(NULL,ifo_file_name, palette, &width, &height, 1, dvdsub_id, lang_abrv)>=0){
      if(verbose){
	  fprintf(stderr,"reading IFO file was sucessful\n");
      }
  } else {
      fprintf(stderr,"Opening ifo file failed. I tried to open %s but got an error\n",
	      ifo_file_name);
      fprintf(stderr,"Using default or command line arguments "
	      "for palette, width and hight instead\n");
  }

  // open the vobsub output file
  vobsub_writer = vobsub_out_open(output_base_name, palette, width, height,
				  lang_abrv, vobsub_out_index);

  if(!vobsub_writer){
      fprintf(stderr,"vobsub_writer instance not created.\n");
      exit(-1);
  }

  if(verbose){
      fprintf(stderr,"Using width:%d height:%d language:%s vobsub index:%d\n",
	      width, 
	      height,
	      lang_abrv,
	      vobsub_out_index);
  }

  // process all packages in the stream
  // the stream is an "augmented" raw subtitle stream
  // where two additional headers are used.
  while(1){
    
    // read the magic "SUBTITLE" identified
    len=fread(read_buf, strlen(subtitle_header_str),1, stdin);

    if(feof(stdin)){
      break;
    }

    if(len != 1){

      fprintf(stderr,"Could not read magic header %s\n", subtitle_header_str);
      perror("Magic header");
      exit(1);
    }
    if(strncmp(read_buf,subtitle_header_str,strlen(subtitle_header_str))){
      fprintf(stderr,"Header %s not found\n",subtitle_header_str);
      fprintf(stderr,"%s\n",read_buf);
      exit(1);
    }
    
    // read the real header
    len=fread(&subtitle_header, sizeof(subtitle_header_v3_t), 1, stdin);
    
    if(len != 1){
      fprintf(stderr,"Could not read subtitle header\n");
      perror("Subtitle header");
      exit(1);
    }


    // check for version mismatch and warn the user
    if( (subtitle_header.header_version < MIN_VERSION_CODE) && show_version_mismatch){
      fprintf(stderr,"Warning: subtitle2pgm was compiled for header version %u.%u"
	      " and the stream was produced with version %u.%u.\n",
	      major_version(MIN_VERSION_CODE), 
	      minor_version(MIN_VERSION_CODE),
	      major_version(subtitle_header.header_version),
	      minor_version(subtitle_header.header_version));
      // don't show this message again
      show_version_mismatch=0;
    }

    // we only try to proceed if the major versions match
    if( major_version(subtitle_header.header_version) != major_version(MIN_VERSION_CODE) ){
      fprintf(stderr,"Versions are not compatible. Please extract subtitle stream\n"
	      " with a newer transcode version\n");
      exit(1);
    }


    // calculate exessive header bytes
    skip_len = subtitle_header.header_length - sizeof(subtitle_header_v3_t);

    // handle versions mismatch
    if(skip_len){

      // header size can only grow (unless something nasty happend)
      assert(skip_len > 0);

      // put the rest of the header into read buffer
      len = fread(read_buf, sizeof(char), skip_len, stdin);
      
      if(len != skip_len){
	perror("Header skip:");
	exit(1);
      }
    }
    
    /* depending on the minor version some additional information might
       be available. */

    // since version 3.1 discont_ctr is available but works only sine 4-Mar-2002. Allow extra
    // adjustment if requested.
    if(minor_version(subtitle_header.header_version) > 1){
      discont_ctr=*((unsigned int*) read_buf);
      layer_skip_adjust = discont_ctr*layer_skip_offset;
    }
   


    // debug output
#ifdef DEBUG
        fprintf(stderr,"subtitle_header: length=%d version=%0x lpts=%u (%f), rpts=%f, payload=%d, discont=%d\n",
		subtitle_header.header_length,
		subtitle_header.header_version,
		subtitle_header.lpts,
		(double)(subtitle_header.lpts/300)/90000.0,
		subtitle_header.rpts,
		subtitle_header.payload_length,
		discont_ctr);
#endif
    
    // read one byte subtitle stream id (should match the number given to tcextract -a)
    len=fread(read_buf, sizeof(char), 1, stdin);
    if(len != 1){
      perror("stream id");
      exit(1);
    }
    // debug output
    //fprintf(stderr,"stream id: %x \n",(int)*read_buf);


    // read numer of bytes given in header
    len = fread(read_buf, subtitle_header.payload_length-1, 1, stdin);

    
    if(len >0){
      if(subtitle_header.rpts > 0){
	// if rpts is something useful take it
	pts_seconds=subtitle_header.rpts;
      } else {
	// calculate the time from lpts
	fprintf(stderr, "fallback to lpts!\n");
	pts_seconds = (double)(subtitle_header.lpts/300)/90000.0;
      }

      // add offset for layer skip
      pts_seconds += layer_skip_adjust;

      
      // output only in the requested range (-e option)
      if( (extract_start_pts <= pts_seconds) && (extract_end_pts >= pts_seconds) ){ 
	output_pts = pts_seconds - extract_start_pts + extract_output_start_pts;
	vobsub_out_output(vobsub_writer,read_buf, subtitle_header.payload_length-1,output_pts);
      }
    } else {
      perror("Input file processing finished");
      exit(errno);
    }
  }
  
  fprintf(stderr,"Conversion finished\n");

  vobsub_out_close(vobsub_writer);

  return 0;
}

  
  
  
  




















  
