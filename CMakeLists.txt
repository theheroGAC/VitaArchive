cmake_minimum_required(VERSION 3.10)

# VitaSDK defines
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable pointing to your SDK path!")
  endif()
endif()

project(VitaArchive VERSION 1.0 LANGUAGES C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "VitaArchive")
set(VITA_TITLEID  "VITARCH01")
set(VITA_VERSION  "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -fno-lto -Wl,-z,noexecstack")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s CATEGORY=gdb")
set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2808000000000000")

include_directories(
  ${PROJECT_SOURCE_DIR}
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  "${VITASDK}/arm-vita-eabi/lib"
)

add_executable(${PROJECT_NAME}
  main.c
  zip.c
  vpk.c
  filebrowser.c
  rar.c
  archive7z.c
  language.c
)

target_link_libraries(${PROJECT_NAME}
  vita2d
  SceAppMgr_stub
  SceLibKernel_stub
  SceCtrl_stub
  SceIofilemgr_stub
  SceDisplay_stub
  SceGxm_stub
  SceCommonDialog_stub
  ScePromoterUtil_stub
  SceSysmodule_stub
  SceIme_stub
  ScePgf_stub
  archive
  freetype
  png
  lzma
  unrar
  bz2
  z
  m
  pthread
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

vita_create_vpk(${PROJECT_NAME}.vpk
  ${VITA_TITLEID}
  eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}

  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml

  # Language files
  FILE lang/it.txt lang/it.txt
  FILE lang/en.txt lang/en.txt
  FILE lang/es.txt lang/es.txt
  FILE lang/fr.txt lang/fr.txt
  FILE lang/de.txt lang/de.txt
  FILE lang/jpn.txt lang/jpn.txt
)
