
# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I. 

noinst_LIBRARIES = libbbmpeg.a

if HAVE_ASM_NASM
BB_SRC2 = fdct_mmx.s idct_mmx.s quant_mmx.s motion_mmx.s variance_mmx.s
endif

if HAVE_MMX 
BB_SRC1 = fdctdata.c idctdata.c quantdata.c predict_mmx.S 
MMX=-DHAVE_MMX
endif

if HAVE_SSE
BB_SRC3 = predict_sse.S
endif

CFLAGS = -funroll-loops -ffast-math @CFLAGS@ $(MMX)

libbbmpeg_a_SOURCES = dovideo.c params.c bits.c puthdr.c putpic.c putmpg.c putvlc.c stats.c quantize.c predict.c ratectl.c idct.c motion.c transfrm.c fdctref.c resize.c $(BB_SRC1) $(BB_SRC2) $(BB_SRC3)

noinst_HEADERS = mtable.h mplex.h mmx.h main.h imgresample.h gmo_read.h encaudio.h consts.h consts1.h common.h bbencode.h 

EXTRA_DIST = README.TXT *.cfg *.h *.S *.s mmxasm.c

SUBDIRS = . tcmplex

ASM = @NASM@
ASMFLAGS = @NASMFLAGS@

%.o: %.s ;	$(ASM) -g $(ASMFLAGS) -o $@ $<



