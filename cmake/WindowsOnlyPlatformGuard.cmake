# Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 or (at your option)
# version 3 of the License.

function(kpxc_enforce_windows_only_scope)
    set(forbidden_paths
        "cmake/MacOSCodesign.cmake.in"
        "cmake/compiler-checks/macos"
        "share/macosx"
        "src/autotype/mac"
        "src/gui/osutils/macutils"
        "src/quickunlock/TouchID.h"
        "src/quickunlock/TouchID.mm"
        "utils/keepassxc-flatpak-wrapper.sh"
        "utils/keepassxc-snap-helper.sh")

    foreach(relative_path IN LISTS forbidden_paths)
        if(EXISTS "${CMAKE_SOURCE_DIR}/${relative_path}")
            message(FATAL_ERROR
                "Windows-only platform scope violation: unsupported path remains: ${relative_path}")
        endif()
    endforeach()

    set(build_files
        "CMakeLists.txt"
        "src/CMakeLists.txt"
        "src/autotype/CMakeLists.txt"
        "share/CMakeLists.txt"
        "share/translations/CMakeLists.txt"
        "docs/CMakeLists.txt")
    set(forbidden_registrations
        "if(APPLE"
        "elseif(APPLE"
        "if(UNIX"
        "elseif(UNIX"
        "WITH_APP_BUNDLE"
        "MacOSCodesign"
        "AutoTypeMac"
        "TouchID.mm"
        "macutils/")

    foreach(relative_file IN LISTS build_files)
        file(READ "${CMAKE_SOURCE_DIR}/${relative_file}" contents)
        foreach(registration IN LISTS forbidden_registrations)
            string(FIND "${contents}" "${registration}" match_index)
            if(NOT match_index EQUAL -1)
                message(FATAL_ERROR
                    "Windows-only platform scope violation: ${relative_file} registers '${registration}'")
            endif()
        endforeach()
    endforeach()

    if(KPXC_WINDOWS_ONLY_GUARD_PROBE)
        message(FATAL_ERROR "Windows-only platform scope negative-regression probe")
    endif()
endfunction()

# Running this file directly is the focused, dependency-free platform-scope
# check used by local verification and the negative-regression probe.
if(CMAKE_SCRIPT_MODE_FILE)
    kpxc_enforce_windows_only_scope()
endif()
