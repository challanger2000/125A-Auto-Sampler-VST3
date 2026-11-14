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

# P002: Steinberg VST3 SDK 3.8.1 (pinned by Windows CI). No third-party host.
option(BUILD_VST3 "Build the two 125A VST3 components" OFF)
if(BUILD_VST3)
    if(NOT DEFINED VST3_SDK_ROOT OR NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "Set VST3_SDK_ROOT to a checked-out Steinberg VST3 SDK")
    endif()
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    option(BUILD_VST3_VALIDATOR "Compile Steinberg VST3 validator" OFF)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ${BUILD_VST3_VALIDATOR} CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK 0 CACHE BOOL "" FORCE)
    add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/steinberg-sdk" EXCLUDE_FROM_ALL)

    # Steinberg's VST3 CMake macro expands source lists via a CMake string.
    # Use relative forward-slash paths: absolute Windows paths contain invalid \\a escapes.
    file(RELATIVE_PATH _125A_SCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
    file(RELATIVE_PATH _125A_DLLMAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
    smtg_add_vst3plugin(AutosamplerVST3
        ${_125A_SCE_SOURCE}
        ${_125A_DLLMAIN_SOURCE}
        src/vst3/Plugin.cpp
        src/core/SamplingPlan.h
        src/core/SampleProcessing.h
        src/core/Timeline.h
    )
    target_include_directories(AutosamplerVST3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AutosamplerVST3 PRIVATE sdk vstgui_support ole32 shell32 uuid)
    smtg_target_add_plugin_resources(AutosamplerVST3 RESOURCES resource/autosampler.uidesc)
    target_compile_features(AutosamplerVST3 PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(AutosamplerVST3 PRIVATE /W4 /permissive-)
    endif()
    if(BUILD_TESTING AND WIN32)
        # The QA host is a private test executable. It never ships inside
        # the VST3 bundle and never hosts any customer VST instruments.
        file(RELATIVE_PATH _125A_WINHOST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}"
            "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp")
        add_library(AutosamplerQAWinModule OBJECT
            ${_125A_WINHOST_SOURCE})
        set_target_properties(AutosamplerQAWinModule PROPERTIES
            CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
        target_compile_definitions(AutosamplerQAWinModule PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(AutosamplerQAWinModule PRIVATE sdk_hosting sdk_common base pluginterfaces)

        add_executable(AutosamplerIntegration tests/vst3_integration.cpp)
        target_compile_features(AutosamplerIntegration PRIVATE cxx_std_23)
        target_compile_definitions(AutosamplerIntegration PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(AutosamplerIntegration PRIVATE
            AutosamplerQAWinModule sdk_hosting sdk_common base pluginterfaces user32 gdi32 ole32)
        if(MSVC)
            target_compile_options(AutosamplerIntegration PRIVATE /W4 /permissive- /utf-8)
        endif()
    endif()
endif()
