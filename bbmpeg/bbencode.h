#ifndef _BBENCODE_H
#define _BBENCODE_H 1

#define ENCODE_ERR    0
#define ENCODE_STOP   1
#define ENCODE_RUN    2
#define ENCODE_UNDO   3
#define ENCODE_NEWGOP 4

#define ENCODE_NTSC 0
#define ENCODE_PAL  1

typedef struct t_bbmpeg_ctx
{
  unsigned char *pY;
  unsigned char *pU;
  unsigned char *pV;
  
  int pic_size_l;
  int pic_size_c;
    
  int check_pending;
  int gop_size_max;
  int gop_size_undo;
  int gop_size;
  
  int ret;

  int    verbose;  
  char   *progress_str;
  double bitrate;
  double file_size;
  double max_file_size;
  
} T_BBMPEG_CTX;


T_BBMPEG_CTX *bb_start(char *fname, int w, int h, int sh_info);
int          bb_stop(T_BBMPEG_CTX *ctx);
int          bb_encode(T_BBMPEG_CTX *ctx, int check);

int  bb_set_profile(char *profile_name, char ref_type, 
                    int tv_type, int asr, int frc, int pulldown, int sh_info);
void bb_gen_profile(void);

void bb_resize_setup(int src_w, int src_h, int dst_w, int dst_h, int sh_info);
void bb_resize_frame(unsigned char *py_src,
                     unsigned char *pu_src, 
                     unsigned char *pv_src,
                     unsigned char *py_dst, 
                     unsigned char *pu_dst, 
                     unsigned char *pv_dst);

#endif
 
