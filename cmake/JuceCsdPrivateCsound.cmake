include_guard(GLOBAL)

function(_juce_csd_find_global_csound out_real out_name out_inc)
    find_library(JUCE_CSD_GLOBAL_CSOUND_LIBRARY
        NAMES
            csound64
            csound
        REQUIRED
    )

    get_filename_component(_real "${JUCE_CSD_GLOBAL_CSOUND_LIBRARY}" REALPATH)
    get_filename_component(_name "${_real}" NAME)

    if(JUCE_CSD_CSOUND_INCLUDE_DIR)
        set(_inc "${JUCE_CSD_CSOUND_INCLUDE_DIR}")
    else()
        find_path(_inc
            NAMES
                csound/csound.h
                csound.h
        )
    endif()

    set(${out_real} "${_real}" PARENT_SCOPE)
    set(${out_name} "${_name}" PARENT_SCOPE)
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
    else()
        _juce_csd_find_global_csound(_global_real _global_name _global_inc)

        if(NOT _global_real MATCHES "\\.so")
            message(FATAL_ERROR
                "Global Csound library does not look like a Linux shared library: "
                "${_global_real}"
            )
        endif()

        if(_global_inc)
            target_include_directories(juce_csd_csound
                INTERFACE
                    "${_global_inc}"
            )
        endif()

        find_program(JUCE_CSD_PATCHELF_EXECUTABLE patchelf)

        set(_private_dir "${CMAKE_BINARY_DIR}/juce_csd_private_csound")

        if(JUCE_CSD_PATCHELF_EXECUTABLE)
            set(_private_soname "lib${plugin_target}_csound.so")
        else()
            message(WARNING
                "patchelf was not found. "
                "Using original Csound file name '${_global_name}'. "
                "This gives you a local copy, but not a fully private SONAME. "
                "Install patchelf for better isolation."
            )
            set(_private_soname "${_global_name}")
        endif()

        set(_private_lib "${_private_dir}/${_private_soname}")

        file(MAKE_DIRECTORY "${_private_dir}")

        if(JUCE_CSD_PATCHELF_EXECUTABLE)
            add_custom_command(
                OUTPUT "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E rm -f "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E copy "${_global_real}" "${_private_lib}"
                COMMAND chmod u+w "${_private_lib}"
                COMMAND ${JUCE_CSD_PATCHELF_EXECUTABLE}
                        --set-soname "${_private_soname}"
                        "${_private_lib}"
                DEPENDS "${_global_real}"
                COMMENT "Creating private Csound library ${_private_soname}"
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E remove -f "${_private_lib}"
                COMMAND ${CMAKE_COMMAND} -E copy "${_global_real}" "${_private_lib}"
                DEPENDS "${_global_real}"
                COMMENT "Copying global Csound library as private copy"
                VERBATIM
            )
        endif()

        if(NOT TARGET juce_csd_private_csound_build)
            add_custom_target(juce_csd_private_csound_build
                DEPENDS "${_private_lib}"
            )
        endif()

        if(NOT TARGET juce_csd_private_csound)
            add_library(juce_csd_private_csound SHARED IMPORTED GLOBAL)

            set_target_properties(juce_csd_private_csound
                PROPERTIES
                    IMPORTED_LOCATION "${_private_lib}"
            )

            if(JUCE_CSD_PATCHELF_EXECUTABLE)
                set_target_properties(juce_csd_private_csound
                    PROPERTIES
                        IMPORTED_SONAME "${_private_soname}"
                )
            endif()
        endif()

        # Important:
        #
        # Attach private Csound to juce_csd's existing Csound interface target.
        # This makes the final link line place Csound after libjuce_csd.a.
        target_link_libraries(juce_csd_csound
            INTERFACE
                juce_csd_private_csound
        )

        set_target_properties(juce_csd_csound
            PROPERTIES
                JUCE_CSD_PRIVATE_CSOUND_APPLIED TRUE
                JUCE_CSD_PRIVATE_CSOUND_LIB "${_private_lib}"
                JUCE_CSD_PRIVATE_CSOUND_SONAME "${_private_soname}"
        )

        if(TARGET juce_csd)
            add_dependencies(juce_csd juce_csd_private_csound_build)
        endif()
    endif()

    set(_rpath "$ORIGIN")

    if(APPLE)
        set(_rpath "@loader_path")
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

            set_target_properties(${_target}
                PROPERTIES
                    BUILD_RPATH "${_rpath}"
                    INSTALL_RPATH "${_rpath}"
            )

            # Copy before link so JUCE's post-link VST3 helper can already see it.
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


