# basic testapp

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo /opt/homebrew/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib /opt/homebrew/lib)

include_directories (
        "${PROJECT_SOURCE_DIR}/.."
        ${LIBCAIRO_INC}

)


set(TEST_SRC "nuitest.cpp")
add_executable(nuitest ${TEST_SRC})
target_link_libraries (nuitest nuilite ${LIBCAIRO} pthread )

#if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
#  target_link_libraries(fatestest atomic)
#endif()
#
#target_link_libraries(fatestest pthread)

