#===-- runtime/POSIX/Makefile ------------------------------*- Makefile -*--===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

LEVEL=../..
PAPI_ROOT=../../../papi-5.0.0/src # FIXME move this into configure

LIBRARYNAME=kleeRuntimePAPI
DONT_BUILD_RELINKED=1
BUILD_ARCHIVE=1
BYTECODE_LIBRARY=1
# Don't strip debug info from the module.
DEBUG_RUNTIME=1
NO_PEDANTIC=1

C.Flags += -I$(PAPI_ROOT)
LD.Flags += -L$(PAPI_ROOT)

include $(LEVEL)/Makefile.common
