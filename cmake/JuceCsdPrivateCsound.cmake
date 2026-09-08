include_guard(GLOBAL)

function(_juce_csd_find_global_csound out_link out_runtime out_name out_inc)
    find_library(JUCE_CSD_GLOBAL_CSOUND_LIBRARY
        NAMES
            csound64
            csound
            libcsound64
            libcsound
        REQUIRED
    )

    get_filename_component(_link "${JUCE_CSD_GLOBAL_CSOUND_LIBRARY}" ABSOLUTE)
    get_filename_component(_real "${_link}" REALPATH)
    get_filename_component(_real_dir "${_real}" DIRECTORY)
    get_filename_component(_real_name "${_real}" NAME)

    set(_link_file "${_real}")

    if(WIN32)
        if(_real_name MATCHES "\\.dll$")
            set(_runtime "${_real}")
        else()
            find_file(_juce_csd_global_runtime_dll
                NAMES
                    csound64.dll
                    libcsound64.dll
                    csound.dll
                    libcsound.dll
                HINTS
                    "${_real_dir}"
                    "${_real_dir}/../bin"
                    "${_real_dir}/../../bin"
                    "${_real_dir}/../lib"
                    "${_real_dir}/../runtime"
                NO_DEFAULT_PATH
            )

            if(NOT _juce_csd_global_runtime_dll)
                find_file(_juce_csd_global_runtime_dll_system
                    NAMES
                        csound64.dll
                        libcsound64.dll
                        csound.dll
                        libcsound.dll
                )

                if(_juce_csd_global_runtime_dll_system)
                    set(_juce_csd_global_runtime_dll "${_juce_csd_global_runtime_dll_system}")
                endif()
            endif()

            if(NOT _juce_csd_global_runtime_dll)
                message(FATAL_ERROR
                    "Could not find the Csound DLL runtime library. "
                    "find_library found '${_real}'. "
                    "Set JUCE_CSD_GLOBAL_CSOUND_RUNTIME manually or install Csound with its DLL."
                )
            endif()

            set(_runtime "${_juce_csd_global_runtime_dll}")
        endif()

        get_filename_component(_runtime "${_runtime}" ABSOLUTE)
        get_filename_component(_runtime_dir "${_runtime}" DIRECTORY)

        find_file(_juce_csd_global_implib
            NAMES
                csound64.lib
                csound64.dll.a
                libcsound64.dll.a
                libcsound64.a
                csound.lib
                csound.dll.a
                libcsound.dll.a
                libcsound.a
            HINTS
                "${_runtime_dir}"
                "${_real_dir}"
                "${_real_dir}/../lib"
                "${_real_dir}/../bin"
                "${_runtime_dir}/../lib"
                "${_runtime_dir}/../bin"
            NO_DEFAULT_PATH
        )

        if(NOT _juce_csd_global_implib)
            find_file(_juce_csd_global_implib_system
                NAMES
                    csound64.lib
                    csound64.dll.a
                    libcsound64.dll.a
                    libcsound64.a
                    csound.lib
                    csound.dll.a
                    libcsound.dll.a
                    libcsound.a
            )

            if(_juce_csd_global_implib_system)
                set(_juce_csd_global_implib "${_juce_csd_global_implib_system}")
            endif()
        endif()

        if(_juce_csd_global_implib)
            set(_link_file "${_juce_csd_global_implib}")
        elseif(MSVC AND _real_name MATCHES "\\.dll$")
            message(FATAL_ERROR
                "Found Csound DLL '${_real}' but no import library. "
                "With MSVC, CMake needs a .lib import library to link against Csound."
            )
        endif()
    else()
        if(NOT _real_name MATCHES "\\.(so|dylib)(\\.[0-9]+)*$")
            message(FATAL_ERROR
                "Global Csound library does not look like a shared library: ${_real}"
            )
        endif()

        set(_runtime "${_real}")
        get_filename_component(_runtime "${_runtime}" ABSOLUTE)
    endif()

    get_filename_component(_runtime_name "${_runtime}" NAME)

    if(JUCE_CSD_CSOUND_INCLUDE_DIR)
        set(_inc "${JUCE_CSD_CSOUND_INCLUDE_DIR}")
    else()
        find_path(_inc
            NAMES
                csound/csound.h
                csound.h
        )
    endif()

    set(${out_link} "${_link_file}" PARENT_SCOPE)
    set(${out_runtime} "${_runtime}" PARENT_SCOPE)
    set(${out_name} "${_runtime_name}" PARENT_SCOPE)
    set(${out_inc} "${_inc}" PARENT_SCOPE)
endfunction()

function(juce_csd_use_private_csound_vst3 plugin_target)
    if(NOT TARGET juce_csd_csound)
        message(FATAL_ERROR
            "juce_csd_csound target not found. "
            "Add juce_csd before calling juce_csd_use_private_csound_vst3()."
        )
    endif()

    if(JUCE_CSD_LINK_GLOBAL_CSOUND)
        message(FATAL_ERROR
            "JUCE_CSD_LINK_GLOBAL_CSOUND must be OFF when using private Csound. "
            "Set it to OFF before add_subdirectory() of juce_csd."
        )
    endif()

    get_target_property(_already_applied
        juce_csd_csound
        JUCE_CSD_PRIVATE_CSOUND_APPLIED
    )

    if(_already_applied)
        get_target_property(_private_lib
            juce_csd_csound
            JUCE_CSD_PRIVATE_CSOUND_LIB
        )
        get_target_property(_private_soname
            juce_csd_csound
            JUCE_CSD_PRIVATE_CSOUND_SONAME
        )
        get_target_property(_private_implib
            juce_csd_csound
            JUCE_CSD_PRIVATE_CSOUND_IMPLIB
        )
    else()
        _juce_csd_find_global_csound(_global_link _global_runtime _global_name _global_inc)

        if(NOT _global_runtime)
            message(FATAL_ERROR "Could not locate global Csound runtime library.")
        endif()

        if(_global_inc)
            target_include_directories(juce_csd_csound
                INTERFACE
                    "${_global_inc}"
            )
        else()
            message(WARNING
                "Could not find Csound include directory. "
                "Set JUCE_CSD_CSOUND_INCLUDE_DIR if compilation fails."
            )
        endif()

        set(_private_dir "${CMAKE_BINARY_DIR}/juce_csd_private_csound")
        file(MAKE_DIRECTORY "${_private_dir}")

        if(APPLE)
            find_program(JUCE_CSD_INSTALL_NAME_TOOL_EXECUTABLE install_name_tool)

            if(NOT JUCE_CSD_INSTALL_NAME_TOOL_EXECUTABLE)
                message(FATAL_ERROR
                    "install_name_tool is required to create a private Csound dylib on macOS."
                )
            endif()

            find_program(JUCE_CSD_CODESIGN_EXECUTABLE codesign)

            set(_private_soname "lib${plugin_target}_csound.dylib")
            set(_private_lib "${_private_dir}/${_private_soname}")
            set(_private_implib "${_private_lib}")

            add_custom_command(
                OUTPUT "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E rm -f "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E copy "${_global_runtime}" "${_private_lib}"
                COMMAND chmod u+w "${_private_lib}"
                COMMAND ${JUCE_CSD_INSTALL_NAME_TOOL_EXECUTABLE}
                        -id "@loader_path/${_private_soname}"
                        "${_private_lib}"
                DEPENDS "${_global_runtime}"
                COMMENT "Creating private Csound dylib ${_private_soname}"
                VERBATIM
            )

            if(JUCE_CSD_CODESIGN_EXECUTABLE)
                add_custom_command(
                    OUTPUT "${_private_lib}"
                    COMMAND ${JUCE_CSD_CODESIGN_EXECUTABLE}
                            --force
                            --sign -
                            "${_private_lib}"
                    VERBATIM
                    APPEND
                )
            endif()
        elseif(WIN32)
            set(_private_soname "${_global_name}")
            set(_private_lib "${_private_dir}/${_private_soname}")
            set(_private_implib "${_global_link}")

            add_custom_command(
                OUTPUT "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E rm -f "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E copy "${_global_runtime}" "${_private_lib}"
                DEPENDS "${_global_runtime}"
                COMMENT "Copying global Csound DLL as private copy"
                VERBATIM
            )
        else()
            find_program(JUCE_CSD_PATCHELF_EXECUTABLE patchelf)

            if(JUCE_CSD_PATCHELF_EXECUTABLE)
                set(_private_soname "lib${plugin_target}_csound.so")
                set(_private_lib "${_private_dir}/${_private_soname}")
                set(_private_implib "${_private_lib}")

                add_custom_command(
                    OUTPUT "${_private_lib}"
                    COMMAND ${CMAKE_COMMAND} -E rm -f "${_private_lib}"
                    COMMAND ${CMAKE_COMMAND} -E copy "${_global_runtime}" "${_private_lib}"
                    COMMAND chmod u+w "${_private_lib}"
                    COMMAND ${JUCE_CSD_PATCHELF_EXECUTABLE}
                            --set-soname "${_private_soname}"
                            "${_private_lib}"
                    DEPENDS "${_global_runtime}"
                    COMMENT "Creating private Csound library ${_private_soname}"
                    VERBATIM
                )
            else()
                message(WARNING
                    "patchelf was not found. "
                    "Using original Csound file name '${_global_name}'. "
                    "This gives you a local copy, but not a fully private SONAME. "
                    "Install patchelf for better isolation."
                )

                set(_private_soname "${_global_name}")
                set(_private_lib "${_private_dir}/${_private_soname}")
                set(_private_implib "${_private_lib}")

                add_custom_command(
                    OUTPUT "${_private_lib}"
                    COMMAND ${CMAKE_COMMAND} -E rm -f "${_private_lib}"
                    COMMAND ${CMAKE_COMMAND} -E copy "${_global_runtime}" "${_private_lib}"
                    DEPENDS "${_global_runtime}"
                    COMMENT "Copying global Csound library as private copy"
                    VERBATIM
                )
            endif()
        endif()

        if(NOT TARGET juce_csd_private_csound_build)
            add_custom_target(juce_csd_private_csound_build
                DEPENDS "${_private_lib}"
            )
        endif()

        if(NOT TARGET juce_csd_private_csound)
            add_library(juce_csd_private_csound SHARED IMPORTED GLOBAL)

            if(WIN32)
                set_target_properties(juce_csd_private_csound
                    PROPERTIES
                        IMPORTED_LOCATION "${_private_lib}"
                        IMPORTED_IMPLIB "${_private_implib}"
                )
            else()
                set_target_properties(juce_csd_private_csound
                    PROPERTIES
                        IMPORTED_LOCATION "${_private_lib}"
                )

                if(NOT APPLE AND JUCE_CSD_PATCHELF_EXECUTABLE)
                    set_target_properties(juce_csd_private_csound
                        PROPERTIES
                            IMPORTED_SONAME "${_private_soname}"
                    )
                endif()
            endif()
        endif()

        target_link_libraries(juce_csd_csound
            INTERFACE
                juce_csd_private_csound
        )

        set_target_properties(juce_csd_csound
            PROPERTIES
                JUCE_CSD_PRIVATE_CSOUND_APPLIED TRUE
                JUCE_CSD_PRIVATE_CSOUND_LIB "${_private_lib}"
                JUCE_CSD_PRIVATE_CSOUND_SONAME "${_private_soname}"
                JUCE_CSD_PRIVATE_CSOUND_IMPLIB "${_private_implib}"
        )

        if(TARGET juce_csd)
            add_dependencies(juce_csd juce_csd_private_csound_build)
        endif()

        message(STATUS
            "Private Csound runtime: ${_private_lib}. "
            "If Csound depends on additional shared libraries, bundle those separately."
        )
    endif()

    foreach(_suffix VST3 Standalone)
        set(_target "${plugin_target}_${_suffix}")

        if(TARGET ${_target})
            add_dependencies(${_target} juce_csd_private_csound_build)

            set_property(TARGET ${_target}
                APPEND
                PROPERTY
                    LINK_DEPENDS "${_private_lib}"
            )

            if(NOT WIN32)
                set(_rpath "$ORIGIN")

                if(APPLE)
                    set(_rpath "@loader_path")
                endif()

                set_property(TARGET ${_target}
                    APPEND
                    PROPERTY
                        BUILD_RPATH "${_rpath}"
                )

                set_property(TARGET ${_target}
                    APPEND
                    PROPERTY
                        INSTALL_RPATH "${_rpath}"
                )
            endif()

            add_custom_command(TARGET ${_target}
                PRE_LINK
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${_target}>"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_private_lib}"
                        "$<TARGET_FILE_DIR:${_target}>/${_private_soname}"
                COMMENT "Bundling private Csound into $<TARGET_FILE_DIR:${_target}>"
                VERBATIM
            )
        endif()
    endforeach()
endfunction()
