# LIBUCI2_FOUND - true if library and headers were found
# LIBUCI2_INCLUDE_DIRS - include directories
# LIBUCI2_LIBRARIES - library directories

find_package(PkgConfig)
pkg_check_modules(PC_LUBUCI2 QUIET libuci2)

find_path(LIBUCI2_INCLUDE_DIR uci2 libuci2.h
	HINTS ${PC_LIBUCI2_INCLUDEDIR} ${PC_LIBUCI2_INCLUDE_DIRS})

find_library(LIBUCI2_LIBRARY NAMES uci2
	HINTS ${PC_LIBUCI2_LIBDIR} ${PC_LIBUCI2_LIBRARY_DIRS})

set(LIBUCI2_LIBRARIES ${LIBUCI2_LIBRARY})
set(LIBUCI2_INCLUDE_DIRS ${LIBUCI2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBUCI2 DEFAULT_MSG LIBUCI2_LIBRARY LIBUCI2_INCLUDE_DIR)

mark_as_advanced(LIBUCI2_INCLUDE_DIR LIBUCI2_LIBRARY)
