# libnabo builds its Python bindings by default; nabo.cpp then fails with
#   fatal error: numpy/arrayobject.h: No such file or directory
# because nothing stages the numpy headers (the recipe depends on neither
# python3-numpy nor python3). Nothing on this image uses pynabo -- libnabo is
# here only as libpointmatcher's kd-tree backend for RTAB-Map -- so turn the
# bindings off rather than adding a numpy build dependency.
EXTRA_OECMAKE += "-DLIBNABO_BUILD_PYTHON=OFF"
