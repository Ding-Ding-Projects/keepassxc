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

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(core_SOURCES
        core/Alloc.cpp
        core/AutoTypeAssociations.cpp
        core/Base32.cpp
        core/Bootstrap.cpp
        core/Clock.cpp
        core/Config.cpp
        core/CustomData.cpp
        core/Database.cpp
        core/DatabaseStats.cpp
        core/Entry.cpp
        core/EntryAttachments.cpp
        core/EntryAttributes.cpp
        core/EntrySearcher.cpp
        core/FileWatcher.cpp
        core/Group.cpp
        core/HibpOffline.cpp
        core/InactivityTimer.cpp
        core/Merger.cpp
        core/Metadata.cpp
        core/ModifiableObject.cpp
        core/PasswordGenerator.cpp
        core/PasswordHealth.cpp
        core/PassphraseGenerator.cpp
        core/Resources.cpp
        core/SignalMultiplexer.cpp
        core/TimeDelta.cpp
        core/TimeInfo.cpp
        core/Tools.cpp
        core/Totp.cpp
        core/Translator.cpp
        cli/Utils.cpp
        crypto/Crypto.cpp
        crypto/CryptoHash.cpp
        crypto/Random.cpp
        crypto/SymmetricCipher.cpp
        crypto/kdf/Kdf.cpp
        crypto/kdf/AesKdf.cpp
        crypto/kdf/Argon2Kdf.cpp
        format/BitwardenReader.cpp
        format/CsvExporter.cpp
        format/CsvParser.cpp
        format/HtmlExporter.cpp
        format/KeePass1Reader.cpp
        format/KeePass2.cpp
        format/KeePass2RandomStream.cpp
        format/KdbxReader.cpp
        format/KdbxWriter.cpp
        format/KdbxXmlReader.cpp
        format/KeePass2Reader.cpp
        format/KeePass2Writer.cpp
        format/Kdbx3Reader.cpp
        format/Kdbx3Writer.cpp
        format/Kdbx4Reader.cpp
        format/Kdbx4Writer.cpp
        format/KdbxXmlWriter.cpp
        format/OpData01.cpp
        format/OPUXReader.cpp
        format/OpVaultReader.cpp
        format/OpVaultReaderAttachments.cpp
        format/OpVaultReaderBandEntry.cpp
        format/OpVaultReaderSections.cpp
        format/ProtonPassReader.cpp
        keys/CompositeKey.cpp
        keys/FileKey.cpp
        keys/PasswordKey.cpp
        keys/ChallengeResponseKey.cpp
        keys/drivers/YubiKey.h
        keys/drivers/YubiKey.cpp
        keys/drivers/YubiKeyInterface.cpp
        keys/drivers/YubiKeyInterfaceUSB.cpp
        keys/drivers/YubiKeyInterfacePCSC.cpp
        streams/HashedBlockStream.cpp
        streams/HashingStream.cpp
        streams/HmacBlockStream.cpp
        streams/LayeredStream.cpp
        streams/qtiocompressor.cpp
        streams/StoreDataStream.cpp
        streams/SymmetricCipherStream.cpp)

set(gui_SOURCES
        gui/styles/StateColorPalette.cpp
        platform/SquirrelLifecycle.cpp
        gui/material/MaterialBreakpoints.cpp
        gui/material/MaterialButtons.cpp
        gui/material/MaterialCard.cpp
        gui/material/MaterialChangelogFeed.cpp
        gui/material/MaterialChangelogScreen.cpp
        gui/material/MaterialChip.cpp
        gui/material/MaterialCommandPalette.cpp
        gui/material/MaterialDialog.cpp
        gui/material/MaterialDimSum.cpp
        gui/material/MaterialElevation.cpp
        gui/material/MaterialEntryDelegate.cpp
        gui/material/MaterialEntryDetail.cpp
        gui/material/MaterialGeneratorSheet.cpp
        gui/material/MaterialGroupDelegate.cpp
        gui/material/MaterialHistoryFeed.cpp
        gui/material/MaterialHistoryScreen.cpp
        gui/material/MaterialHistoryStore.cpp
        gui/material/MaterialIcons.cpp
        gui/material/MaterialNavigationRail.cpp
        gui/material/MaterialNotificationCentre.cpp
        gui/material/MaterialNotifier.cpp
        gui/material/MaterialOverlay.cpp
        gui/material/MaterialRegexBuilder.cpp
        gui/material/MaterialReportsFeed.cpp
        gui/material/MaterialReportsScreen.cpp
        gui/material/MaterialScreen.cpp
        gui/material/MaterialSearchBar.cpp
        gui/material/MaterialSearchRegistry.cpp
        gui/material/MaterialSegmentedButton.cpp
        gui/material/MaterialSettingsHub.cpp
        gui/material/MaterialSettingsScreen.cpp
        gui/material/MaterialSheetCatalogue.cpp
        gui/material/MaterialShell.cpp
        gui/material/MaterialSnackbar.cpp
        gui/material/MaterialSpecSheet.cpp
        gui/material/MaterialStyle.cpp
        gui/material/MaterialStyleSheet.cpp
        gui/material/MaterialSwitch.cpp
        gui/material/MaterialTabStrip.cpp
        gui/material/MaterialTheme.cpp
        gui/material/MaterialTopAppBar.cpp
        gui/material/MaterialVaultScreen.cpp
        gui/material/MaterialVaultSidebar.cpp
        gui/material/MaterialVoice.cpp
        gui/AboutDialog.cpp
        gui/ActionCollection.cpp
        gui/Application.cpp
        gui/CategoryListWidget.cpp
        gui/Clipboard.cpp
        gui/CloneDialog.cpp
        gui/DatabaseIcons.cpp
        gui/DatabaseOpenWidget.cpp
        gui/DatabaseTabWidget.cpp
        gui/DatabaseWidget.cpp
        gui/DatabaseWidgetStateSync.cpp
        gui/EntryPreviewWidget.cpp
        gui/DialogyWidget.cpp
        gui/EditWidget.cpp
        gui/EditWidgetIcons.cpp
        gui/EditWidgetProperties.cpp
        gui/FileDialog.cpp
        gui/Font.cpp
        gui/GuiTools.cpp
        gui/HtmlGuiExporter.cpp
        gui/IconModels.cpp
        gui/KMessageWidget.cpp
        gui/MainWindow.cpp
        gui/MergeDialog.cpp
        gui/MessageBox.cpp
        gui/MessageWidget.cpp
        gui/PasswordWidget.cpp
        gui/PasswordGeneratorWidget.cpp
        gui/ApplicationSettingsWidget.cpp
        gui/Icons.cpp
        gui/SearchWidget.cpp
        gui/SettingsWidget.cpp
        gui/SortFilterHideProxyModel.cpp
        gui/SquareSvgWidget.cpp
        gui/ShortcutSettingsPage.cpp
        gui/TotpSetupDialog.cpp
        gui/TotpDialog.cpp
        gui/TotpExportSettingsDialog.cpp
        gui/DatabaseOpenDialog.cpp
        gui/URLEdit.cpp
        gui/UrlTools.cpp
        gui/WelcomeWidget.cpp
        gui/csvImport/CsvImportWidget.cpp
        gui/csvImport/CsvParserModel.cpp
        gui/entry/AutoTypeAssociationsModel.cpp
        gui/entry/EditEntryWidget.cpp
        gui/entry/EntryAttachmentsModel.cpp
        gui/entry/EntryAttachmentsWidget.cpp
        gui/entry/EntryAttributesModel.cpp
        gui/entry/EditEntryAttachmentsDialog.cpp
        gui/entry/PreviewEntryAttachmentsDialog.cpp
        gui/entry/attachments/TextAttachmentsWidget.cpp
        gui/entry/attachments/ImageAttachmentsWidget.cpp
        gui/entry/attachments/ImageAttachmentsView.cpp
        gui/entry/attachments/TextAttachmentsPreviewWidget.cpp
        gui/entry/attachments/TextAttachmentsEditWidget.cpp
        gui/entry/attachments/AttachmentWidget.cpp
        gui/entry/EntryHistoryModel.cpp
        gui/entry/EntryModel.cpp
        gui/entry/EntryView.cpp
        gui/export/ExportDialog.cpp
        gui/group/EditGroupWidget.cpp
        gui/group/GroupModel.cpp
        gui/group/GroupView.cpp
        gui/tag/TagModel.cpp
        gui/tag/TagView.cpp
        gui/tag/TagsEdit.cpp
        gui/databasekey/KeyComponentWidget.cpp
        gui/databasekey/PasswordEditWidget.cpp
        gui/databasekey/YubiKeyEditWidget.cpp
        gui/databasekey/KeyFileEditWidget.cpp
        gui/dbsettings/DatabaseSettingsWidget.cpp
        gui/dbsettings/DatabaseSettingsDialog.cpp
        gui/dbsettings/DatabaseSettingsWidgetGeneral.cpp
        gui/dbsettings/DatabaseSettingsWidgetMaintenance.cpp
        gui/dbsettings/DatabaseSettingsWidgetMetaDataSimple.cpp
        gui/dbsettings/DatabaseSettingsWidgetEncryption.cpp
        gui/dbsettings/DatabaseSettingsWidgetDatabaseKey.cpp
        gui/remote/DatabaseSettingsWidgetRemote.cpp
        gui/remote/RemoteHandler.cpp
        gui/remote/RemoteProcess.cpp
        gui/remote/RemoteSettings.cpp
        gui/reports/ReportsWidget.cpp
        gui/reports/ReportsDialog.cpp
        gui/reports/ReportsWidgetHealthcheck.cpp
        gui/reports/ReportsPageHealthcheck.cpp
        gui/reports/ReportsWidgetHibp.cpp
        gui/reports/ReportsPageHibp.cpp
        gui/reports/ReportsWidgetStatistics.cpp
        gui/reports/ReportsPageStatistics.cpp
        gui/osutils/DeviceListener.cpp
        gui/osutils/OSUtilsBase.cpp
        gui/osutils/ScreenLockListener.cpp
        gui/osutils/ScreenLockListenerPrivate.cpp
        gui/widgets/ElidedLabel.cpp
        gui/widgets/KPToolBar.cpp
        gui/widgets/PopupHelpWidget.cpp
        gui/widgets/ShortcutWidget.cpp
        gui/wizard/ImportWizard.cpp
        gui/wizard/ImportWizardPageReview.cpp
        gui/wizard/ImportWizardPageSelect.cpp
        gui/wizard/NewDatabaseWizard.cpp
        gui/wizard/NewDatabaseWizardPage.cpp
        gui/wizard/NewDatabaseWizardPageMetaData.cpp
        gui/wizard/NewDatabaseWizardPageEncryption.cpp
        gui/wizard/NewDatabaseWizardPageDatabaseKey.cpp
        quickunlock/QuickUnlockInterface.cpp
        ../share/changelog/changelog.qrc
        ../share/dimsum/dimsum.qrc
        ../share/icons/icons.qrc
        ../share/icons/material/material_icons.qrc
        ../share/voice/voice.qrc
        ../share/wizard/wizard.qrc)

if(WIN32)
    list(APPEND gui_SOURCES
            gui/material/MaterialWindowChrome.cpp
            gui/osutils/winutils/DeviceListenerWin.cpp
            gui/osutils/winutils/ScreenLockListenerWin.cpp
            gui/osutils/winutils/WinUtils.cpp)
    if (WINSDK)
        list(APPEND gui_SOURCES quickunlock/WindowsHello.cpp)
    endif()
endif()

add_subdirectory(browser)
add_subdirectory(proxy)
if(KPXC_FEATURE_BROWSER)
    set(browser_LIB browser)
    list(APPEND gui_SOURCES
            gui/dbsettings/DatabaseSettingsWidgetBrowser.cpp
            gui/entry/EntryURLModel.cpp
            gui/reports/ReportsPageBrowserStatistics.cpp
            gui/reports/ReportsPagePasskeys.cpp
            gui/reports/ReportsWidgetBrowserStatistics.cpp
            gui/reports/ReportsWidgetPasskeys.cpp
            gui/passkeys/PasskeyClipboardImportDialog.cpp
            gui/passkeys/PasskeyExporter.cpp
            gui/passkeys/PasskeyExportDialog.cpp
            gui/passkeys/PasskeyImporter.cpp
            gui/passkeys/PasskeyImportDialog.cpp)
endif()

if(KPXC_FEATURE_NETWORK)
    list(APPEND gui_SOURCES
            networking/HibpDownloader.cpp
            networking/NetworkManager.cpp
            networking/UpdateChecker.cpp
            gui/UpdateCheckDialog.cpp
            gui/IconDownloader.cpp
            gui/IconDownloaderDialog.cpp)
endif()

add_subdirectory(cli)
add_subdirectory(thirdparty)

add_subdirectory(autotype)
set(autotype_LIB autotype)

# TODO: Refactor to gui sources
add_subdirectory(qrcode)
set(qrcode_LIB qrcode)

# TODO: Move to gui sources on refactor
add_subdirectory(keeshare)
set(keeshare_LIB keeshare)

add_subdirectory(sshagent)
if(KPXC_FEATURE_SSHAGENT)
    set(sshagent_LIB sshagent)
endif()

configure_file(config-keepassx.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-keepassx.h)
configure_file(git-info.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/git-info.h)

# SVG Widgets
find_package(Qt6 REQUIRED COMPONENTS SvgWidgets)

# Core Library Definition
add_library(keepassxc_core STATIC ${core_SOURCES})
target_compile_definitions(keepassxc_core PRIVATE KPXC_BUILDING_CORE)
target_link_libraries(keepassxc_core 
        ${qrcode_LIB}
        Qt6::Core
        Qt6::Concurrent
        ${BOTAN_LIBRARIES}
        ${PCSC_LIBRARIES}
        ${ZXCVBN_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${MINIZIP_LIBRARIES}
        ${thirdparty_LIBRARIES})

# GUI Library Definition
add_library(keepassxc_gui STATIC ${gui_SOURCES})
target_compile_definitions(keepassxc_gui PRIVATE KPXC_BUILDING_CORE)
target_link_libraries(keepassxc_gui
        keepassxc_core
        Qt6::Network
        Qt6::SvgWidgets
        Qt6::Widgets
        ${autotype_LIB}
        ${browser_LIB}
        ${keeshare_LIB}
        ${sshagent_LIB})

if(WIN32)
    target_link_libraries(keepassxc_gui Wtsapi32.lib Ws2_32.lib)
    if (WINSDK)
        target_link_libraries(keepassxc_gui WindowsApp.lib)
    endif()
endif()

# Main Executable Definition
add_executable(${PROGNAME} main.cpp)
target_link_libraries(${PROGNAME} keepassxc_gui)
set_target_properties(${PROGNAME} PROPERTIES ENABLE_EXPORTS ON)

if(WIN32)
    set_target_properties(${PROGNAME} PROPERTIES WIN32_EXECUTABLE ON)
    include(GenerateProductVersion)
    generate_product_version(
            WIN32_ResourceFiles
            NAME "KeePassXC"
            COMPANY_NAME "KeePassXC Team"
            VERSION_MAJOR ${KEEPASSXC_VERSION_MAJOR}
            VERSION_MINOR ${KEEPASSXC_VERSION_MINOR}
            VERSION_PATCH ${KEEPASSXC_VERSION_PATCH}
    )
    list(APPEND WIN32_ResourceFiles "${CMAKE_SOURCE_DIR}/share/windows/icon.rc")
    target_sources(${PROGNAME} PUBLIC ${WIN32_ResourceFiles})

endif()

install(TARGETS ${PROGNAME}
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION ${BIN_INSTALL_DIR} COMPONENT Runtime)

# Windows Installer Definition
if(WIN32)
    # Deploy used libraries into build directory for debugging
    add_custom_command(TARGET ${PROGNAME} POST_BUILD
        COMMAND ${WINDEPLOYQT_EXE}
            --dir $<TARGET_FILE_DIR:${PROGNAME}>
            $<IF:$<CONFIG:Debug>,--debug,--release>
            $<TARGET_FILE:${PROGNAME}>)

    # We have to copy the license file in the configuration phase.
    # CMake checks that CPACK_RESOURCE_FILE_LICENSE actually exists and
    # we have to copy it because WiX needs it to have a .txt extension.
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENSE.GPL-2"
            "${CMAKE_CURRENT_BINARY_DIR}/INSTALLER_LICENSE.txt")

    # Prepare post-install script and set to run prior to building cpack installers
    configure_file("${CMAKE_SOURCE_DIR}/cmake/WindowsCodesign.cmake.in" "${CMAKE_BINARY_DIR}/WindowsCodesign.cmake" @ONLY)
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/WindowsCodesign.cmake")
    set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/WindowsCodesign.cmake")

    string(REGEX REPLACE "-.*$" "" KEEPASSXC_VERSION_CLEAN ${KEEPASSXC_VERSION})

    set(CPACK_GENERATOR "ZIP;WIX")
    set(CPACK_COMPONENTS_ALL "")
    set(CPACK_STRIP_FILES OFF)
    set(CPACK_PACKAGE_FILE_NAME "${PROGNAME}-${KEEPASSXC_VERSION}-${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROGNAME})
    set(CPACK_PACKAGE_VERSION ${KEEPASSXC_VERSION_CLEAN})
    set(CPACK_PACKAGE_VENDOR "${PROGNAME} Team")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/INSTALLER_LICENSE.txt")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/share/windows/keepassxc.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CPACK_NSIS_MUI_ICON}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "\\\\${PROGNAME}.exe")
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/share/windows/installer-wizard.png")
    set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/share/windows/installer-wizard.png")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PROGNAME}.lnk' '$INSTDIR\\\\${PROGNAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${PROGNAME}.lnk'")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "ExecWait 'Taskkill /IM KeePassXC.exe'")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS}\nExecWait 'Taskkill /IM keepassxc-proxy.exe /F'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "ExecWait 'Taskkill /IM KeePassXC.exe'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}\nExecWait 'Taskkill /IM keepassxc-proxy.exe /F'")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://keepassxc.org")
    set(CPACK_NSIS_DISPLAY_NAME ${PROGNAME})
    set(CPACK_NSIS_PACKAGE_NAME "${PROGNAME} v${KEEPASSXC_VERSION}")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "../${PROGNAME}.exe")
    set(CPACK_WIX_UPGRADE_GUID 88785A72-3EAE-4F29-89E3-BC6B19BA9A5B)
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/share/windows/keepassxc.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/share/windows/installer-banner.png")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/share/windows/installer-wizard.png")
    set(CPACK_WIX_UI_REF "KPXC_InstallDir")
    # -----------------------------------------------------------------------
    # The Start Menu documentation shortcuts, and why the template is generated
    #
    # wix-template.xml declares two Start Menu shortcuts that target installed
    # files:
    #
    #     Target="[#CM_FP_share.docs.KeePassXC_GettingStarted.html]"
    #     Target="[#CM_FP_share.docs.KeePassXC_UserGuide.html]"
    #
    # Those ids exist only because docs/CMakeLists.txt installs the pages into
    # ${DATA_INSTALL_DIR}/docs, and that whole directory is added only when
    # KPXC_FEATURE_DOCS is on. With it off the pages are never built, never
    # installed, and CPack never emits a File row for them - but the shortcuts
    # named them anyway. light.exe does not treat that as a missing icon, it
    # refuses to link:
    #
    #   main.wxs(75) : error LGHT0204 : ICE67: The shortcut
    #       'GettingStartedShortcut' is a non-advertised shortcut with a file
    #       target, but the target file does not exist.
    #   main.wxs(75) : error LGHT0204 : ICE69:
    #       'CM_FP_share.docs.KeePassXC_GettingStarted.html' references
    #       invalid file.
    #
    # and the build ends at "CPack Error: Fatal WiX Generator Error". That is
    # the whole of the long-standing MSI failure: the package step is coupled
    # to a feature flag nothing connected it to.
    #
    # So the shortcut block is substituted in rather than hard-coded, and the
    # template is generated into the build tree. The MSI now builds either way:
    # with the shortcuts when the documentation ships, without them when it
    # does not. @ONLY matters - the template is full of WiX's own
    # $(var.CPACK_...) syntax, which CMake must not touch.
    # -----------------------------------------------------------------------
    if(KPXC_FEATURE_DOCS)
        set(KPXC_WIX_DOC_SHORTCUTS
"                        <Shortcut Id=\"GettingStartedShortcut\"
                            Name=\"KeePassXC - Getting Started\"
                            Target=\"[#CM_FP_share.docs.KeePassXC_GettingStarted.html]\"
                            WorkingDirectory=\"INSTALL_ROOT\" />
                        <Shortcut Id=\"UserGuideShortcut\"
                            Name=\"KeePassXC - User Guide\"
                            Target=\"[#CM_FP_share.docs.KeePassXC_UserGuide.html]\"
                            WorkingDirectory=\"INSTALL_ROOT\" />")
    else()
        set(KPXC_WIX_DOC_SHORTCUTS
            "                        <!-- KPXC_FEATURE_DOCS is off; the pages these pointed at are not installed. -->")
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/share/windows/wix-template.xml"
                   "${CMAKE_BINARY_DIR}/wix-template.xml" @ONLY)
    set(CPACK_WIX_TEMPLATE "${CMAKE_BINARY_DIR}/wix-template.xml")
    set(CPACK_WIX_PATCH_FILE "${CMAKE_SOURCE_DIR}/share/windows/wix-patch.xml")
    set(CPACK_WIX_EXTRA_SOURCES
            "${CMAKE_SOURCE_DIR}/share/windows/KPXC_InstallDir.wxs"
            "${CMAKE_SOURCE_DIR}/share/windows/KPXC_InstallDirDlg.wxs"
            "${CMAKE_SOURCE_DIR}/share/windows/KPXC_ExitDlg.wxs")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT  "https://keepassxc.org")
    # Do NOT add WixUIExtension here. The KPXC_*.wxs dialog sets reference WixUI
    # symbols, which makes it look necessary, but CPack's WIX generator already
    # passes WixUIExtension to candle and light whenever CPACK_WIX_UI_REF is set -
    # and it is set, to KPXC_InstallDir, eleven lines above. Naming it again loads
    # the same extension twice and every symbol in it is then defined twice:
    #
    #   WixUIExtension.dll : error LGHT0091 : Duplicate symbol 'Dialog:MaintenanceTypeDlg'
    #   WixUIExtension.dll : error LGHT0091 : Duplicate symbol 'Dialog:MaintenanceWelcomeDlg'
    #   WixUIExtension.dll : error LGHT0091 : Duplicate symbol 'WixUI:WixUI_Common'
    #
    # Those three are what this list produced when it read
    # "WixUIExtension.dll" "WixUtilExtension.dll".
    set(CPACK_WIX_EXTENSIONS "WixUtilExtension.dll")
    include(CPack)

    
    # Use windeployqt.exe to setup Qt dependencies
    install(CODE "
        if(\"\${CMAKE_INSTALL_CONFIG_NAME}\" STREQUAL \"Debug\")
            set(_wdqt_mode --debug)
        else()
            set(_wdqt_mode --release)
        endif()
        execute_process(
            COMMAND ${WINDEPLOYQT_EXE} ${PROGNAME}.exe \${_wdqt_mode}
            WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX}
            OUTPUT_QUIET
            RESULT_VARIABLE _wdqt_result
        )
        if(NOT \${_wdqt_result} EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed with exit code \${_wdqt_result}\")
        endif()"
        COMPONENT Runtime)

    if(NOT VCPKG_INSTALLED_DIR)
        install(CODE "set(gp_tool \"objdump\")" COMPONENT Runtime)

        # Deploy all 3rd party library dependencies first
        install(CODE "include(BundleUtilities)
                      fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${PROGNAME}.exe\" \"\" \"\")"
                COMPONENT Runtime)

        # install OpenSSL library
        if(KPXC_FEATURE_NETWORK)
            find_file(OPENSSL_DLL
                    NAMES libssl-3.dll libssl-3-x64.dll
                    HINTS "${OPENSSL_ROOT_DIR}/bin"
                    )
            if (NOT OPENSSL_DLL)
                message(FATAL_ERROR "Cannot find libssl dll, ensure openssl is properly installed.")
            endif()

            find_file(CRYPTO_DLL
                    NAMES libcrypto-3.dll libcrypto-3-x64.dll
                    HINTS "${OPENSSL_ROOT_DIR}/bin"
                    )
            if (NOT CRYPTO_DLL)
                message(FATAL_ERROR "Cannot find libcrypto dll, ensure openssl is properly installed.")
            endif()

            install(FILES ${OPENSSL_DLL} ${CRYPTO_DLL} DESTINATION ".")
        endif()
    endif()

    # install CA cert chains
    find_file(SSL_CA_BUNDLE ca-bundle.crt PATHS "${Qt6_PREFIX}/ssl/certs")
    if(SSL_CA_BUNDLE)
        install(FILES ${SSL_CA_BUNDLE} DESTINATION "ssl/certs")
    else()
        file(DOWNLOAD "https://curl.se/ca/cacert.pem" "${CMAKE_BINARY_DIR}/ca-bundle.crt" TLS_VERIFY ON)
        install(FILES "${CMAKE_BINARY_DIR}/ca-bundle.crt" DESTINATION "ssl/certs")
    endif()

endif()

# The install commands in this subdirectory will be executed after all the install commands in the
# current scope are run. This keeps deployment behavior deterministic.
add_subdirectory(post_install)
