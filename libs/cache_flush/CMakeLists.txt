# ---------------------------------------------------------------
# CacheFlush C Library
# ---------------------------------------------------------------

add_library(CacheFlush STATIC
    cache_flush.c
    cache_flush_utility.c
)

# Public include directory (for dependents)
target_include_directories(CacheFlush
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enforce C standard (explicit and portable)
target_compile_features(CacheFlush
    PUBLIC
        c_std_11
)

# Recommended properties for static C libs
set_target_properties(CacheFlush PROPERTIES
    OUTPUT_NAME cache_flush
    POSITION_INDEPENDENT_CODE ON
)
