
EXTRA_FLAGS = -O2

# check if gcc version is 10 or higher (9.2020)
#GCCVERSION10 := $(shell expr `gcc -dumpversion | cut -f1 -d.` \>= 10)

#ifeq ($(GCCVERSION10),0)
#EXTRA_FLAGS_FORTRAN  = $(EXTRA_FLAGS)
#EXTRA_FLAGS_C        = $(EXTRA_FLAGS)
#EXTRA_FLAGS_CPLUS    =
#else
# for gcc v10, allow argument-mismatch errors (9.2020)
#EXTRA_FLAGS_FORTRAN = $(EXTRA_FLAGS) -fallow-argument-mismatch -fcommon
#EXTRA_FLAGS_C       = $(EXTRA_FLAGS) -fcommon
#EXTRA_FLAGS_CPLUS   = -std=c++17
#endif

PROFILEFLAG =

if I686
SNFFLAGS  = -c -static -fno-automatic -fsecond-underscore $(EXTRA_FLAGS_FORTRAN)
SNCFLAGS  = -static -m64 $(EXTRA_FLAGS)
SNLDFLAGS = -m64 -lgfortran -lpthread $(PROFILEFLAG)
else
SNFFLAGS  = -c -static -fno-automatic -m64 -fsecond-underscore $(EXTRA_FLAGS_FORTRAN)
SNCFLAGS  = -c -static -m64 $(EXTRA_FLAGS_C)
SNLDFLAGS = -m64 -lpthread $(PROFILEFLAG)
endif

## HMK These two options look the same in Makefile.legacy?
if OSX
if BIT64
SNFFLAGS  = -c -static -fno-automatic -fsecond-underscore -m64
SNCFLAGS  = -c -mdynamic-no-pic -m64
SNLDFLAGS =
else
SNFFLAGS  = -c -static -fno-automatic -fsecond-underscore -m64
SNCFLAGS  = -c -mdynamic-no-pic -m64
SNLDFLAGS =
endif
endif

# HMK need to define replacement for this in configure.ac
#GIT_VERSION := "$(shell git describe --always --tags)"
SNCFLAGS += -DGIT_SNANA_VERSION=\"$(GIT_VERSION)\"



AM_CFLAGS = $(EXTRA_FLAGS) $(SNCFLAGS)

AM_LDFLAGS = $(SNLDFLAGS) -lgsl -lgslcblas -lcfitsio -lm 


bin_PROGRAMS = sntable_dump


#SNTOOLS_OUTPUT = \
#        sntools_output.c \
        #sntools_output.h \
        #sntools_output_hbook.c \
#        sntools_output_marz.c \
#        sntools_output_text.c \
        $(SNTOOLS_ROOT)

SNTOOLS_OUTPUT = \
        sntools_output.c 

sntable_dump_SOURCES = sntable_dump.c sntools.c sntools_output.c
#

#bin_PROGRAMS = psnid

#psnid.f90 : psnid.cra  psnid.cra  psnid.car  snana.car  snlc_fit.cra snlc_fit.car
#        for item in  snana  snlc_fit psnid ; do \
#            rm -f $$item.lnk; \
#            ln -s $(SRC)/$$item.car $$item.lnk; \
#        done
#        ypatchy.pl - $@ psnid.cra .go
#        rm snana.lnk snlc_fit.lnk psnid.lnk

#psnid_SOURCES = psnid.f90 psnid_tools.c sntools_modelgrid_read.c psnid_BEST.c psnid_BEST2.c

