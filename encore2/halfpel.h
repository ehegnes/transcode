#include "portab.h"

void interpolate_halfpel_h(
	uint8_t *src, 
	uint8_t *dstH, 
	int width,  /* width % 16 == 0 */
	int height,
	int rounding);

void interpolate_halfpel_v(
	uint8_t *src, 
	uint8_t *dstV, 
	int width,  /* width % 16 == 0 */
	int height,
	int rounding);
	
void interpolate_halfpel_hv(
	uint8_t *src, 
	uint8_t *dstHV, 
	int width,  /* width % 16 == 0 */
	int height,
	int rounding);
