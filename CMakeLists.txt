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
    src/mocks/MockLauncherData.cpp
    src/mocks/MockLauncherData.h
    src/config/LauncherConfig.h
    src/constants/LauncherConstants.h
    src/styles/Theme.h
    src/types/LauncherState.h
    src/services/IFileServerService.h
    src/services/IUpdateService.h
    src/services/IMinecraftLauncherService.h
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
target_link_libraries(EternalLauncher PRIVATE gdiplus dwmapi uxtheme comctl32)

if(MSVC)
    # O gerador do Visual Studio injeta /W3 por padrão. Removê-lo antes de /W4
    # evita o aviso de opção sobrescrita e mantém a saída da compilação limpa.
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(EternalLauncher PRIVATE /W4 /permissive- /utf-8)
endif()

# TODO: adicionar testes automatizados quando a camada de regras de negócio existir.
