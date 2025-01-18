cmake_minimum_required(VERSION 3.10)
project(Lab4)

add_library(serial SHARED serial.c)

add_executable(main main.c)

add_executable(emulator emulator.c)

target_link_libraries(main serial)

target_link_libraries(emulator serial)

file(WRITE tmp.txt "")
file(WRITE log.txt "")
file(WRITE log_hour.txt "")
file(WRITE log_day.txt "")
