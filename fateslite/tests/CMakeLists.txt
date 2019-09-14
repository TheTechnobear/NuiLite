# basic testapp

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib)

include_directories (
        "${PROJECT_SOURCE_DIR}/.."
        ${LIBCAIRO_INC}

)

set(FLTEST1_SRC "rawfatestest.cpp")
add_executable(rawfatestest ${FLTEST1_SRC})
target_link_libraries (rawfatestest flite ${LIBCAIRO} pthread )


set(FLTEST2_SRC "libfatestest.cpp")
add_executable(libfatestest ${FLTEST2_SRC})
target_link_libraries (libfatestest flite ${LIBCAIRO} pthread )

#if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
#  target_link_libraries(fatestest atomic)
#endif()
#
#target_link_libraries(fatestest pthread)

