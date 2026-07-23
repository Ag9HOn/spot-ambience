cmake_minimum_required(VERSION 3.16)

project(harbour-spotambience
    VERSION 1.0.0
    DESCRIPTION "Sailfish spot-price ambience automation"
    LANGUAGES CXX
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
find_package(Qt5 COMPONENTS Core DBus Gui Network Qml Quick REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SAILFISHAPP REQUIRED sailfishapp)
pkg_check_modules(MLITE REQUIRED mlite5)

set(SPOTAMBIENCE_CORE_SOURCES
    src/appconfig.cpp
    src/appconfig.h
    src/pricedata.cpp
    src/pricedata.h
    src/pricestore.cpp
    src/pricestore.h
    src/ambienceinventory.cpp
    src/ambienceinventory.h
    src/automationrunner.cpp
    src/automationrunner.h
    src/timedscheduler.cpp
    src/timedscheduler.h
    src/automationdbus.cpp
    src/automationdbus.h
)

add_executable(harbour-spotambience
    src/main.cpp
    src/spotcontroller.cpp
    src/spotcontroller.h
    src/pricechartitem.cpp
    src/pricechartitem.h
    ${SPOTAMBIENCE_CORE_SOURCES}
)

target_include_directories(harbour-spotambience PRIVATE
    ${SAILFISHAPP_INCLUDE_DIRS} ${MLITE_INCLUDE_DIRS} ${MLITE_INCLUDEDIR})
target_compile_options(harbour-spotambience PRIVATE ${SAILFISHAPP_CFLAGS_OTHER} ${MLITE_CFLAGS_OTHER})
target_link_libraries(harbour-spotambience PRIVATE
    Qt5::Core Qt5::DBus Qt5::Gui Qt5::Network Qt5::Qml Qt5::Quick
    ${SAILFISHAPP_LDFLAGS} ${MLITE_LDFLAGS}
)
add_custom_command(TARGET harbour-spotambience POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:harbour-spotambience>
    COMMENT "Stripping release binary")

if(BUILD_TESTING)
    find_package(Qt5 COMPONENTS Test REQUIRED)
    add_executable(spotambience-unit-tests
        tests/test_core.cpp
        src/appconfig.cpp
        src/pricedata.cpp
        src/pricestore.cpp
        src/ambienceinventory.cpp
        src/automationrunner.cpp
        src/timedscheduler.cpp
    )
    target_include_directories(spotambience-unit-tests PRIVATE src ${MLITE_INCLUDE_DIRS} ${MLITE_INCLUDEDIR})
    target_link_libraries(spotambience-unit-tests PRIVATE Qt5::Core Qt5::DBus Qt5::Network Qt5::Test
        ${MLITE_LDFLAGS})
    add_test(NAME spotambience-unit-tests COMMAND spotambience-unit-tests)
endif()

install(TARGETS harbour-spotambience RUNTIME DESTINATION bin)
install(DIRECTORY qml DESTINATION share/harbour-spotambience
        PATTERN ".DS_Store" EXCLUDE
        PATTERN "._*" EXCLUDE)
install(FILES harbour-spotambience.desktop DESTINATION share/applications)
foreach(SPOTAMBIENCE_ICON_SIZE 86 108 128 172)
    install(FILES icons/${SPOTAMBIENCE_ICON_SIZE}x${SPOTAMBIENCE_ICON_SIZE}/harbour-spotambience.png
            DESTINATION share/icons/hicolor/${SPOTAMBIENCE_ICON_SIZE}x${SPOTAMBIENCE_ICON_SIZE}/apps)
endforeach()
