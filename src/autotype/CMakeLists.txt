include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(autotype_SOURCES
        AutoType.cpp
        AutoTypeAction.cpp
        AutoTypeMatchModel.cpp
        AutoTypeMatchView.cpp
        AutoTypeSelectDialog.cpp
        PickcharsDialog.cpp
        WindowSelectComboBox.cpp
        test/AutoTypeTest.cpp)

# Platform specific auto-type implementations
if(APPLE)
    list(APPEND autotype_SOURCES
            mac/AutoTypeMac.cpp)
elseif(WIN32)
    list(APPEND autotype_SOURCES
            windows/AutoTypeWindows.cpp)
endif()

add_library(autotype STATIC ${autotype_SOURCES})
target_link_libraries(autotype Qt6::Core Qt6::Widgets)

if(APPLE)
    target_link_libraries(autotype "-framework Foundation" "-framework AppKit" "-framework Carbon" "-framework ScreenCaptureKit")
endif()
