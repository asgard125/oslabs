cmake_minimum_required(VERSION 3.10)
project(Lab5)

set(SQLITE3_SRC sqlite3.c)

add_executable(main main.c ${SQLITE3_SRC})

add_executable(emulator emulator.c)

add_executable(temp.cgi temp.c ${SQLITE3_SRC})

if(WIN32)
    target_link_libraries(main ws2_32)
endif()