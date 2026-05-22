# ---------------------------------------------------------------
# PAPI Wrapper Library
# ---------------------------------------------------------------

find_package(PAPI REQUIRED)

add_library(PAPIWrapper STATIC
    papi_wrapper.cpp
)

target_include_directories(PAPIWrapper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(PAPIWrapper
    PRIVATE
        PAPI::PAPI
)

target_compile_features(PAPIWrapper
    PUBLIC
        cxx_std_17
)

set_target_properties(PAPIWrapper PROPERTIES
    OUTPUT_NAME papi_wrapper
    POSITION_INDEPENDENT_CODE ON
)

