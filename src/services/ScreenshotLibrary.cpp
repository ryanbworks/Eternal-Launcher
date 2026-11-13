#include "services/ScreenshotLibrary.h"

#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwctype>

namespace
{
    bool isImage(const std::filesystem::path& file)
    {
        auto extension = file.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return extension == L".png" || extension == L".jpg" || extension == L".jpeg";
    }

    void collect(const std::filesystem::path& directory, bool vanilla, std::vector<ScreenshotEntry>& entries)
    {
        std::error_code error;
        if (directory.empty() || !std::filesystem::is_directory(directory, error)) return;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            std::error_code ignored;
            if (!it->is_regular_file(ignored) || !isImage(it->path())) continue;
            ScreenshotEntry entry;
            entry.path = it->path();
            entry.size = it->file_size(ignored);
            const auto written = it->last_write_time(ignored);
            if (!ignored) {
                const auto system = std::chrono::clock_cast<std::chrono::system_clock>(written);
                entry.modified = std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
            }
            entry.vanilla = vanilla;
            entries.push_back(std::move(entry));
        }
    }
}

std::vector<ScreenshotEntry> ScreenshotLibrary::scan(const std::filesystem::path& modpackDirectory,
                                                     const std::filesystem::path& vanillaDirectory)
{
    std::vector<ScreenshotEntry> entries;
    collect(modpackDirectory, false, entries);
    collect(vanillaDirectory, true, entries);
    std::stable_sort(entries.begin(), entries.end(), [](const ScreenshotEntry& a, const ScreenshotEntry& b) {
        if (a.modified != b.modified) return a.modified > b.modified;
        return a.path.filename().wstring() > b.path.filename().wstring();
    });
    return entries;
}

ScreenshotImage ScreenshotLibrary::load(const std::filesystem::path& file, int maxWidth, int maxHeight, bool cover)
{
    ScreenshotImage image;
    image.path = file.wstring();
    image.large = !cover;
    if (maxWidth <= 0 || maxHeight <= 0) return image;
    // The source keeps the file locked while it exists, so it only lives inside this call.
    Gdiplus::Bitmap source(file.c_str(), FALSE);
    if (source.GetLastStatus() != Gdiplus::Ok || source.GetWidth() == 0 || source.GetHeight() == 0) return image;
    image.width = static_cast<int>(source.GetWidth());
    image.height = static_cast<int>(source.GetHeight());
    const float sw = static_cast<float>(image.width), sh = static_cast<float>(image.height);
    const float scale = cover ? std::max(static_cast<float>(maxWidth) / sw, static_cast<float>(maxHeight) / sh)
                              : std::min({static_cast<float>(maxWidth) / sw, static_cast<float>(maxHeight) / sh, 1.0F});
    const int width = cover ? maxWidth : std::max(1, static_cast<int>(sw * scale));
    const int height = cover ? maxHeight : std::max(1, static_cast<int>(sh * scale));
    auto scaled = std::make_unique<Gdiplus::Bitmap>(width, height, PixelFormat32bppPARGB);
    if (scaled->GetLastStatus() != Gdiplus::Ok) return image;
    {
        Gdiplus::Graphics g(scaled.get());
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        // Cover crops from the center; contain keeps the whole frame.
        const float cropW = static_cast<float>(width) / scale, cropH = static_cast<float>(height) / scale;
        Gdiplus::ImageAttributes clamp;
        clamp.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g.DrawImage(&source, Gdiplus::RectF(0, 0, static_cast<float>(width), static_cast<float>(height)),
            (sw - cropW) / 2, (sh - cropH) / 2, cropW, cropH, Gdiplus::UnitPixel, &clamp);
    }
    image.bitmap = std::move(scaled);
    return image;
}

std::wstring ScreenshotLibrary::formatDate(std::int64_t unixSeconds)
{
    if (unixSeconds <= 0) return L"Data desconhecida";
    const auto time = static_cast<__time64_t>(unixSeconds);
    tm local{};
    if (_localtime64_s(&local, &time) != 0) return L"Data desconhecida";
    static constexpr const wchar_t* Months[] = {L"jan", L"fev", L"mar", L"abr", L"mai", L"jun",
                                                L"jul", L"ago", L"set", L"out", L"nov", L"dez"};
    wchar_t clock[8]{};
    swprintf_s(clock, L"%02d:%02d", local.tm_hour, local.tm_min);
    return std::to_wstring(local.tm_mday) + L" " + Months[local.tm_mon] + L" " + std::to_wstring(local.tm_year + 1900) +
        L" · " + clock;
}

bool ScreenshotLibrary::remove(const std::filesystem::path& file, std::wstring& error)
{
    std::wstring from = file.wstring();
    from.push_back(L'\0'); // SHFileOperation expects a double-null-terminated list.
    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (SHFileOperationW(&operation) != 0 || operation.fAnyOperationsAborted) {
        error = L"Não foi possível mover a captura para a Lixeira.";
        return false;
    }
    return true;
}

bool ScreenshotLibrary::copyToClipboard(HWND owner, const std::filesystem::path& file, std::wstring& error)
{
    error = L"Não foi possível copiar a captura.";
    Gdiplus::Bitmap source(file.c_str(), FALSE);
    if (source.GetLastStatus() != Gdiplus::Ok) return false;
    HBITMAP bitmap = nullptr;
    if (source.GetHBITMAP(Gdiplus::Color(255, 0, 0, 0), &bitmap) != Gdiplus::Ok || !bitmap) return false;
    // CF_DIB is what chat apps read; build it from the DDB GDI+ produced.
    BITMAP info{};
    GetObjectW(bitmap, sizeof(info), &info);
    BITMAPINFOHEADER header{sizeof(BITMAPINFOHEADER), info.bmWidth, info.bmHeight, 1, 24, BI_RGB};
    const DWORD stride = ((static_cast<DWORD>(info.bmWidth) * 3 + 3) & ~3U);
    const SIZE_T pixels = static_cast<SIZE_T>(stride) * static_cast<SIZE_T>(info.bmHeight);
    bool copied = false;
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(header) + pixels)) {
        if (auto* target = static_cast<BYTE*>(GlobalLock(memory))) {
            std::memcpy(target, &header, sizeof(header));
            HDC screen = GetDC(nullptr);
            auto* dibInfo = reinterpret_cast<BITMAPINFO*>(target);
            const int lines = GetDIBits(screen, bitmap, 0, static_cast<UINT>(info.bmHeight), target + sizeof(header), dibInfo, DIB_RGB_COLORS);
            ReleaseDC(nullptr, screen);
            GlobalUnlock(memory);
            if (lines == info.bmHeight && OpenClipboard(owner)) {
                EmptyClipboard();
                copied = SetClipboardData(CF_DIB, memory) != nullptr;
                CloseClipboard();
            }
        }
        if (!copied) GlobalFree(memory);
    }
    DeleteObject(bitmap);
    if (copied) error.clear();
    return copied;
}
