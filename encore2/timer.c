#include "timer.h"
#if (defined(LINUX) && defined(PROFILING))
struct timer2 tim2;
static inline int64_t read_counter()
{
    int64_t a;
    uint32_t a1, a2;
    __asm__ __volatile__("rdtsc\n\t"
    :"=a"(a1), "=d"(a2)
    );
    a=((uint64_t)a1) | ((uint64_t)a2 << 32);
    return a;
}

void start_etimer()
{
    tim2.current=read_counter();
}

void stop_dct_etimer()
{
    tim2.dct_times+=read_counter()-tim2.current;
}
void stop_idct_etimer()
{
    tim2.idct_times+=read_counter()-tim2.current;
}
void stop_quant_etimer()
{
    tim2.quant_times+=read_counter()-tim2.current;
}
void stop_iquant_etimer()
{
    tim2.iquant_times+=read_counter()-tim2.current;
}
void stop_motest_etimer()
{
    tim2.motest_times+=read_counter()-tim2.current;
}
void stop_inter_etimer()
{
    tim2.inter_times+=read_counter()-tim2.current;
}
void stop_conv_etimer()
{
    tim2.conv_times+=read_counter()-tim2.current;
}
void stop_transfer_etimer()
{
    tim2.trans_times+=read_counter()-tim2.current;
}

void clear_etimer()
{
    tim2.dct_times=tim2.quant_times=tim2.idct_times=tim2.iquant_times=tim2.motest_times=tim2.conv_times=tim2.inter_times=tim2.trans_times=0LL;
}
extern const double frequency;
void write_etimer()
{
    if(tim2.dct_times || tim2.iquant_times)
	printf("DCT: %f ms\nQuant: %f ms\nIDCT: %f ms\nIQuant: %f ms\n"
	"Mot est/comp: %f ms\nInterpolation: %f ms\nRGB2YUV: %f ms\nTransfer: %f ms\n", 
	(float)(tim2.dct_times/frequency),
	(float)(tim2.quant_times/frequency),
	(float)(tim2.idct_times/frequency),
	(float)(tim2.iquant_times/frequency),
	(float)(tim2.motest_times/frequency),
	(float)(tim2.inter_times/frequency),
	(float)(tim2.conv_times/frequency),
	(float)(tim2.trans_times/frequency)	
	);
}
#endif
