cmake_minimum_required(VERSION 3.5)

project(JRE)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SOURCE_DIR ${ROOT_DIR}/src)
set(IMAGE_DIR ${ROOT_DIR}/images)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_language(ASM)



option(GAMEBOY_ADVANCE "GameboyAdvance" ON)


if(GAMEBOY_ADVANCE AND NOT DEVKITARM)
  message(WARNING "Note: GAMEBOY_ADVANCE option is ON by default.")
  message(FATAL_ERROR "GAMEBOY_ADVANCE option is set, but missing ARM toolchain.")
endif()


if(GAMEBOY_ADVANCE AND WIN32)
  message(FATAL_ERROR "Gameboy Advance builds not supported in windows")
endif()


include_directories(${SOURCE_DIR})


if(APPLE)
  set(EMACS_FLYCHECK_FRONTEND clang)
else()
  set(EMACS_FLYCHECK_FRONTEND gcc)
endif()

# For emacs flycheck users
configure_file(dirlocals.in ${SOURCE_DIR}/.dir-locals.el)



set(SOURCES
  ${SOURCE_DIR}/array.cpp
  ${SOURCE_DIR}/class.cpp
  ${SOURCE_DIR}/classfile.cpp
  ${SOURCE_DIR}/classtable.cpp
  ${SOURCE_DIR}/constantPool.cpp
  ${SOURCE_DIR}/crc32.cpp
  ${SOURCE_DIR}/debuggerConnection.cpp
  ${SOURCE_DIR}/debugger.cpp
  ${SOURCE_DIR}/gba_boot_jar.cpp
  ${SOURCE_DIR}/gc.cpp
  ${SOURCE_DIR}/jar.cpp
  ${SOURCE_DIR}/jdwp.cpp
  ${SOURCE_DIR}/jni.cpp
  ${SOURCE_DIR}/vm.cpp
  ${SOURCE_DIR}/memory.cpp
  ${SOURCE_DIR}/methodTable.cpp
  ${SOURCE_DIR}/test_print.cpp)


if(APPLE)
  add_executable(JRE MACOSX_BUNDLE ${SOURCES})


else()
  add_executable(JRE
    ${SOURCES})
endif()


if(APPLE OR UNIX OR GAMEBOY_ADVANCE)

  set(SHARED_COMPILE_OPTIONS
    -g
    -Ofast
    -Wall
    -Werror
    -pedantic
    -nostdlib
    -ffast-math
    -fno-math-errno
    -Wdouble-promotion
    -fomit-frame-pointer
    -D__BLINDJUMP_ENABLE_LOGS)

  if(GAMEBOY_ADVANCE)
    set(SHARED_COMPILE_OPTIONS
      ${SHARED_COMPILE_OPTIONS}
      -fno-exceptions)
  endif()

elseif(WIN32)

  set(SHARED_COMPILE_OPTIONS
    /W4)

  include_directories(${ROOT_DIR}/external/SFML-2.5.1/include/)

  set(SFML_LIB_DIR ${ROOT_DIR}/external/SFML-2.5.1/lib/Release)

  target_link_libraries(JRE
    ${SFML_LIB_DIR}/sfml-graphics.lib
    ${SFML_LIB_DIR}/sfml-window.lib
    ${SFML_LIB_DIR}/sfml-system.lib
    ${SFML_LIB_DIR}/sfml-audio.lib
    ${SFML_LIB_DIR}/sfml-network.lib)

  add_custom_command(TARGET JRE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-graphics-2.dll"
        $<TARGET_FILE_DIR:JRE>)

  add_custom_command(TARGET JRE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-audio-2.dll"
        $<TARGET_FILE_DIR:JRE>)

  add_custom_command(TARGET JRE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-window-2.dll"
        $<TARGET_FILE_DIR:JRE>)

  add_custom_command(TARGET JRE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-network-2.dll"
        $<TARGET_FILE_DIR:JRE>)

  add_custom_command(TARGET JRE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-system-2.dll"
        $<TARGET_FILE_DIR:JRE>)

   add_custom_command(TARGET JRE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ROOT_DIR}/external/SFML-2.5.1/extlibs/bin/x64/openal32.dll"
        $<TARGET_FILE_DIR:JRE>)
endif()


if(GAMEBOY_ADVANCE)

  # I am setting CMAKE_AR in the toolchain file, but for some reason, the
  # variable no longer exists within the cmakelists... which is strange, because
  # other variables set in the toolchain file seem to be persistent...
  set(CMAKE_AR $ENV{DEVKITARM}/bin/arm-none-eabi-ar)

  # add_library(ARM_ROUTINES ${SOURCE_DIR}/platform/gba/gba_arm_routines.cpp)

  # target_compile_options(ARM_ROUTINES PRIVATE
  #   ${SHARED_COMPILE_OPTIONS})

  # target_compile_options(ARM_ROUTINES PRIVATE
  #   -marm
  #   -mlong-calls
  #   -mthumb-interwork
  #   -mcpu=arm7tdmi
  #   -mtune=arm7tdmi)

  set(CMAKE_EXE_LINKER_FLAGS
    "-mthumb -mthumb-interwork -Wl,-Map,JRE.elf.map -specs=gba.specs"
    CACHE INTERNAL "" FORCE)

  target_link_libraries(JRE PRIVATE
    ${DEVKITPRO}/libtonc/lib/libtonc.a
    # ARM_ROUTINES
    )

  target_compile_options(JRE PRIVATE
    -mthumb
    -mthumb-interwork
    -mcpu=arm7tdmi
    -mtune=arm7tdmi)

  add_custom_command(TARGET JRE POST_BUILD
    COMMENT "building cartridge"
    COMMAND ${DEVKITARM}/bin/arm-none-eabi-objcopy -O binary JRE JRE.gba
    # NOTE: using DrillDozer's ROM id code for testing purposes, because many
    # emulator developers do not give you the option to manually override which
    # gamepacks support gpio rumble. JRE's actual ROM code was originally CBJE.
    COMMAND ${DEVKITPRO}/tools/bin/gbafix -tJRE -cV49E -r0 -mEB JRE.gba)

  target_compile_options(JRE PRIVATE
    -D__GBA__)

elseif(APPLE)
  target_link_libraries(JRE
    "-framework sfml-window -framework sfml-graphics -framework sfml-system -framework sfml-audio -framework sfml-network -framework Cocoa")
  set_target_properties(JRE
    PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")

  include_directories("/usr/local/include/")

  add_custom_target(pkg ALL mkdir -p JRE.app/Contents/Resources
    COMMAND mkdir -p JRE.app/Contents/Frameworks
    COMMAND cp -r ${ROOT_DIR}/images/ JRE.app/Contents/images/
    COMMAND cp -r ${ROOT_DIR}/shaders/ JRE.app/Contents/shaders/
    COMMAND cp -r ${ROOT_DIR}/sounds/ JRE.app/Contents/sounds/
    COMMAND cp -r ${ROOT_DIR}/scripts/ JRE.app/Contents/scripts/
    COMMAND cp -r ${ROOT_DIR}/strings/ JRE.app/Contents/strings/
    # COMMAND cp macOS/icon.icns JRE.app/Contents/Resources
    # COMMAND cp -r ${SFML_DIR}/lib/* JRE.app/Contents/Frameworks
    # COMMAND cp -r ${SFML_DIR}/extlibs/libs-osx/Frameworks/* JRE.app/Contents/Frameworks
    )

elseif(UNIX)

  target_link_libraries(JRE
    -lpthread
    -lsfml-window
    -lsfml-graphics
    -lsfml-system
    -lsfml-audio
    -lsfml-network)

endif()



target_compile_options(JRE PRIVATE
  ${SHARED_COMPILE_OPTIONS})



file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE HEADERS "${SOURCE_DIR}/*.hpp")

add_custom_target(clang-format
  COMMAND clang-format --style=file -i ${SOURCES} ${HEADERS}
  COMMENT "Running clang-format"
  VERBATIM)
