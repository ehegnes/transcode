#ifndef _RESIZE_H
#define _RESIZE_H
#include "libmpeg3/libmpeg3.h"
#include "libmpeg3/mpeg3protos.h"
#include "lve/media_buf.h"
#include "main.h"
#include "bbencode.h"

#define TYPE_SINGLE_MPEG 0
#define TYPE_MEDIA_LST   1

#define g_nSamples      nSamples
#define g_currentSample currentSample

#define g_nframes nframes
#define g_frame0  frame0
#define g_M       M
#define g_N       N
#define g_tc0     tc0

#define g_outfile          videobs.bitfile
#define g_topfirst         topfirst
#define g_prog_frame       prog_frame
#define g_prog_seq         prog_seq
#define g_frame_rate       frame_rate
#define g_bit_rate         bit_rate
#define g_aspectratio      aspectratio
#define g_fixed_mquant     mquant_value
#define g_vbv_buffer_size  vbv_buffer_size

typedef struct
{
  int media_type;

  int    smart;  

  double start_smart;
  double end_smart;

  long   smart_bytes;
  int    smart_frames_s;
  int    smart_frames_e;
  int    smart_sc_loaded;
  int    smart_copy_done;

  int    total_scenes;
  int    total_frames;
    
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  
  int pic_size_l;
  int pic_size_c;
  int npages;
  //int page_num;

  unsigned int frame_num;
  //unsigned int gop_frames;
  //int          gop_cnt;
  //int          new_gop; 

  double       frame_rate;
  int          aspect_ratio;

  int          total_audio;
  int          has_audio;
  int	       sample_rate;
  int          channels;

  unsigned char *src_buf;
  unsigned char *py_src, *py_dst;
  unsigned char *pu_src, *pu_dst;
  unsigned char *pv_src, *pv_dst;

  char     *lst_fname; 
  mpeg3_t  *file;  
  int      vstream;
  int      astream;

  unsigned int n_frames;   // real count of (encoded) frames (per scene)

  T_MEDIA_HDR md_header;
  T_MEDIA_BUF md_buf;

} T_COM;

extern void gmo_read_init(mpeg3_t *file, int smart, int *pd_w, int *pd_h);
extern void gmo_read_fillbuf(T_BBMPEG_CTX *ctx);
extern int  gmo_read_audio(void);
extern int  gmo_read_wav(int frame_size, short *buffer);
extern int  gmo_read_eof(void);

extern void gmo_check_copy(void);

extern int      gmo_read_chk_sig(char *fname);
extern mpeg3_t *gmo_read_open(char *fname);
extern void     gmo_read_close(mpeg3_t *file);
#endif