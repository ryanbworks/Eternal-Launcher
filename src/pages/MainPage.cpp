#include "pages/MainPage.h"

#include "config/LauncherConfig.h"
#include "constants/LauncherConstants.h"
#include "mocks/MockLauncherData.h"
#include "services/HttpFileService.h"
#include "services/JavaRuntimeService.h"
#include "services/MinecraftLauncherService.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <array>
#include <string>

namespace
{
    const Gdiplus::RectF HeroBounds(32.0F, 104.0F, 720.0F, 260.0F);
    const Gdiplus::RectF StatusBounds(32.0F, 380.0F, 720.0F, 128.0F);
    const Gdiplus::RectF NewsBounds(32.0F, 524.0F, 720.0F, 166.0F);
    const Gdiplus::RectF ProfileBounds(772.0F, 104.0F, 376.0F, 174.0F);
    const Gdiplus::RectF PlayBounds(772.0F, 294.0F, 376.0F, 214.0F);
    const Gdiplus::RectF ProgressBounds(772.0F, 524.0F, 376.0F, 166.0F);
}

MainPage::MainPage(HINSTANCE instance)
    : instance_(instance),
      backgroundBrush_(CreateSolidBrush(RGB(11, 13, 18))),
      inputBrush_(CreateSolidBrush(RGB(25, 29, 39))),
      viewState_(MockLauncherData::createInitialState()),
      fileServerService_(std::make_unique<HttpFileService>(
          std::wstring(LauncherConfig::FileServerBaseUrl))),
      javaRuntimeService_(std::make_unique<JavaRuntimeService>()),
      minecraftLauncherService_(std::make_unique<MinecraftLauncherService>())
{
}

MainPage::~MainPage()
{
    if (inputFont_ != nullptr)
    {
        DeleteObject(inputFont_);
    }
    if (inputBrush_ != nullptr)
    {
        DeleteObject(inputBrush_);
    }
    if (backgroundBrush_ != nullptr)
    {
        DeleteObject(backgroundBrush_);
    }
}

bool MainPage::create(int showCommand)
{
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = MainPage::windowProcedure;
    windowClass.lpszClassName = LauncherConstants::WindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = backgroundBrush_;
    windowClass.style = CS_HREDRAW | CS_VREDRAW;

    if (RegisterClassEx(&windowClass) == 0)
    {
        return false;
    }

    // A primeira versão possui layout fixo para preservar ritmo e alinhamento.
    // Uma estratégia responsiva poderá ser adicionada quando surgirem novas páginas.
    constexpr DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT desiredRect{0, 0, LauncherConstants::WindowWidth, LauncherConstants::WindowHeight};
    AdjustWindowRect(&desiredRect, windowStyle, FALSE);

    window_ = CreateWindowEx(
        0,
        LauncherConstants::WindowClassName,
        LauncherConstants::WindowTitle,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desiredRect.right - desiredRect.left,
        desiredRect.bottom - desiredRect.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (window_ == nullptr)
    {
        return false;
    }

    enableDarkTitleBar();
    createNicknameInput();
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    checkFileServerAsync();
    return true;
}

LRESULT CALLBACK MainPage::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainPage* page = nullptr;

    if (message == WM_NCCREATE)
    {
        const auto createData = reinterpret_cast<CREATESTRUCT*>(lParam);
        page = static_cast<MainPage*>(createData->lpCreateParams);
        page->window_ = window;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(page));
    }
    else
    {
        page = reinterpret_cast<MainPage*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }

    return page != nullptr
        ? page->handleMessage(message, wParam, lParam)
        : DefWindowProc(window, message, wParam, lParam);
}

LRESULT MainPage::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONUP:
    {
        const float x = static_cast<float>(GET_X_LPARAM(lParam));
        const float y = static_cast<float>(GET_Y_LPARAM(lParam));
        if (DrawingUtils::contains(playPanel_.buttonBounds(PlayBounds), x, y))
        {
            handlePrimaryAction();
        }
        else if (DrawingUtils::contains(header_.settingsBounds(
                     static_cast<float>(LauncherConstants::WindowWidth)), x, y))
        {
            showSettingsPlaceholder();
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == LauncherConstants::GameProcessTimerId)
        {
            pollGameProcess();
        }
        return 0;
    case LauncherConstants::FileServerResultMessage:
    {
        // A posse do resultado passa para a thread da interface neste ponto.
        std::unique_ptr<FileServerManifestResponse> response(
            reinterpret_cast<FileServerManifestResponse*>(lParam));
        if (response != nullptr)
        {
            applyFileServerResult(*response);
        }
        return 0;
    }
    case LauncherConstants::JavaRuntimeProgressMessage:
    {
        std::unique_ptr<JavaRuntimeProgress> progress(
            reinterpret_cast<JavaRuntimeProgress*>(lParam));
        if (progress != nullptr)
        {
            applyJavaRuntimeProgress(*progress);
        }
        return 0;
    }
    case LauncherConstants::JavaRuntimeResultMessage:
    {
        std::unique_ptr<JavaRuntimeResult> result(
            reinterpret_cast<JavaRuntimeResult*>(lParam));
        if (result != nullptr)
        {
            applyJavaRuntimeResult(*result);
        }
        return 0;
    }
    case WM_CTLCOLOREDIT:
    {
        const HDC editContext = reinterpret_cast<HDC>(wParam);
        SetTextColor(editContext, RGB(245, 247, 250));
        SetBkColor(editContext, RGB(25, 29, 39));
        return reinterpret_cast<LRESULT>(inputBrush_);
    }
    case WM_DESTROY:
        KillTimer(window_, LauncherConstants::GameProcessTimerId);
        fileServerThread_.request_stop();
        javaRuntimeThread_.request_stop();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window_, message, wParam, lParam);
    }
}

void MainPage::paint()
{
    PAINTSTRUCT paintStruct{};
    HDC deviceContext = BeginPaint(window_, &paintStruct);

    // O buffer intermediário evita cintilação enquanto todos os componentes são redesenhados.
    Gdiplus::Bitmap frame(LauncherConstants::WindowWidth, LauncherConstants::WindowHeight,
                          PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&frame);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    graphics.Clear(Theme::Background);

    // Brilhos discretos dão profundidade ao fundo sem exigir imagens externas.
    Gdiplus::SolidBrush topGlow(Gdiplus::Color(18, 139, 92, 246));
    graphics.FillEllipse(&topGlow, 730.0F, -300.0F, 700.0F, 520.0F);

    header_.draw(graphics, static_cast<float>(LauncherConstants::WindowWidth));
    hero_.draw(graphics, HeroBounds);
    statusPanel_.draw(graphics, StatusBounds, viewState_.statusCards);
    newsPanel_.draw(graphics, NewsBounds, viewState_.featuredNews);
    playerProfile_.draw(graphics, ProfileBounds);
    playPanel_.draw(graphics, PlayBounds, viewState_);
    progress_.draw(graphics, ProgressBounds, viewState_.progressPercent,
                   viewState_.progressVisible
                       ? viewState_.operationMessage
                       : viewState_.operation == LauncherOperation::LaunchError
                           ? L"Java ou arquivos do jogo ainda precisam ser preparados."
                           : L"Nenhuma operação de arquivo está em andamento.",
                   viewState_.progressVisible,
                   viewState_.operation != LauncherOperation::LaunchError &&
                       viewState_.operation != LauncherOperation::ServerUnavailable);

    Gdiplus::Graphics screen(deviceContext);
    screen.DrawImage(&frame, 0, 0);
    EndPaint(window_, &paintStruct);
}

void MainPage::enableDarkTitleBar() const
{
    // Integra a moldura nativa ao tema escuro quando a versão do Windows oferece suporte.
    constexpr DWORD useImmersiveDarkMode = 20;
    BOOL enabled = TRUE;
    DwmSetWindowAttribute(window_, useImmersiveDarkMode, &enabled, sizeof(enabled));
}

void MainPage::createNicknameInput()
{
    nicknameInput_ = CreateWindowEx(
        0,
        L"EDIT",
        LauncherConfig::DefaultNickname.data(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(LauncherConstants::NicknameInputId)),
        instance_,
        nullptr);

    inputFont_ = CreateFont(
        -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(nicknameInput_, WM_SETFONT, reinterpret_cast<WPARAM>(inputFont_), TRUE);
    SendMessage(nicknameInput_, EM_SETCUEBANNER, TRUE,
                reinterpret_cast<LPARAM>(L"Digite seu nickname"));
    SendMessage(nicknameInput_, EM_SETLIMITTEXT, 16, 0);
    SetWindowTheme(nicknameInput_, L"DarkMode_Explorer", nullptr);
    layoutNicknameInput();
}

void MainPage::layoutNicknameInput() const
{
    const auto field = playerProfile_.inputBounds(ProfileBounds);
    SetWindowPos(nicknameInput_, nullptr,
                 static_cast<int>(field.X + 14.0F), static_cast<int>(field.Y + 10.0F),
                 static_cast<int>(field.Width - 28.0F), static_cast<int>(field.Height - 18.0F),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainPage::handlePrimaryAction()
{
    if (!viewState_.playEnabled)
    {
        return;
    }

    std::array<wchar_t, 32> nickname{};
    GetWindowText(nicknameInput_, nickname.data(), static_cast<int>(nickname.size()));
    if (std::wstring_view(nickname.data()).empty())
    {
        viewState_.headline = L"Informe seu nickname";
        viewState_.operationMessage = L"Precisamos saber como identificar você no jogo.";
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        InvalidateRect(window_, nullptr, FALSE);
        SetFocus(nicknameInput_);
        return;
    }

    viewState_.operation = LauncherOperation::LaunchingMinecraft;
    viewState_.headline = L"Iniciando Minecraft";
    viewState_.operationMessage = L"Validando Java, Forge e arquivos da instância...";
    viewState_.primaryActionLabel = L"INICIANDO...";
    viewState_.playEnabled = false;
    InvalidateRect(window_, nullptr, FALSE);
    UpdateWindow(window_);

    const auto launchResult = minecraftLauncherService_->launch(nickname.data());
    if (launchResult.success)
    {
        viewState_.operation = LauncherOperation::MinecraftRunning;
        viewState_.headline = L"Minecraft em execução";
        viewState_.operationMessage =
            L"Forge 47.4.10 iniciado para " + std::wstring(nickname.data()) + L".";
        viewState_.primaryActionLabel = L"JOGO EM EXECUÇÃO";
        viewState_.playEnabled = false;
        SetTimer(window_, LauncherConstants::GameProcessTimerId, 1000, nullptr);
    }
    else
    {
        if (launchResult.issue == MinecraftLaunchIssue::JavaMissing)
        {
            startJavaRuntimeInstall();
            return;
        }

        viewState_.operation = LauncherOperation::LaunchError;
        viewState_.headline = L"Não foi possível iniciar";
        viewState_.operationMessage = L"A instalação local precisa ser preparada ou corrigida.";
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        viewState_.playEnabled = true;

        MessageBox(window_, launchResult.message.c_str(), L"Eternal Launcher",
                   MB_OK | MB_ICONWARNING);
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::pollGameProcess()
{
    if (minecraftLauncherService_->isGameRunning())
    {
        return;
    }

    KillTimer(window_, LauncherConstants::GameProcessTimerId);
    viewState_.operation = LauncherOperation::ReadyToPlay;
    viewState_.headline = L"Minecraft encerrado";
    viewState_.operationMessage = L"Você pode iniciar uma nova sessão quando quiser.";
    viewState_.primaryActionLabel = L"JOGAR NOVAMENTE";
    viewState_.playEnabled = true;
    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::showSettingsPlaceholder() const
{
    MessageBox(
        window_,
        L"As configurações serão adicionadas em uma etapa futura.\n\n"
        L"Nenhuma preferência foi alterada.",
        L"Configurações — em breve",
        MB_OK | MB_ICONINFORMATION);
}

void MainPage::checkFileServerAsync()
{
    // A chamada HTTP nunca é executada na thread da janela, preservando a
    // resposta do campo de nickname, do desenho e dos demais controles.
    fileServerThread_ = std::jthread([this](std::stop_token stopToken)
    {
        auto response = fileServerService_->fetchManifest();
        if (stopToken.stop_requested())
        {
            return;
        }

        auto* transferredResponse = new FileServerManifestResponse(std::move(response));
        if (!PostMessage(
                window_, LauncherConstants::FileServerResultMessage, 0,
                reinterpret_cast<LPARAM>(transferredResponse)))
        {
            // Se a janela tiver sido encerrada durante a requisição, ninguém
            // receberá a mensagem; por isso o resultado deve ser liberado aqui.
            delete transferredResponse;
        }
    });
}

void MainPage::applyFileServerResult(const FileServerManifestResponse& response)
{
    if (viewState_.statusCards.size() < 3)
    {
        return;
    }

    const auto installation = minecraftLauncherService_->validateInstallation();
    updateInstallationState(installation);

    auto& serverCard = viewState_.statusCards[2];
    if (response.success)
    {
        serverCard.value = L"Disponível";
        serverCard.detail = L"HTTP " + std::to_wstring(response.httpStatus) +
                            L" • " + std::to_wstring(response.elapsedMilliseconds) + L" ms";
        serverCard.tone = StatusTone::Success;

        viewState_.operation = installation.success
            ? LauncherOperation::ReadyToPlay
            : LauncherOperation::LaunchError;
        viewState_.headline = installation.success
            ? L"Pronto para jogar"
            : L"Instalação necessária";
        viewState_.operationMessage = installation.success
            ? L"Manifesto e instalação local foram validados."
            : L"Clique abaixo para consultar o requisito ausente.";
        viewState_.primaryActionLabel = installation.success
            ? L"JOGAR AGORA"
            : L"VERIFICAR INSTALAÇÃO";
        viewState_.playEnabled = true;

        if (installation.issue == MinecraftLaunchIssue::JavaMissing)
        {
            startJavaRuntimeInstall();
        }
    }
    else
    {
        serverCard.value = L"Indisponível";
        serverCard.detail = response.errorMessage.empty()
            ? L"Não foi possível obter o manifesto"
            : response.errorMessage;
        serverCard.tone = StatusTone::Error;

        viewState_.operation = LauncherOperation::ServerUnavailable;
        viewState_.headline = L"Servidor indisponível";
        viewState_.operationMessage = L"O manifesto HTTP não pôde ser carregado.";
        viewState_.primaryActionLabel = L"SERVIDOR OFFLINE";
        viewState_.playEnabled = false;
    }

    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::startJavaRuntimeInstall()
{
    if (javaRuntimeThread_.joinable())
    {
        javaRuntimeThread_.join();
    }

    viewState_.operation = LauncherOperation::DownloadingFiles;
    viewState_.headline = L"Preparando Java 17";
    viewState_.operationMessage = L"O runtime portátil será instalado automaticamente.";
    viewState_.primaryActionLabel = L"BAIXANDO JAVA...";
    viewState_.progressPercent = 1;
    viewState_.progressVisible = true;
    viewState_.playEnabled = false;
    auto& installationCard = viewState_.statusCards[0];
    installationCard.value = L"Preparando...";
    installationCard.detail = L"Baixando Java 17 portátil";
    installationCard.tone = StatusTone::Information;
    InvalidateRect(window_, nullptr, FALSE);

    javaRuntimeThread_ = std::jthread([this](std::stop_token stopToken)
    {
        const auto result = javaRuntimeService_->ensureInstalled(
            stopToken,
            [this, stopToken](int percent, std::wstring_view message)
            {
                if (stopToken.stop_requested())
                {
                    return;
                }
                auto* progress = new JavaRuntimeProgress{percent, std::wstring(message)};
                if (!PostMessage(
                        window_, LauncherConstants::JavaRuntimeProgressMessage, 0,
                        reinterpret_cast<LPARAM>(progress)))
                {
                    delete progress;
                }
            });

        if (stopToken.stop_requested())
        {
            return;
        }
        auto* transferredResult = new JavaRuntimeResult(result);
        if (!PostMessage(
                window_, LauncherConstants::JavaRuntimeResultMessage, 0,
                reinterpret_cast<LPARAM>(transferredResult)))
        {
            delete transferredResult;
        }
    });
}

void MainPage::applyJavaRuntimeProgress(const JavaRuntimeProgress& progress)
{
    viewState_.progressPercent = progress.percent;
    viewState_.operationMessage = progress.message;
    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::applyJavaRuntimeResult(const JavaRuntimeResult& result)
{
    if (javaRuntimeThread_.joinable())
    {
        javaRuntimeThread_.join();
    }

    viewState_.progressVisible = false;
    viewState_.progressPercent = result.success ? 100 : 0;
    if (!result.success)
    {
        viewState_.operation = LauncherOperation::LaunchError;
        viewState_.headline = L"Falha ao preparar o Java";
        viewState_.operationMessage = result.message;
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        viewState_.playEnabled = true;
        auto& installationCard = viewState_.statusCards[0];
        installationCard.value = L"Falha no Java";
        installationCard.detail = L"Clique para tentar novamente";
        installationCard.tone = StatusTone::Error;
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    const auto installation = minecraftLauncherService_->validateInstallation();
    updateInstallationState(installation);
    viewState_.operation = installation.success
        ? LauncherOperation::ReadyToPlay
        : LauncherOperation::LaunchError;
    viewState_.headline = installation.success
        ? L"Pronto para jogar"
        : L"Java 17 preparado";
    viewState_.operationMessage = installation.success
        ? L"Java, Minecraft e Forge estão prontos."
        : L"O Java foi instalado; ainda faltam arquivos do Minecraft/Forge.";
    viewState_.primaryActionLabel = installation.success
        ? L"JOGAR AGORA"
        : L"VERIFICAR INSTALAÇÃO";
    viewState_.playEnabled = true;
    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::updateInstallationState(const MinecraftLaunchResult& installation)
{
    auto& installationCard = viewState_.statusCards[0];
    if (installation.success)
    {
        installationCard.value = L"Pronta";
        installationCard.detail = L"Minecraft 1.20.1 + Forge 47.4.10";
        installationCard.tone = StatusTone::Success;
        return;
    }

    installationCard.value = L"Incompleta";
    switch (installation.issue)
    {
    case MinecraftLaunchIssue::JavaMissing:
        installationCard.detail = L"Java 17 será baixado automaticamente";
        break;
    case MinecraftLaunchIssue::NativeLibrariesMissing:
        installationCard.detail = L"Bibliotecas nativas ausentes";
        break;
    default:
        installationCard.detail = L"Arquivos do Minecraft/Forge ausentes";
        break;
    }
    installationCard.tone = StatusTone::Error;
}
