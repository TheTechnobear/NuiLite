project(sidekick)

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo /opt/homebrew/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib /opt/homebrew/lib)

include_directories(
        "${PROJECT_SOURCE_DIR}/../nuilite"
        ${LIBCAIRO_INC}

)


set(SK_SRC 
    sidekick.cpp 
    SKApp.cpp SKApp.h 
    SKPrefs.cpp SKPrefs.h
    SKHardwareCallback.cpp SKHardwareCallback.h 
    SKOscCallback.cpp SKOscCallback.h
    )
add_executable(sidekick ${SK_SRC})
target_link_libraries(sidekick nuilite ${LIBCAIRO} cjson pthread oscpack moodycamel)

