#	$NetBSD: Makefile,v 1.12 2011/10/17 14:20:54 chs Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined
NOPROFILE=	# defined

LIB=		gcc

.include <bsd.own.mk>

.if ${MKGCC} != "no"

.cc: # disable .cc->NULL transform

.if !defined(MKNATIVE_LIBGCC_NEW)

SRCS+=		${LIB2FUNCS} ${LIB2FUNCS_ST} ${LIB2DIVMOD} \
		${G_LIB2ADD:T:S/.asm/.S/} ${LIB1ASMFUNCS}
.if ${MKPIC} == "no"
SRCS+=		${LIB2_EH}
.endif

.else	# defined(MKNATIVE_LIBGCC_NEW)

.include "../Makefile.srcs"

# New style.
TMPLSRCS=	${TMPLSRCS.libgcc.a}
TMPLFPSRCS=	${TMPLFPSRCS.libgcc.a}
TMPLASMSRCS=	${TMPLASMSRCS.libgcc.a}
SRCS=		${SRCS.libgcc.a}

.include "../Makefile.wrapper"

.endif	# defined(MKNATIVE_LIBGCC_NEW)

.include <bsd.lib.mk>

.if !defined(MKNATIVE_LIBGCC_NEW)

CLEANFILES+=	${SOBJS:=.tmp1} ${SOBJS:=.tmp2}

.c.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${CSHLIBFLAGS} ${.IMPSRC} -o ${.TARGET}.tmp1
	${NM} -pg ${.TARGET}.tmp1 | \
	    ${TOOL_AWK} 'NF == 3 { print "\t.hidden", $$3 }' | \
	    ${CC} ${COPTS} -Wl,-x -r -nostdinc -nostdlib ${CPUFLAGS} -o ${.TARGET}.tmp2 ${.TARGET}.tmp1 -xassembler -
.if defined(COPTS) && !empty(COPTS:M*-g*)
	mv ${.TARGET}.tmp2 ${.TARGET}
.else
	${LD} -x -r ${.TARGET}.tmp2 -o ${.TARGET}
	rm -f ${.TARGET}.tmp2
.endif
	rm -f ${.TARGET}.tmp1

.S.pico .s.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${CAPICFLAGS} ${CFLAGS:M-[ID]*} ${.IMPSRC} -o ${.TARGET}.tmp1
	${NM} -pg ${.TARGET}.tmp1 | \
	    ${TOOL_AWK} 'NF == 3 { print "\t.hidden", $$3 }' | \
	    ${CC} -Wl,-x -r -nostdinc -nostdlib -o ${.TARGET}.tmp2 ${.TARGET}.tmp1 -xassembler -
.if defined(COPTS) && !empty(COPTS:M*-g*)
	mv ${.TARGET}.tmp2 ${.TARGET}
.else
	${LD} -x -r ${.TARGET}.tmp2 -o ${.TARGET}
	rm -f ${.TARGET}.tmp2
.endif
	rm -f ${.TARGET}.tmp1

.endif	# !defined(MKNATIVE_LIBGCC_NEW)

.else
.include <bsd.prog.mk> # do nothing
.endif
