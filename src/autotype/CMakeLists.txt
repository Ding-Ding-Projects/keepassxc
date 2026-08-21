include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(autotype_SOURCES
        AutoType.cpp
        AutoTypeAction.cpp
        AutoTypeMatchModel.cpp
        AutoTypeMatchView.cpp
        AutoTypeSelectDialog.cpp
        PickcharsDialog.cpp
        WindowSelectComboBox.cpp
        test/AutoTypeTest.cpp
        windows/AutoTypeWindows.cpp)

add_library(autotype STATIC ${autotype_SOURCES})
target_link_libraries(autotype Qt6::Core Qt6::Widgets)
