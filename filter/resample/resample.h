#ifndef RESAMPLE_H
#define RESAMPLE_H
int filter_resample_stop(char *stopo);
int filter_resample_flow(char *flowi, int isamp, char *flowo);
int filter_resample_init(int irate, int orate);
#endif
