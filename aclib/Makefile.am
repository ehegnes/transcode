# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

noinst_LIBRARIES = libac.a

if HAVE_ASM_NASM
ASM_SRC = memcpy.s rescale.s average.s swap.s asm_io.s
endif

CFLAGS = -funroll-loops -ffast-math -DLINUX @CFLAGS@

libac_a_SOURCES = cputest.c $(ASM_SRC)

EXTRA_DIST = \
  ac.h \
  asm_io.inc \
  asm_io.s \
  average.s \
  cputest.h \
  memcpy.s \
  rescale.s \
  swap.s \
  movtest.c actest.c make.sh

ASM = @NASM@
ASMFLAGS = @NASMFLAGS@ -g

%.o: %.s ;	$(ASM) $(ASMFLAGS) -o $@ $<

