find_package(Threads REQUIRED)

find_library(HSA_LIBRARY hsa-runtime64 PATHS /opt/rocm/lib)
if (NOT EXISTS ${HSA_LIBRARY})
    message(STATUS "HSA runtime not found. Tests disabled.")
endif()

set(TEST_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(hostcall_add_test prefix filename language extralibs)
    get_filename_component(basename_we ${filename} NAME_WE)
    get_filename_component(basename ${filename} NAME)
    set(testname ${prefix}-${basename_we})
    add_executable(${testname} EXCLUDE_FROM_ALL ${filename})

    add_dependencies(build-tests ${testname})

    add_test(${testname} ${testname})
    target_include_directories(${testname} PRIVATE ${TEST_INC_DIR})
    target_link_libraries(${testname} amd_hostcall ${CMAKE_DL_LIBS} ${extralibs})
    target_link_libraries(${testname} Threads::Threads)

    if(${language} STREQUAL "C++")
        set_target_properties(${testname} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED Yes
        CXX_EXTENSIONS No)
    endif()
endfunction()

function(add_tests prefix)
    file(GLOB files *.c)
    list(REMOVE_ITEM files ${CMAKE_CURRENT_SOURCE_DIR}/no-hsa.c)
    foreach(file ${files})
        hostcall_add_test(${prefix} ${file} "C" ${HSA_LIBRARY})
    endforeach()

    file(GLOB files *.cpp)
    foreach(file ${files})
        hostcall_add_test(${prefix} ${file} "C++" ${HSA_LIBRARY})
    endforeach()
endfunction()

add_subdirectory(basic)
add_subdirectory(function-call)
