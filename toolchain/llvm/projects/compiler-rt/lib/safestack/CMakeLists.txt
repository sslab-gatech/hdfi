# Build for the safestack runtime support library.

set(SAFESTACK_SOURCES safestack.cc)

include_directories(..)

set(SAFESTACK_CFLAGS
    -fno-builtin -fno-rtti -fno-exceptions
    -O3 -DNDEBUG
    #-fno-cpi -fno-stack-protector -g -DNDEBUG
    )

# Architectures supported by SAFESTACK.
filter_available_targets(SAFESTACK_SUPPORTED_ARCH
  x86_64 i386)

set(SAFESTACK_RUNTIME_LIBRARIES)

if(APPLE)
  # Build universal binary on APPLE.
  add_compiler_rt_osx_static_runtime(clang_rt.safestack_osx
    ARCH ${SAFESTACK_SUPPORTED_ARCH}
    SOURCES ${SAFESTACK_SOURCES}
            $<TARGET_OBJECTS:RTInterception.osx>
    CFLAGS ${SAFESTACK_CFLAGS})
  list(APPEND SAFESTACK_RUNTIME_LIBRARIES clang_rt.safestack_osx)
else()
  # Otherwise, build separate libraries for each target.
  foreach(arch ${SAFESTACK_SUPPORTED_ARCH})
    add_compiler_rt_static_runtime(clang_rt.safestack-${arch} ${arch}
      SOURCES ${SAFESTACK_SOURCES}
              $<TARGET_OBJECTS:RTInterception.${arch}>
      CFLAGS ${SAFESTACK_CFLAGS})
    list(APPEND SAFESTACK_RUNTIME_LIBRARIES clang_rt.safestack-${arch})
  endforeach()
endif()
