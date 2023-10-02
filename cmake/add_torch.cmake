set(torch_dir ${CMAKE_CURRENT_BINARY_DIR}/torch)
set(torch_lib_name torch)

message("${torch_library_path}")
find_library(torch_lib
  NAMES ${torch_lib_name}
  PATHS ${torch_dir}/libtorch/lib
)

if (DEFINED torch_version)
  message("setting torch version : ${torch_version}")
else()
  set(torch_version "2.0.1")
  message("torch version : ${torch_version}")
endif()

if (NOT torch_lib)
  message(STATUS "Downloading torch C API pre-built")

  # Download
  if (UNIX AND NOT APPLE)  # Linux
    set(torch_url
        "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-${torch_version}%2Bcpu.zip")
  elseif (UNIX AND APPLE)  # OSXx
    if (APPLE_ARM64)
      set(torch_url
          "https://anaconda.org/pytorch/pytorch/${torch_version}/download/osx-arm64/pytorch-2.0.1-py3.10_0.tar.bz2")
    else()
      set(torch_url
        "https://download.pytorch.org/libtorch/cpu/libtorch-macos-${torch_version}.zip")
    endif()
    message("ARM64 ${CMAKE_SYSTEM_PROCESSOR} detected.\n torch download link : ${torch_url}")
  else()                   # Windows
    set(torch_url
        "https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-${torch_version}%2Bcpu.zip")
  endif()

  file(DOWNLOAD
    ${torch_url}
    ${torch_dir}/torch_cc.zip
    SHOW_PROGRESS
  )
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf torch_cc.zip
                  WORKING_DIRECTORY ${torch_dir})

  file(REMOVE ${torch_dir}/torch_cc.zip)

endif()

# Find the libraries again
find_library(torch_lib
  NAMES ${torch_lib_name}
  PATHS ${torch_dir}/libtorch/lib
)

if (NOT torch_lib)
  message(FATAL_ERROR "torch could not be included")
endif()
