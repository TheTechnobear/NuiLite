# basic testapp

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib)

include_directories (
        "${PROJECT_SOURCE_DIR}/.."
        ${LIBCAIRO_INC}

)

set(TEST1_SRC "rawnuitest.cpp")
add_executable(rawnuitest ${TEST1_SRC})
target_link_libraries (rawnuitest nuilite ${LIBCAIRO} pthread )


set(TEST2_SRC "libnuitest.cpp")
add_executable(libnuitest ${TEST2_SRC})
target_link_libraries (libnuitest nuilite ${LIBCAIRO} pthread )

#if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
#  target_link_libraries(fatestest atomic)
#endif()
#
#target_link_libraries(fatestest pthread)

