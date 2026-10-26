#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <string_view>

// Notification-area icon shown while Minecraft runs, so the launcher can keep
// counting play time after its window is closed.
class TrayIcon
{
public:
    ~TrayIcon();
    bool show(HWND owner, UINT callbackMessage, HICON icon, std::wstring_view tooltip);
    void setTooltip(std::wstring_view tooltip);
    void notify(std::wstring_view title, std::wstring_view message);
    void hide();
    // Explorer restarts drop every icon; call this after the TaskbarCreated broadcast.
    void restore();
    [[nodiscard]] bool visible() const { return visible_; }
    static UINT taskbarCreatedMessage();
private:
    NOTIFYICONDATAW data_{};
    bool visible_ = false;
};
