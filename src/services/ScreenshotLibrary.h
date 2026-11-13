#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// One image in a game's `screenshots` folder (F2 in Minecraft).
struct ScreenshotEntry
{
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::int64_t modified = 0;  // Unix seconds
    bool vanilla = false;       // taken in the classic 1.8.9 instead of the modpack
};

// A decoded image scaled for the screen. Produced on a worker thread.
struct ScreenshotImage
{
    std::wstring path;
    bool large = false;  // viewer image instead of a grid thumbnail
    std::unique_ptr<Gdiplus::Bitmap> bitmap;
    int width = 0;       // original size
    int height = 0;
};

namespace ScreenshotLibrary
{
    // PNG and JPEG files of every folder, newest first. Missing folders are simply empty.
    std::vector<ScreenshotEntry> scan(const std::filesystem::path& modpackDirectory,
                                      const std::filesystem::path& vanillaDirectory);
    // Decodes `file` and scales it into at most maxWidth x maxHeight. `cover` crops to fill
    // the box exactly (thumbnails); otherwise the whole picture fits inside it (viewer).
    ScreenshotImage load(const std::filesystem::path& file, int maxWidth, int maxHeight, bool cover);
    // "13 nov 2026 · 21:04"
    std::wstring formatDate(std::int64_t unixSeconds);
    // Moves the file to the Recycle Bin.
    bool remove(const std::filesystem::path& file, std::wstring& error);
    // Puts the full-size picture on the clipboard, ready to paste in Discord or WhatsApp.
    bool copyToClipboard(HWND owner, const std::filesystem::path& file, std::wstring& error);
}
