#include <string.h>
#include <stdio.h>
#include "ac.h"

/*************************************************************************/

#ifdef ARCH_X86

void *ac_memcpy_mmx(void *dest, const void *src, size_t bytes)
{
    asm("\
PENTIUM_LINE_SIZE = 32		# PMMX/PII cache line size		\n\
PENTIUM_CACHE_SIZE = 8192	# PMMX/PII total cache size		\n\
# Leave room because writes may touch the cache too (PII)		\n\
PENTIUM_CACHE_BLOCK = (PENTIUM_CACHE_SIZE - PENTIUM_LINE_SIZE*2)	\n\
									\n\
	push %%ebx		# save PIC register			\n\
									\n\
	mov %%eax, %%edi	# dest					\n\
	mov %%edx, %%esi	# src					\n\
	#mov %%ecx, %%ecx	# bytes					\n\
	mov %%ecx, %%edx						\n\
									\n\
	mov $64, %%eax		# constant				\n\
									\n\
	cmp %%eax, %%ecx						\n\
	jb .lastcopy		# Just rep movs if <64 bytes		\n\
									\n\
	# Align destination pointer to a multiple of 8 bytes		\n\
	xor %%ecx, %%ecx						\n\
	mov $7, %%cl							\n\
	and %%edi, %%ecx						\n\
	jz .blockloop		# already aligned (edi%%8 == 0)		\n\
	neg %%cl							\n\
	and $7, %%cl		# ecx = 8 - edi%%8			\n\
	sub %%ecx, %%edx	# subtract from count			\n\
	rep movsb		# and copy				\n\
									\n\
.blockloop:								\n\
	mov %%edx, %%ebx						\n\
	shr $6, %%ebx							\n\
	jz .lastcopy		# <64 bytes left			\n\
	cmp $PENTIUM_CACHE_BLOCK/64, %%ebx				\n\
	jb .small							\n\
	mov $PENTIUM_CACHE_BLOCK/64, %%ebx				\n\
.small:									\n\
	mov %%ebx, %%ecx						\n\
	shl $6, %%ecx							\n\
	sub %%ecx, %%edx						\n\
	add %%ecx, %%esi						\n\
.loop1:									\n\
	test -32(%%esi), %%eax	# touch each cache line in reverse order\n\
	test -64(%%esi), %%eax						\n\
	sub %%eax, %%esi						\n\
	dec %%ebx							\n\
	jnz .loop1							\n\
	shr $6, %%ecx							\n\
.loop2:									\n\
	movq   (%%esi), %%mm0	# do the actual copy, 64 bytes at a time\n\
	movq  8(%%esi), %%mm1						\n\
	movq 16(%%esi), %%mm2						\n\
	movq 24(%%esi), %%mm3						\n\
	movq 32(%%esi), %%mm4						\n\
	movq 40(%%esi), %%mm5						\n\
	movq 48(%%esi), %%mm6						\n\
	movq 56(%%esi), %%mm7						\n\
	movq %%mm0,   (%%edi)						\n\
	movq %%mm1,  8(%%edi)						\n\
	movq %%mm2, 16(%%edi)						\n\
	movq %%mm3, 24(%%edi)						\n\
	movq %%mm4, 32(%%edi)						\n\
	movq %%mm5, 40(%%edi)						\n\
	movq %%mm6, 48(%%edi)						\n\
	movq %%mm7, 56(%%edi)						\n\
	add %%eax, %%esi						\n\
	add %%eax, %%edi						\n\
	dec %%ecx							\n\
	jnz .loop2							\n\
	jmp .blockloop							\n\
									\n\
.lastcopy:								\n\
	mov %%edx, %%ecx						\n\
	shr $2, %%ecx							\n\
	jz .lastcopy2							\n\
	rep movsd							\n\
.lastcopy2:								\n\
	xor %%ecx, %%ecx						\n\
	mov $3, %%cl							\n\
	and %%edx, %%ecx						\n\
	jz .end								\n\
	rep movsb							\n\
									\n\
.end:									\n\
	emms			# clean up MMX state			\n\
	pop %%ebx		# restore PIC register			\n\
    " : /* no outputs */
      : "a" (dest), "d" (src), "c" (bytes)
      : "%esi", "%edi"
    );
    return dest;
}

/*************************************************************************/

void *ac_memcpy_amdmmx(void *dest, const void *src, size_t bytes)
{
    asm("\
# Code taken from AMD's Athlon Processor x86 Code Optimization Guide	\n\
# 'Use MMX Instructions for Block Copies and Block Fills'		\n\
									\n\
TINY_BLOCK_COPY = 	64						\n\
IN_CACHE_COPY = 	64 * 1024					\n\
UNCACHED_COPY =		197 * 1024					\n\
CACHEBLOCK =		0x80						\n\
									\n\
	push %%ebx		# save PIC register			\n\
									\n\
	mov %%eax, %%edi	# dest					\n\
	mov %%edx, %%esi	# src					\n\
	#mov %%ecx, %%ecx	# bytes					\n\
	mov %%ecx, %%ebx	# keep a copy of count			\n\
									\n\
	cld								\n\
	cmp $TINY_BLOCK_COPY, %%ecx					\n\
	jb .memcpy_ic_3	# tiny? skip mmx copy			\n\
									\n\
	cmp $32*1024, %%ecx	# don't align between 32k-64k because	\n\
	jbe .memcpy_do_align	# it appears to be slower		\n\
	cmp $64*1024, %%ecx						\n\
	jbe .memcpy_align_done						\n\
									\n\
.memcpy_do_align:							\n\
	mov $8, %%ecx		# a trick that's faster than rep movsb...\n\
	sub %%edi, %%ecx	# align destination to qword		\n\
	and $0b111, %%ecx	# get the low bits			\n\
	sub %%ecx, %%ebx	# update copy count			\n\
	neg %%ecx		# set up to jump into the array		\n\
	add $.memcpy_align_done, %%ecx # offset?			\n\
	jmp *%%ecx		# jump to array of movsb's		\n\
									\n\
	.align 4							\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
									\n\
.memcpy_align_done:		# destination is dword aligned		\n\
	mov %%ebx, %%ecx	# number of bytes left to copy		\n\
	shr $6, %%ecx		# get 64-byte block count		\n\
	jz .memcpy_ic_2		# finish the last few bytes		\n\
	cmp $IN_CACHE_COPY/64, %%ecx # too big 4 cache? use uncached copy\n\
	jae .memcpy_uc_test						\n\
									\n\
	.align 16							\n\
.memcpy_ic_1:			# 64-byte block copies, in-cache copy	\n\
	prefetchnta (200*64/34+192)(%%esi) # start reading ahead	\n\
	movq (%%esi), %%mm0	# read 64 bits				\n\
	movq 8(%%esi), %%mm1						\n\
	movq %%mm0, (%%edi)	# write 64 bits				\n\
	movq %%mm1, 8(%%edi)	# note: the normal movq writes the	\n\
	movq 16(%%esi), %%mm2	# data to cache; a cache line will be	\n\
	movq 24(%%esi), %%mm3	# allocated as needed, to store the data\n\
	movq %%mm2, 16(%%edi)						\n\
	movq %%mm3, 24(%%edi)						\n\
	movq 32(%%esi), %%mm0						\n\
	movq 40(%%esi), %%mm1						\n\
	movq %%mm0, 32(%%edi)						\n\
	movq %%mm1, 40(%%edi)						\n\
	movq 48(%%esi), %%mm2						\n\
	movq 56(%%esi), %%mm3						\n\
	movq %%mm2, 48(%%edi)						\n\
	movq %%mm3, 56(%%edi)						\n\
									\n\
	add $64, %%esi		# update source pointer			\n\
	add $64, %%edi		# update destination pointer		\n\
	dec %%ecx		# count down				\n\
	jnz .memcpy_ic_1	# last 64-byte block?			\n\
									\n\
.memcpy_ic_2:								\n\
	mov %%ebx, %%ecx	# has valid low 6 bits of the byte count\n\
.memcpy_ic_3:								\n\
	shr $2, %%ecx		# dword count				\n\
	and $0b1111, %%ecx	# only look at the 'remainder' bits	\n\
	neg %%ecx		# set up to jump into the array		\n\
	add $.memcpy_last_few, %%ecx  # offset?				\n\
	jmp *%%ecx		# jump to array of movsd's		\n\
									\n\
.memcpy_uc_test:							\n\
	cmp $UNCACHED_COPY/64, %%ecx # big enough? use block prefetch copy\n\
	jae .memcpy_bp_1						\n\
.memcpy_64_test:							\n\
	or %%ecx, %%ecx		# tail end of block prefetch will jump here\n\
	jz .memcpy_ic_2		# no more 64-byte blocks left		\n\
									\n\
	.align 16							\n\
.memcpy_uc_1:			# 64-byte blocks, uncached copy		\n\
	prefetchnta (200*64/34+192)(%%esi) # start reading ahead	\n\
	movq (%%esi), %%mm0	# read 64 bits				\n\
	add $64, %%edi		# update destination pointer		\n\
	movq 8(%%esi), %%mm1						\n\
	add $64, %%esi		# update source pointer			\n\
	movq -48(%%esi), %%mm2						\n\
	movntq %%mm0,-64(%%edi)	# write 64 bits, bypassing the cache	\n\
	movq -40(%%esi), %%mm0	# note: movntq also prevents the CPU	\n\
	movntq %%mm1,-56(%%edi)	# from READING the destination address	\n\
	movq -32(%%esi), %%mm1	# into the cache, only to be overwritten\n\
	movntq %%mm2,-48(%%edi)	# so that also helps performance	\n\
	movq -24(%%esi), %%mm2						\n\
	movntq %%mm0, -40(%%edi)					\n\
	movq -16(%%esi), %%mm0						\n\
	movntq %%mm1, -32(%%edi)					\n\
	movq -8(%%esi), %%mm1						\n\
	movntq %%mm2, -24(%%edi)					\n\
	movntq %%mm0, -16(%%edi)					\n\
	dec %%ecx							\n\
	movntq %%mm1, -8(%%edi)						\n\
	jnz .memcpy_uc_1	# last 64-byte block?			\n\
	jmp .memcpy_ic_2	# almost done				\n\
									\n\
.memcpy_bp_1:			# large blocks, block prefetch copy	\n\
	cmp $CACHEBLOCK, %%ecx	# big enough to run another prefetch loop?\n\
	jl .memcpy_64_test	# no, back to regular uncached copy	\n\
	mov $CACHEBLOCK/2,%%eax	# block prefetch loop, unrolled 2X	\n\
	add $CACHEBLOCK*64,%%esi # move to the top of the block		\n\
	.align 16							\n\
.memcpy_bp_2:								\n\
	mov -64(%%esi), %%edx	# grab one address per cache line	\n\
	mov -128(%%esi), %%edx	# grab one address per cache line	\n\
	sub $128, %%esi		# go reverse order			\n\
	dec %%eax		# count down the cache lines		\n\
	jnz .memcpy_bp_2	# keep grabbing more lines into cache	\n\
	mov $CACHEBLOCK, %%eax	# now that it's in cache, do the copy	\n\
									\n\
	.align 16							\n\
.memcpy_bp_3:								\n\
	movq   (%%esi), %%mm0	# read 64 bits				\n\
	movq  8(%%esi), %%mm1						\n\
	movq 16(%%esi), %%mm2						\n\
	movq 24(%%esi), %%mm3						\n\
	movq 32(%%esi), %%mm4						\n\
	movq 40(%%esi), %%mm5						\n\
	movq 48(%%esi), %%mm6						\n\
	movq 56(%%esi), %%mm7						\n\
	add $64, %%esi		# update source pointer			\n\
	movntq %%mm0,   (%%edi)	# write 64 bits, bypassing cache	\n\
	movntq %%mm1,  8(%%edi)	# note: movntq also prevents the CPU	\n\
	movntq %%mm2, 16(%%edi)	# from READING the destination address	\n\
	movntq %%mm3, 24(%%edi)	# into the cache, only to be overwritten,\n\
	movntq %%mm4, 32(%%edi)	# so that also helps performance	\n\
	movntq %%mm5, 40(%%edi)						\n\
	movntq %%mm6, 48(%%edi)						\n\
	movntq %%mm7, 56(%%edi)						\n\
	add $64, %%edi		# update dest pointer			\n\
	dec %%eax		# count down				\n\
	jnz .memcpy_bp_3	# keep copying				\n\
	sub $CACHEBLOCK, %%ecx	# update the 64-byte block count	\n\
	jmp .memcpy_bp_1	# keep processing blocks		\n\
									\n\
	.align 4							\n\
	movsd								\n\
	movsd			# perform last 1-15 dword copies	\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd			# perform last 1-7 dword copies		\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
	movsd								\n\
									\n\
.memcpy_last_few:		# dword aligned from before movsd's	\n\
	mov %%ebx, %%ecx	# has valid low 2 bits of the byte count\n\
	and $0b11, %%ecx	# the last few cows must come home	\n\
	jz .memcpy_final	# no more, let's leave			\n\
	rep movsb		# the last 1, 2, or 3 bytes		\n\
									\n\
.memcpy_final:								\n\
	emms			# clean up the MMX state		\n\
	sfence			# flush the write buffer		\n\
	pop %%ebx		# restore PIC register			\n\
    " : /* no outputs */
      : "a" (dest), "d" (src), "c" (bytes)
      : "%esi", "%edi"
    );
    return dest;
}

#endif  /* ARCH_X86 */

/*************************************************************************/

#ifdef ARCH_X86_64

/* AMD64-optimized routine, using SSE2.  Derived from AMD64 optimization
 * guide section 5.13: Appropriate Memory Copying Routines. */

/* The block copying code--macroized because we use two versions of it
 * depending on whether the source is 16-byte-aligned or not.  Pass either
 * movdqa or movdqu (unquoted) for the parameter. */
#define AMD64_BLOCK_MEMCPY(movdq) \
"	# First prefetch (note that if we end on an odd number of cache	\n\
	# lines, we skip prefetching the last one--faster that way than	\n\
	# prefetching line by line or treating it as a special case)	\n\
0:	mov %%ecx, %%edx	# EDX: temp counter (always <32 bits)	\n\
	shr $6, %%edx		# Divide by cache line size (64 bytes)	\n\
	cmp %%r8, %%rdx		# ... and cap at 128 (8192 bytes)	\n\
	cmova %%r8, %%rdx						\n\
	shl $3, %%edx		# EDX <- cache lines to copy * 8	\n\
	mov %%edx, %%eax	# EAX <- cache lines to preload * 8	\n\
				#        (also used as memory offset)	\n\
1:	prefetchnta -64(%%rsi,%%rax,8)	# Preload cache lines in pairs	\n\
	prefetchnta -128(%%rsi,%%rax,8)	# (going backwards)		\n\
	sub $16, %%eax		# And loop				\n\
	jg 1b								\n\
									\n\
	# Then copy, also in reverse order				\n\
	mov %%edx, %%eax	# EAX <- cache lines to copy * 8	\n\
2:	" #movdq " -16(%%rsi,%%rax,8), %%xmm0				\n\
	movntdq %%xmm0, -16(%%rdi,%%rax,8)				\n\
	sub $2, %%eax		# And loop				\n\
	jg 2b								\n\
									\n\
	# Finally, update pointers and count, and loop			\n\
	shl $3, %%edx		# EDX <- bytes copied			\n\
	add %%rdx, %%rsi						\n\
	add %%rdx, %%rdi						\n\
	sub %%rdx, %%rcx						\n\
	cmp $64, %%rcx		# At least one cache line left?		\n\
	jae 0b			# Yup, loop				\n"

void *ac_memcpy_amd64(void *dest, const void *src, size_t bytes)
{
    asm("\
	push %%rdi		# Save destination for return value	\n\
	cld			# MOVS* should ascend			\n\
									\n\
	cmp $64, %%rcx		# Skip block copy for small blocks	\n\
	jb amd64.memcpy_last						\n\
									\n\
	mov $128, %%r8		# Constant used later			\n\
									\n\
	# First align destination address to a multiple of 16 bytes	\n\
	mov $8, %%eax		# EAX <- (8-dest) & 7			\n\
	sub %%edi, %%eax	# (we don't care about the top 32 bits)	\n\
	and $0b111, %%eax	# ... which is the number of bytes to copy\n\
	lea 0f(%%rip), %%rdx	# Use a computed jump--faster than a loop\n\
	sub %%rax, %%rdx						\n\
	jmp *%%rdx		# Execute 0-7 MOVSB's			\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
0:	sub %%rax, %%rcx	# Update count				\n\
	test $0b1000, %%edi	# Is destination not 16-byte aligned?	\n\
	je 1f								\n\
	movsq			# Then move 8 bytes to align it		\n\
	sub $8, %%rcx							\n\
									\n\
1:	cmp $0x38000, %%rcx	# Is this a large block? (0x38000 is an	\n\
				# arbitrary value where prefetching and	\n\
				# write combining seem to start becoming\n\
				# faster)		\n\
	jb amd64.memcpy_small_bp # Nope, use small copy (no prefetch/WC)\n\
	test $0b1111, %%esi	# Is source also 16-byte aligned?	\n\
				# (use ESI to save a REX prefix byte)	\n\
	jnz amd64.memcpy_normal_bp  # Nope, use slow copy		\n\
									\n\
	.align 16							\n\
amd64.memcpy_small_bp:		# Small block copy routine--no prefetch	\n\
	mov %%ecx, %%edx	# EDX <- bytes to copy / 16		\n\
	shr $4, %%edx		# (count known to fit in 32 bits)	\n\
	mov %%edx, %%eax	# Leave remainder in ECX for later	\n\
	shl $4, %%eax							\n\
	sub %%eax, %%ecx						\n\
0:	movdqu (%%rsi), %%xmm0	# Copy 16 bytes of data			\n\
	movdqa %%xmm0, (%%rdi)						\n\
	add $16, %%rsi		# Update pointers			\n\
	add $16, %%rdi							\n\
	dec %%edx		# And loop				\n\
	jnz 0b								\n\
	jmp amd64.memcpy_last	# Copy any remaining bytes		\n\
									\n\
	.align 16							\n\
amd64.memcpy_fast_bp:		# Fast block prefetch loop		\n"
AMD64_BLOCK_MEMCPY(movdqa)
"	jmp amd64.memcpy_last	# Copy any remaining bytes		\n\
									\n\
	.align 16							\n\
amd64.memcpy_normal_bp:		# Normal (unaligned) block prefetch loop\n"
AMD64_BLOCK_MEMCPY(movdqu)
"									\n\
amd64.memcpy_last:							\n\
	# Copy last <64 bytes, using the computed jump trick		\n\
	mov %%ecx, %%eax	# EAX <- ECX>>3				\n\
	shr $3, %%eax							\n\
	lea 0f(%%rip), %%rdx						\n\
	add %%eax, %%eax	# Watch out, MOVSQ is 2 bytes!		\n\
	sub %%rax, %%rdx						\n\
	jmp *%%rdx		# Execute 0-7 MOVSQ's			\n\
	movsq								\n\
	movsq								\n\
	movsq								\n\
	movsq								\n\
	movsq								\n\
	movsq								\n\
	movsq								\n\
0:	and $0b111, %%ecx	# ECX <- ECX & 7			\n\
	lea 0f(%%rip), %%rdx						\n\
	sub %%rcx, %%rdx						\n\
	jmp *%%rdx		# Execute 0-7 MOVSB's			\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
	movsb								\n\
									\n\
	# All done!							\n\
0:	emms			# Clean up after MMX instructions	\n\
	sfence			# Flush the write buffer		\n\
	pop %%rdi		# Restore destination (return value)	\n\
    " : /* no outputs */
      : "D" (dest), "S" (src), "c" (bytes)
      : "%rax", "%rdx"
    );
    return dest;
}

#endif  /* ARCH_X86_64 */

/*************************************************************************/

void * (*tc_memcpy)(void *, const void *, size_t) = memcpy;

void tc_memcpy_init(int verbose, int mmflags)
{
	const char * method = "libc";
	
#if defined(ARCH_X86) | defined(ARCH_X86_64)
	int accel = mmflags == -1 ? ac_mmflag() : mmflags;
#endif

#ifdef ARCH_X86
	if((accel & MM_MMXEXT) || (accel & MM_SSE))
	{
		method = "mmxext";
		tc_memcpy = ac_memcpy_amdmmx;
	}
	else
	{
		if(accel & MM_MMX)
		{
			method = "mmx";
			tc_memcpy = ac_memcpy_mmx;
		}
	}
#endif

#ifdef ARCH_X86_64
	if((accel & MM_CMOVE) && (accel & MM_SSE2))
	{
		method = "amd64";
		tc_memcpy = ac_memcpy_amd64;
	}
#endif

	if(verbose)
		fprintf(stderr, "tc_memcpy: using %s for memcpy\n", method);
}
