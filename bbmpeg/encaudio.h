/* encoder.h */

//#define CREATOR_ENCODE  'MpgD'

/* This is the smallest MNR a subband can have before it is counted
   as 'noisy' by the logic which chooses the number of JS subbands */

#define NOISY_MIN_MNR   0.0

/* Psychacoustic Model 1 Definitions */

#define CB_FRACTION     0.33
#define MAX_SNR         1000
#define NOISE           10
#define TONE            20
#define DBMIN           -200.0
#define LAST            -1
#define STOP            -100
#define POWERNORM       90.3090 /* = 20 * log10(32768) to normalize */
                                /* max output power to 96 dB per spec */

/* Psychoacoustic Model 2 Definitions */

#define LOGBLKSIZE      10
#define BLKSIZE         1024
#define HBLKSIZE        513
#define CBANDS          63
#define LXMIN           32.0

/***********************************************************************
*
*  Encoder Type Definitions
*
***********************************************************************/

/* Psychoacoustic Model 1 Type Definitions */

typedef int        IFFT2[FFT_SIZE/2];
typedef int        IFFT[FFT_SIZE];
typedef double     D9[9];
typedef double     D10[10];
typedef double     D640[640];
typedef double     D1408[1408];
typedef double     DFFT2[FFT_SIZE/2];
typedef double     DFFT[FFT_SIZE];
typedef double     DSBL[SBLIMIT];
typedef double     D2SBL[2][SBLIMIT];

typedef struct {
        int        line;
        double     bark, hear, x;
} g_thres, *g_ptr;

typedef struct {
        double     x;
        int        type, next, map;
} mask, *mask_ptr;

/* Psychoacoustic Model 2 Type Definitions */

typedef int        ICB[CBANDS];
typedef int        IHBLK[HBLKSIZE];
typedef double      F32[32];
typedef double      F2_32[2][32];
typedef double      FCB[CBANDS];
typedef double      FCBCB[CBANDS][CBANDS];
typedef double      FBLK[BLKSIZE];
typedef double      FHBLK[HBLKSIZE];
typedef double      F2HBLK[2][HBLKSIZE];
typedef double      F22HBLK[2][2][HBLKSIZE];
typedef double     DCBS[CBANDS];

/***********************************************************************
*
*  Encoder Function Prototype Declarations
*
***********************************************************************/

/* The following functions are in the file "doaudio.cpp" */

int doaudio();

/* The following functions are in the file "encode.cpp" */

void   encode_init();
int   fill_buffer();
int   init_read_samples();
int   read_samples(int);
unsigned long    get_audio(short[2][1152], int, int);
void   window_subband(short**, double[HAN_SIZE], int);
void   create_ana_filter(double[SBLIMIT][64]);
void   filter_subband(double[HAN_SIZE], double[SBLIMIT]);
void   encode_info(frame_params*);
double mod(double);
void   I_combine_LR(double[2][3][SCALE_BLOCK][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT]);
void   II_combine_LR(double[2][3][SCALE_BLOCK][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT], int);
void   I_scale_factor_calc(double[][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[][3][SBLIMIT], int);
void   II_scale_factor_calc(double[][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[][3][SBLIMIT], int, int);
void   pick_scale(unsigned int[2][3][SBLIMIT], frame_params*,
                           double[2][SBLIMIT]);
void   put_scale(unsigned int[2][3][SBLIMIT], frame_params*,
                           double[2][SBLIMIT]);
void   II_transmission_pattern(unsigned int[2][3][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*);
void   II_encode_scale(unsigned int[2][SBLIMIT],
                           unsigned int[2][SBLIMIT],
                           unsigned int[2][3][SBLIMIT], frame_params*);
void   I_encode_scale(unsigned int[2][3][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*);
int    II_bits_for_nonoise(double[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           frame_params*);
void   II_main_bit_allocation(double[2][SBLIMIT],
                           unsigned int[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           int*, frame_params*);
int    II_a_bit_allocation(double[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           unsigned int[2][SBLIMIT], int*, frame_params*);
int    I_bits_for_nonoise(double[2][SBLIMIT], frame_params*);
void   I_main_bit_allocation(double[2][SBLIMIT],
                           unsigned int[2][SBLIMIT], int*, frame_params*);
int    I_a_bit_allocation(double[2][SBLIMIT], unsigned int[2][SBLIMIT],
                           int*, frame_params*);
void   I_subband_quantization(unsigned int[2][3][SBLIMIT],
                           double[2][3][SCALE_BLOCK][SBLIMIT], unsigned int[3][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT], unsigned int[2][SBLIMIT],
                           unsigned int[2][3][SCALE_BLOCK][SBLIMIT], frame_params*);
void   II_subband_quantization(unsigned int[2][3][SBLIMIT],
                           double[2][3][SCALE_BLOCK][SBLIMIT], unsigned int[3][SBLIMIT],
                           double[3][SCALE_BLOCK][SBLIMIT], unsigned int[2][SBLIMIT],
                           unsigned int[2][3][SCALE_BLOCK][SBLIMIT], frame_params*);
void   II_encode_bit_alloc(unsigned int[2][SBLIMIT], frame_params*);
void   I_encode_bit_alloc(unsigned int[2][SBLIMIT], frame_params*);
void   I_sample_encoding(unsigned int[2][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*);
void   II_sample_encoding(unsigned int[2][3][SCALE_BLOCK][SBLIMIT],
                           unsigned int[2][SBLIMIT], frame_params*);
void   encode_CRC(unsigned int);

/* The following functions are in the file "tonal.cpp" */

void tonal_init();
void        read_cbound(int, int);
void        read_freq_band(g_ptr*, int, int);
void        make_map(mask[HAN_SIZE], g_thres*);
double      add_db(double, double);
void        II_f_f_t(double[FFT_SIZE], mask[HAN_SIZE]);
void        II_hann_win(double[FFT_SIZE]);
void        II_pick_max(mask[HAN_SIZE], double[SBLIMIT]);
void        II_tonal_label(mask[HAN_SIZE], int*);
void        noise_label(mask*, int*, g_thres*);
void        subsampling(mask[HAN_SIZE], g_thres*, int*, int*);
void        threshold(mask[HAN_SIZE], g_thres*, int*, int*, int);
void        II_minimum_mask(g_thres*, double[SBLIMIT], int);
void        II_smr(double[SBLIMIT], double[SBLIMIT], double[SBLIMIT],
                           int);
void        II_Psycho_One(short[2][1152], double[2][SBLIMIT],
                           double[2][SBLIMIT], frame_params*);
void        I_f_f_t(double[FFT_SIZE/2], mask[HAN_SIZE/2]);
void        I_hann_win(double[FFT_SIZE/2]);
void        I_pick_max(mask[HAN_SIZE/2], double[SBLIMIT]);
void        I_tonal_label(mask[HAN_SIZE/2], int*);
void        I_minimum_mask(g_thres*, double[SBLIMIT]);
void        I_smr(double[SBLIMIT], double[SBLIMIT], double[SBLIMIT]);
void        I_Psycho_One(short[2][1152], double[2][SBLIMIT],
                           double[2][SBLIMIT], frame_params*);

/* The following functions are in the file "psy.cpp" */

void        psy_init();
void        psycho_anal(short int*, short int[1056], int, int,
                           double[32], double);

/* The following functions are in the file "subs.cpp" of "fft.cpp" */

//void subs_init();
//void        fft(double[BLKSIZE], double[BLKSIZE], double[BLKSIZE],
//                           double[BLKSIZE], int );
void fft(double *x_real, double *x_imag, double *energy, double *phi, int N);

