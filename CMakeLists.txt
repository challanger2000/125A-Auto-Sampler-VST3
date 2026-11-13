cmake_minimum_required(VERSION 3.25)
project(125A_AutoSampler VERSION 0.1.0 LANGUAGES CXX)

# P001: platform-neutral engine compiled on the Windows-2022 CI runner.
# Neither a VST host nor a finished VST3 binary is claimed in P001.
add_library(autosampler_core INTERFACE)
target_include_directories(autosampler_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(autosampler_core INTERFACE cxx_std_23)

include(CTest)
if(BUILD_TESTING)
    add_executable(autosampler_core_tests tests/core_tests.cpp)
    target_link_libraries(autosampler_core_tests PRIVATE autosampler_core)
    if(MSVC)
        target_compile_options(autosampler_core_tests PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(autosampler_core_tests PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME autosampler_core_tests COMMAND autosampler_core_tests)
endif()
