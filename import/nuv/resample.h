#ifndef RESAMPLE_H
#define RESAMPLE_H
int resample_stop(char *stopo);
int resample_flow(char *flowi, int isamp, char *flowo);
int resample_init(int irate, int orate);
#endif
