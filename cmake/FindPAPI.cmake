find_path(
  PAPI_INCLUDE_DIR
  NAMES "papi.h"
  PATH_SUFFIXES "include"
)

find_library(
  PAPI_LIBRARY
  NAMES libpapi.so libpapi.a papi
  PATH_SUFFIXES "lib" "lib64"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	PAPI REQUIRED_VARS PAPI_INCLUDE_DIR PAPI_LIBRARY
)

if(PAPI_FOUND)
  if(NOT TARGET PAPI::PAPI)
	  add_library(PAPI::PAPI UNKNOWN IMPORTED GLOBAL)
    set_target_properties(
      PAPI::PAPI PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PAPI_INCLUDE_DIR}
                            IMPORTED_LOCATION ${PAPI_LIBRARY}
    )
  endif()
endif()
mark_as_advanced(PAPI_INCLUDE_DIR PAPI_LIBRARY)