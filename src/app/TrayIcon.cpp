#include "app/TrayIcon.h"

#include <cwchar>

TrayIcon::~TrayIcon()
{
    hide();
}

bool TrayIcon::show(HWND owner, UINT callbackMessage, HICON icon, std::wstring_view tooltip)
{
    if (visible_) {
        setTooltip(tooltip);
        return true;
    }
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = icon;
    wcsncpy_s(data_.szTip, std::wstring(tooltip).c_str(), _TRUNCATE);
    if (!Shell_NotifyIconW(NIM_ADD, &data_)) return false;
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    visible_ = true;
    return true;
}

void TrayIcon::setTooltip(std::wstring_view tooltip)
{
    if (!visible_) return;
    data_.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(data_.szTip, std::wstring(tooltip).c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::notify(std::wstring_view title, std::wstring_view message)
{
    if (!visible_) return;
    data_.uFlags = NIF_INFO;
    data_.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(data_.szInfoTitle, std::wstring(title).c_str(), _TRUNCATE);
    wcsncpy_s(data_.szInfo, std::wstring(message).c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::hide()
{
    if (!visible_) return;
    data_.uFlags = 0;
    Shell_NotifyIconW(NIM_DELETE, &data_);
    visible_ = false;
}

void TrayIcon::restore()
{
    if (!visible_) return;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    if (Shell_NotifyIconW(NIM_ADD, &data_)) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
}

UINT TrayIcon::taskbarCreatedMessage()
{
    static const UINT message = RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}
