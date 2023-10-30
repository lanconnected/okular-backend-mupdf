include(FindPackageHandleStandardArgs)

find_path(MuPDF_INCLUDE_DIR
    NAMES mupdf/pdf.h
)

find_path(MuJS_INCLUDE_DIR
    NAMES mujs.h
)

find_library(MuPDF_MAIN_LIBRARY
    NAMES mupdf
)
find_library(MuJS_LIBRARY
    NAMES mujs
)

find_package_handle_standard_args(MuPDF
    FOUND_VAR
        MuPDF_FOUND
    REQUIRED_VARS
        MuPDF_INCLUDE_DIR
        MuPDF_MAIN_LIBRARY
        MuJS_LIBRARY
)

if(MuPDF_FOUND)
    if (NOT TARGET MuPDF::Main)
        add_library(MuPDF::Main UNKNOWN IMPORTED)
        set_target_properties(MuPDF::Main PROPERTIES
            IMPORTED_LOCATION "${MuPDF_MAIN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MuPDF_INCLUDE_DIR}")
    endif()
    if (NOT TARGET MuPDF::MuJS)
        add_library(MuPDF::MuJS UNKNOWN IMPORTED)
        set_target_properties(MuPDF::MuJS PROPERTIES
            IMPORTED_LOCATION "${MuJS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MuJS_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(MuPDF_MAIN_LIBRARY MuJS_LIBRARY MuPDF_INCLUDE_DIR MuJS_INCLUDE_DIR)

if(MuPDF_FOUND)
    set(MuPDF_LIBRARIES ${MuPDF_MAIN_LIBRARY} ${MuJS_LIBRARY})
    set(MuPDF_INCLUDE_DIRS ${MuPDF_INCLUDE_DIR})
endif()
