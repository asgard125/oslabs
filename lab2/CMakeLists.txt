cmake_minimum_required(VERSION 3.10.0)
project(ProcessManager)

if(WIN32)
    add_library(process_manager SHARED process_manager.c)

    add_executable(test_util test_util.c)

    target_link_libraries(test_util process_manager)

    add_executable(child_process child_process.c)
    

else()
    add_library(process_manager SHARED process_manager.c)

    add_executable(test_util test_util.c)

    target_link_libraries(test_util process_manager)

    add_executable(child_process child_process.c)

    find_package (Threads REQUIRED)

    target_link_libraries(test_util    ${CMAKE_THREAD_LIBS_INIT})

endif()
