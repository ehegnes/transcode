#ifndef _static_xio_h
#define _static_xio_h

#include <xio.h>
#include <fcntl.h>
#include <unistd.h>

void dummy(void);
void dummy(void)
{
	int i;
	struct stat dummy;

	i = xio_open("", O_RDONLY);
	xio_read(i, NULL, 0);
	xio_write(i, NULL, 0);
	xio_ftruncate(i, 0);
	xio_lseek(i, 0, 0);
	xio_fstat(i, &dummy);
	xio_lstat("", &dummy);
	xio_stat("", &dummy);
	xio_rename("", "");
	xio_close(i);
}

#endif
