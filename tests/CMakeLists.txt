#  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
#  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/../src)

add_definitions(-DQT_TEST_LIB -DQT_NO_USE_NODISCARD_FILE_OPEN)

set(KEEPASSX_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
configure_file(config-keepassx-tests.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-keepassx-tests.h)

function(add_unit_test)
    cmake_parse_arguments(TEST "" "NAME" "SOURCES;LIBS;LAUNCHER" ${ARGN})
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    target_link_libraries(${TEST_NAME} ${TEST_LIBS})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_LAUNCHER} ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8")
    if(WIN32)
        # vcpkg's applocal deployment copies the vcpkg DLLs next to the test
        # executables and nothing else, so Qt6Cored.dll and friends are only
        # found if the developer happens to have Qt on PATH. When they are not
        # found, the Windows loader raises a *system-modal* error dialog with
        # nobody in front of it, and the test does not fail - it hangs until
        # ctest's timeout. Put Qt's bin directory on PATH for the test process
        # rather than copying 40-odd megabytes of DLLs next to every target.
        #
        # ENVIRONMENT_MODIFICATION needs CMake 3.22. On anything older ctest
        # ignores the property and you are back to needing Qt on PATH yourself -
        # no worse than before, but no better either.
        set_tests_properties(${TEST_NAME} PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}")
    endif()
endfunction()

set(TEST_LIBRARIES keepassxc_gui Qt6::Test)

set(testsupport_SOURCES
        modeltest.cpp
        FailDevice.cpp
        mock/MockClock.cpp
        util/TemporaryFile.cpp)
add_library(testsupport STATIC ${testsupport_SOURCES})
target_link_libraries(testsupport Qt6::Core Qt6::Concurrent Qt6::Widgets Qt6::Test)

add_unit_test(NAME testgroup SOURCES TestGroup.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkdbx2 SOURCES TestKdbx2.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testkdbx3 SOURCES TestKeePass2Format.cpp FailDevice.cpp mock/MockChallengeResponseKey.cpp TestKdbx3.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkdbx4 SOURCES TestKeePass2Format.cpp FailDevice.cpp mock/MockChallengeResponseKey.cpp TestKdbx4.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkeys SOURCES TestKeys.cpp mock/MockChallengeResponseKey.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testgroupmodel SOURCES TestGroupModel.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testentrymodel SOURCES TestEntryModel.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testcryptohash SOURCES TestCryptoHash.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testsymmetriccipher SOURCES TestSymmetricCipher.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testhashedblockstream SOURCES TestHashedBlockStream.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testhmacblockstream SOURCES TestHmacBlockStream.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkeepass2randomstream SOURCES TestKeePass2RandomStream.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmodified SOURCES TestModified.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testdeletedobjects SOURCES TestDeletedObjects.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testkeepass1reader SOURCES TestKeePass1Reader.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testimports SOURCES TestImports.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testautotype SOURCES TestAutoType.cpp
        LIBS testsupport ${TEST_LIBRARIES})
set_target_properties(testautotype PROPERTIES ENABLE_EXPORTS ON)

add_unit_test(NAME testentry SOURCES TestEntry.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmerge SOURCES TestMerge.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testpasswordgenerator SOURCES TestPasswordGenerator.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testpasswordhealth SOURCES TestPasswordHealth.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testpassphrasegenerator SOURCES TestPassphraseGenerator.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testhibp SOURCES TestHibp.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testtotp SOURCES TestTotp.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testbase32 SOURCES TestBase32.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testcsvparser SOURCES TestCsvParser.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testrandomgenerator SOURCES TestRandomGenerator.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testentrysearcher SOURCES TestEntrySearcher.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testcsvexporter SOURCES TestCsvExporter.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testykchallengeresponsekey SOURCES TestYkChallengeResponseKey.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testsharing SOURCES TestSharing.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testdatabase SOURCES TestDatabase.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testtools SOURCES TestTools.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testconfig SOURCES TestConfig.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testmaterialvoice SOURCES TestMaterialVoice.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testmaterialhistory SOURCES TestMaterialHistory.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialhistory PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialhistory POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialhistory>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialhistory>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialchangelog SOURCES TestMaterialChangelog.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialchangelog PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialchangelog POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialchangelog>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialchangelog>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialreports SOURCES TestMaterialReports.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialreports PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialreports POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialreports>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialreports>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialappearance SOURCES TestMaterialAppearance.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialappearance PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialappearance POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialappearance>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialappearance>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialbreakpoints SOURCES TestMaterialBreakpoints.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmaterialshellresponsive SOURCES TestMaterialShellResponsive.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialshellresponsive PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    if(NOT TARGET Qt6::QOffscreenIntegrationPlugin)
        message(FATAL_ERROR "Qt's offscreen platform plugin is required to run testmaterialshellresponsive")
    endif()
    add_custom_command(TARGET testmaterialshellresponsive POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialshellresponsive>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialshellresponsive>/platforms/"
            VERBATIM)
endif()

if(WIN32)
    add_test(NAME testrepaircontracts
            COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_SOURCE_DIR}/scripts/check-repair-contracts.ps1" -SelfTest)
endif()

add_unit_test(NAME testmaterialsearchregistry SOURCES TestMaterialSearchRegistry.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialsearchregistry PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialsearchregistry POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialsearchregistry>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialsearchregistry>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialregexlab SOURCES TestMaterialRegexLab.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmaterialregexsafety SOURCES TestMaterialRegexSafety.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmaterialtitlebar SOURCES TestMaterialTitleBar.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialtitlebar PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialtitlebar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialtitlebar>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialtitlebar>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialvaultfilters SOURCES TestMaterialVaultFilters.cpp
        LIBS ${TEST_LIBRARIES} testsupport)
set_tests_properties(testmaterialvaultfilters PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialvaultfilters POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialvaultfilters>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialvaultfilters>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialselect SOURCES TestMaterialSelect.cpp
        LIBS ${TEST_LIBRARIES} testsupport)
set_tests_properties(testmaterialselect PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialselect POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialselect>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialselect>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialslider SOURCES TestMaterialSlider.cpp
        LIBS ${TEST_LIBRARIES} testsupport)
set_tests_properties(testmaterialslider PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialslider POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialslider>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialslider>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialdatefield SOURCES TestMaterialDateField.cpp
        LIBS ${TEST_LIBRARIES} testsupport)
set_tests_properties(testmaterialdatefield PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialdatefield POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialdatefield>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialdatefield>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testmaterialtabs SOURCES TestMaterialTabs.cpp
        LIBS ${TEST_LIBRARIES})
set_tests_properties(testmaterialtabs PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testmaterialtabs POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testmaterialtabs>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testmaterialtabs>/platforms/"
            VERBATIM)
endif()

# Fail-closed evidence guards. Node is a build-time dependency of the design-parity
# and feature-inventory tooling; these prove their negative probes turn red then green.
find_program(NODE_EXECUTABLE NAMES node node.exe)
if(NODE_EXECUTABLE)
    add_test(NAME testdesignparityguard COMMAND ${NODE_EXECUTABLE} "${CMAKE_SOURCE_DIR}/design/parity/test-parity-guard.mjs")
    add_test(NAME testfeatureinventoryguard COMMAND ${NODE_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/test-feature-inventory-guard.mjs")
else()
    message(WARNING "node was not found; the design-parity and feature-inventory guard tests are not registered")
endif()

add_unit_test(NAME testsquirrellifecycle SOURCES TestSquirrelLifecycle.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testwelcomeprovenance SOURCES TestWelcomeProvenance.cpp
        LIBS testsupport ${TEST_LIBRARIES})
set_tests_properties(testwelcomeprovenance PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_custom_command(TARGET testwelcomeprovenance POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testwelcomeprovenance>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testwelcomeprovenance>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testdimsum SOURCES TestDimSum.cpp
        LIBS testsupport ${TEST_LIBRARIES})
# The card is a widget, so the test needs a QApplication. Offscreen keeps it
# runnable on a machine with no session, and makes window activation - which the
# suppression rules read - a decision instead of a race with whatever happens to
# own the foreground.
set_tests_properties(testdimsum PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8;QT_QPA_PLATFORM=offscreen")
if(WIN32)
    # Qt looks for platform plugins in <exe dir>/platforms before anywhere else,
    # and the applocal deployment that populates the test output directory puts
    # exactly one there: platforms/qwindows.dll. So QT_QPA_PLATFORM=offscreen
    # found nothing, and a Qt that cannot initialise a platform plugin calls
    # qFatal(), which on Windows is a modal message box with nobody in front of
    # it to press a button. The test did not fail, it sat in that dialog until
    # ctest killed it 900 seconds later - the whole of the reported hang. Put the
    # plugin where the loader actually looks.
    if(NOT TARGET Qt6::QOffscreenIntegrationPlugin)
        message(FATAL_ERROR "Qt's offscreen platform plugin is required to run testdimsum")
    endif()
    add_custom_command(TARGET testdimsum POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:testdimsum>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:testdimsum>/platforms/"
            VERBATIM)
endif()

add_unit_test(NAME testcli SOURCES TestCli.cpp
        LIBS testsupport cli ${ZXCVBN_LIBRARIES} ${TEST_LIBRARIES})
target_compile_definitions(testcli PRIVATE KEEPASSX_CLI_PATH="$<TARGET_FILE:keepassxc-cli>")

if(KPXC_FEATURE_SSHAGENT)
    add_unit_test(NAME testopensshkey SOURCES TestOpenSSHKey.cpp
            LIBS sshagent testsupport ${TEST_LIBRARIES})
    if(NOT WIN32)
        add_unit_test(NAME testsshagent SOURCES TestSSHAgent.cpp
                LIBS sshagent testsupport ${TEST_LIBRARIES})
    endif()
endif()

if(KPXC_FEATURE_BROWSER)
    add_unit_test(NAME testbrowser SOURCES TestBrowser.cpp
        LIBS browser ${TEST_LIBRARIES})

    add_unit_test(NAME testpasskeys SOURCES TestPasskeys.cpp
        LIBS browser ${TEST_LIBRARIES})
endif()

if(KPXC_FEATURE_NETWORK OR KPXC_FEATURE_BROWSER)
    add_unit_test(NAME testurltools SOURCES TestUrlTools.cpp LIBS ${TEST_LIBRARIES})
endif()

if(KPXC_FEATURE_NETWORK)
    add_unit_test(NAME testupdatecheck SOURCES TestUpdateCheck.cpp
        LIBS ${TEST_LIBRARIES})

    add_unit_test(NAME testicondownloader SOURCES TestIconDownloader.cpp 
        LIBS ${TEST_LIBRARIES})
endif()

if(WITH_GUI_TESTS)
    add_subdirectory(gui)
endif()
