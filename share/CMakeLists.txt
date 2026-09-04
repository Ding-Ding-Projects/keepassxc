#  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
#  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 or (at your option)
#  version 3 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

add_subdirectory(translations)

file(GLOB wordlists_files "wordlists/*.wordlist")
install(FILES ${wordlists_files} DESTINATION ${DATA_INSTALL_DIR}/wordlists)

# Copy wordlists to build dir for use in tests
file(COPY "wordlists" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


if(WIN32)
  install(FILES windows/qt.conf DESTINATION ${BIN_INSTALL_DIR})
endif()

if(WIN32)
  install(FILES windows/qt.conf DESTINATION ${BIN_INSTALL_DIR})
endif()

install(FILES icons/application/256x256/apps/keepassxc.png DESTINATION ${DATA_INSTALL_DIR}/icons/application/256x256/apps)

add_custom_target(icons)
add_custom_command(TARGET icons
        POST_BUILD
        COMMAND bash ./icons/minify.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# ICO for Windows
add_custom_command(TARGET icons
    POST_BUILD
    COMMAND bash ./windows/create-ico.sh icons/application/scalable/apps/keepassxc.svg windows/keepassxc.ico
    COMMAND bash ./windows/create-ico.sh icons/application/scalable/mimetypes/application-x-keepassxc.svg windows/keepassxc-kdbx.ico
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
