#pragma once

#include <string_view>

// Fronteira para a futura inicialização do jogo. A página principal dependerá
// deste contrato em vez de conhecer detalhes de Java, Forge ou argumentos.
class IMinecraftLauncherService
{
public:
    virtual ~IMinecraftLauncherService() = default;

    // TODO: implementar a inicialização real do Minecraft 1.20.1 com Forge 47.4.10.
    virtual void launch(std::wstring_view nickname) = 0;
};
