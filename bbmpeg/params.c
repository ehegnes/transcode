//----------------------------------------------------------------------------
// run the settings
//----------------------------------------------------------------------------

#include "main.h"

#define settingsErrNone 0
#define settingsCancel  1
#define settingsError   2

typedef enum {
   VBV_BUFFER_SIZE,           // 0
   FIXED_VBV_DELAY,           // 1
   DISPLAY_HORIZONTAL_SIZE,   // 2
   DISPLAY_VERTICAL_SIZE,     // 3
   PROG_SEQ,                  // 4
   PROG_FRAME,                // 5
   FIELDPIC,                  // 6
   TOPFIRST,                  // 7
   REPEATFIRST,               // 8
   INTRAVLC_TAB_0,            // 9
   INTRAVLC_TAB_1,            // 10
   INTRAVLC_TAB_2,            // 11
   FRAME_PRED_DCT_TAB_0,      // 12
   FRAME_PRED_DCT_TAB_1,      // 13
   FRAME_PRED_DCT_TAB_2,      // 14
   QSCALE_TAB_0,              // 15
   QSCALE_TAB_1,              // 16
   QSCALE_TAB_2,              // 17
   ALTSCAN_TAB_0,             // 18
   ALTSCAN_TAB_1,             // 19
   ALTSCAN_TAB_2,             // 20
   SEQ_DSPLY_EXT,             // 21
   SEQ_ENDCODE,               // 22
   SVCD_USER_BLOCKS,          // 23
   R,                         // 24
   AVG_ACT,                   // 25
   X_I,                       // 26
   X_P,                       // 27
   X_B,                       // 28
   D0_I,                      // 29
   D0_P,                      // 30
   D0_B,                      // 31
   MIN_FRAME_PERCENT,         // 32
   PAD_FRAME_PERCENT,         // 33
   RESET_PBD0,                // 34
   AUTOMOTION,                // 35
   XMOTION,                   // 36
   YMOTION,                   // 37
   MAXNONMOTION               // 38
   } t_non_motion_settings;

#define MAXPARENT      12 + MAXM
#define MAXMOTIONHCODE MAXM * 2
#define MAXMOTIONVCODE MAXM * 2
#define MAXMOTIONSRCH  MAXM * 4

static int vbvlim[4] = {597, 448, 112, 29};

// static int horzlim[4] = {9, 9, 8, 7};
// static int vertlim[4] = {5, 5, 5, 4};

// static int Ndefaults[MAXM] = {1, 12, 15, 16, 15, 12, 14, 16};

static double ratetab[8]=
    {24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};

// static int ratetab1[4] = {62668800, 47001600, 10368000, 3041280};
// static int bitratetab[4] = {80000000, 60000000, 14000000, 4000000};
static int svcdrates[15] = {0, 2568000, 2552000, 2544000, 2536000, 2520000,
                               2504000, 2488000, 2472000, 2440000, 2408000,
                               2376000, 2344000, 2280000, 2216000};

void PutTempSettings(struct mpegOutSettings *set)
{
  int i;

  set->useFP = UseFP;
  set->verbose = bb_verbose;
  set->denoise = bb_denoise;
  set->MplexVideo = MplexVideo;
  set->MplexAudio = MplexAudio;
  set->UserEncodeVideo = UserEncodeVideo;
  set->UserEncodeAudio = UserEncodeAudio;
  set->EncodeVideo = EncodeVideo;
  set->EncodeAudio = EncodeAudio;
  set->SaveTempVideo = SaveTempVideo;
  set->SaveTempAudio = SaveTempAudio;
  set->B_W = B_W;

  strcpy(set->id_string, id_string);
  strcpy(set->iqname, iqname);
  strcpy(set->niqname, niqname);
  strcpy(set->statname, statname);
  set->video_type = video_type;
  set->video_pulldown_flag = video_pulldown_flag;
  set->constrparms = constrparms;
  set->N = N; set->M = M;
  set->fieldpic = fieldpic;
  set->aspectratio = aspectratio;
  set->frame_rate_code = frame_rate_code;
  set->frame_rate = frame_rate;
  set->tc0 = tc0;
  set->hours = hours;
  set->mins = mins;
  set->sec = sec;
  set->tframe = tframe;
  set->bit_rate = bit_rate;
  set->max_bit_rate = max_bit_rate;
  set->avg_bit_rate = avg_bit_rate;
  set->min_bit_rate = min_bit_rate;
  set->auto_bitrate = auto_bitrate;
  set->vbv_buffer_size = vbv_buffer_size;
  set->fixed_vbv_delay = fixed_vbv_delay;
  set->constant_bitrate = constant_bitrate;
  set->mquant_value = mquant_value;
  set->low_delay = low_delay;
  set->profile = profile;
  set->level = level;
  set->prog_seq = prog_seq;
  set->chroma_format = chroma_format;
  set->write_sde = write_sde;
  set->write_sec = write_sec;
  set->video_format = video_format;
  set->color_primaries = color_primaries;
  set->transfer_characteristics = transfer_characteristics;
  set->matrix_coefficients = matrix_coefficients;
  set->display_horizontal_size = display_horizontal_size;
  set->display_vertical_size = display_vertical_size;
  set->dc_prec = dc_prec;
  set->topfirst = topfirst;
  set->embed_SVCD_user_blocks = embed_SVCD_user_blocks;
  set->write_pde = write_pde;
  set->frame_centre_horizontal_offset = frame_centre_horizontal_offset;
  set->frame_centre_vertical_offset = frame_centre_vertical_offset;

  for (i = 0; i < 3; i++)
  {
    set->frame_pred_dct_tab[i] = frame_pred_dct_tab[i];
    set->conceal_tab[i] = conceal_tab[i];
    set->qscale_tab[i] = qscale_tab[i];
    set->intravlc_tab[i] = intravlc_tab[i];
    set->altscan_tab[i] = altscan_tab[i];
  }

  set->repeatfirst = repeatfirst;
  set->prog_frame = prog_frame;
  set->P = P;
  set->r = init_r;
  set->avg_act = (int) init_avg_act;
  set->Xi = init_Xi;
  set->Xp = init_Xp;
  set->Xb = init_Xb;
  set->d0i = init_d0i;
  set->d0p = init_d0p;
  set->d0b = init_d0b;
  set->reset_d0pb = reset_d0pb;
  set->min_frame_percent = min_frame_percent;
  set->pad_frame_percent = pad_frame_percent;

  set->xmotion = xmotion;
  set->ymotion = ymotion;
  set->automotion = automotion;
  set->maxmotion = maxmotion;
  for (i = 0; i < MAXM; i++)
  {
    set->motion_data[i].forw_hor_f_code = motion_data[i].forw_hor_f_code;
    set->motion_data[i].forw_vert_f_code = motion_data[i].forw_vert_f_code;
    set->motion_data[i].sxf = motion_data[i].sxf;
    set->motion_data[i].syf = motion_data[i].syf;
    set->motion_data[i].back_hor_f_code = motion_data[i].back_hor_f_code;
    set->motion_data[i].back_vert_f_code = motion_data[i].back_vert_f_code;
    set->motion_data[i].sxb = motion_data[i].sxb;
    set->motion_data[i].syb = motion_data[i].syb;
  }
  /* audio stuff */
  set->audio_mode = audio_mode;
  set->audio_layer = audio_layer;
  set->psych_model = psych_model;
  set->audio_bitrate = audio_bitrate;
  set->emphasis = emphasis;
  set->extension = extension;
  set->error_protection = error_protection;
  set->copyright = copyright;
  set->original = original;

  /* multiplex stuff */
  set->sectors_delay = sectors_delay;
  set->video_delay_ms = video_delay_ms;
  set->audio_delay_ms = audio_delay_ms;
  set->audio1_delay_ms = audio1_delay_ms;
  set->sector_size = sector_size;
  set->packets_per_pack = packets_per_pack;
  set->audio_buffer_size = init_audio_buffer_size;
  set->audio1_buffer_size = init_audio1_buffer_size;
  set->video_buffer_size = init_video_buffer_size;
  set->always_sys_header = always_sys_header;
  set->use_computed_bitrate = use_computed_bitrate;
  set->mplex_type = mplex_type;
  set->mplex_pulldown_flag = mplex_pulldown_flag;
  set->vcd_audio_pad = vcd_audio_pad;
  set->user_mux_rate = user_mux_rate;
  set->align_sequence_headers = align_sequence_headers;
  set->put_private2 = put_private2;
  set->frame_timestamps = frame_timestamps;
  set->VBR_multiplex = VBR_multiplex;
  set->write_pec = write_pec;
  set->mux_SVCD_scan_offsets = mux_SVCD_scan_offsets;
  set->max_file_size = max_file_size;
  set->mux_start_time = mux_start_time;
  set->mux_stop_time = mux_stop_time;
  set->reset_clocks = reset_clocks;
  set->write_end_codes = write_end_codes;
  set->set_broken_link = set_broken_link;
}

void GetTempSettings(struct mpegOutSettings *set)
{
  int i;

  UseFP = set->useFP;
  bb_verbose = set->verbose;
  bb_denoise = set->denoise;
  MplexVideo = set->MplexVideo;
  MplexAudio = set->MplexAudio;
  UserEncodeVideo = set->UserEncodeVideo;
  UserEncodeAudio = set->UserEncodeAudio;
  EncodeVideo = set->EncodeVideo;
  EncodeAudio = set->EncodeAudio;
  SaveTempVideo = set->SaveTempVideo;
  SaveTempAudio = set->SaveTempAudio;
  B_W = set->B_W;

  strcpy(id_string, set->id_string);
  strcpy(iqname, set->iqname);
  strcpy(niqname, set->niqname);
  strcpy(statname, set->statname);
  video_type = set->video_type;
  video_pulldown_flag = set->video_pulldown_flag;
  constrparms = set->constrparms;
  N = set->N; M = set->M;
  fieldpic = set->fieldpic;
  aspectratio = set->aspectratio;
  frame_rate_code = set->frame_rate_code;
  frame_rate = set->frame_rate;
  tc0 = set->tc0;
  hours = set->hours;
  mins = set->mins;
  sec = set->sec;
  tframe = set->tframe;
  bit_rate = set->bit_rate;
  max_bit_rate = set->max_bit_rate;
  avg_bit_rate = set->avg_bit_rate;
  min_bit_rate = set->min_bit_rate;
  auto_bitrate = set->auto_bitrate;
  vbv_buffer_size = set->vbv_buffer_size;
  fixed_vbv_delay = set->fixed_vbv_delay;
  constant_bitrate = set->constant_bitrate;
  mquant_value = set->mquant_value;
  low_delay = set->low_delay;
  profile = set->profile;
  level = set->level;
  prog_seq = set->prog_seq;
  chroma_format = set->chroma_format;
  write_sde = set->write_sde;
  write_sec = set->write_sec;
  video_format = set->video_format;
  color_primaries = set->color_primaries;
  transfer_characteristics = set->transfer_characteristics;
  matrix_coefficients = set->matrix_coefficients;
  display_horizontal_size = set->display_horizontal_size;
  display_vertical_size = set->display_vertical_size;
  dc_prec = set->dc_prec;
  topfirst = set->topfirst;
  embed_SVCD_user_blocks = set->embed_SVCD_user_blocks;
  write_pde = set->write_pde;
  frame_centre_horizontal_offset = set->frame_centre_horizontal_offset;
  frame_centre_vertical_offset = set->frame_centre_vertical_offset; 

  for (i = 0; i< 3; i++)
  {
    frame_pred_dct_tab[i] = set->frame_pred_dct_tab[i];
    conceal_tab[i] = set->conceal_tab[i];
    qscale_tab[i] = set->qscale_tab[i];
    intravlc_tab[i] = set->intravlc_tab[i];
    altscan_tab[i] = set->altscan_tab[i];
  }

  repeatfirst = set->repeatfirst;
  prog_frame = set->prog_frame;
  P = set->P;
  init_r = set->r;
  init_avg_act = (double) set->avg_act;
  init_Xi = set->Xi;
  init_Xp = set->Xp;
  init_Xb = set->Xb;
  init_d0i = set->d0i;
  init_d0p = set->d0p;
  init_d0b = set->d0b;
  reset_d0pb = set->reset_d0pb;
  min_frame_percent = set->min_frame_percent;
  pad_frame_percent = set->pad_frame_percent;

  xmotion = set->xmotion;
  ymotion = set->ymotion;
  automotion = set->automotion;
  maxmotion = set->maxmotion;
  for (i = 0; i < MAXM; i++)
  {
    motion_data[i].forw_hor_f_code = set->motion_data[i].forw_hor_f_code;
    motion_data[i].forw_vert_f_code = set->motion_data[i].forw_vert_f_code;
    motion_data[i].sxf = set->motion_data[i].sxf;
    motion_data[i].syf = set->motion_data[i].syf;
    motion_data[i].back_hor_f_code = set->motion_data[i].back_hor_f_code;
    motion_data[i].back_vert_f_code = set->motion_data[i].back_vert_f_code;
    motion_data[i].sxb = set->motion_data[i].sxb;
    motion_data[i].syb = set->motion_data[i].syb;
  }
  /* audio stuff */
  audio_mode = set->audio_mode;
  audio_layer = set->audio_layer;
  psych_model = set->psych_model;
  audio_bitrate = set->audio_bitrate;
  emphasis = set->emphasis;
  extension = set->extension;
  error_protection = set->error_protection;
  copyright = set->copyright;
  original = set->original;

  /* multiplex stuff */
  sectors_delay = set->sectors_delay;
  video_delay_ms = set->video_delay_ms;
  audio_delay_ms = set->audio_delay_ms;
  audio1_delay_ms = set->audio1_delay_ms;
  sector_size = set->sector_size;
  packets_per_pack = set->packets_per_pack;
  init_audio_buffer_size = set->audio_buffer_size;
  init_audio1_buffer_size = set->audio1_buffer_size;
  init_video_buffer_size = set->video_buffer_size;
  always_sys_header = set->always_sys_header;
  use_computed_bitrate = set->use_computed_bitrate;
  mplex_type = set->mplex_type;
  mplex_pulldown_flag = set->mplex_pulldown_flag;
  vcd_audio_pad = set->vcd_audio_pad;
  user_mux_rate = set->user_mux_rate;
  align_sequence_headers = set->align_sequence_headers;
  put_private2 = set->put_private2;
  frame_timestamps = set->frame_timestamps;
  VBR_multiplex = set->VBR_multiplex;
  write_pec = set->write_pec;
  mux_SVCD_scan_offsets = set->mux_SVCD_scan_offsets;
  max_file_size = set->max_file_size;
  mux_start_time = set->mux_start_time;
  mux_stop_time = set->mux_stop_time;
  reset_clocks = set->reset_clocks;
  write_end_codes = set->write_end_codes;
  set_broken_link = set->set_broken_link;
}

void SetMPEG2Defaults(struct mpegOutSettings *set, int palDefaults)
{
  int i;

  set->useFP = 0;
  set->verbose = 0;
  set->denoise = 0;
  set->MplexVideo = TRUE;
  set->MplexAudio = TRUE;
  set->UserEncodeVideo = TRUE;
  set->UserEncodeAudio = TRUE;
  set->EncodeVideo = TRUE;
  set->EncodeAudio = TRUE;
  set->SaveTempVideo = FALSE;
  set->SaveTempAudio = FALSE;
  set->write_sde = 1;
  set->write_sec = 1;
  set->B_W = 0;
  if (palDefaults)
  {
    strcpy(set->id_string, "MPEG-2 PAL video and MPEG audio");
    set->frame_rate_code = 3;
    set->display_vertical_size = 576;
    set->color_primaries = 5;
    set->transfer_characteristics = 5;
    set->video_format = 1;
  }
  else
  {
    strcpy(set->id_string, "MPEG-2 NTSC video and MPEG audio");
    set->frame_rate_code = 4;
    set->display_vertical_size = 480;
    set->color_primaries = 4;
    set->transfer_characteristics = 4;
    set->video_format = 2;
  }
  strcpy(set->iqname, "");
  strcpy(set->niqname, "");
  strcpy(set->statname, "");
  set->video_type = MPEG_MPEG2;
  set->video_pulldown_flag = PULLDOWN_NONE;
  set->constrparms = FALSE;
  set->N = 15;
  set->M = 3;
  set->fieldpic = 0;
  set->aspectratio = 2;
  set->frame_rate = ratetab[set->frame_rate_code - 1];
  set->tc0 = 0;
  set->hours = 0;
  set->mins = 0;
  set->sec = 0;
  set->tframe = 0;
  set->bit_rate = 6000000.0;
  set->max_bit_rate = 0.0;
  set->avg_bit_rate = 0.0;
  set->min_bit_rate = 0.0;
  set->auto_bitrate = 0;
  set->vbv_buffer_size = 112;
  set->fixed_vbv_delay = 1;
  set->constant_bitrate = 0;
  set->mquant_value = 4;
  set->low_delay = 0;
  set->profile = 4;
  set->level = 8;
  set->prog_seq = 0;
  set->chroma_format = 1;
  set->matrix_coefficients = 5;
  set->display_horizontal_size = 720;
  set->dc_prec = 1;
  set->topfirst = 0;
  set->embed_SVCD_user_blocks = 0;
  set->write_pde = 0;
  set->frame_centre_horizontal_offset = 0;
  set->frame_centre_vertical_offset = 0;

  for (i = 0; i < 3; i++)
  {
    set->frame_pred_dct_tab[i] = 1;
    set->conceal_tab[i] = 0;
    set->qscale_tab[i] = 1;
    set->intravlc_tab[i] = 0;
    set->altscan_tab[i] = 1;
  }
  set->intravlc_tab[0] = 1;
  set->repeatfirst = 0;
  set->prog_frame = 0;
  set->P = 0;
  set->r = 0;
  set->avg_act = 0;
  set->Xi = 0;
  set->Xp = 0;
  set->Xb = 0;
  set->d0i = 0;
  set->d0p = 0;
  set->d0b = 0;
  set->reset_d0pb = 1;
  set->min_frame_percent = 25;
  set->pad_frame_percent = 90;

  set->xmotion = 3;
  set->ymotion = 3;
  set->automotion = 1;
  set->maxmotion = 58;
  AutoSetMotionData(set);

  /* audio stuff */
  if (audioStereo)
    set->audio_mode = MPG_MD_STEREO;
  else
    set->audio_mode = MPG_MD_MONO;
  set->audio_layer = 2;
  set->psych_model = 2;
  set->audio_bitrate = 11;
  set->emphasis = 0;
  set->extension = 0;
  set->error_protection = 0;
  set->copyright = 0;
  set->original = 0;
  SetMPEG2Mplex(set);
  ChangeVideoFilename(set);
}

void SetMPEG2Mplex(struct mpegOutSettings *set)
{
  /* multiplex stuff */
  set->sectors_delay = 0;
  set->video_delay_ms = 180;
  set->audio_delay_ms = 180;
  set->audio1_delay_ms = 180;
  set->sector_size = SVCD_SECTOR_SIZE;
  set->packets_per_pack = 1;
  set->audio_buffer_size = 4;
  set->audio1_buffer_size = 4;
  set->video_buffer_size = 224;
  set->always_sys_header = FALSE;
  set->mplex_type = MPEG_MPEG2;
  set->mplex_pulldown_flag = PULLDOWN_AUTO;
  set->vcd_audio_pad = FALSE;
  set->user_mux_rate = 0;
  set->align_sequence_headers = 0;
  set->put_private2 = 0;
  set->frame_timestamps = TIMESTAMPS_ALL;
  set->VBR_multiplex = !set->constant_bitrate;
  set->use_computed_bitrate = COMPBITRATE_MAX;
  set->write_pec = 1;
  set->mux_SVCD_scan_offsets = 0;
  set->max_file_size = 0;
  set->mux_start_time = 0;
  set->mux_stop_time = 0;
  set->reset_clocks = 1;
  set->write_end_codes = 1;
  set->set_broken_link = 1;
  AutoSetBitrateData(set);
}

void SetDVDDefaults(struct mpegOutSettings *set, int palDefaults)
{
  int i;

  set->useFP = 0;
  set->verbose = 0;
  set->denoise = 0;
  set->MplexVideo = TRUE;
  set->MplexAudio = TRUE;
  set->UserEncodeVideo = TRUE;
  set->UserEncodeAudio = TRUE;
  set->EncodeVideo = TRUE;
  set->EncodeAudio = TRUE;
  set->SaveTempVideo = FALSE;
  set->SaveTempAudio = FALSE;
  set->write_sde = 1;
  set->write_sec = 1;
  set->B_W = 0;
  if (palDefaults)
  {
    strcpy(set->id_string, "MPEG-2 DVD PAL video and MPEG audio");
    set->N = 12;
    set->frame_rate_code = 3;
    set->display_vertical_size = 576;
    set->color_primaries = 5;
    set->transfer_characteristics = 5;
    set->video_format = 1;
  }
  else
  {
    strcpy(set->id_string, "MPEG-2 DVD NTSC video and MPEG audio");
    set->N = 15;
    set->frame_rate_code = 4;
    set->display_vertical_size = 480;
    set->color_primaries = 4;
    set->transfer_characteristics = 4;
    set->video_format = 2;
  }
  strcpy(set->iqname, "");
  strcpy(set->niqname, "");
  strcpy(set->statname, "");
  set->video_type = MPEG_DVD;
  set->video_pulldown_flag = PULLDOWN_NONE;
  set->constrparms = FALSE;
  set->M = 3;
  set->fieldpic = 0;
  set->aspectratio = 2;
  set->frame_rate = ratetab[set->frame_rate_code - 1];
  set->tc0 = 0;
  set->hours = 0;
  set->mins = 0;
  set->sec = 0;
  set->tframe = 0;
  set->bit_rate = 6000000.0;
  set->max_bit_rate = 9800000.0;
  set->avg_bit_rate = 0.0;
  set->min_bit_rate = 0.0;
  set->auto_bitrate = 0;
  set->vbv_buffer_size = 112;
  set->fixed_vbv_delay = 1;
  set->constant_bitrate = 0;
  set->mquant_value = 4;
  set->low_delay = 0;
  set->profile = 4;
  set->level = 8;
  set->prog_seq = 0;
  set->chroma_format = 1;
  set->matrix_coefficients = 5;
  set->display_horizontal_size = 720;
  set->dc_prec = 1;
  set->topfirst = 0;
  set->embed_SVCD_user_blocks = 0;
  set->write_pde = 0;
  set->frame_centre_horizontal_offset = 0;
  set->frame_centre_vertical_offset = 0;

  for (i = 0; i < 3; i++)
  {
    set->frame_pred_dct_tab[i] = 1;
    set->conceal_tab[i] = 0;
    set->qscale_tab[i] = 1;
    set->intravlc_tab[i] = 0;
    set->altscan_tab[i] = 1;
  }
  set->intravlc_tab[0] = 1;
  set->repeatfirst = 0;
  set->prog_frame = 0;
  set->P = 0;
  set->r = 0;
  set->avg_act = 0;
  set->Xi = 0;
  set->Xp = 0;
  set->Xb = 0;
  set->d0i = 0;
  set->d0p = 0;
  set->d0b = 0;
  set->reset_d0pb = 1;
  set->min_frame_percent = 0;
  set->pad_frame_percent = 0;

  set->xmotion = 3;
  set->ymotion = 3;
  set->automotion = 1;
  set->maxmotion = 58;
  AutoSetMotionData(set);

  /* audio stuff */
  if (audioStereo)
    set->audio_mode = MPG_MD_STEREO;
  else
    set->audio_mode = MPG_MD_MONO;
  set->audio_layer = 2;
  set->psych_model = 2;
  set->audio_bitrate = 11;
  set->emphasis = 0;
  set->extension = 0;
  set->error_protection = 1;
  set->copyright = 0;
  set->original = 0;
  SetDVDMplex(set);
  ChangeVideoFilename(set);
}

void SetDVDMplex(struct mpegOutSettings *set)
{
  /* multiplex stuff */
  set->sectors_delay = 0;
  //ThOe
  set->video_delay_ms = 0;
  //set->video_delay_ms = 180;
  set->audio_delay_ms = 180;
  set->audio1_delay_ms = 180;
  set->sector_size = DVD_SECTOR_SIZE;
  set->packets_per_pack = 1;
  set->audio_buffer_size = 4;
  set->audio1_buffer_size = 4;
  set->video_buffer_size = 232;
  set->always_sys_header = FALSE;
  set->use_computed_bitrate = COMPBITRATE_MAX;
  set->mplex_type = MPEG_DVD;
  set->mplex_pulldown_flag = PULLDOWN_AUTO;
  set->vcd_audio_pad = FALSE;
  set->user_mux_rate = 25200;
  set->align_sequence_headers = TRUE;
  set->put_private2 = TRUE;
  set->frame_timestamps = TIMESTAMPS_IONLY;
  set->VBR_multiplex = !set->constant_bitrate;
  set->write_pec = 1;
  set->mux_SVCD_scan_offsets = 0;
  set->max_file_size = 0;
  set->mux_start_time = 0;
  set->mux_stop_time = 0;
  set->reset_clocks = 0;
  set->write_end_codes = 0;
  set->set_broken_link = 0;
  AutoSetBitrateData(set);
}

void SetSVCDDefaults(struct mpegOutSettings *set, int palDefaults)
{
  int i;

  set->useFP = FALSE;
  set->verbose = 0;
  set->denoise = 0;
  set->MplexVideo = TRUE;
  set->MplexAudio = TRUE;
  set->UserEncodeVideo = TRUE;
  set->UserEncodeAudio = TRUE;
  set->EncodeVideo = TRUE;
  set->EncodeAudio = TRUE;
  set->SaveTempVideo = FALSE;
  set->SaveTempAudio = FALSE;
  strcpy(set->iqname, "");
  strcpy(set->niqname, "");
  strcpy(set->statname, "");
  set->video_type = MPEG_SVCD;
  set->video_pulldown_flag = PULLDOWN_NONE;
  set->constrparms = FALSE;
  set->write_sde = 0;
  set->write_sec = 1;
  set->B_W = 0;
  if (palDefaults)
  {
    strcpy(set->id_string, "MPEG-2 SuperVCD PAL video and MPEG audio");
    set->frame_rate_code = 3;
    set->video_format = 1;
    set->color_primaries = 5;
    set->transfer_characteristics = 5;
    set->display_vertical_size = 576;
  }
  else
  {
    strcpy(set->id_string, "MPEG-2 SuperVCD NTSC video and MPEG audio");
    set->frame_rate_code = 4;
    set->video_format = 2;
    set->color_primaries = 4;
    set->transfer_characteristics = 4;
    set->display_vertical_size = 480;
  }
  set->N = 15;
  set->M = 3;
  set->aspectratio = 2;
  set->fieldpic = 0;
  set->frame_rate = ratetab[set->frame_rate_code - 1];
  set->tc0 = 0;
  set->hours = 0;
  set->mins = 0;
  set->sec = 0;
  set->tframe = 0;
  set->bit_rate = 2300000.0;
  set->max_bit_rate = 2376000.0;
  set->avg_bit_rate = 0.0;
  set->min_bit_rate = 0.0;
  set->auto_bitrate = 0;
  set->vbv_buffer_size = 112;
  set->fixed_vbv_delay = 1;
  set->constant_bitrate = 0;
  set->mquant_value = 10;
  set->low_delay = 0;
  set->profile = 4;
  set->level = 8;
  set->prog_seq = 0;
  set->chroma_format = 1;
  set->matrix_coefficients = 5;
  set->display_horizontal_size = 720;
  set->dc_prec = 1;
  set->topfirst = 0;
  set->embed_SVCD_user_blocks = 0;
  set->write_pde = 0;
  set->frame_centre_horizontal_offset = 0;
  set->frame_centre_vertical_offset = 0;

  for (i = 0; i < 3; i++)
  {
    set->frame_pred_dct_tab[i] = 1;
    set->conceal_tab[i] = 0;
    set->qscale_tab[i] = 1;
    set->intravlc_tab[i] = 0;
    set->altscan_tab[i] = 1; 
  }
  set->intravlc_tab[0] = 1;
  set->repeatfirst = 0;
  set->prog_frame = 0;
  set->P = 0;
  set->r = 0;
  set->avg_act = 0;
  set->Xi = 0;
  set->Xp = 0;
  set->Xb = 0;
  set->d0i = 0;
  set->d0p = 0;
  set->d0b = 0;
  set->reset_d0pb = 1;
  set->min_frame_percent = 25;
  set->pad_frame_percent = 90;

  set->xmotion = 3;
  set->ymotion = 3;
  set->automotion = 1;
  set->maxmotion = 58;
  AutoSetMotionData(set);

  /* audio stuff */
  if (audioStereo)
    set->audio_mode = MPG_MD_STEREO;
  else
    set->audio_mode = MPG_MD_MONO;
  set->audio_layer = 2;
  set->psych_model = 2;
  set->audio_bitrate = 11;
  set->emphasis = 0;
  set->extension = 0;
  set->error_protection = 1;
  set->copyright = 0;
  set->original = 0;
  SetSVCDMplex(set);
  ChangeVideoFilename(set);
}

void SetSVCDMplex(struct mpegOutSettings *set)
{
  /* multiplex stuff */
  set->sectors_delay = 0;
  set->video_delay_ms = 180;
  set->audio_delay_ms = 180;
  set->audio1_delay_ms = 180;
  set->sector_size = SVCD_SECTOR_SIZE;
  set->packets_per_pack = 1;
  set->audio_buffer_size = 4;
  set->audio1_buffer_size = 4;
  set->video_buffer_size = 230;
  set->always_sys_header = FALSE;
  set->use_computed_bitrate = COMPBITRATE_MAX;
  set->mplex_type = MPEG_SVCD;
  set->mplex_pulldown_flag = PULLDOWN_AUTO;
  set->vcd_audio_pad = FALSE;
  //set->user_mux_rate = 6972;
  set->user_mux_rate = 0;
  set->align_sequence_headers = 1;
  set->put_private2 = 0;
  set->frame_timestamps = TIMESTAMPS_ALL;
  set->VBR_multiplex = TRUE;
  set->write_pec = 1;
  set->mux_SVCD_scan_offsets = 1;
  set->max_file_size = 0;
  set->mux_start_time = 0;
  set->mux_stop_time = 0;
  set->reset_clocks = 1;
  set->write_end_codes = 1;
  set->set_broken_link = 1;
  AutoSetBitrateData(set);
}

void SetMPEG1Defaults(struct mpegOutSettings *set, int palDefaults)
{
  int i;

  set->useFP = 0;
  set->verbose = 0;
  set->denoise = 0;
  set->MplexVideo = TRUE;
  set->MplexAudio = TRUE;
  set->UserEncodeVideo = TRUE;
  set->UserEncodeAudio = TRUE;
  set->EncodeVideo = TRUE;
  set->EncodeAudio = TRUE;
  set->SaveTempVideo = FALSE;
  set->SaveTempAudio = FALSE;
  set->write_sde = 1;
  set->write_sec = 1;
  set->B_W = 0;
  if (palDefaults)
  {
    strcpy(set->id_string, "MPEG-1 PAL video and MPEG audio");
    set->aspectratio = 8;
    set->frame_rate_code = 3;
    set->display_vertical_size = 288;
    set->video_format = 1;
    set->color_primaries = 5;
    set->transfer_characteristics = 5;
  }
  else
  {
    strcpy(set->id_string, "MPEG-1 NTSC video and MPEG audio");
    set->aspectratio = 12;
    set->frame_rate_code = 4;
    set->display_vertical_size = 240;
    set->video_format = 2;
    set->color_primaries = 4;
    set->transfer_characteristics = 4;
  }
  strcpy(set->iqname, "");
  strcpy(set->niqname, "");
  strcpy(set->statname, "");
  set->video_type = MPEG_MPEG1;
  set->video_pulldown_flag = PULLDOWN_NONE;
  set->constrparms = FALSE;
  set->N = 15;
  set->M = 3;
  set->fieldpic = 0;
  set->frame_rate = ratetab[set->frame_rate_code - 1];
  set->tc0 = 0;
  set->hours = 0;
  set->mins = 0;
  set->sec = 0;
  set->tframe = 0;
  set->bit_rate = 1800000.0;
  set->max_bit_rate = 0.0;
  set->avg_bit_rate = 0.0;
  set->min_bit_rate = 0.0;
  set->auto_bitrate = 0;
  set->vbv_buffer_size = 23;
  set->fixed_vbv_delay = 0;
  set->constant_bitrate = 0;
  set->mquant_value = 4;
  set->low_delay = 0;
  set->profile = 4;
  set->level = 8;
  set->prog_seq = 1;
  set->chroma_format = 1;
  set->matrix_coefficients = 5;
  set->display_horizontal_size = 352;
  set->dc_prec = 0;
  set->topfirst = 0;
  set->embed_SVCD_user_blocks = 0;
  set->write_pde = 0;
  set->frame_centre_horizontal_offset = 0;
  set->frame_centre_vertical_offset = 0;

  for (i = 0; i < 3; i++)
  {
    set->frame_pred_dct_tab[i] = 1;
    set->conceal_tab[i] = 0;
    set->qscale_tab[i] = 0;
    set->intravlc_tab[i] = 0;
    set->altscan_tab[i] = 0;
  }
  set->repeatfirst = 0;
  set->prog_frame = 1;
  set->P = 0;
  set->r = 0;
  set->avg_act = 0;
  set->Xi = 0;
  set->Xp = 0;
  set->Xb = 0;
  set->d0i = 0;
  set->d0p = 0;
  set->d0b = 0;
  set->reset_d0pb = 1;
  set->min_frame_percent = 25;
  set->pad_frame_percent = 90;

  set->xmotion = 3;
  set->ymotion = 3;
  set->automotion = 1;
  set->maxmotion = 58;
  AutoSetMotionData(set);

  /* audio stuff */
  if (audioStereo)
    set->audio_mode = MPG_MD_STEREO;
  else
    set->audio_mode = MPG_MD_MONO;
  set->audio_layer = 2;
  set->psych_model = 2;
  set->audio_bitrate = 11;
  set->emphasis = 0;
  set->extension = 0;
  set->error_protection = 0;
  set->copyright = 0;
  set->original = 0;
  SetMPEG1Mplex(set);
  ChangeVideoFilename(set);
}

void SetMPEG1Mplex(struct mpegOutSettings *set)
{
  /* multiplex stuff */
  set->sectors_delay = 0;
  set->video_delay_ms = 180;
  set->audio_delay_ms = 180;
  set->audio1_delay_ms = 180;
  set->sector_size = VIDEOCD_SECTOR_SIZE;
  set->packets_per_pack = 1;
  set->audio_buffer_size = 4;
  set->audio1_buffer_size = 4;
  set->video_buffer_size = 46;
  set->always_sys_header = FALSE;
  set->mplex_type = MPEG_MPEG1;
  set->mplex_pulldown_flag = PULLDOWN_NONE;
  set->vcd_audio_pad = FALSE;
  set->user_mux_rate = 0;
  set->align_sequence_headers = 0;
  set->put_private2 = 0;
  set->frame_timestamps = TIMESTAMPS_ALL;
  set->VBR_multiplex = !set->constant_bitrate;
  set->use_computed_bitrate = COMPBITRATE_MAX;
  set->write_pec = 1;
  set->mux_SVCD_scan_offsets = 0;
  set->max_file_size = 0;
  set->mux_start_time = 0;
  set->mux_stop_time = 0;
  set->reset_clocks = 1;
  set->write_end_codes = 1;
  set->set_broken_link = 1;
  AutoSetBitrateData(set);
}

void SetVCDDefaults(struct mpegOutSettings *set, int palDefaults)
{
  int i;

  set->useFP = 0;
  set->verbose = 0;
  set->denoise = 0;
  set->MplexVideo = TRUE;
  set->MplexAudio = TRUE;
  set->UserEncodeVideo = TRUE;
  set->UserEncodeAudio = TRUE;
  set->EncodeVideo = TRUE;
  set->EncodeAudio = TRUE;
  set->SaveTempVideo = FALSE;
  set->SaveTempAudio = FALSE;
  strcpy(set->iqname, "");
  strcpy(set->niqname, "");
  strcpy(set->statname, "");
  set->video_type = MPEG_VCD;
  set->video_pulldown_flag = PULLDOWN_NONE;
  set->constrparms = TRUE;
  set->write_sde = 1;
  set->write_sec = 1;
  set->B_W = 0;
  if (palDefaults)
  {
    strcpy(set->id_string, "MPEG-1 VideoCD PAL video and MPEG audio");
    set->aspectratio = 8;
    set->frame_rate_code = 3;
    set->video_format = 1;
    set->color_primaries = 5;
    set->transfer_characteristics = 5;
    set->display_vertical_size = 288;
  }
  else
  {
    strcpy(set->id_string, "MPEG-1 VideoCD NTSC video and MPEG audio");
    set->aspectratio = 12;
    set->frame_rate_code = 4;
    set->video_format = 2;
    set->color_primaries = 4;
    set->transfer_characteristics = 4;
    set->display_vertical_size = 240;
  }
  set->N = 15;
  set->M = 3;
  set->fieldpic = 0;
  set->frame_rate = ratetab[set->frame_rate_code - 1];
  set->tc0 = 0;
  set->hours = 0;
  set->mins = 0;
  set->sec = 0;
  set->tframe = 0;
  set->bit_rate = 1150000.0; // max = 1151929.0;
  set->max_bit_rate = 1151929.0;
  set->avg_bit_rate = 0.0;
  set->min_bit_rate = 0.0;
  set->auto_bitrate = 0;
  set->vbv_buffer_size = 20;
  set->fixed_vbv_delay = 0;
  set->constant_bitrate = 1;
  set->mquant_value = 4;
  set->low_delay = 0;
  set->profile = 4;
  set->level = 8;
  set->prog_seq = 1;
  set->chroma_format = 1;
  set->matrix_coefficients = 5;
  set->display_horizontal_size = 352;
  set->dc_prec = 0;
  set->topfirst = 0;
  set->embed_SVCD_user_blocks = 0;
  set->write_pde = 0;
  set->frame_centre_horizontal_offset = 0;
  set->frame_centre_vertical_offset = 0;

  for (i = 0; i < 3; i++)
  {
    set->frame_pred_dct_tab[i] = 1;
    set->conceal_tab[i] = 0;
    set->qscale_tab[i] = 0;
    set->intravlc_tab[i] = 0;
    set->altscan_tab[i] = 0;
  }
  set->repeatfirst = 0;
  set->prog_frame = 1;
  set->P = 0;
  set->r = 0;
  set->avg_act = 0;
  set->Xi = 0;
  set->Xp = 0;
  set->Xb = 0;
  set->d0i = 0;
  set->d0p = 0;
  set->d0b = 0;
  set->reset_d0pb = 1;
  set->min_frame_percent = 25;
  set->pad_frame_percent = 90;

  set->xmotion = 3;
  set->ymotion = 3;
  set->automotion = 1;
  set->maxmotion = 58;
  AutoSetMotionData(set);

  /* audio stuff */
  if (audioStereo)
    set->audio_mode = MPG_MD_STEREO;
  else
    set->audio_mode = MPG_MD_MONO;
  set->audio_layer = 2;
  set->psych_model = 2;
  set->audio_bitrate = 11;
  set->emphasis = 0;
  set->extension = 0;
  set->error_protection = 0;
  set->copyright = 0;
  set->original = 0;
  SetVCDMplex(set);
  ChangeVideoFilename(set);
}

void SetVCDMplex(struct mpegOutSettings *set)
{
  /* multiplex stuff */
  set->sectors_delay = 400;
  set->video_delay_ms = 344;
  set->audio_delay_ms = 344;
  set->audio1_delay_ms = 344;
  set->sector_size = VIDEOCD_SECTOR_SIZE;
  set->packets_per_pack = 1;
  set->audio_buffer_size = 4;
  set->audio1_buffer_size = 4;
  set->video_buffer_size = 46;
  set->always_sys_header = FALSE;
  set->use_computed_bitrate = COMPBITRATE_NONE;
  set->mplex_type = MPEG_VCD;
  set->mplex_pulldown_flag = PULLDOWN_NONE;
  set->vcd_audio_pad = FALSE;
  set->user_mux_rate = 3528;
  set->align_sequence_headers = 0;
  set->put_private2 = 0;
  set->frame_timestamps = TIMESTAMPS_ALL;
  set->VBR_multiplex = 0;
  set->write_pec = 1;
  set->mux_SVCD_scan_offsets = 0;
  set->max_file_size = 0;
  set->mux_start_time = 0;
  set->mux_stop_time = 0;
  set->reset_clocks = 1;
  set->write_end_codes = 1;
  set->set_broken_link = 1;
  AutoSetBitrateData(set);
}

void ChangeVideoFilename(struct mpegOutSettings *set)
{
  char *tmpPtr;

  tmpPtr = strrchr(VideoFilename, 0x2e); // look for a .
  if (strlen(VideoFilename) && tmpPtr)
  {
    if (set->video_type < MPEG_MPEG2)
    {
      if (!strcmp(tmpPtr, ".m2v"))
        tmpPtr[2] = 0x31;
    }
    else
    {
      if (!strcmp(tmpPtr, ".m1v"))
        tmpPtr[2] = 0x32;
    }
  }
}

int HorzMotionCode(struct mpegOutSettings *set, int i)
{
  if (i < 8)
    return 1;
  if (i < 16)
    return 2;
  if (i < 32)
    return 3;
  if ((i < 64) || (set->constrparms))
    return 4;
  if (i < 128)
    return 5;
  if (i < 256)
    return 6;
  if ((i < 512) || (set->level == 10) || (set->video_type < MPEG_MPEG2))
    return 7;
  if ((i < 1024) || (set->level == 8))
    return 8;
  if (i < 2048)
    return 9;
  return 1;
}

int VertMotionCode(struct mpegOutSettings *set, int i)
{
  if (i < 8)
    return 1;
  if (i < 16)
    return 2;
  if (i < 32)
    return 3;
  if ((i < 64) || (set->level == 10) || (set->constrparms))
    return 4;
  return 5;
}

void AutoSetMotionData(struct mpegOutSettings *set)
{
  int i;

  if (set->M != 1)
  {
    for (i = 1; i < set->M; i++)
    {
      set->motion_data[i].sxf = set->xmotion * i;
      set->motion_data[i].forw_hor_f_code = HorzMotionCode(set, set->motion_data[i].sxf);
      set->motion_data[i].syf = set->ymotion * i;
      set->motion_data[i].forw_vert_f_code = VertMotionCode(set, set->motion_data[i].syf);
      set->motion_data[i].sxb = set->xmotion * (set->M - i);
      set->motion_data[i].back_hor_f_code = HorzMotionCode(set, set->motion_data[i].sxb);
      set->motion_data[i].syb = set->ymotion * (set->M - i);
      set->motion_data[i].back_vert_f_code = VertMotionCode(set, set->motion_data[i].syb);
    }
  }
  set->motion_data[0].sxf = set->xmotion * set->M;
  set->motion_data[0].forw_hor_f_code = HorzMotionCode(set, set->motion_data[0].sxf);
  set->motion_data[0].syf = set->ymotion * set->M;
  set->motion_data[0].forw_vert_f_code = VertMotionCode(set, set->motion_data[0].syf);
}

void AutoSetBitrateData(struct mpegOutSettings *set)
{
  if (!set->auto_bitrate)
    return;

  if (horizontal_size && vertical_size)
  {
    if (set->video_type == MPEG_VCD)
    {
      set->bit_rate = 1150000;
      set->vbv_buffer_size = 20;
      set->video_buffer_size = 46;
      return;
    }
    if (set->video_type == MPEG_SVCD)
    {
      set->bit_rate = svcdrates[set->audio_bitrate];
      set->vbv_buffer_size = 112;
      set->video_buffer_size = 230;
      return;
    }
    set->bit_rate = floor((double)horizontal_size * (double)vertical_size * (double)set->frame_rate * 0.74);
    set->vbv_buffer_size = (int)( floor(((double)set->bit_rate * 0.20343) / 
                                  16384.0) );
    if (set->video_type < MPEG_MPEG2)
    {
      if (set->vbv_buffer_size > 1023)
        set->vbv_buffer_size = 1023;
    }
    else
    {
      if (set->vbv_buffer_size > vbvlim[(set->level - 4) >> 1])
        set->vbv_buffer_size = vbvlim[(set->level - 4) >> 1];
    }
    if (set->mplex_type < MPEG_DVD)
      set->video_buffer_size = set->vbv_buffer_size << 1;
    else
      set->video_buffer_size = 232;
  }
}

//======================
//== PROFILE HANDLING ==
//======================
typedef struct 
{
  char *name;
  char type;
  void *pv;
  char *range;
} T_PARAM;

static struct mpegOutSettings iset;

static T_PARAM param_tab[] =
{
  //-- general settings --
  //----------------------
  {"useFloatingPoint",'b', &iset.useFP,            ""},
  {"verbose",         'b', &iset.verbose,          ""},
  {"denoise",         'b', &iset.denoise,          ""},
  
  //-- control flags --
  //-------------------
  {"multiplexVideo",  'b', &iset.MplexVideo,       ""},
  {"multiplexAudio",  'b', &iset.MplexAudio,       ""},
  {"userEncodeVideo", 'b', &iset.UserEncodeVideo,  ""},
  {"userEncodeAudio", 'b', &iset.UserEncodeAudio,  ""},
  {"encodeVideo",     'd', &iset.EncodeVideo,      ""},
  {"encodeAudio",     'd', &iset.EncodeAudio,      ""},
  {"saveTempVideo",   'b', &iset.SaveTempVideo,    ""},
  {"saveTempAudio",   'b', &iset.SaveTempAudio,    ""},  
  {"blackAndWhite",   'b', &iset.B_W,              ""},

  //-- name strings --
  //------------------
  {"description",        's', &iset.id_string,        ""},
  {"iqname",             's', &iset.iqname,           ""},
  {"intraQuantMatrix",   's', &iset.iqname,           ""},
  {"niqname",            's', &iset.niqname,          ""},
  {"nonIntraQuantMatrix",'s', &iset.niqname,          ""},
  {"statName",           'd', &iset.statname,         ""},

  //-- coding model --
  //------------------
  {"videoType",              'i', &iset.video_type,             "0:4"},
  {"videoPulldownFlag",      'b', &iset.video_pulldown_flag,    ""}, 
  {"constrParms",            'd', &iset.constrparms,            ""},
  {"iFramesInGOP",           'i', &iset.N,                      "1:15"},
  {"gop_size",               'i', &iset.N,                      "1:15"},
  {"ipFrameDistance",        'i', &iset.M,                      "1:3"},
  {"p_distance",             'i', &iset.M,                      "1:3"},
  {"intra_slice",            'i', &iset.P,                      "1:3"},
  {"tc0",                    'd', &iset.tc0,                    ""},
  {"firstFrameHours",        'i', &iset.hours,                  "0:10"},
  {"firstFrameMinutes",      'i', &iset.mins,                   "0:59"}, 
  {"firstFrameSeconds",      'i', &iset.sec,                    "0:59"},
  {"firstFrameFrame",        'i', &iset.tframe,                 "0:59"},
  {"fieldPictures",          'b', &iset.fieldpic,               ""},
  {"fieldpic",               'b', &iset.fieldpic,               ""},
  {"writeSequenceEndCodes",  'b', &iset.write_sec,              ""},
  {"use_seq_end",            'b', &iset.write_sec,              ""},
  {"embedSVCDUserBlocks",    'b', &iset.embed_SVCD_user_blocks, ""},

  //-- sequence spec. data (header) --
  //----------------------------------
  {"aspectRatio",         'i', &iset.aspectratio,            "1:12"},
  {"aspect",              'i', &iset.aspectratio,            "1:12"},
  {"frameRateCode",       'd', &iset.frame_rate_code,        ""}, 
  {"frame_rate",          'f', &iset.frame_rate,             "23.0:60.0"},
  {"constBitrate",        'f', &iset.bit_rate,               "500000.0:6000000.0"},
  {"cbr_bitrate",         'f', &iset.bit_rate,               "500000.0:6000000.0"},
  {"maxBitrate",          'f', &iset.max_bit_rate,           "500000.0:6000000.0"},
  {"max_bitrate",         'f', &iset.max_bit_rate,           "500000.0:6000000.0"},
  {"avgBitrate",          'f', &iset.avg_bit_rate,           "500000.0:6000000.0"},
  {"avg_bitrate",         'f', &iset.avg_bit_rate,           "500000.0:6000000.0"},
  {"minBitrate",          'f', &iset.min_bit_rate,           "500000.0:6000000.0"},
  {"min_bitrate",         'f', &iset.min_bit_rate,           "500000.0:6000000.0"},
  {"autoBitrate",         'b', &iset.auto_bitrate,           ""},
  {"vbvBufferSize",       'i', &iset.vbv_buffer_size,        "23:112"},
  {"vbv_buffer_size",     'i', &iset.vbv_buffer_size,        "23:112"},
  {"constBitrateFlag",    'b', &iset.constant_bitrate,       ""},
  {"cbr",                 'b', &iset.constant_bitrate,       ""},
  {"mquantValue",         'i', &iset.mquant_value,           "1:31"},
  {"quant_value",         'i', &iset.mquant_value,           "1:31"},
  {"max_mquant",          'i', &iset.maxquality,             "1:31"},
  {"min_mquant",          'i', &iset.minquality,             "1:31"},  
 
  //-- sequence spec. data (ext.) --
  //--------------------------------
  {"profile",             'd', &iset.profile,                ""},
  {"level",               'd', &iset.level,                  ""},
  {"progressiveSequence", 'b', &iset.prog_seq,               ""},
  {"prog_seq",            'b', &iset.prog_seq,               ""},
  {"chromaFormat",        'b', &iset.chroma_format,          ""},
  {"low_delay",           'b', &iset.low_delay,              ""},

  //-- motion data --
  //-----------------
  {"forwHorzFCodeP",       'i', &iset.motion_data[0].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeP",       'i', &iset.motion_data[0].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchP",      'i', &iset.motion_data[0].sxf,                ""},
  {"forwVertSearchP",      'i', &iset.motion_data[0].syf,                ""},
  {"forwHorzFCodeB1",      'i', &iset.motion_data[1].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB1",      'i', &iset.motion_data[1].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB1",     'i', &iset.motion_data[1].sxf,                ""},
  {"forwVertSearchB1",     'i', &iset.motion_data[1].syf,                ""},
  {"backHorzFCodeB1",      'i', &iset.motion_data[1].back_hor_f_code,    "0:9"},
  {"backVertFCodeB1",      'i', &iset.motion_data[1].back_vert_f_code,   "0:9"},
  {"backHorzSearchB1",     'i', &iset.motion_data[1].sxb,                ""},
  {"backVertSearchB1",     'i', &iset.motion_data[1].syb,                ""}, 
  {"forwHorzFCodeB2",      'i', &iset.motion_data[2].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB2",      'i', &iset.motion_data[2].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB2",     'i', &iset.motion_data[2].sxf,                ""},
  {"forwVertSearchB2",     'i', &iset.motion_data[2].syf,                ""},
  {"backHorzFCodeB2",      'i', &iset.motion_data[2].back_hor_f_code,    "0:9"},
  {"backVertFCodeB2",      'i', &iset.motion_data[2].back_vert_f_code,   "0:9"},
  {"backHorzSearchB2",     'i', &iset.motion_data[2].sxb,                ""},
  {"backVertSearchB2",     'i', &iset.motion_data[2].syb,                ""},
  {"forwHorzFCodeB3",      'i', &iset.motion_data[3].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB3",      'i', &iset.motion_data[3].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB3",     'i', &iset.motion_data[3].sxf,                ""},
  {"forwVertSearchB3",     'i', &iset.motion_data[3].syf,                ""},
  {"backHorzFCodeB3",      'i', &iset.motion_data[3].back_hor_f_code,    "0:9"},
  {"backVertFCodeB3",      'i', &iset.motion_data[3].back_vert_f_code,   "0:9"},
  {"backHorzSearchB3",     'i', &iset.motion_data[3].sxb,                ""},
  {"backVertSearchB3",     'i', &iset.motion_data[3].syb,                ""},
  {"forwHorzFCodeB4",      'i', &iset.motion_data[4].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB4",      'i', &iset.motion_data[4].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB4",     'i', &iset.motion_data[4].sxf,                ""},
  {"forwVertSearchB4",     'i', &iset.motion_data[4].syf,                ""},
  {"backHorzFCodeB4",      'i', &iset.motion_data[4].back_hor_f_code,    "0:9"},
  {"backVertFCodeB4",      'i', &iset.motion_data[4].back_vert_f_code,   "0:9"},
  {"backHorzSearchB4",     'i', &iset.motion_data[4].sxb,                ""},
  {"backVertSearchB4",     'i', &iset.motion_data[4].syb,                ""},
  {"forwHorzFCodeB5",      'i', &iset.motion_data[5].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB5",      'i', &iset.motion_data[5].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB5",     'i', &iset.motion_data[5].sxf,                ""},
  {"forwVertSearchB5",     'i', &iset.motion_data[5].syf,                ""},
  {"backHorzFCodeB5",      'i', &iset.motion_data[5].back_hor_f_code,    "0:9"},
  {"backVertFCodeB5",      'i', &iset.motion_data[5].back_vert_f_code,   "0:9"},
  {"backHorzSearchB5",     'i', &iset.motion_data[5].sxb,                ""},
  {"backVertSearchB5",     'i', &iset.motion_data[5].syb,                ""},
  {"forwHorzFCodeB6",      'i', &iset.motion_data[6].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB6",      'i', &iset.motion_data[6].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB6",     'i', &iset.motion_data[6].sxf,                ""},
  {"forwVertSearchB6",     'i', &iset.motion_data[6].syf,                ""},
  {"backHorzFCodeB6",      'i', &iset.motion_data[6].back_hor_f_code,    "0:9"},
  {"backVertFCodeB6",      'i', &iset.motion_data[6].back_vert_f_code,   "0:9"},
  {"backHorzSearchB6",     'i', &iset.motion_data[6].sxb,                ""},
  {"backVertSearchB6",     'i', &iset.motion_data[6].syb,                ""},
  {"forwHorzFCodeB7",      'i', &iset.motion_data[7].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB7",      'i', &iset.motion_data[7].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB7",     'i', &iset.motion_data[7].sxf,                ""},
  {"forwVertSearchB7",     'i', &iset.motion_data[7].syf,                ""},
  {"backHorzFCodeB7",      'i', &iset.motion_data[7].back_hor_f_code,    "0:9"},
  {"backVertFCodeB7",      'i', &iset.motion_data[7].back_vert_f_code,   "0:9"},
  {"backHorzSearchB7",     'i', &iset.motion_data[7].sxb,                ""},
  {"backVertSearchB7",     'i', &iset.motion_data[7].syb,                ""},
  {"forwHorzFCodeB8",      'i', &iset.motion_data[8].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB8",      'i', &iset.motion_data[8].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB8",     'i', &iset.motion_data[8].sxf,                ""},
  {"forwVertSearchB8",     'i', &iset.motion_data[8].syf,                ""},
  {"backHorzFCodeB8",      'i', &iset.motion_data[8].back_hor_f_code,    "0:9"},
  {"backVertFCodeB8",      'i', &iset.motion_data[8].back_vert_f_code,   "0:9"},
  {"backHorzSearchB8",     'i', &iset.motion_data[8].sxb,                ""},
  {"backVertSearchB8",     'i', &iset.motion_data[8].syb,                ""},
  {"forwHorzFCodeB9",      'i', &iset.motion_data[9].forw_hor_f_code,    "0:9"},
  {"forwVertFCodeB9",      'i', &iset.motion_data[9].forw_vert_f_code,   "0:9"},
  {"forwHorzSearchB9",     'i', &iset.motion_data[9].sxf,                ""},
  {"forwVertSearchB9",     'i', &iset.motion_data[9].syf,                ""},
  {"backHorzFCodeB9",      'i', &iset.motion_data[9].back_hor_f_code,    "0:9"},
  {"backVertFCodeB9",      'i', &iset.motion_data[9].back_vert_f_code,   "0:9"},
  {"backHorzSearchB9",     'i', &iset.motion_data[9].sxb,                ""},
  {"backVertSearchB9",     'i', &iset.motion_data[9].syb,                ""},
  {"forwHorzFCodeB10",     'i', &iset.motion_data[10].forw_hor_f_code,   "0:9"},
  {"forwVertFCodeB10",     'i', &iset.motion_data[10].forw_vert_f_code,  "0:9"},
  {"forwHorzSearchB10",    'i', &iset.motion_data[10].sxf,               ""},
  {"forwVertSearchB10",    'i', &iset.motion_data[10].syf,               ""},
  {"backHorzFCodeB10",     'i', &iset.motion_data[10].back_hor_f_code,   "0:9"},
  {"backVertFCodeB10",     'i', &iset.motion_data[10].back_vert_f_code,  "0:9"},
  {"backHorzSearchB10",    'i', &iset.motion_data[10].sxb,               ""},
  {"backVertSearchB10",    'i', &iset.motion_data[10].syb,               ""},
  {"forwHorzFCodeB11",     'i', &iset.motion_data[11].forw_hor_f_code,   "0:9"},
  {"forwVertFCodeB11",     'i', &iset.motion_data[11].forw_vert_f_code,  "0:9"},
  {"forwHorzSearchB11",    'i', &iset.motion_data[11].sxf,               ""},
  {"forwVertSearchB11",    'i', &iset.motion_data[11].syf,               ""},
  {"backHorzFCodeB11",     'i', &iset.motion_data[11].back_hor_f_code,   "0:9"},
  {"backVertFCodeB11",     'i', &iset.motion_data[11].back_vert_f_code,  "0:9"},
  {"backHorzSearchB11",    'i', &iset.motion_data[11].sxb,               ""},
  {"backVertSearchB11",    'i', &iset.motion_data[11].syb,               ""},
  {"forwHorzFCodeB12",     'i', &iset.motion_data[12].forw_hor_f_code,   "0:9"},
  {"forwVertFCodeB12",     'i', &iset.motion_data[12].forw_vert_f_code,  "0:9"},
  {"forwHorzSearchB12",    'i', &iset.motion_data[12].sxf,               ""},
  {"forwVertSearchB12",    'i', &iset.motion_data[12].syf,               ""},
  {"backHorzFCodeB12",     'i', &iset.motion_data[12].back_hor_f_code,   "0:9"},
  {"backVertFCodeB12",     'i', &iset.motion_data[12].back_vert_f_code,  "0:9"},
  {"backHorzSearchB12",    'i', &iset.motion_data[12].sxb,               ""},
  {"backVertSearchB12",    'i', &iset.motion_data[12].syb,               ""},
  {"forwHorzFCodeB13",     'i', &iset.motion_data[13].forw_hor_f_code,   "0:9"},
  {"forwVertFCodeB13",     'i', &iset.motion_data[13].forw_vert_f_code,  "0:9"},
  {"forwHorzSearchB13",    'i', &iset.motion_data[13].sxf,               ""},
  {"forwVertSearchB13",    'i', &iset.motion_data[13].syf,               ""},
  {"backHorzFCodeB13",     'i', &iset.motion_data[13].back_hor_f_code,   "0:9"},
  {"backVertFCodeB13",     'i', &iset.motion_data[13].back_vert_f_code,  "0:9"},
  {"backHorzSearchB13",    'i', &iset.motion_data[13].sxb,               ""},
  {"backVertSearchB13",    'i', &iset.motion_data[13].syb,               ""},
  {"forwHorzFCodeB14",     'i', &iset.motion_data[14].forw_hor_f_code,   "0:9"},
  {"forwVertFCodeB14",     'i', &iset.motion_data[14].forw_vert_f_code,  "0:9"},
  {"forwHorzSearchB14",    'i', &iset.motion_data[14].sxf,               ""},
  {"forwVertSearchB14",    'i', &iset.motion_data[14].syf,               ""},
  {"backHorzFCodeB14",     'i', &iset.motion_data[14].back_hor_f_code,   "0:9"},
  {"backVertFCodeB14",     'i', &iset.motion_data[14].back_vert_f_code,  "0:9"},
  {"backHorzSearchB14",    'i', &iset.motion_data[14].sxb,               ""},
  {"backVertSearchB14",    'i', &iset.motion_data[14].syb,               ""},
  {"forwHorzFCodeB15",     'i', &iset.motion_data[15].forw_hor_f_code,   "0:9"},
  {"forwVertFCodeB15",     'i', &iset.motion_data[15].forw_vert_f_code,  "0:9"},
  {"forwHorzSearchB15",    'i', &iset.motion_data[15].sxf,               ""},
  {"forwVertSearchB15",    'i', &iset.motion_data[15].syf,               ""},
  {"backHorzFCodeB15",     'i', &iset.motion_data[15].back_hor_f_code,   "0:9"},
  {"backVertFCodeB15",     'i', &iset.motion_data[15].back_vert_f_code,  "0:9"},
  {"backHorzSearchB15",    'i', &iset.motion_data[15].sxb,               ""},
  {"backVertSearchB15",    'i', &iset.motion_data[15].syb,               ""},
  {"autoVectorLengths",    'b', &iset.automotion,                        ""},
  {"horzPelMovement",      'i', &iset.xmotion,                           ""},
  {"vertPelMovement",      'i', &iset.ymotion,                           ""},
  {"variableMaxMotion",    'd', &iset.maxmotion,                         ""}, 

  // -- sequence specific data (sequence display extension) --
  // ---------------------------------------------------------
  {"writeSequenceDisplayExt", 'b', &iset.write_sde,                    ""},
  {"use_seq_dspext",          'b', &iset.write_sde,                    ""},
  {"videoFormat",             'd', &iset.video_format,                 ""},
  {"colorPrimaries",          'd', &iset.color_primaries,              ""},
  {"transferCharacteristics", 'd', &iset.transfer_characteristics,     ""},
  {"matrixCoefficients",      'd', &iset.matrix_coefficients,          ""},
  {"displayHorizontalSize",   'i', &iset.display_horizontal_size,      ""},
  {"displayVerticalSize",     'i', &iset.display_vertical_size,        ""},

  //-- picture specific data (picture coding extension) --
  //------------------------------------------------------
  {"intraDCPrec",             'i', &iset.dc_prec,                      "0:2"}, 
  {"dc_prec",                 'i', &iset.dc_prec,                      "0:2"},
  {"topFieldFirst",           'b', &iset.topfirst,                     ""},
  {"topfirst",                'b', &iset.topfirst,                     ""},

  //-- picture display extension --
  //-------------------------------
  {"writePictureDisplayExt",      'b', &iset.write_pde,                      ""},
  {"use_pic_dspext",              'b', &iset.write_pde,                      ""},
  {"frameCentreHorizontalOffset", 'd', &iset.frame_centre_horizontal_offset, ""}, 
  {"frameCentreVerticalOffset",   'd', &iset.frame_centre_vertical_offset,   ""},

  //-- use only frame prediction and frame DCT (I,P,B,current) --
  //-------------------------------------------------------------
  {"framePredDCTI",               'd', &iset.frame_pred_dct_tab[0],          ""},
  {"framePredDCTP",               'd', &iset.frame_pred_dct_tab[1],          ""},
  {"framePredDCTB",               'd', &iset.frame_pred_dct_tab[2],          ""},
  {"concealPredDCTI",             'd', &iset.conceal_tab[0],                 ""},
  {"concealPredDCTP",             'd', &iset.conceal_tab[1],                 ""},
  {"concealPredDCTB",             'd', &iset.conceal_tab[2],                 ""},
  {"quantizationScaleI",          'b', &iset.qscale_tab[0],                  "0:1"},
  {"quantizationScaleP",          'b', &iset.qscale_tab[1],                  "0:1"},
  {"quantizationScaleB",          'b', &iset.qscale_tab[2],                  "0:1"},
  {"intraVLCFormatI",             'd', &iset.intravlc_tab[0],                ""},
  {"intraVLCFormatP",             'd', &iset.intravlc_tab[1],                ""},
  {"intraVLCFormatB",             'd', &iset.intravlc_tab[2],                ""},
  {"alternateScanI",              'd', &iset.altscan_tab[0],                 ""},
  {"alternateScanP",              'd', &iset.altscan_tab[1],                 ""},
  {"alternateScanB",              'd', &iset.altscan_tab[2],                 ""},
  {"repeatFirstField",            'b', &iset.repeatfirst,                    ""}, 
  {"repeatfirst",                 'b', &iset.repeatfirst,                    ""},
  {"progressiveFrame",            'b', &iset.prog_frame,                     ""},
  {"prog_frame",                  'b', &iset.prog_frame,                     ""},
  {"qscale_type",                 'b', &iset.qscale_tab[0],                  "0:1"},
 
  //-- rate control vars --
  //-----------------------
  {"forceVBVDelay",               'b', &iset.fixed_vbv_delay,                ""},
  {"fixed_vbv_delay",             'b', &iset.fixed_vbv_delay,                ""},
  {"minFramePercentage",          'i', &iset.min_frame_percent,              "0:100"},
  {"padFramePercentage",          'i', &iset.pad_frame_percent,              "0:100"},
  /*
  !!  unsortiert !!
  int Xi;
  int Xp;
  int Xb;
  int r;
  int d0i;
  int d0p;
  int d0b;
  int reset_d0pb;
  int avg_act;
  */

  //-- audio vars --
  //----------------
  {"audioMode",                   'i', &iset.audio_mode,                     "0:3"},
  {"audioLayer",                  'i', &iset.audio_layer,                    "1:2"},
  {"psychModel",                  'i', &iset.psych_model,                    "1:2"},
  {"audioBitrate",                'i', &iset.audio_bitrate,                  "0:32"},   
  {"deEmphasis",                  'd', &iset.emphasis,                       ""},
  {"extension",                   'd', &iset.extension,                      ""},
  {"errorProtection",             'b', &iset.error_protection,               ""},
  {"copyrightBit",                'b', &iset.copyright,                      ""},
  {"originalBit",                 'b', &iset.original,                       ""},
  // privateBit is missing    

  //-- multiplex vars --
  //--------------------
  {"sectorDelay",                 'l', &iset.sectors_delay,                  ""},
  {"videoDelay",                  'l', &iset.video_delay_ms,                 ""},
  {"audioDelay",                  'l', &iset.audio_delay_ms,                 ""},
  {"audio1Delay",                 'l', &iset.audio1_delay_ms,                ""},
  {"sectorSize",                  'l', &iset.sector_size,                    "2048:2324"}, 
  {"sector_size",                 'l', &iset.sector_size,                    "2048,2324"},
  {"packetsPerPack",              'l', &iset.packets_per_pack,               ""},
  {"audioBufferSize",             'l', &iset.audio_buffer_size,              ""},
  {"audio1BufferSize",            'l', &iset.audio1_buffer_size,             ""}, 
  {"videoBufferSize",             'l', &iset.video_buffer_size,              "46:224"},
  {"video_buf_size",              'l', &iset.video_buffer_size,              "46:224"},
  {"alwaysWriteSysHeader",        'b', &iset.always_sys_header,              ""}, 
  {"always_sys_hdr",              'b', &iset.always_sys_header,              ""},
  {"useComputedBitrate",          'b', &iset.use_computed_bitrate,           ""},
  {"use_comp_bitrate",            'b', &iset.use_computed_bitrate,           ""},
  {"programStreamType",           'i', &iset.mplex_type,                     "0:4"},  
  {"muxPulldownFlag",             'd', &iset.mplex_pulldown_flag,            ""},
  {"padVCDAudio",                 'b', &iset.vcd_audio_pad,                  ""},
  {"alignSequenceHeaders",        'b', &iset.align_sequence_headers,         ""},
  {"align_seq_hdr",               'b', &iset.align_sequence_headers,         ""},
  {"userMuxRate",                 'l', &iset.user_mux_rate,                  "0:6972"},
  {"forced_mux_rate",             'l', &iset.user_mux_rate,                  "3528,6972"},
  {"usePrivateStream2",           'b', &iset.put_private2,                   ""},
  {"use_private2",                'b', &iset.put_private2,                   ""},
  {"frameTimestamps",             'i', &iset.frame_timestamps,               "0:2"},
  {"vbrMultiplex",                'b', &iset.VBR_multiplex,                  ""},
  {"vbr_mux",                     'b', &iset.VBR_multiplex,                  ""},
  {"writeProgramEndCode",         'b', &iset.write_pec,                      ""},
  {"use_prg_end",                 'b', &iset.write_pec,                      ""},
  {"muxSVCDScanOffsets",          'b', &iset.mux_SVCD_scan_offsets,          ""},
  {"svcd_scan_ofs",               'b', &iset.mux_SVCD_scan_offsets,          ""},
  {"maxFileSize",                 'i', &iset.max_file_size,                  ""},
  {"max_file_size",               'i', &iset.max_file_size,                  ""},
  {"muxStartTime",                'i', &iset.mux_start_time,                 ""}, 
  {"muxStopTime",                 'i', &iset.mux_stop_time,                  ""},
  {"resetClocks",                 'b', &iset.reset_clocks,                   ""},
  {"writeEndCodes",               'b', &iset.write_end_codes,                ""},
  {"setBrokenLink",               'b', &iset.set_broken_link,                ""},

  {NULL, ' ', NULL, ""}
};

static void validate_settings()
{
  int i;
  
  //-- check frame_rate --
  //----------------------
  if (iset.frame_rate != ratetab[iset.frame_rate_code-1])
  { 
    int hit = 0;
    
    for (i=0; i<8; i++)
    {
      if ((int)(ratetab[i] * 100.0) == (int)(iset.frame_rate * 100.0 + 0.01)) 
      {
        iset.frame_rate_code = i+1;
        iset.frame_rate = ratetab[i];
        hit = 1;
        break;
      }  
    }
    
    if (!hit) 
    {
      fprintf(stderr, "WARNING: unrecognized value (%1.2f) for (frame_rate) !\n", iset.frame_rate);
      fprintf(stderr, "  -> forcing 25.0 fps\n");
      
      iset.frame_rate_code = 3;
      iset.frame_rate      = 25.0;
    }
    
  }
  
  
  //-- check VBR-mode --
  //--------------------
  if (iset.constant_bitrate == iset.VBR_multiplex)
  {
    fprintf(stderr, "WARNING: parameters (cbr) and (vbr_mux) dosn't harmonize !\n");
    fprintf(stderr, "  -> forcing VBR\n");
    iset.constant_bitrate = 0;
    iset.VBR_multiplex    = 1;    
  }

  //-- check bitrate-value for CBR-mode --
  //--------------------------------------  
  if (iset.constant_bitrate)
  {
    if (iset.bit_rate < 500000) 
    {
      if (iset.max_bit_rate < 500000) 
        iset.bit_rate = 500000;
      else
        iset.bit_rate = iset.max_bit_rate;
    }  
  }
  
  //-- check interlaced mode --
  //---------------------------
  if (iset.prog_seq != iset.prog_frame)
  {
    fprintf(stderr, "WARNING: parameters (prog_seq) and (prog_frame) dosn't harmonize !\n");
    fprintf(stderr, "  -> forcing interlaced\n");
  
    iset.prog_seq   = 0;
    iset.prog_frame = 0;
  }
  
  //-- check user-muxrate --
  //------------------------
  if (!iset.use_computed_bitrate && iset.user_mux_rate == 0.0)
  {
    fprintf(stderr, "WARNING: no or wrong value for (forced_mux_rate)\n");
    fprintf(stderr, "  -> forcing to compute mux-rate\n");
    iset.use_computed_bitrate = 1;
    iset.user_mux_rate        = 0.0;
  }
  
  //-- check qscale_type ( 0=linear / 1=non-linear) --
  //-- set it even for all picture types !          --
  //--------------------------------------------------
  for (i=1; i<3; i++) iset.qscale_tab[i] = iset.qscale_tab[0];
}

static void set_param(char *p, char *v, int sh_info)
{
  int i   = 0;
  int hit = 0;

  unsigned long *l_p;  
  double        *d_p;
  int           *b_p;
  int           *i_p;
  char          *c_p;
  
  while (param_tab[i].name) 
  {
    if (!strcasecmp(p, param_tab[i].name))
    {
      hit = 1;
      break; 
    } 
    i++; 
  }
  
  if (hit)
  {
    if (sh_info) fprintf(stderr, "  %s = %s\n", p, v);
    switch (param_tab[i].type)
    {
      case 'i':
        i_p  = (int *)param_tab[i].pv;
        *i_p = atoi(v); 
        break;
        
      case 'l':
        l_p  = (unsigned long *)param_tab[i].pv;
        *l_p = (unsigned long)strtod(v, NULL); 
        break;
        
      case 'b':
        b_p  = (int *)param_tab[i].pv;
        *b_p = (v[0] != '0'); 
        break;
      
      case 'f':
        d_p = (double *)param_tab[i].pv;
        *d_p = strtod(v, NULL);
        break;
        
      case 'c':
        c_p = (char *)param_tab[i].pv;
        *c_p = v[0];
        break;
        
      case 's':
        strcpy((char *)param_tab[i].pv, v);
        break;            

      case 'd':
        // this is only a dummy
        // option reserved for further use ...
	fprintf(stderr, "IGNORE: (%s)\n", p); 
        break; 
    }
  }
  else
     fprintf(stderr, "ERROR: unknown parameter (%s) - ignored\n",  p);
}

static void rem_white_spaces(char *src)
{
  char *dst = src;
  
  while (*src) 
  {
    if ( (*src != ' ') && (*src != '\t') && (*src != '\n') )
    {
      *dst = *src;
      dst++;
    }  
    src++;  
  }
  *dst = '\0';
}

//=====================
//== public routines ==
//=====================

mpegOutSettings *bb_get_profile()
{
  return(&iset);
}

int bb_set_profile(char *profile_name, char ref_type, int tv_type, int asr, int frc, int pulldown, int sh_info, int bitrate, int max_bitrate)
{
  FILE *profile = NULL;
  char line[128];
  char param[64];
  char value[32];
  char *pstr;
  int  line_cnt = 0;
  
  fprintf(stderr, "\n");
  
  //-- load reference profile --
  //----------------------------
  switch (tolower(ref_type))
  {
      //-- PAL, VBR, 2048 packets, ... --
      case PRO_DVD:
        if (sh_info)
          fprintf(stderr, "INFO: using reference profile (DVD)\n");
        SetDVDDefaults(&iset, tv_type);
        break;

      //-- PAL, VBR, 2324 packets, svcd scan-offsets, --
      //-- align sequence header, video-buffer 230K   --
      case PRO_SVCD:
        if (sh_info)
          fprintf(stderr, "INFO: using reference profile (SVCD)\n");
        SetSVCDDefaults(&iset, tv_type);   // flag signals PAL 
        break;

      //-- PAL, CBR 1152, 2324 packets --
      case PRO_VCD:
        if (sh_info)
          fprintf(stderr, "INFO: using reference profile (VCD)\n");
        SetVCDDefaults(&iset, tv_type);   // flag signals PAL 
        break;

      //-- PAL, VBR, 2324 packets, video-buffer 224K --
      case PRO_MPEG2:
        if (sh_info)
          fprintf(stderr, "INFO: using reference profile (MPEG2)\n");
        SetMPEG2Defaults(&iset, tv_type);
        break;

      //-- PAL, VBR, 2324 packets, video-buffer 224K --
      case PRO_MPEG1_BIG:
        if (sh_info)
          fprintf(stderr, "INFO: using reference profile (MPEG1_BIG)\n");
        SetMPEG1Defaults(&iset, tv_type);
        iset.vbv_buffer_size   = 112;
        iset.video_buffer_size = 224;
        break;
  
      //-- PAL, VBR, 2324 packets, video-buffer 46K --
      case PRO_MPEG1:
      default:
        if (sh_info)
          fprintf(stderr, "INFO: using reference profile (MPEG1)\n");
        SetMPEG1Defaults(&iset, tv_type);
        break;
	
  }

  // set bitrate passed by -w
  if (tolower(ref_type) !=  PRO_VCD) {
    if (bitrate > 0) {
      iset.bit_rate=bitrate*1000.0;
      if(bitrate >= 8000 && iset.mquant_value > 2)
	iset.mquant_value=2;
    }

    if (max_bitrate > 0)
      iset.max_bit_rate=max_bitrate*1000.0;
  }
  
  if (sh_info)
    fprintf(stderr, "INFO: profile type is (%s)\n", tv_type ? "PAL":"NTSC");

  //ThOe
  if(pulldown) { 
    
    if(frc==2) {
      // 24->30
      iset.video_pulldown_flag=PULLDOWN_23;
      iset.mplex_pulldown_flag=PULLDOWN_23;
    } else {
      
      //23.976 -> 29.97
      iset.video_pulldown_flag=PULLDOWN_32;
      iset.mplex_pulldown_flag=PULLDOWN_32;
    }
    fprintf(stderr, "INFO: 3:2 pulldown flags enabled\n");
  }
 
  //ThOe
  if(frc) {
    iset.frame_rate = ratetab[frc-1];
    iset.frame_rate_code = frc;
  }

  //-- if valid, override aspect-ratio with value from source video -- 
  switch (asr)
  {
    case 1:
    case 2:
    case 3:
    case 4:
    case 8:
    case 12:
      iset.aspectratio = asr; 
      break;
      
    default:
      break;
  }
    
  if (!profile_name) 
  { 
    //-- now take settings from temporal storage -- 
    //---------------------------------------------
    GetTempSettings(&iset);
    return 1;  
  }
  
  //-- process user profile --
  //--------------------------
  if ( (profile = fopen(profile_name, "r")) != NULL )
  {
    if (sh_info)
      fprintf(stderr, "INFO: mixing up parameters from profile (%s)\n", profile_name);
     
    //-- set parameters from user profile (line by line) --
    //-----------------------------------------------------
    while (fgets(line, 128, profile))
    {
      line_cnt++;
      
      //-- skip comments --
      pstr = strchr(line, '#');
      if (pstr) *pstr='\0';
      
      //-- remove whitespaces --
      rem_white_spaces(line);
      
      if (strlen(line))
      {
        if ( (pstr = strchr(line, '=')) != NULL ) 
        {
          *pstr = '\0';
          strcpy(param, line);
          strcpy(value, pstr+1);
          
          set_param(param, value, sh_info);
        }
        else
        {
          fprintf(stderr, "ERROR: syntax error in profile (%s), line (%d)\n",
                  profile_name, line_cnt);
          fprintf(stderr, "  parameter ignored\n");
        }
      }
    } 
    fclose(profile);

    //-- perhaps some adjustments --
    //------------------------------ 
    validate_settings();

    //-- now take settings from temporal storage -- 
    //---------------------------------------------
    GetTempSettings(&iset);

    return 1;
  }
  else
    fprintf(stderr, "ERROR: opening profile (%s)\n", profile_name); 

  //-- now take settings from temporal storage -- 
  //---------------------------------------------
  GetTempSettings(&iset);
  
  return 0;  
}

void bb_gen_profile(void)
{
  int  i = 0;
  char *pstr;
  char value[128];
  char comment[128];
  char hlpstr[128];
  
  printf(
         "#---------------------------------------------------\n"
         "#-- Profile Template for bbencode and bbmplex     --\n"
         "#---------------------------------------------------\n"
         "#-- list of all possible parameters               --\n" 
         "#-- uncomment your parameters and set your value, --\n" 
         "#-- preset values are suggestions only to get the --\n"
         "#-- imagination of the useable range !            --\n"   
         "#---------------------------------------------------\n"
         "#\n"
        );
  
  while (param_tab[i].name) 
  {
    strcpy(value, "");
    strcpy(hlpstr, param_tab[i].range);
    
    if ( strlen(hlpstr) )
    {
      if ( (pstr = strchr(hlpstr, ':')) != NULL )
      {
        *pstr = '\0';
        strcpy(value, pstr+1);
        sprintf(comment, "suggested minimum (%s)", hlpstr);
      }  
      else if ( (pstr = strchr(hlpstr, ',')) != NULL)
      {
        *pstr = '\0';     
        strcpy(value, hlpstr);
        sprintf(comment, "other values: %s", pstr+1);
      }
      else
      {
        strcpy(value, param_tab[i].range);
        strcpy(comment, "");
      }
    }  
        
    if (!strlen(value))
    {
      strcpy(value, "0");     
      strcpy(comment, "boolean (or no suggestion)");
    }
    
    printf("#%s = %s  # %s\n", param_tab[i].name, value, comment);
    i++;
  }
   
}
