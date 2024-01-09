cmake_minimum_required(VERSION 3.22)

### Declare project ###
project(rave-vst VERSION 0.0.2)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")


### Set up build type ###
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

### Import compiler settings ###
include(compiler_settings)
setup_compilers()
add_definitions(-w)


### Dependency versions ###
set(LIB_JUCE_TAG "7.0.8")

### IDE Generator pre-config ###
# Xcode: Disable automatic build scheme generation globally.
#        Instead, we explicitely specify which targets require schemes.
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# IDEs:  Enable grouping of source files into folders in IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# IDEs:  Create a folder in the IDE with the JUCE Module code.
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

### Dependency management ###
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/Libs" CACHE PATH "External dependencies path." FORCE)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${LIB_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_CONFIG     advice.detachedHead=false    # Disable detached HEAD warning for fetching a specific tag
    SOURCE_DIR     "${FETCHCONTENT_BASE_DIR}/JUCE"
    SUBBUILD_DIR   "${FETCHCONTENT_BASE_DIR}/JUCE-Subbuild"
    BINARY_DIR     "${FETCHCONTENT_BASE_DIR}/JUCE-Build")
FetchContent_MakeAvailable(juce)

### PyTorch related content ###
include(add_torch)
list(APPEND CMAKE_PREFIX_PATH "${torch_dir}/libtorch")
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

### Plugin Project Setup ###
set(active_formats AU VST3 Standalone)
juce_add_plugin("${PROJECT_NAME}"
    FORMATS ${active_formats} 
    PRODUCT_NAME "RAVE"
    ICON_BIG assets/icon.png
    ICON_SMALL assets/icon.png
    COMPANY_NAME acids
    COMPANY_WEBSITE acids.github.io
    PLUGIN_MANUFACTURER_CODE Acid
    PLUGIN_CODE Rave
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE 
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx, Generator, Mono, Stereo
    AU_MAIN_TYPE kAudioUnitType_Effect kAudioUnitType_Generator
    COPY_PLUGIN_AFTER_BUILD TRUE)

### Set C++ standard ###
target_compile_features("${PROJECT_NAME}" PUBLIC cxx_std_17)
get_property(project_targets DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
set_target_properties(${project_targets} PROPERTIES CXX_EXTENSIONS OFF)

### Adding source ###
add_subdirectory(source)

### Adding MKL shared libs for Linux ###
if (UNIX AND NOT APPLE)
  if (NOT EXISTS "${TORCH_INSTALL_PREFIX}/lib/libiomp5.so")
    message(STATUS "UNIX plateform detected: Downloading MKL shared libs")

    set(mkl_dir ${TORCH_INSTALL_PREFIX}/lib/)
    set(mkl_url "https://github.com/01org/mkl-dnn/releases/download/v0.9/mklml_lnx_2018.0.20170425.tgz")

    file(DOWNLOAD
      ${mkl_url}
      ${mkl_dir}/mkl_cc.tgz
      SHOW_PROGRESS
    )
    message(STATUS "extracting")

    execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf mkl_cc.tgz
                    WORKING_DIRECTORY ${mkl_dir})

    file(REMOVE ${mkl_dir}/mkl_cc.tgz)
    configure_file(
      "${TORCH_INSTALL_PREFIX}/lib/mklml_lnx_2018.0.20170425/lib/libiomp5.so"
      "${TORCH_INSTALL_PREFIX}/lib/libiomp5.so"
    )
  endif()
  set(libtorch_files
    "${TORCH_INSTALL_PREFIX}/lib/libtorch.so"
    "${TORCH_INSTALL_PREFIX}/lib/libtorch_cpu.so"
    "${TORCH_INSTALL_PREFIX}/lib/libc10.so"
    "${TORCH_INSTALL_PREFIX}/lib/libiomp5.so"
  )
endif (UNIX AND NOT APPLE)

### Generate JUCE header ###
juce_generate_juce_header("${PROJECT_NAME}")

### Additional definitions ###
target_compile_definitions("${PROJECT_NAME}"
    PUBLIC
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0
)

### Add binary assets ###
set (ImagesDir ${CMAKE_CURRENT_LIST_DIR}/assets)
file(GLOB_RECURSE images
        "${ImagesDir}/*.jpg"
        "${ImagesDir}/*.png"
        "${ImagesDir}/*.jpeg"
        )
set (ImagesTarget "${PROJECT_NAME}-Images")
juce_add_binary_data(${ImagesTarget} SOURCES ${images})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ImagesTarget})

### Link Torch libraries and headers ###
set(libtorch_files
    "${TORCH_INSTALL_PREFIX}/lib/libtorch.dylib"
    "${TORCH_INSTALL_PREFIX}/lib/libtorch_cpu.dylib"
    "${TORCH_INSTALL_PREFIX}/lib/libc10.dylib"
    "${TORCH_INSTALL_PREFIX}/lib/libiomp5.dylib"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${TORCH_INSTALL_PREFIX}/include/torch/*.h)

foreach(kind IN LISTS active_formats)
    foreach(resource_file IN LISTS libtorch_files)
        target_sources(${PROJECT_NAME}_${kind} PRIVATE ${resource_file})
        set_source_files_properties(${resource_file} PROPERTIES
          MACOSX_PACKAGE_LOCATION "Resources/libtorch"
        )
    endforeach()
endforeach()


### Link JUCE libraries ###
target_link_libraries("${PROJECT_NAME}"
    PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_audio_utils
        juce::juce_audio_basics
        juce::juce_dsp

    PUBLIC
        # ${libtorch_files}
        ${TORCH_LIBRARIES}
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


### IDE Generator post-config ###
set_target_properties(${project_targets} PROPERTIES
    FOLDER "Targets"
    XCODE_GENERATE_SCHEME ON)

### XCode signature preferences ###
# IDEs:  Move the "shared code" source group (main target) out of the "Targets" folder
#        to the top level.
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

### Windows adjustments ###
##### Windows
if (MSVC)
  set(base_folder "${CMAKE_CURRENT_BINARY_DIR}/rave-vst_artefacts/${CMAKE_BUILD_TYPE}/Standalone")
  add_custom_target(build-time-make-directory ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${base_folder})
  file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
  add_custom_command(TARGET ${target_name} POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     ${base_folder}/)
endif()

### Organize file groupings ###
get_target_property(project_sources "${PROJECT_NAME}" SOURCES)
get_target_property(juce_library_code "${PROJECT_NAME}" JUCE_GENERATED_SOURCES_DIRECTORY)
set(juce_header "${juce_library_code}/JuceHeader.h")
list(REMOVE_ITEM project_sources "${juce_header}")
source_group("JUCE Library Code" FILES "${juce_header}")
source_group("JUCE Library Code/CMake Rules" FILES "${juce_header}.rule")
source_group(TREE "${PROJECT_SOURCE_DIR}/Source" FILES "Source/*")
source_group(TREE "${PROJECT_SOURCE_DIR}/Build/torch" FILES "Build/torch")
if(TARGET ${ImagesTarget})
    get_target_property(project_assets ${ImagesTarget} SOURCES)
    source_group("" FILES ${project_assets})
endif()

