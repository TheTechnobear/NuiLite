# basic testapp

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib)


set(FLTEST1_SRC "fatestest.cpp")
include_directories (
    "${PROJECT_SOURCE_DIR}/.."
    ${LIBCAIRO_INC}
    )
add_executable(fatestest ${FLTEST1_SRC})
target_link_libraries (fatestest flite ${LIBCAIRO})

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  target_link_libraries(fatestest atomic)
endif()

target_link_libraries(fatestest pthread)

