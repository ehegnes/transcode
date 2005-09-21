#ifndef _static_xio_h
#define _static_xio_h

#include <xio.h>
#include <fcntl.h>
#include <unistd.h>

void
dummy()
{
	int i;
	struct stat tmp;

	i = xio_open("", O_RDONLY);
	xio_read(i, NULL, 0);
	xio_write(i, NULL, 0);
	xio_ftruncate(i, 0);
	xio_lseek(i, 0, 0);
	xio_fstat(i, &tmp);
	xio_lstat("", &tmp);
	xio_stat("", &tmp);
	xio_rename("", "");
	xio_close(i);
}

#endif
