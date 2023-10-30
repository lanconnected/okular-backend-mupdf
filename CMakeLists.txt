cmake_minimum_required(VERSION 3.16.0)
project(okular-mupdf)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENJPEG2 REQUIRED libopenjp2)
pkg_check_modules(OPENSSL REQUIRED openssl)

find_package(ECM 5.19.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
include(FeatureSummary)
include(KDEInstallDirs)

include(ECMEnableSanitizers)
# Unless we do this we get some really fucking annoying warnings about stuff
# noone cares about (yes, I'm hangry).
set(KDE_SKIP_TEST_SETTINGS TRUE)
include(KDECMakeSettings)


find_package(Okular5 REQUIRED)
find_package(MuPDF REQUIRED)
find_package(JPEG REQUIRED)

# TODO: old crap style, fix
find_package(jbig2dec REQUIRED)
find_package(OpenJPEG REQUIRED)
find_package(LCMS2 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Gumbo REQUIRED IMPORTED_TARGET gumbo)

find_package(KF5 REQUIRED COMPONENTS
    CoreAddons
    I18n
    )

set(okularGenerator_mupdf_SRCS
  document.cpp
  page.cpp
  generator_mupdf.cpp
  document.hpp
  page.hpp
  generator_mupdf.hpp
)

kcoreaddons_add_plugin(okularGenerator_mupdf
    JSON "libokularGenerator_mupdf.json"
    INSTALL_NAMESPACE "okular/generators"
    SOURCES ${okularGenerator_mupdf_SRCS}
    )

target_link_libraries(okularGenerator_mupdf
    Okular::Core
    KF5::I18n
    MuPDF::Main
    MuPDF::MuJS
    JPEG::JPEG
    ${OPENJPEG_LIBRARIES}
    ${JBIG2DEC_LIBRARIES}
    ${LCMS2_LIBRARIES}
    PkgConfig::Gumbo
)
include_directories(
    ${OPENJPEG_INCLUDE_DIRS}
    ${JBIG2DEC_INCLUDE_DIRS}
    ${LCMS2_INCLUDE_DIR}
    )

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
