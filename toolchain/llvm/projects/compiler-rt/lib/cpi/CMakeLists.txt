# Build for the CPI runtime support library.

set(CPI_SOURCES cpi.c)
set(CPI_INLINES cpi_inlines.c)
set(CPI_DEPENDS cpi.h
    common.h common.c common_inlines.c
    hashtable.h hashtable.c hashtable_inlines.c
    lookuptable.h lookuptable.c lookuptable_inlines.c
    simpletable.h simpletable.c simpletable_inlines.c)

include_directories(..)

set(CPI_CFLAGS
    -fno-builtin -fno-rtti -fno-exceptions
    -flto -O3 -DNDEBUG
    -Wno-gnu
    #-I/usr/src/lib/libthr/arch/amd64/include/
    #-g -DNDEBUG
    #-fcpi=none -fno-stack-protector
    )

set(CPI_INLINES_CFLAGS -DCPI_INLINES_ONLY ${CPI_CFLAGS})

# Architectures supported by CPI.
filter_available_targets(CPI_SUPPORTED_ARCH
  x86_64 i386)

set(CPI_RUNTIME_LIBRARIES)

function(_compile_bitcode name arch)
  if(CAN_TARGET_${arch})
    parse_arguments(BC "SOURCE;CFLAGS;DEPENDS" "" ${ARGN})

    string(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" CMAKE_CXX_FLAGS_BUILD)
    string(REPLACE " " ";" CXX_FLAGS
      ${CMAKE_CXX_FLAGS} ${${CMAKE_CXX_FLAGS_BUILD}}
      ${TARGET_${arch}_CFLAGS} ${LIB_CFLAGS})
    add_custom_command(OUTPUT ${name}
      COMMAND
        $<TARGET_FILE:clang> ${CXX_FLAGS} ${BC_CFLAGS}
                             ${CMAKE_CURRENT_SOURCE_DIR}/${BC_SOURCE}
                             -c -o ${name}
        MAIN_DEPENDENCY ${BC_SOURCE}
        DEPENDS clang ${BC_DEPENDS} IMPLICIT_DEPENDS CXX ${BC_SOURCE})
  else()
    message(FATAL_ERROR "Archtecture ${arch} can't be targeted")
  endif()
endfunction()

function(add_compiler_rt_bitcode_file name arch)
  set(filename ${COMPILER_RT_LIBRARY_OUTPUT_DIR}/../${name}.${arch}.bc)
  _compile_bitcode(${filename} ${arch} ${ARGN})

  add_custom_target(${name}.${arch} ALL DEPENDS ${filename})
  install(FILES ${filename} DESTINATION ${COMPILER_RT_LIBRARY_INSTALL_DIR}/..)
endfunction()

function(add_compiler_rt_bitcode_runtime name arch)
  if(CAN_TARGET_${arch})
    parse_arguments(BC "SOURCES;CFLAGS;DEPENDS" "" ${ARGN})

    set(objects)
    foreach(source ${BC_SOURCES})
      set(object ${name}.${source}.${arch}.o)
      _compile_bitcode(${object} ${arch} SOURCE ${source}
                       CFLAGS ${BC_CFLAGS} DEPENDS ${BC_DEPENDS})
      list(APPEND objects ${object})
    endforeach()

    set_source_files_properties(${objects}
        PROPERTIES GENERATED true EXTERNAL_OBJECT true)

    add_library(${name} STATIC ${objects})
    set_target_properties(${name} PROPERTIES LINKER_LANGUAGE CXX)
    # Setup correct output directory in the build tree.
    set_target_properties(${name} PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY ${COMPILER_RT_LIBRARY_OUTPUT_DIR})

    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
      add_custom_command(TARGET ${name} POST_BUILD
          COMMAND ${CMAKE_AR} --plugin=$<TARGET_FILE:LLVMgold> -s $<TARGET_FILE:${name}>)
    endif()

    # Add installation command.
    install(TARGETS ${name}
      ARCHIVE DESTINATION ${COMPILER_RT_LIBRARY_INSTALL_DIR})
  else()
    message(FATAL_ERROR "Archtecture ${arch} can't be targeted")
  endif()
endfunction()

# FIXME: Universal binaries on OSX cannot contain LLVM bitcode.
# Therefore, we build per-arch libraries on both OSX and Linux
foreach(arch ${CPI_SUPPORTED_ARCH})
  add_compiler_rt_bitcode_runtime(clang_rt.cps-${arch} ${arch}
    SOURCES ${CPI_SOURCES}
    CFLAGS ${CPI_CFLAGS}
    DEPENDS ${CPI_DEPENDS} ${CPI_INLINES})
  add_compiler_rt_bitcode_runtime(clang_rt.cpi-${arch} ${arch}
    SOURCES ${CPI_SOURCES}
    CFLAGS -DCPI_BOUNDS ${CPI_CFLAGS}
    DEPENDS ${CPI_DEPENDS} ${CPI_INLINES})
  add_compiler_rt_bitcode_file(clang_rt.cps-inlines ${arch}
    SOURCE ${CPI_INLINES}
    CFLAGS ${CPI_INLINES_CFLAGS}
    DEPENDS ${CPI_DEPENDS})
  add_compiler_rt_bitcode_file(clang_rt.cpi-inlines ${arch}
    SOURCE ${CPI_INLINES}
    CFLAGS -DCPI_BOUNDS ${CPI_INLINES_CFLAGS}
    DEPENDS ${CPI_DEPENDS})
endforeach()
