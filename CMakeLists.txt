cmake_minimum_required(VERSION 3.21)

# The launcher only uses C++; declaring the language avoids unnecessary
# C compiler checks while configuring the project.
project(EternalLauncher VERSION 2.0.2 LANGUAGES CXX)
set(ETERNAL_CODENAME "Horizon")
configure_file(src/config/LauncherVersion.h.in generated/config/LauncherVersion.h @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")
enable_language(RC)
include(CTest)
option(ETERNAL_REQUIRE_UPDATE_TESTS "Require the PowerShell 7 AutoUpdate integration suite for a release" OFF)
if(ETERNAL_REQUIRE_UPDATE_TESTS AND NOT BUILD_TESTING)
    message(FATAL_ERROR "ETERNAL_REQUIRE_UPDATE_TESTS requires BUILD_TESTING=ON.")
endif()

# Bundle the C++ runtime into the distributed executable so the Visual C++
# Redistributable is not required on the computer that downloads the launcher.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Test executables use a deliberately different signing key and never contact production.
if(BUILD_TESTING)
    find_program(ETERNAL_POWERSHELL NAMES pwsh
        HINTS "$ENV{ProgramW6432}/PowerShell/7" "$ENV{ProgramFiles}/PowerShell/7")
    if(NOT ETERNAL_POWERSHELL)
        if(ETERNAL_REQUIRE_UPDATE_TESTS)
            message(FATAL_ERROR "PowerShell 7 is required for release validation. Set ETERNAL_POWERSHELL to pwsh.exe.")
        endif()
        message(STATUS "PowerShell 7 not found: skipping AutoUpdate integration tests. The launcher and C++ tests can still be built.")
    endif()
endif()
if(BUILD_TESTING AND ETERNAL_POWERSHELL)
    set(update_fixture_sources tests/LauncherUpdateFixture.cpp
        src/services/LauncherUpdateService.cpp src/services/LauncherUpdateManifest.cpp
        src/utils/JsonValue.cpp src/utils/FileHash.cpp)
    function(add_update_fixture target next)
        add_executable(${target} ${update_fixture_sources})
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_include_directories(${target} BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/update-trust")
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(${target} PRIVATE bcrypt)
        if(next)
            math(EXPR PROJECT_VERSION_PATCH "${PROJECT_VERSION_PATCH}+1")
            set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
            configure_file(src/config/LauncherVersion.h.in "${target}/config/LauncherVersion.h" @ONLY)
            target_include_directories(${target} BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${target}")
            set(ETERNAL_TEST_NEXT_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)
        endif()
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        endif()
    endfunction()
    add_update_fixture(LauncherUpdateFixture FALSE)
    add_update_fixture(LauncherUpdateNextFixture TRUE)
    add_test(NAME LauncherAutoUpdateTests COMMAND "${ETERNAL_POWERSHELL}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestLauncherUpdate.ps1"
        -OldExecutable "$<TARGET_FILE:LauncherUpdateFixture>"
        -NewExecutable "$<TARGET_FILE:LauncherUpdateNextFixture>"
        -NewVersion "${ETERNAL_TEST_NEXT_VERSION}" -WorkingDirectory "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(LauncherAutoUpdateTests PROPERTIES TIMEOUT 150)
    add_executable(LauncherUpdateProbe tools/LauncherUpdateProbe.cpp
        src/services/LauncherUpdateService.cpp src/services/LauncherUpdateTransport.cpp
        src/services/LauncherUpdateManifest.cpp src/utils/JsonValue.cpp src/utils/FileHash.cpp)
    target_compile_features(LauncherUpdateProbe PRIVATE cxx_std_20)
    target_compile_definitions(LauncherUpdateProbe PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(LauncherUpdateProbe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(LauncherUpdateProbe PRIVATE bcrypt winhttp)
    if(MSVC)
        target_compile_options(LauncherUpdateProbe PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()

add_executable(EternalLauncher WIN32
    src/resources/Launcher.rc
    src/main.cpp
    src/app/LauncherApplication.cpp
    src/app/LauncherApplication.h
    src/pages/MainPage.cpp
    src/pages/MainPage.h
    src/pages/SettingsPage.cpp
    src/pages/SettingsPage.h
    src/app/TrayIcon.cpp
    src/app/TrayIcon.h
    src/components/Buttons.cpp
    src/components/Buttons.h
    src/components/ChromeView.cpp
    src/components/ChromeView.h
    src/components/ContentViews.cpp
    src/components/ContentViews.h
    src/components/HeroComponent.cpp
    src/components/HeroComponent.h
    src/components/HomeView.cpp
    src/components/HomeView.h
    src/components/InstallationPanelComponent.cpp
    src/components/InstallationPanelComponent.h
    src/components/Layout.h
    src/services/InstallationCoordinator.cpp
    src/services/ModInventory.cpp
    src/services/ModInventory.h
    src/services/NewsFeed.cpp
    src/services/NewsFeed.h
    src/services/PlaytimeStore.cpp
    src/services/PlaytimeStore.h
    src/utils/DrawingUtils.cpp
    src/utils/DrawingUtils.h
    src/utils/Icons.cpp
    src/utils/Icons.h
    src/utils/BrandEmblem.cpp
    src/utils/BrandEmblem.h
    src/utils/ArchiveExtractor.cpp
    src/utils/ArchiveExtractor.h
    src/utils/FileHash.cpp
    src/utils/FileHash.h
    src/utils/JsonValue.cpp
    src/utils/JsonValue.h
    src/utils/UrlEncoding.h
    src/mocks/MockLauncherData.cpp
    src/mocks/MockLauncherData.h
    src/config/LauncherSettings.cpp
    src/config/LauncherSettings.h
    src/config/LauncherConfig.h
    src/config/NicknameStore.cpp
    src/config/NicknameStore.h
    src/constants/LauncherConstants.h
    src/styles/Theme.h
    src/types/LauncherState.h
    src/services/IFileServerService.h
    src/services/HttpFileService.cpp
    src/services/HttpFileService.h
    src/services/HttpDownloadClient.cpp
    src/services/HttpDownloadClient.h
    src/services/IUpdateService.h
    src/services/LauncherUpdateManifest.cpp
    src/services/LauncherUpdateTransport.cpp
    src/services/LauncherUpdateService.cpp
    src/services/IMinecraftLauncherService.h
    src/services/IJavaRuntimeService.h
    src/services/IGameInstallationService.h
    src/services/GameInstallationService.cpp
    src/services/GameInstallationService.h
    src/services/ModCleanup.cpp
    src/services/ModCleanup.h
    src/services/MinecraftManifestService.cpp
    src/services/MinecraftManifestService.h
    src/services/VerifiedDownloader.cpp
    src/services/VerifiedDownloader.h
    src/services/JavaRuntimeService.cpp
    src/services/JavaRuntimeService.h
    src/services/JavaRuntimePackage.cpp
    src/services/JavaRuntimePackage.h
    src/services/PortableJavaInstaller.cpp
    src/services/PortableJavaInstaller.h
    src/services/MinecraftLauncherService.cpp
    src/services/MinecraftLauncherService.h
    src/services/MinecraftLaunchCommandBuilder.cpp
    src/services/MinecraftLaunchCommandBuilder.h
    src/services/MicrosoftAuthService.cpp
    src/services/MicrosoftAuthService.h
    src/types/MinecraftIdentity.h
)

target_compile_features(EternalLauncher PRIVATE cxx_std_20)
target_compile_definitions(EternalLauncher PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_include_directories(EternalLauncher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# GDI+ draws the interface, while DWM and UxTheme integrate the window with the
# Windows dark theme. None of these libraries adds an external dependency.
target_link_libraries(EternalLauncher PRIVATE gdiplus dwmapi uxtheme comctl32 winhttp bcrypt ole32 shell32 comdlg32)

if(MSVC)
    # The Visual Studio generator injects /W3 by default. Removing it before /W4
    # avoids the overridden-option warning and keeps the build output clean.
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(EternalLauncher PRIVATE /W4 /permissive- /utf-8)
endif()

if(BUILD_TESTING)
    add_executable(EternalLauncherTests
        tests/MinecraftLauncherServiceTests.cpp
        src/services/MinecraftLauncherService.cpp
        src/services/MinecraftLauncherService.h
        src/services/MinecraftLaunchCommandBuilder.cpp
        src/services/MinecraftLaunchCommandBuilder.h
        src/types/MinecraftIdentity.h
        src/services/IMinecraftLauncherService.h
        src/services/HttpDownloadClient.cpp
        src/services/HttpDownloadClient.h
        src/services/IJavaRuntimeService.h
        src/services/JavaRuntimeService.cpp
        src/services/JavaRuntimeService.h
        src/services/JavaRuntimePackage.cpp
        src/services/JavaRuntimePackage.h
        src/services/PortableJavaInstaller.cpp
        src/services/PortableJavaInstaller.h
        src/utils/ArchiveExtractor.cpp
        src/utils/ArchiveExtractor.h
        src/utils/FileHash.cpp
        src/utils/FileHash.h
        src/utils/JsonValue.cpp
        src/utils/JsonValue.h
        src/utils/UrlEncoding.h
        src/config/LauncherSettings.cpp
        src/config/LauncherSettings.h
        src/config/LauncherConfig.h
        src/config/NicknameStore.cpp
        src/config/NicknameStore.h
        src/services/ModCleanup.cpp
        src/services/ModCleanup.h
    )
    target_compile_features(EternalLauncherTests PRIVATE cxx_std_20)
    target_compile_definitions(EternalLauncherTests PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(EternalLauncherTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(EternalLauncherTests PRIVATE bcrypt winhttp)
    if(MSVC)
        target_compile_options(EternalLauncherTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME MinecraftLauncherServiceTests COMMAND EternalLauncherTests)
endif()
if(BUILD_TESTING)
    add_executable(LauncherSettingsTests
        tests/LauncherSettingsTests.cpp
        src/config/LauncherSettings.cpp
        src/utils/JsonValue.cpp
        src/services/MinecraftLaunchCommandBuilder.cpp
        src/services/MinecraftLauncherService.cpp)
    target_compile_features(LauncherSettingsTests PRIVATE cxx_std_20)
    target_compile_definitions(LauncherSettingsTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(LauncherSettingsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(LauncherSettingsTests PRIVATE bcrypt)
    if(MSVC)
        target_compile_options(LauncherSettingsTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME LauncherSettingsTests COMMAND LauncherSettingsTests)
endif()
if(BUILD_TESTING)
    add_executable(SettingsPageTests
        tests/SettingsPageTests.cpp
        src/pages/SettingsPage.cpp
        src/config/LauncherSettings.cpp
        src/utils/JsonValue.cpp
        src/utils/DrawingUtils.cpp
        src/utils/Icons.cpp
        src/utils/BrandEmblem.cpp
        src/components/Buttons.cpp)
    target_compile_features(SettingsPageTests PRIVATE cxx_std_20)
    target_compile_definitions(SettingsPageTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(SettingsPageTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(SettingsPageTests PRIVATE gdiplus uxtheme comctl32 comdlg32 ole32 shell32)
    if(MSVC)
        target_compile_options(SettingsPageTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME SettingsPageTests COMMAND SettingsPageTests)
endif()
if(BUILD_TESTING)
    get_target_property(painting_sources EternalLauncher SOURCES)
    list(REMOVE_ITEM painting_sources src/main.cpp src/app/LauncherApplication.cpp src/app/LauncherApplication.h)
    add_executable(WindowPaintingTests tests/WindowPaintingTests.cpp ${painting_sources})
    target_compile_features(WindowPaintingTests PRIVATE cxx_std_20)
    target_compile_definitions(WindowPaintingTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(WindowPaintingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    get_target_property(painting_libraries EternalLauncher LINK_LIBRARIES)
    target_link_libraries(WindowPaintingTests PRIVATE ${painting_libraries})
    if(MSVC)
        target_compile_options(WindowPaintingTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME WindowPaintingTests COMMAND WindowPaintingTests)
endif()

if(BUILD_TESTING)
    add_executable(InstallationTrackingTests tests/InstallationTrackingTests.cpp
        src/services/InstallationCoordinator.cpp src/services/HttpDownloadClient.cpp
        src/services/VerifiedDownloader.cpp src/services/GameInstallationService.cpp
        src/services/MinecraftManifestService.cpp src/services/JavaRuntimeService.cpp
        src/services/JavaRuntimePackage.cpp src/services/PortableJavaInstaller.cpp
        src/services/ModCleanup.cpp src/utils/ArchiveExtractor.cpp src/utils/FileHash.cpp
        src/utils/JsonValue.cpp src/config/LauncherSettings.cpp)
    target_compile_features(InstallationTrackingTests PRIVATE cxx_std_20)
    target_compile_definitions(InstallationTrackingTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(InstallationTrackingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(InstallationTrackingTests PRIVATE winhttp bcrypt ws2_32)
    if(MSVC)
        target_compile_options(InstallationTrackingTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME InstallationTrackingTests COMMAND InstallationTrackingTests)
    set_tests_properties(InstallationTrackingTests PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
    add_executable(LauncherServicesTests tests/LauncherServicesTests.cpp
        src/services/PlaytimeStore.cpp src/services/NewsFeed.cpp src/services/ModInventory.cpp
        src/services/HttpDownloadClient.cpp src/utils/JsonValue.cpp)
    target_compile_features(LauncherServicesTests PRIVATE cxx_std_20)
    target_compile_definitions(LauncherServicesTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(LauncherServicesTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(LauncherServicesTests PRIVATE winhttp shell32)
    if(MSVC)
        target_compile_options(LauncherServicesTests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME LauncherServicesTests COMMAND LauncherServicesTests)
endif()
