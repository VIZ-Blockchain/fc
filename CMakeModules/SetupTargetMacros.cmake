# This module defines several macros that are useful for setting up library,
# plugin, and executable targets.


include(ArgumentParser)

function(enable_unity_build UB_SUFFIX SOURCE_VARIABLE_NAME)
    set(files ${${SOURCE_VARIABLE_NAME}})
    # Generate a unique filename for the unity build translation unit
    set(unit_build_file ${CMAKE_CURRENT_BINARY_DIR}/ub_${UB_SUFFIX}.cpp)
    # Exclude all translation units from compilation
    set_source_files_properties(${files} PROPERTIES HEADER_FILE_ONLY true)
    # Open the ub file
    file(WRITE ${unit_build_file} "// Unity Build generated by CMake\n")
    # Add include statement for each translation unit
    foreach(source_file ${files})
        file(APPEND ${unit_build_file} "#include <${CMAKE_CURRENT_SOURCE_DIR}/${source_file}>\n")
    endforeach(source_file)
    # Complement list of translation units with the name of ub
    set(${SOURCE_VARIABLE_NAME} ${${SOURCE_VARIABLE_NAME}} ${unit_build_file} PARENT_SCOPE)
endfunction(enable_unity_build)

# SETUP_LIBRARY Macro
# Sets up to build a library target. The macro uses the following global
# variables to define default values (you may change these variables to change
# the defaults:
#     DEFAULT_HEADER_INSTALL_DIR
#     DEFAULT_LIBRARY_INSTALL_DIR
#
# Usage:
#     SETUP_LIBRARY( target
#                    SOURCES source1 [source2...]
#                    MOC_HEADERS header1 [header2...]
#                    LIBRARIES library1 [library2...]
#                    INSTALL_HEADERS header1 [header2...]
#                    HEADER_INSTALL_DIR dir
#                    LIBRARY_INSTALL_DIR dir
#                    DEBUG_POSTFIX string
#                    LIBRARY_TYPE string
#                    AUTO_INSTALL_HEADERS
#                    DONT_INSTALL_LIBRARY )
#
# Parameters:
#     target                  The target library.
#     SOURCES                 Follow with the sources to compile.
#     MOC_HEADERS             Follow with the headers to moc (Requires Qt).
#     LIBRARIES               Follow with the libraries to link.
#     INSTALL_HEADERS         Follow with the headers to install.
#     HEADER_INSTALL_DIR      Follow with the directory to install the headers
#                             in (${DEFAULT_HEADER_INSTALL_DIR} by default).
#     LIBRARY_INSTALL_DIR     Follow with the directory to install the library
#                             in (${DEFAULT_LIBRARY_INSTALL_DIR} by default).
#     DEBUG_POSTFIX           Follow with the postfix to use when building in
#                             debug mode (${CMAKE_DEBUG_POSTFIX} by default).
#     LIBRARY_TYPE            Follow with the type of library to build: SHARED,
#                             STATIC, or MODULE (if not passed, then the
#                             behavior is defined by BUILD_SHARED_LIBS).
#     AUTO_INSTALL_HEADERS    If passed, all *.h files in the current directory
#                             will be installed.
#     DONT_INSTALL_LIBRARY    If passed, the library will not be installed.
macro(SETUP_LIBRARY target)

    # Setup the list headers.
    set(list_headers
        SOURCES
        MOC_HEADERS
        LIBRARIES
        INSTALL_HEADERS
        HEADER_INSTALL_DIR
        LIBRARY_INSTALL_DIR
        DEBUG_POSTFIX
        LIBRARY_TYPE
        )

    # Setup the boolean headers.
    set(bool_headers
        AUTO_INSTALL_HEADERS
        DONT_INSTALL_LIBRARY
        )

    # Parse the arguments into variables.
    argument_parser("" "${list_headers}" "${bool_headers}" ${ARGN})

    # Set the default values for the header_install_dir, library_install_dir,
    # and debug_postfix.
    if(NOT "${ARGN}" MATCHES "(^|;)HEADER_INSTALL_DIR($|;)")
        set(header_install_dir ${DEFAULT_HEADER_INSTALL_DIR})
    endif(NOT "${ARGN}" MATCHES "(^|;)HEADER_INSTALL_DIR($|;)")

    if(NOT "${ARGN}" MATCHES "(^|;)LIBRARY_INSTALL_DIR($|;)")
        set(library_install_dir ${DEFAULT_LIBRARY_INSTALL_DIR})
    endif(NOT "${ARGN}" MATCHES "(^|;)LIBRARY_INSTALL_DIR($|;)")

    if(NOT "${ARGN}" MATCHES "(^|;)DEBUG_POSTFIX($|;)")
        set(debug_postfix ${CMAKE_DEBUG_POSTFIX})
    endif(NOT "${ARGN}" MATCHES "(^|;)DEBUG_POSTFIX($|;)")

    # Configure the header_install_dir and library_install_dir so that ${target}
    # may be used in them. Setting target to itself is REQUIRED for the
    # configuration to work.
    set(target "${target}")
    string(CONFIGURE "${header_install_dir}" header_install_dir)
    string(CONFIGURE "${library_install_dir}" library_install_dir)

    # Setup the library_type.
    if(NOT library_type)
        set(library_type STATIC)
        if(BUILD_SHARED_LIBS)
            set(library_type SHARED)
        endif(BUILD_SHARED_LIBS)
    endif(NOT library_type)

    # Clear the moc_sources.
    set(moc_sources "")

    # If Qt is being used...
    if(QT_FOUND AND QT_LIBRARIES)
        # Setup QT to build a shared library.
        if(library_type MATCHES SHARED)
            add_definitions(-DQT_SHARED)
        endif(library_type MATCHES SHARED)

        # Setup the moc sources.
        if(moc_headers)
            qt4_wrap_cpp(moc_sources ${moc_headers})
        endif(moc_headers)
    endif(QT_FOUND AND QT_LIBRARIES)

    # Fatal error if moc_headers given but moc_sources not created.
    if(moc_headers AND NOT moc_sources)
        message(FATAL_ERROR "Calling SETUP_LIBRARY() with MOC_HEADERS failed. "
                "Make sure that you included \${QT_USE_FILE} prior to calling "
                "SETUP_LIBRARY().")
    endif(moc_headers AND NOT moc_sources)


    if(UNITY_BUILD)
        enable_unity_build(${target} sources)
    endif(UNITY_BUILD)

    # Add the library.
    add_library("${target}" ${library_type} ${sources} ${moc_sources})

    # Setup the debug_postfix.
    set_target_properties("${target}" PROPERTIES
                          DEBUG_POSTFIX "${debug_postfix}")

    # Link in the dependency libraries.
    target_link_libraries("${target}" ${libraries})

    # If auto_install_headers, then set the headers to all .h files in the
    # directory.
    if(auto_install_headers)
        file(GLOB install_headers *.h)
    endif(auto_install_headers)

    # Install the headers.
    if(install_headers)
        install(FILES ${install_headers} DESTINATION "${header_install_dir}")
    endif(install_headers)

    # Install the library.
    if(NOT dont_install_library)
        install(TARGETS "${target}"
                LIBRARY DESTINATION "${library_install_dir}"
                ARCHIVE DESTINATION "${library_install_dir}")
    endif(NOT dont_install_library)

endmacro(SETUP_LIBRARY)


# SETUP_MODULE Macro
# Sets up to build a module (also setup as a Qt plugin if using Qt). A module is
# built as a shared library; however, modules are typically loaded dynamically
# rather than linked against. Therefore, this macro does not install header
# files and uses its own default install directory. The macro uses the following
# global variables to define default values (you may change these variables to
# change the defaults:
#     DEFAULT_MODULE_INSTALL_DIR
#
# Usage:
#     SETUP_MODULE( target
#                   SOURCES source1 [source2...]
#                   MOC_HEADERS header1 [header2...]
#                   LIBRARIES library1 [library2...]
#                   MODULE_INSTALL_DIR dir
#                   DEBUG_POSTFIX string
#                   DONT_INSTALL_MODULE )
#
# Parameters:
#     target                 The target module (built as a shared library).
#     SOURCES                Follow with the sources to compile.
#     MOC_HEADERS            Follow with the headers to moc (Requires Qt).
#     LIBRARIES              Follow with the libraries to link.
#     MODULE_INSTALL_DIR     Follow with the directory to install the module in
#                            (${DEFAULT_MODULE_INSTALL_DIR} by default).
#     DEBUG_POSTFIX          Follow with the postfix to use when building in
#                            debug mode (${CMAKE_DEBUG_POSTFIX} by default).
#     DONT_INSTALL_MODULE    If passed, the module will not be installed.
macro(SETUP_MODULE target)

    # Setup the list headers.
    set(list_headers
        SOURCES
        MOC_HEADERS
        LIBRARIES
        MODULE_INSTALL_DIR
        DEBUG_POSTFIX
        )

    # Setup the boolean headers.
    set(bool_headers
        DONT_INSTALL_MODULE
        )

    # Parse the arguments into variables.
    argument_parser("" "${list_headers}" "${bool_headers}" ${ARGN})

    # Set the default values for the module_install_dir and debug postfix.
    if(NOT "${ARGN}" MATCHES "(^|;)MODULE_INSTALL_DIR($|;)")
        set(module_install_dir ${DEFAULT_MODULE_INSTALL_DIR})
    endif(NOT "${ARGN}" MATCHES "(^|;)MODULE_INSTALL_DIR($|;)")

    if(NOT "${ARGN}" MATCHES "(^|;)DEBUG_POSTFIX($|;)")
        set(debug_postfix ${CMAKE_DEBUG_POSTFIX})
    endif(NOT "${ARGN}" MATCHES "(^|;)DEBUG_POSTFIX($|;)")

    # Configure the module_install_dir so that ${target} may be used in it.
    # Setting target to itself is REQUIRED for the configuration to work.
    set(target "${target}")
    string(CONFIGURE "${module_install_dir}" module_install_dir)

    # Clear the moc_sources.
    set(moc_sources "")

    # If Qt is being used...
    if(QT_FOUND AND QT_LIBRARIES)
        add_definitions(-DQT_PLUGIN)

        # Setup the moc sources.
        if(moc_headers)
            qt4_wrap_cpp(moc_sources ${moc_headers})
        endif(moc_headers)
    endif(QT_FOUND AND QT_LIBRARIES)

    # Fatal error if moc_headers given but moc_sources not created.
    if(moc_headers AND NOT moc_sources)
        message(FATAL_ERROR "Calling SETUP_MODULE() with MOC_HEADERS failed. "
                "Make sure that you included \${QT_USE_FILE} prior to calling "
                "SETUP_MODULE().")
    endif(moc_headers AND NOT moc_sources)

    # Add the module (built as a shared library).
    add_library("${target}" SHARED ${sources} ${moc_sources})

    # Setup the debug postfix.
    set_target_properties("${target}" PROPERTIES
                          DEBUG_POSTFIX "${debug_postfix}")

    # Link in the dependency libraries.
    target_link_libraries("${target}" ${libraries})

    # Install the module.
    if(NOT dont_install_module)
        install(TARGETS "${target}"
                LIBRARY DESTINATION "${module_install_dir}")
    endif(NOT dont_install_module)

endmacro(SETUP_MODULE)


# SETUP_EXECUTABLE Macro
# Sets up to build an executable target. The macro uses the following global
# variables to define default values (you may change these variables to change
# the defaults:
#     DEFAULT_EXECUTABLE_INSTALL_DIR
#
# Usage:
#     SETUP_EXECUTABLE( target
#                       SOURCES source1 [source2...]
#                       MOC_HEADERS header1 [header2...]
#                       LIBRARIES library1 [library2...]
#                       EXECUTABLE_INSTALL_DIR dir
#                       DEBUG_POSTFIX string
#                       DONT_INSTALL_EXECUTABLE )
#
# Parameters:
#     target                     The target executable.
#     SOURCES                    Follow with the sources to compile.
#     MOC_HEADERS                Follow with the headers to moc (Requires Qt).
#     LIBRARIES                  Follow with the libraries to link.
#     EXECUTABLE_INSTALL_DIR     Follow with the directory to install the
#                                executable in
#                                (${DEFAULT_EXECUTABLE_INSTALL_DIR} by default).
#     DEBUG_POSTFIX              Follow with the postfix to use when building in
#                                debug mode (${CMAKE_DEBUG_POSTFIX} by
#                                default).
#     DONT_INSTALL_EXECUTABLE    If passed, the executable will not be
#                                installed.
macro(SETUP_EXECUTABLE target)

    # Setup the list headers.
    set(list_headers
        SOURCES
        MOC_HEADERS
        LIBRARIES
        EXECUTABLE_INSTALL_DIR
        DEBUG_POSTFIX
        )

    # Setup the boolean headers.
    set(bool_headers
        DONT_INSTALL_EXECUTABLE
        )

    # Parse the arguments into variables.
    argument_parser("" "${list_headers}" "${bool_headers}" ${ARGN})

    # Set the default values for the executable_install_dir and debug postfix.
    if(NOT "${ARGN}" MATCHES "(^|;)EXECUTABLE_INSTALL_DIR($|;)")
        set(executable_install_dir ${DEFAULT_EXECUTABLE_INSTALL_DIR})
    endif(NOT "${ARGN}" MATCHES "(^|;)EXECUTABLE_INSTALL_DIR($|;)")

    if(NOT "${ARGN}" MATCHES "(^|;)DEBUG_POSTFIX($|;)")
        set(debug_postfix ${CMAKE_DEBUG_POSTFIX})
    endif(NOT "${ARGN}" MATCHES "(^|;)DEBUG_POSTFIX($|;)")

    # Configure the executable_install_dir so that ${target} may be used in it.
    # Setting target to itself is REQUIRED for the configuration to work.
    set(target "${target}")
    string(CONFIGURE "${executable_install_dir}" executable_install_dir)

    # Clear the moc_sources.
    set(moc_sources "")

    # If Qt is being used...
    if(QT_FOUND AND QT_LIBRARIES)
        add_definitions(-DQT_SHARED)

        # Setup the moc sources.
        if(moc_headers)
            qt4_wrap_cpp(moc_sources ${moc_headers})
        endif(moc_headers)
    endif(QT_FOUND AND QT_LIBRARIES)

    # Fatal error if moc_headers given but moc_sources not created.
    if(moc_headers AND NOT moc_sources)
        message(FATAL_ERROR "Calling SETUP_EXECUTABLE() with MOC_HEADERS failed. "
                "Make sure that you included \${QT_USE_FILE} prior to calling "
                "SETUP_EXECUTABLE().")
    endif(moc_headers AND NOT moc_sources)

    # Add the executable.
    add_executable("${target}" ${sources} ${moc_sources})

    # Setup the debug postfix.
    set_target_properties("${target}" PROPERTIES
                          DEBUG_POSTFIX "${debug_postfix}")

    # Link in the dependency libraries.
    target_link_libraries("${target}" ${libraries})

    # Install the executable.
    if(NOT dont_install_executable)
        install(TARGETS "${target}" RUNTIME DESTINATION
                "${executable_install_dir}")
    endif(NOT dont_install_executable)

endmacro(SETUP_EXECUTABLE)