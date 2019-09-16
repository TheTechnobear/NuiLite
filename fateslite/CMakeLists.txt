project(fateslite)

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib)

find_path(LIBFT_INC NAME ft2build.h PATHS /usr/include/freetype2 /usr/local/include/freetype2)
find_library(LIBFT NAME  libfreetyped PATHS /usr/local/lib /usr/lib)

include_directories(
    ${LIBCAIRO_INC}
    ${LIBFT_INC}
   "${PROJECT_SOURCE_DIR}/../external/moodycamel"
)

set(FL_H
        FatesDevice.h
        )

set(FL_SRC
        FatesDevice.cpp
        )



add_library(fateslite SHARED ${FL_SRC} ${FL_H})

#target_link_libraries(fateslite oscpack portaudio cjson)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(fateslite atomic)
endif ()

target_link_libraries(fateslite "dl" "pthread" ${LIBCAIRO})

target_include_directories(fateslite PUBLIC
        .
        )

add_subdirectory(tests)


