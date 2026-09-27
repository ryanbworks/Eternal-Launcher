cmake_minimum_required(VERSION 3.21)

# O launcher utiliza somente C++; declarar a linguagem evita verificações
# desnecessárias de um compilador C durante a configuração do projeto.
project(EternalLauncher VERSION 0.1.0 LANGUAGES CXX)

add_executable(EternalLauncher WIN32
    src/main.cpp
    src/app/LauncherApplication.cpp
    src/app/LauncherApplication.h
    src/pages/MainPage.cpp
    src/pages/MainPage.h
    src/components/HeaderComponent.cpp
    src/components/HeaderComponent.h
    src/components/HeroComponent.cpp
    src/components/HeroComponent.h
    src/components/StatusPanelComponent.cpp
    src/components/StatusPanelComponent.h
    src/components/NewsPanelComponent.cpp
    src/components/NewsPanelComponent.h
    src/components/PlayerProfileComponent.cpp
    src/components/PlayerProfileComponent.h
    src/components/PlayPanelComponent.cpp
    src/components/PlayPanelComponent.h
    src/components/ProgressComponent.cpp
    src/components/ProgressComponent.h
    src/utils/DrawingUtils.cpp
    src/utils/DrawingUtils.h
    src/utils/ArchiveExtractor.cpp
    src/utils/ArchiveExtractor.h
    src/utils/FileHash.cpp
    src/utils/FileHash.h
    src/utils/JsonValue.cpp
    src/utils/JsonValue.h
    src/mocks/MockLauncherData.cpp
    src/mocks/MockLauncherData.h
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
    src/services/IMinecraftLauncherService.h
    src/services/IJavaRuntimeService.h
    src/services/IGameInstallationService.h
    src/services/GameInstallationService.cpp
    src/services/GameInstallationService.h
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
)

target_compile_features(EternalLauncher PRIVATE cxx_std_20)
target_compile_definitions(EternalLauncher PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_include_directories(EternalLauncher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# GDI+ desenha a interface, enquanto DWM e UxTheme integram a janela ao tema
# escuro do Windows. Nenhuma dessas bibliotecas adiciona dependência externa.
target_link_libraries(EternalLauncher PRIVATE gdiplus dwmapi uxtheme comctl32 winhttp bcrypt)

if(MSVC)
    # O gerador do Visual Studio injeta /W3 por padrão. Removê-lo antes de /W4
    # evita o aviso de opção sobrescrita e mantém a saída da compilação limpa.
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(EternalLauncher PRIVATE /W4 /permissive- /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(EternalLauncherTests
        tests/MinecraftLauncherServiceTests.cpp
        src/services/MinecraftLauncherService.cpp
        src/services/MinecraftLauncherService.h
        src/services/MinecraftLaunchCommandBuilder.cpp
        src/services/MinecraftLaunchCommandBuilder.h
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
        src/config/LauncherConfig.h
        src/config/NicknameStore.cpp
        src/config/NicknameStore.h
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
