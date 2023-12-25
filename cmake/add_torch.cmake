set(torch_dir ${CMAKE_CURRENT_BINARY_DIR}/torch)
set(torch_lib_name torch)

message("${torch_library_path}")
find_library(torch_lib
  NAMES ${torch_lib_name}
  PATHS ${torch_dir}/libtorch/lib
)

function (download_library url out)
  message("download ${url} to ${out}...")
  file(DOWNLOAD
      ${url}
      ${out}/torch_cc.zip
      SHOW_PROGRESS
    )
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf torch_cc.zip
                    WORKING_DIRECTORY ${out})
    file(REMOVE ${out}/torch_cc.zip)
endfunction()

if (DEFINED torch_version)
  message("setting torch version : ${torch_version}")
else()
  set(torch_version "2.1.0")
  message("torch version : ${torch_version}")
endif()

if (NOT torch_lib)
  message(STATUS "Downloading torch C API pre-built")

  # Download
  if (UNIX AND NOT APPLE)  # Linux
    set(torch_url
        "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-${torch_version}%2Bcpu.zip")
    download_library(${torch_url} ${torch_dir})
  elseif (UNIX AND APPLE)  # OSXx
    # Check if architecutre == ARM64
    if (NOT DEFINED APPLE_ARM64)
      set (APPLE_ARM64 (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64"))
    endif()
    ## If ARM, download both libraries and pre-compile UB libraries
    if (APPLE_ARM64)
      # download arm64 library
      download_library("https://anaconda.org/pytorch/pytorch/${torch_version}/download/osx-arm64/pytorch-${torch_version}-py3.10_0.tar.bz2" ${torch_dir}-arm64)
      execute_process(COMMAND mkdir ${torch_dir})
      execute_process(COMMAND cp -r ${torch_dir}-arm64/lib/python3.10/site-packages/torch ${torch_dir}/libtorch)
      # file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/torch/lib/python3.10/site-packages/torch" "${CMAKE_CURRENT_BINARY_DIR}/torch/torch")
      # file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/torch/torch" "${CMAKE_CURRENT_BINARY_DIR}/torch/libtorch")
      # download x86 library
      download_library("https://download.pytorch.org/libtorch/cpu/libtorch-macos-${torch_version}.zip" ${torch_dir}-x86)
      # export UB libs to main path
      execute_process(COMMAND find ${torch_dir}/libtorch/lib -maxdepth 1 -type f -execdir lipo -create ${torch_dir}/libtorch/lib/{} ${torch_dir}-x86/libtorch/lib/{} -output ${torch_dir}/libtorch/lib/{} \;)
      execute_process(COMMAND rm -rf ${torch_dir}-x86)
      file(REMOVE ${torch_dir}-x86)
    else()
      download_library("https://download.pytorch.org/libtorch/cpu/libtorch-macos-${torch_version}.zip" ${torch_dir})
    endif()
  else()
    download_library("https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-${torch_version}%2Bcpu.zip")
  endif()
endif()

# Find the libraries again
find_library(torch_lib
  NAMES ${torch_lib_name}
  PATHS ${torch_dir}/libtorch/lib
)

if (NOT torch_lib)
  message(FATAL_ERROR "torch could not be included")
endif()
