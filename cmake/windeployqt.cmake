if(WIN32)

function(windeployqt target)

    # Locate windeployqt next to qmake of the Qt version in use.
    # Resolved at call time, after find_package(Qt...) has run.
    get_target_property(_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

    # --no-angle and --no-opengl were removed from windeployqt in Qt 6.
    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        set(_windeployqt_extra_flags "")
    else()
        set(_windeployqt_extra_flags --no-angle --no-opengl)
    endif()

    # POST_BUILD step
    # - after build, we have a bin/lib for analyzing qt dependencies
    # - we run windeployqt on target and deploy Qt libs

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${_qt_bin_dir}/windeployqt.exe"
                --verbose 1
                "--$(Configuration.toLower())"
                --no-svg
                ${_windeployqt_extra_flags}
                --no-opengl-sw
                --no-compiler-runtime
                --no-system-d3d-compiler
                \"$<TARGET_FILE:${target}>\"
        COMMENT "Deploying Qt libraries using windeployqt for compilation target '${target}' ..."
    )

endfunction()
endif()
