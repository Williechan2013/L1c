
#******************************************************************************

# CPP_VCL_FLAGS  =  -Iinclude/vcl $(DBG) $(VCL_OPT) -fabi-version=0 -fPIC


# Using noinst_LTLLIBRARIES instead of noinst_LIBRARIES prevents the warning
# about linking against a static library when we compile and link the interfaces

noinst_LTLIBRARIES = libvcl_math.la
libvcl_math_la_SOURCES = vcl_math.cpp \
						 $(top_srcdir)/include/vcl_math.h


#  -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/include/vcl $(DBG) $(OPT) -fabi-version=0

AM_CXXFLAGS = $(L1CCXXFLAGS)
