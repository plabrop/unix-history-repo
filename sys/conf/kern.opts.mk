# $FreeBSD$

# Options set in the build system that affect the kernel somehow.

#
# Define MK_* variables (which are either "yes" or "no") for users
# to set via WITH_*/WITHOUT_* in /etc/src.conf and override in the
# make(1) environment.
# These should be tested with `== "no"' or `!= "no"' in makefiles.
# The NO_* variables should only be set by makefiles for variables
# that haven't been converted over.
#

# Note: bsd.own.mk must be included before the rest of kern.opts.mk to make
# building on 10.x and earlier work. This should be removed when that's no
# longer supported since it confounds the defaults (since it uses the host's
# notion of defaults rather than what's default in current when building
# within sys/modules).
.include <bsd.own.mk>

# These options are used by the kernel build process (kern.mk and kmod.mk)
# They have to be listed here so we can build modules outside of the
# src tree.

__DEFAULT_YES_OPTIONS = \
    AUTOFS \
    BHYVE \
    BLUETOOTH \
    CCD \
    CDDL \
    CRYPT \
    CUSE \
    FORMAT_EXTENSIONS \
    INET \
    INET6 \
    IPFILTER \
    ISCSI \
    KERNEL_SYMBOLS \
    NETGRAPH \
    PF \
    SOURCELESS_HOST \
    SOURCELESS_UCODE \
    USB_GADGET_EXAMPLES \
    ZFS

__DEFAULT_NO_OPTIONS = \
    EISA \
    FAST_DEPEND \
    NAND \
    OFED

# Some options are totally broken on some architectures. We disable
# them. If you need to enable them on an experimental basis, you
# must change this code.
# Note: These only apply to the list of modules we build by default
# and sometimes what is in the opt_*.h files by default.
# Kernel config files are unaffected, though some targets can be
# affected by KERNEL_SYMBOLS, FORMAT_EXTENSIONS, CTF and SSP.

# Things that don't work based on the CPU
.if ${MACHINE_CPUARCH} == "arm"
BROKEN_OPTIONS+= ZFS
. if ${MACHINE_ARCH:Marmv6*} == ""
BROKEN_OPTIONS+= CDDL
. endif
.endif

.if ${MACHINE_CPUARCH} == "mips"
BROKEN_OPTIONS+= CDDL ZFS
.endif

.if ${MACHINE_CPUARCH} == "powerpc" && ${MACHINE_ARCH} == "powerpc"
BROKEN_OPTIONS+= ZFS
.endif

# Things that don't work because the kernel doesn't have the support
# for them.
.if ${MACHINE} != "i386"
BROKEN_OPTIONS+= EISA
.endif

.if ${MACHINE} != "i386" && ${MACHINE} != "amd64"
BROKEN_OPTIONS+= OFED
.endif

# expanded inline from bsd.mkopt.mk to avoid share/mk dependency

# Those that default to yes
.for var in ${__DEFAULT_YES_OPTIONS}
.if !defined(MK_${var})
.if defined(WITHOUT_${var})			# WITHOUT always wins
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.else
.if ${MK_${var}} != "yes" && ${MK_${var}} != "no"
.error "Illegal value for MK_${var}: ${MK_${var}}"
.endif
.endif # !defined(MK_${var})
.endfor
.undef __DEFAULT_YES_OPTIONS

# Those that default to no
.for var in ${__DEFAULT_NO_OPTIONS}
.if !defined(MK_${var})
.if defined(WITH_${var}) && !defined(WITHOUT_${var}) # WITHOUT always wins
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.else
.if ${MK_${var}} != "yes" && ${MK_${var}} != "no"
.error "Illegal value for MK_${var}: ${MK_${var}}"
.endif
.endif # !defined(MK_${var})
.endfor
.undef __DEFAULT_NO_OPTIONS

#
# MK_* options which are always no, usually because they are
# unsupported/badly broken on this architecture.
#
.for var in ${BROKEN_OPTIONS}
MK_${var}:=	no
.endfor
.undef BROKEN_OPTIONS
#end of bsd.mkopt.mk expanded inline.

#
# MK_*_SUPPORT options which default to "yes" unless their corresponding
# MK_* variable is set to "no".
#
.for var in \
    INET \
    INET6
.if defined(WITHOUT_${var}_SUPPORT) || ${MK_${var}} == "no"
MK_${var}_SUPPORT:= no
.else
.if defined(KERNBUILDDIR)	# See if there's an opt_foo.h
OPT_${var}!= cat ${KERNBUILDDIR}/opt_${var:tl}.h; echo
.if ${OPT_${var}} == ""		# nothing -> no
MK_${var}_SUPPORT:= no
.else
MK_${var}_SUPPORT:= yes
.endif
.else				# otherwise, yes
MK_${var}_SUPPORT:= yes
.endif
.endif
.endfor
