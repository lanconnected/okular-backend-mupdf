cmake_minimum_required(VERSION 3.16.0)
project(okular-mupdf)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)

find_package(ECM 6.3.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
include(FeatureSummary)
include(KDEInstallDirs)

include(ECMEnableSanitizers)
include(ECMSetupQtPluginMacroNames)
include(KDECMakeSettings)


find_package(Okular6 REQUIRED)
find_package(MuPDF REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(KF6 REQUIRED COMPONENTS
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

ecm_setup_qtplugin_macro_names(
    JSON_ARG1 "libokularGenerator_mupdf.json"
    )

kcoreaddons_add_plugin(okularGenerator_mupdf
    INSTALL_NAMESPACE "okular_generators"
    SOURCES ${okularGenerator_mupdf_SRCS}
    )

target_link_libraries(okularGenerator_mupdf
    Okular::Core
    KF6::I18n
    MuPDF::Main
    MuPDF::MuJS
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
