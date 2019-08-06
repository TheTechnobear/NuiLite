project(flite)

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib)


include_directories(
    ${LIBCAIRO_INC}
#        "${PROJECT_SOURCE_DIR}/../external/libusb"
)

set(FL_H
        FatesLite.h
        )

set(FL_SRC
        FatesLite.cpp
        )



add_library(flite SHARED ${FL_SRC} ${FL_H})

#target_link_libraries(flite oscpack portaudio cjson)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(flite atomic)
endif ()

target_link_libraries(flite "dl" "pthread" ${LIBCAIRO})

target_include_directories(flite PUBLIC
        .
        )

add_subdirectory(tests)


