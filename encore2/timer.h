#ifndef _ENCORE_TIMER_H
#define _ENCORE_TIMER_H

#if (defined(LINUX) && defined(PROFILING))

#include "portab.h"
struct timer2
{	
    int64_t dct_times;
    int64_t idct_times;
    int64_t quant_times;
    int64_t iquant_times;
    int64_t motest_times;
    int64_t inter_times;
    int64_t conv_times;
    int64_t trans_times;
    int64_t current;
};
extern struct timer2 tim2;
#ifdef __cplusplus
extern "C" {
#endif
extern void start_etimer();
extern void stop_dct_etimer();
extern void stop_idct_etimer();
extern void stop_motest_etimer();
extern void stop_inter_etimer();
extern void stop_quant_etimer();
extern void stop_iquant_etimer();
extern void stop_conv_etimer();
extern void stop_transfer_etimer();
extern void clear_etimer();
extern void write_etimer();
#ifdef __cplusplus
};
#endif

#else
static __inline void start_etimer(){}
static __inline void stop_dct_etimer(){}
static __inline void stop_idct_etimer(){}
static __inline void stop_motest_etimer(){}
static __inline void stop_inter_etimer(){}
static __inline void stop_quant_etimer(){}
static __inline void stop_iquant_etimer(){}
static __inline void stop_conv_etimer(){}
static __inline void stop_transfer_etimer(){}
static __inline void clear_etimer(){}
static __inline void write_etimer(){}
#endif

#endif
