project(sidekick)

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib)

include_directories(
        "${PROJECT_SOURCE_DIR}/../nuilite"
        ${LIBCAIRO_INC}

)


set(SK_SRC "sidekick.cpp" SKApp.cpp SKApp.h SKCallback.cpp SKCallback.h SKPrefs.cpp SKPrefs.h)
add_executable(sidekick ${SK_SRC})
target_link_libraries(sidekick nuilite ${LIBCAIRO} cjson pthread)

