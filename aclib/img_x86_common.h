/*
 * img_x86_common.h - common x86/x86-64 assembly macros
 * Written by Andrew Church <achurch@achurch.org>
 */

#ifndef ACLIB_IMG_X86_COMMON_H
#define ACLIB_IMG_X86_COMMON_H

/*************************************************************************/

/* Register names for pointers */
#ifdef ARCH_X86_64
# define EAX "%%rax"
# define EBX "%%rbx"
# define ECX "%%rcx"
# define EDX "%%rdx"
# define ESI "%%rsi"
# define EDI "%%rdi"
#else
# define EAX "%%eax"
# define EBX "%%ebx"
# define ECX "%%ecx"
# define EDX "%%edx"
# define ESI "%%esi"
# define EDI "%%edi"
#endif

/* Data for isolating particular bytes.  Used by the SWAP32 macros; if you
 * use them, make sure to define DEFINE_MASK_DATA before including this
 * file! */
#ifdef DEFINE_MASK_DATA
static struct { uint32_t n[64]; } __attribute__((aligned(16))) mask_data = {{
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF,
    0x0000FF00, 0x0000FF00, 0x0000FF00, 0x0000FF00,
    0x0000FFFF, 0x0000FFFF, 0x0000FFFF, 0x0000FFFF,
    0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000,
    0x00FF00FF, 0x00FF00FF, 0x00FF00FF, 0x00FF00FF,
    0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF,
    0xFF00FF00, 0xFF00FF00, 0xFF00FF00, 0xFF00FF00,
    0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF,
    0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000,
    0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF,
    0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
}};
#endif

/*************************************************************************/

/* Basic assembly macros, used for odd-count loops */

/* Swap bytes in pairs of 16-bit values */
#define X86_SWAP16_2 \
	"movl -4("ESI","ECX",4), %%eax					\n\
	movl %%eax, %%edx						\n\
	shll $8, %%eax							\n\
	andl $0xFF00FF00, %%eax						\n\
	shrl $8, %%edx							\n\
	andl $0x00FF00FF, %%edx						\n\
	orl %%edx, %%eax						\n\
	movl %%eax, -4("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* Swap words in a 32-bit value */
#define X86_SWAP32 \
	"movl -4("ESI","ECX",4), %%eax					\n\
	roll $16, %%eax							\n\
	movl %%eax, -4("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* Swap bytes 0 and 2 of a 32-bit value */
#define X86_SWAP32_02 \
	"movw -4("ESI","ECX",4), %%ax					\n\
	movw -2("ESI","ECX",4), %%dx					\n\
	xchg %%dl, %%al							\n\
	movw %%ax, -4("EDI","ECX",4)					\n\
	movw %%dx, -2("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* Swap bytes 1 and 3 of a 32-bit value */
#define X86_SWAP32_13 \
	"movw -4("ESI","ECX",4), %%ax					\n\
	movw -2("ESI","ECX",4), %%dx					\n\
	xchg %%dh, %%ah							\n\
	movw %%ax, -4("EDI","ECX",4)					\n\
	movw %%dx, -2("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* Reverse the order of bytes in a 32-bit value */
#define X86_REV32 \
	"movl -4("ESI","ECX",4), %%eax					\n\
	xchg %%ah, %%al							\n\
	roll $16, %%eax							\n\
	xchg %%ah, %%al							\n\
	movl %%eax, -4("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* The same, using the BSWAP instruction */
#define X86_REV32_BSWAP \
	"movl -4("ESI","ECX",4), %%eax					\n\
	bswap %%eax							\n\
	movl %%eax, -4("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* Rotate a 32-bit value left 8 bits */
#define X86_ROL32 \
	"movl -4("ESI","ECX",4), %%eax					\n\
	roll $8, %%eax							\n\
	movl %%eax, -4("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/* Rotate a 32-bit value right 8 bits */
#define X86_ROR32 \
	"movl -4("ESI","ECX",4), %%eax					\n\
	rorl $8, %%eax							\n\
	movl %%eax, -4("EDI","ECX",4)					\n\
	subl $1, %%ecx"

/*************************************************************************/

/* Basic assembly routines.  Sizes are all given in 32-bit units. */

#define ASM_SWAP16_2_X86(size) \
    asm("0: "X86_SWAP16_2"						\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax", "edx")

#define ASM_SWAP32_X86(size) \
    asm("0: "X86_SWAP32"						\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax", "edx")

#define ASM_SWAP32_02_X86(size) \
    asm("0: "X86_SWAP32_02"						\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax", "edx")

#define ASM_SWAP32_13_X86(size) \
    asm("0: "X86_SWAP32_13"						\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax", "edx")

#define ASM_REV32_X86(size) \
    asm("0: "X86_REV32"							\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax")

#define ASM_ROL32_X86(size) \
    asm("0: "X86_ROL32"							\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax")

#define ASM_ROR32_X86(size) \
    asm("0: "X86_ROR32"							\n\
	jnz 0b"								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax")

/*************************************************************************/

/* MMX- and SSE2-optimized routines.  These routines are identical save for
 * data size, so we use common macros to implement them, with register names
 * and data offsets replaced by parameters to the macros. */

#define ASM_SIMD_MMX(name,size) \
    name((size), "movq", "movq", "movq", "",		\
         "%%mm0", "%%mm1", "%%mm2", "%%mm3",		\
         "%%mm4", "%%mm5", "%%mm6", "%%mm7",		\
         "8", "16", "24", "32", "3", "4", "7", "8")
#define ASM_SIMD_SSE2(name,size) \
    name((size), "movdqu", "movdqa", "movntdq", "sfence",\
         "%%xmm0", "%%xmm1", "%%xmm2", "%%xmm3",	\
         "%%xmm4", "%%xmm5", "%%xmm6", "%%xmm7",	\
         "16", "32", "48", "64", "7", "8", "15", "16")

#define ASM_SWAP16_2_MMX(size)    ASM_SIMD_MMX(ASM_SWAP16_2_SIMD,(size))
#define ASM_SWAP16_2_SSE2(size)   ASM_SIMD_SSE2(ASM_SWAP16_2_SIMD,(size))
#define ASM_SWAP32_MMX(size)      ASM_SIMD_MMX(ASM_SWAP32_SIMD,(size))
#define ASM_SWAP32_SSE2(size)     ASM_SIMD_SSE2(ASM_SWAP32_SIMD,(size))
#define ASM_SWAP32_02_MMX(size)   ASM_SIMD_MMX(ASM_SWAP32_02_SIMD,(size))
#define ASM_SWAP32_02_SSE2(size)  ASM_SIMD_SSE2(ASM_SWAP32_02_SIMD,(size))
#define ASM_SWAP32_13_MMX(size)   ASM_SIMD_MMX(ASM_SWAP32_13_SIMD,(size))
#define ASM_SWAP32_13_SSE2(size)  ASM_SIMD_SSE2(ASM_SWAP32_13_SIMD,(size))
#define ASM_REV32_MMX(size)       ASM_SIMD_MMX(ASM_REV32_SIMD,(size))
#define ASM_REV32_SSE2(size)      ASM_SIMD_SSE2(ASM_REV32_SIMD,(size))
#define ASM_ROL32_MMX(size)       ASM_SIMD_MMX(ASM_ROL32_SIMD,(size))
#define ASM_ROL32_SSE2(size)      ASM_SIMD_SSE2(ASM_ROL32_SIMD,(size))
#define ASM_ROR32_MMX(size)       ASM_SIMD_MMX(ASM_ROR32_SIMD,(size))
#define ASM_ROR32_SSE2(size)      ASM_SIMD_SSE2(ASM_ROR32_SIMD,(size))

#define ASM_SWAP16_2_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("0:	# Handle up to "n8" pixels first to align the counter	\n\
	"X86_SWAP16_2"							\n\
	testl $"n7", %%ecx						\n\
	jnz 0b								\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:	# Now do "n8" pixels (sets of 4 bytes) at a time	\n\
	"ldq" -"ofs4"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs3"("ESI","ECX",4), "MM2"	# likewise		\n\
	"movq" "MM2", "MM3"						\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM6"				\n\
	"movq" "MM6", "MM7"						\n\
	psrlw $8, "MM0"			# MM0: - 7 - 5 - 3 - 1		\n\
	psllw $8, "MM1"			# MM1: 6 - 4 - 2 - 0 -		\n\
	por "MM1", "MM0"		# MM0: 6 7 4 5 2 3 0 1		\n\
	psrlw $8, "MM2"			# likewise			\n\
	psllw $8, "MM3"							\n\
	por "MM3", "MM2"						\n\
	psrlw $8, "MM4"							\n\
	psllw $8, "MM5"							\n\
	por "MM5", "MM4"						\n\
	psrlw $8, "MM6"							\n\
	psllw $8, "MM7"							\n\
	por "MM7", "MM6"						\n\
	"stq" "MM0", -"ofs4"("EDI","ECX",4)				\n\
	"stq" "MM2", -"ofs3"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM6", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n8", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax", "edx")

#define ASM_SWAP32_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("0:								\n\
	"X86_SWAP16_2"							\n\
	testl $"n7", %%ecx						\n\
	jnz 0b								\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:								\n\
	"ldq" -"ofs4"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs3"("ESI","ECX",4), "MM2"				\n\
	"movq" "MM2", "MM3"						\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM6"				\n\
	"movq" "MM6", "MM7"						\n\
	psrld $16, "MM0"		# MM0: - - 7 6 - - 3 2		\n\
	pslld $16, "MM1"		# MM1: 5 4 - - 1 0 - -		\n\
	por "MM1", "MM0"		# MM0: 5 4 7 6 1 0 3 2		\n\
	psrld $16, "MM2"						\n\
	pslld $16, "MM3"						\n\
	por "MM3", "MM2"						\n\
	psrld $16, "MM4"						\n\
	pslld $16, "MM5"						\n\
	por "MM5", "MM4"						\n\
	psrld $16, "MM6"						\n\
	pslld $16, "MM7"						\n\
	por "MM7", "MM6"						\n\
	"stq" "MM0", -"ofs4"("EDI","ECX",4)				\n\
	"stq" "MM2", -"ofs3"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM6", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n8", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax")

#define ASM_SWAP32_02_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("pushl "EDX"							\n\
	0: "X86_SWAP32_02"						\n\
	testl $"n3", %%ecx						\n\
	jnz 0b								\n\
	popl "EDX"							\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:								\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"movq" "MM0", "MM2"		# MM2: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"movq" "MM4", "MM6"						\n\
	pand 16("EDX"), "MM1"		# MM1: - - - 4 - - - 0		\n\
	pslld $16, "MM1"		# MM1: - 4 - - - 0 - -		\n\
	pand 64("EDX"), "MM2"		# MM2: - 6 - - - 2 - -		\n\
	psrld $16, "MM2"		# MM2: - - - 6 - - - 2		\n\
	pand 160("EDX"), "MM0"		# MM0: 7 - 5 - 3 - 1 -		\n\
	por "MM1", "MM0"		# MM0: 7 4 5 - 3 0 1 -		\n\
	por "MM2", "MM0"		# MM0: 7 4 5 6 3 0 1 2		\n\
	pand 16("EDX"), "MM5"						\n\
	pslld $16, "MM5"						\n\
	pand 64("EDX"), "MM6"						\n\
	psrld $16, "MM6"						\n\
	pand 160("EDX"), "MM4"						\n\
	por "MM5", "MM4"						\n\
	por "MM6", "MM4"						\n\
	"stq" "MM0", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n4", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size), "d" (&mask_data),	\
	  "m" (mask_data)						\
	: "eax")

#define ASM_SWAP32_13_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("pushl "EDX"							\n\
	0: "X86_SWAP32_13"						\n\
	testl $"n3", %%ecx						\n\
	jnz 0b								\n\
	popl "EDX"							\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:								\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"movq" "MM0", "MM2"		# MM2: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"movq" "MM4", "MM6"						\n\
	pand 32("EDX"), "MM1"		# MM1: - - 5 - - - 1 -		\n\
	pslld $16, "MM1"		# MM1: 5 - - - 1 - - -		\n\
	pand 128("EDX"), "MM2"		# MM2: 7 - - - 3 - - -		\n\
	psrld $16, "MM2"		# MM2: - - 7 - - - 3 -		\n\
	pand 80("EDX"), "MM0"		# MM0: - 6 - 4 - 2 - 0		\n\
	por "MM1", "MM0"		# MM0: 5 6 - 4 1 2 - 0		\n\
	por "MM2", "MM0"		# MM0: 5 6 7 4 1 2 3 0		\n\
	pand 32("EDX"), "MM5"						\n\
	pslld $16, "MM5"						\n\
	pand 128("EDX"), "MM6"						\n\
	psrld $16, "MM6"						\n\
	pand 80("EDX"), "MM4"						\n\
	por "MM5", "MM4"						\n\
	por "MM6", "MM4"						\n\
	"stq" "MM0", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n4", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size), "d" (&mask_data),	\
	  "m" (mask_data)						\
	: "eax");

#define ASM_REV32_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("0:	# Handle up to "n4" pixels first to align the counter	\n\
	"X86_REV32_BSWAP"						\n\
	testl $"n3", %%ecx						\n\
	jnz 0b								\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:	# Now do "n4" pixels at a time				\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"movq" "MM0", "MM2"		# MM2: 7 6 5 4 3 2 1 0		\n\
	"movq" "MM0", "MM3"		# MM3: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"movq" "MM4", "MM6"						\n\
	"movq" "MM4", "MM7"						\n\
	psrld $24, "MM0"		# MM0: - - - 7 - - - 3		\n\
	pand 32("EDX"), "MM2"		# MM2: - - 5 - - - 1 -		\n\
	psrld $8, "MM1"			# MM1: - 7 6 5 - 3 2 1		\n\
	pand 32("EDX"), "MM1"		# MM1: - - 6 - - - 2 -		\n\
	pslld $8, "MM2"			# MM2: - 5 - - - 1 - -		\n\
	pslld $24, "MM3"		# MM3: 4 - - - 0 - - -		\n\
	por "MM1", "MM0"		# MM0: - - 6 7 - - 2 3		\n\
	por "MM2", "MM0"		# MM0: - 5 6 7 - 1 2 3		\n\
	por "MM3", "MM0"		# MM0: 4 5 6 7 0 1 2 3		\n\
	psrld $24, "MM4"						\n\
	pand 32("EDX"), "MM6"						\n\
	psrld $8, "MM5"							\n\
	pand 32("EDX"), "MM5"						\n\
	pslld $8, "MM6"							\n\
	pslld $24, "MM7"						\n\
	por "MM5", "MM4"						\n\
	por "MM6", "MM4"						\n\
	por "MM7", "MM4"						\n\
	"stq" "MM0", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n4", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size), "d" (&mask_data),	\
	  "m" (mask_data)						\
	: "eax")

#define ASM_ROL32_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("0: "X86_ROL32"							\n\
	testl $"n7", %%ecx						\n\
	jnz 0b								\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:								\n\
	"ldq" -"ofs4"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs3"("ESI","ECX",4), "MM2"				\n\
	"movq" "MM2", "MM3"						\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM6"				\n\
	"movq" "MM6", "MM7"						\n\
	pslld $8, "MM0"			# MM0: 6 5 4 - 2 1 0 -		\n\
	psrld $24, "MM1"		# MM1: - - - 7 - - - 3		\n\
	por "MM1", "MM0"		# MM0: 6 5 4 7 2 1 0 3		\n\
	pslld $8, "MM2"							\n\
	psrld $24, "MM3"						\n\
	por "MM3", "MM2"						\n\
	pslld $8, "MM4"							\n\
	psrld $24, "MM5"						\n\
	por "MM5", "MM4"						\n\
	pslld $8, "MM6"							\n\
	psrld $24, "MM7"						\n\
	por "MM7", "MM6"						\n\
	"stq" "MM0", -"ofs4"("EDI","ECX",4)				\n\
	"stq" "MM2", -"ofs3"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM6", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n8", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax")

#define ASM_ROR32_SIMD(size,ldq,movq,stq,sfence,MM0,MM1,MM2,MM3,MM4,MM5,MM6,MM7,ofs1,ofs2,ofs3,ofs4,n3,n4,n7,n8) \
    asm("0: "X86_ROR32"							\n\
	testl $"n7", %%ecx						\n\
	jnz 0b								\n\
	testl %%ecx, %%ecx						\n\
	jz 2f								\n\
	1:								\n\
	"ldq" -"ofs4"("ESI","ECX",4), "MM0"	# MM0: 7 6 5 4 3 2 1 0	\n\
	"movq" "MM0", "MM1"		# MM1: 7 6 5 4 3 2 1 0		\n\
	"ldq" -"ofs3"("ESI","ECX",4), "MM2"				\n\
	"movq" "MM2", "MM3"						\n\
	"ldq" -"ofs2"("ESI","ECX",4), "MM4"				\n\
	"movq" "MM4", "MM5"						\n\
	"ldq" -"ofs1"("ESI","ECX",4), "MM6"				\n\
	"movq" "MM6", "MM7"						\n\
	psrld $8, "MM0"			# MM0: - 7 6 5 - 3 2 1		\n\
	pslld $24, "MM1"		# MM1: 4 - - - 0 - - -		\n\
	por "MM1", "MM0"		# MM0: 4 7 6 5 0 3 2 1		\n\
	psrld $8, "MM2"							\n\
	pslld $24, "MM3"						\n\
	por "MM3", "MM2"						\n\
	psrld $8, "MM4"							\n\
	pslld $24, "MM5"						\n\
	por "MM5", "MM4"						\n\
	psrld $8, "MM6"							\n\
	pslld $24, "MM7"						\n\
	por "MM7", "MM6"						\n\
	"stq" "MM0", -"ofs4"("EDI","ECX",4)				\n\
	"stq" "MM2", -"ofs3"("EDI","ECX",4)				\n\
	"stq" "MM4", -"ofs2"("EDI","ECX",4)				\n\
	"stq" "MM6", -"ofs1"("EDI","ECX",4)				\n\
	subl $"n8", %%ecx						\n\
	jnz 1b								\n\
	2: emms								\n\
	"sfence								\
	: /* no outputs */						\
	: "S" (src[0]), "D" (dest[0]), "c" (size)			\
	: "eax")

/*************************************************************************/

#endif  /* ACLIB_IMG_X86_COMMON_H */
