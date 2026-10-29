// SPDX-License-Identifier: GPL-3.0-only
#include "hid.hpp"
#include <commctrl.h>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <mutex>
#include <optional>
#include <shellapi.h>
#include <shlobj.h>
#include <sstream>
#include <thread>
using namespace miao;
constexpr UINT TrayMessage = WM_APP + 1, ResultMessage = WM_APP + 2;
constexpr int IdDpi = 101, IdInterval = 102, IdTooltip = 103, IdHidden = 104, IdStartup = 105,
              IdRefresh = 106;
struct Result
{
    std::optional<Battery> battery;
    std::optional<Dpi> dpi;
    std::wstring error, dpiError;
};
HWND windowHandle{}, dpiCombo{}, intervalCombo{}, tooltipBox{}, hiddenBox{}, startupBox{}, refreshButton{};
HFONT font{}, numberFont{}, titleFont{};
HBRUSH background{};
HICON trayIcon{};
UINT taskbarCreated{};
ULONG_PTR gdiplus{};
std::atomic_bool stopping = false, popupVisible = false;
std::thread worker;
std::mutex workMutex;
std::condition_variable workChanged;
bool requested = true;
int requestedStage = -1, interval = 30;
bool showTooltip = true, startHidden = true, busy = false;
int scale = 96;
Result current;
SYSTEMTIME lastSuccess{};
ULONGLONG lastHidden = 0;
NOTIFYICONDATAW tray{};
std::wstring configPath, logPath;
HANDLE singleton{};
int px(int value)
{
    return MulDiv(value, scale, 96);
}
std::wstring widen(const char *s)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(count), 0);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), count);
    if (!out.empty())
        out.pop_back();
    return out;
}
void logError(const std::wstring &error)
{
    try
    {
        if (std::filesystem::exists(logPath) && std::filesystem::file_size(logPath) > 1048576)
        {
            std::error_code ec;
            std::filesystem::remove(logPath + L".old", ec);
            std::filesystem::rename(logPath, logPath + L".old", ec);
        }
        std::ofstream file(std::filesystem::path(logPath), std::ios::app | std::ios::binary);
        SYSTEMTIME t{};
        GetLocalTime(&t);
        int size = WideCharToMultiByte(CP_UTF8, 0, error.c_str(), static_cast<int>(error.size()), nullptr, 0,
                                       nullptr, nullptr);
        std::string utf8(static_cast<size_t>(size), 0);
        WideCharToMultiByte(CP_UTF8, 0, error.c_str(), static_cast<int>(error.size()), utf8.data(), size,
                            nullptr, nullptr);
        file << t.wYear << '-' << t.wMonth << '-' << t.wDay << ' ' << t.wHour << ':' << t.wMinute << ':'
             << t.wSecond << ' ' << utf8 << '\n';
    }
    catch (...)
    {
    }
}
std::wstring executable()
{
    std::wstring path(32768, 0);
    auto n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(n);
    return path;
}
bool startupEnabled()
{
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;
    DWORD type = 0, size = 0;
    auto result = RegQueryValueExW(key, L"AngryMiaoBattery", nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ && size > 2;
}
bool setStartup(bool enabled)
{
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr,
                        0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    auto command = L"\"" + executable() + L"\"";
    auto status = enabled ? RegSetValueExW(key, L"AngryMiaoBattery", 0, REG_SZ,
                                           reinterpret_cast<const BYTE *>(command.c_str()),
                                           static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)))
                          : RegDeleteValueW(key, L"AngryMiaoBattery");
    RegCloseKey(key);
    return status == ERROR_SUCCESS || (!enabled && status == ERROR_FILE_NOT_FOUND);
}
bool saveSettings()
{
    auto write = [&](const wchar_t *key, int value) {
        return WritePrivateProfileStringW(L"preferences", key, std::to_wstring(value).c_str(),
                                          configPath.c_str()) != FALSE;
    };
    bool ok = write(L"interval", interval) & write(L"tooltip", showTooltip) & write(L"hidden", startHidden);
    if (!ok)
    {
        current.error = L"设置未能保存";
        logError(current.error);
        InvalidateRect(windowHandle, nullptr, FALSE);
    }
    return ok;
}
void request(int stage = -1)
{
    {
        std::lock_guard guard(workMutex);
        requested = true;
        if (stage >= 0)
            requestedStage = stage;
    }
    workChanged.notify_one();
    busy = true;
    EnableWindow(dpiCombo, FALSE);
    InvalidateRect(windowHandle, nullptr, FALSE);
}
void runWorker()
{
    int failures = 0;
    std::unique_lock lock(workMutex);
    while (!stopping)
    {
        if (!requested)
        {
            int delay = failures ? std::min(300, interval * (1 << std::min(failures, 3))) : interval;
            workChanged.wait_for(lock, std::chrono::seconds(delay), [] { return stopping || requested; });
        }
        if (stopping)
            break;
        int stage = requestedStage;
        requestedStage = -1;
        requested = false;
        lock.unlock();
        auto result = std::make_unique<Result>();
        try
        {
            Hid hid(stopping);
            result->battery = hid.battery();
            // DPI is also needed by the tooltip while the popup is hidden.
            {
                try
                {
                    result->dpi = hid.dpi(stage);
                }
                catch (const Cancelled &)
                {
                    throw;
                }
                catch (const std::exception &e)
                {
                    result->dpiError = widen(e.what());
                }
            }
        }
        catch (const Cancelled &)
        {
        }
        catch (const Disconnected &)
        {
            result->error = L"设备未连接";
        }
        catch (const std::exception &e)
        {
            result->error = L"读取不可用 · " + widen(e.what());
        }
        failures = result->battery ? 0 : std::min(failures + 1, 3);
        if (!stopping)
        {
            if (PostMessageW(windowHandle, ResultMessage, 0, reinterpret_cast<LPARAM>(result.get())))
                result.release();
        }
        lock.lock();
    }
}
COLORREF tone(int percent)
{
    return percent <= 30 ? RGB(244, 67, 73) : percent <= 50 ? RGB(255, 158, 45) : RGB(52, 185, 112);
}
HICON makePaw()
{
    Gdiplus::Bitmap bitmap(32, 32, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&bitmap);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    auto rgb = current.battery ? tone(current.battery->mouse) : RGB(145, 153, 163);
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)));
    g.FillEllipse(&brush, 2.0f, 9.0f, 7.0f, 9.0f);
    g.FillEllipse(&brush, 8.0f, 3.0f, 7.0f, 10.0f);
    g.FillEllipse(&brush, 17.0f, 3.0f, 7.0f, 10.0f);
    g.FillEllipse(&brush, 23.0f, 9.0f, 7.0f, 9.0f);
    Gdiplus::GraphicsPath path;
    path.AddBezier(7, 25, 6, 22, 11, 18, 13, 16);
    path.AddBezier(13, 16, 15, 14, 17, 14, 19, 16);
    path.AddBezier(19, 16, 21, 18, 26, 22, 25, 25);
    path.AddBezier(25, 25, 25, 30, 19, 29, 16, 27);
    path.AddBezier(16, 27, 13, 29, 7, 30, 7, 25);
    path.CloseFigure();
    g.FillPath(&brush, &path);
    if (!current.battery)
    {
        Gdiplus::Font f(L"Segoe UI", 11, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush ink(Gdiplus::Color(255, 40, 45, 50));
        g.DrawString(L"?", -1, &f, Gdiplus::PointF(12, 16), &ink);
    }
    HICON icon{};
    bitmap.GetHICON(&icon);
    return icon;
}
void updateTray(bool add = false)
{
    auto icon = makePaw();
    tray.cbSize = sizeof(tray);
    tray.hWnd = windowHandle;
    tray.uID = 1;
    // Version 4 suppresses the standard tooltip unless NIF_SHOWTIP is explicit.
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    tray.uCallbackMessage = TrayMessage;
    tray.hIcon = icon;
    std::wstring tip = L"Miao";
    if (showTooltip)
    {
        tip = L"\u26A1\uFE0F";
        tip += current.battery ? std::to_wstring(current.battery->mouse) + L"%/" +
                                     std::to_wstring(current.battery->dock) + L"%"
                               : L"?%/?%";
        tip += L"\u00B7";
        if (current.dpi)
        {
            auto stage = current.dpi->stages[current.dpi->active];
            tip += std::to_wstring(stage.x);
            if (stage.x != stage.y)
                tip += L"/" + std::to_wstring(stage.y);
        }
        else
            tip += L"?";
    }
    wcsncpy_s(tray.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(add ? NIM_ADD : NIM_MODIFY, &tray);
    if (add)
    {
        tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray);
    }
    if (trayIcon)
        DestroyIcon(trayIcon);
    trayIcon = icon;
}
HWND control(const wchar_t *kind, const wchar_t *text, DWORD style, int id)
{
    return CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, windowHandle,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr),
                           nullptr);
}
void layout()
{
    if (font)
        DeleteObject(font);
    if (numberFont)
        DeleteObject(numberFont);
    if (titleFont)
        DeleteObject(titleFont);
    font = CreateFontW(-px(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                       CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
    titleFont = CreateFontW(-px(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
    numberFont = CreateFontW(-px(18), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                             CLEARTYPE_QUALITY, 0, L"Segoe UI");
    for (auto h : {dpiCombo, intervalCombo, tooltipBox, hiddenBox, startupBox, refreshButton})
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    auto place = [](HWND h, int x, int y, int w, int height) {
        MoveWindow(h, px(x), px(y), px(w), px(height), TRUE);
    };
    place(dpiCombo, 116, 188, 170, 220);
    place(intervalCombo, 186, 242, 100, 220);
    place(tooltipBox, 18, 278, 260, 22);
    place(hiddenBox, 18, 308, 260, 22);
    place(startupBox, 18, 338, 260, 22);
    place(refreshButton, 190, 379, 96, 28);
}
void hidePopup()
{
    ShowWindow(windowHandle, SW_HIDE);
    popupVisible = false;
    lastHidden = GetTickCount64();
}
void showPopup()
{
    NOTIFYICONIDENTIFIER id{sizeof(id)};
    id.hWnd = windowHandle;
    id.uID = 1;
    RECT anchor{};
    if (FAILED(Shell_NotifyIconGetRect(&id, &anchor)))
    {
        POINT p{};
        GetCursorPos(&p);
        anchor = {p.x, p.y, p.x + 1, p.y + 1};
    }
    HMONITOR monitor = MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    RECT work = info.rcWork;
    int width = px(304), height = px(426);
    int x =
        std::clamp(anchor.right - width + 8, work.left + 8, std::max(work.left + 8, work.right - width - 8));
    int y =
        std::clamp(anchor.top - height - 8, work.top + 8, std::max(work.top + 8, work.bottom - height - 8));
    SetWindowPos(windowHandle, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    popupVisible = true;
    SetForegroundWindow(windowHandle);
    request();
}
void text(HDC dc, int x, int y, int w, int height, const std::wstring &value, HFONT f, COLORREF color,
          UINT flags = DT_LEFT | DT_SINGLELINE | DT_VCENTER)
{
    RECT r{px(x), px(y), px(x + w), px(y + height)};
    auto old = SelectObject(dc, f);
    SetTextColor(dc, color);
    DrawTextW(dc, value.c_str(), -1, &r, flags);
    SelectObject(dc, old);
}
void paint()
{
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(windowHandle, &ps);
    RECT rect{};
    GetClientRect(windowHandle, &rect);
    FillRect(dc, &rect, background);
    SetBkMode(dc, TRANSPARENT);
    auto dark = RGB(48, 53, 59), muted = RGB(107, 116, 127);
    text(dc, 18, 14, 268, 25, L"Miao · 电量与偏好", titleFont, dark);
    text(dc, 18, 53, 150, 25, L"鼠标本体", font, dark);
    text(dc, 180, 53, 106, 25, current.battery ? std::to_wstring(current.battery->mouse) + L"%" : L"—",
         numberFont, current.battery ? tone(current.battery->mouse) : muted,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    text(dc, 18, 85, 150, 25, L"备用电池", font, dark);
    text(dc, 180, 85, 106, 25, current.battery ? std::to_wstring(current.battery->dock) + L"%" : L"—",
         numberFont, current.battery ? tone(current.battery->dock) : muted,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    std::wstring status = busy ? L"正在读取…" : current.battery ? L"已连接" : L"未知 / 离线";
    text(dc, 18, 119, 120, 20, status, font, muted);
    wchar_t time[40] = L"尚未更新";
    if (lastSuccess.wYear)
        swprintf_s(time, L"更新于 %02u:%02u:%02u", lastSuccess.wHour, lastSuccess.wMinute,
                   lastSuccess.wSecond);
    text(dc, 130, 119, 156, 20, time, font, muted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    text(dc, 18, 153, 115, 25, L"当前 DPI", font, dark);
    std::wstring dpi = L"—";
    if (current.dpi)
    {
        auto s = current.dpi->stages[current.dpi->active];
        dpi = std::to_wstring(s.x) + (s.x == s.y ? L"" : L" / " + std::to_wstring(s.y));
    }
    text(dc, 120, 153, 166, 25, dpi, numberFont, dark, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    text(dc, 18, 188, 90, 24, L"切换档位", font, dark);
    text(dc, 18, 242, 150, 24, L"刷新间隔", font, dark);
    text(dc, 18, 379, 160, 28, L"更改自动保存", font, muted);
    auto error = !current.error.empty()     ? current.error
                 : current.dpiError.empty() ? L""
                                            : L"DPI 未确认，点击刷新重试";
    text(dc, 18, 212, 268, 24, error, font, RGB(165, 92, 33), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EndPaint(windowHandle, &ps);
}
LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    if (message == taskbarCreated && taskbarCreated)
    {
        updateTray(true);
        return 0;
    }
    switch (message)
    {
    case WM_CREATE: {
        windowHandle = hwnd;
        scale = static_cast<int>(GetDpiForWindow(hwnd));
        background = CreateSolidBrush(RGB(241, 242, 244));
        dpiCombo = control(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, IdDpi);
        intervalCombo = control(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, IdInterval);
        tooltipBox = control(L"BUTTON", L"悬停显示双电量", BS_AUTOCHECKBOX | WS_TABSTOP, IdTooltip);
        hiddenBox = control(L"BUTTON", L"启动时隐藏浮窗", BS_AUTOCHECKBOX | WS_TABSTOP, IdHidden);
        startupBox = control(L"BUTTON", L"开机启动", BS_AUTOCHECKBOX | WS_TABSTOP, IdStartup);
        refreshButton = control(L"BUTTON", L"立即刷新", BS_PUSHBUTTON | WS_TABSTOP, IdRefresh);
        for (int seconds : {5, 15, 30, 60, 120, 300, 600, 1800, 3600})
        {
            auto label =
                seconds < 60 ? std::to_wstring(seconds) + L" 秒" : std::to_wstring(seconds / 60) + L" 分钟";
            auto index =
                SendMessageW(intervalCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            SendMessageW(intervalCombo, CB_SETITEMDATA, index, seconds);
            if (seconds == interval)
                SendMessageW(intervalCombo, CB_SETCURSEL, index, 0);
        }
        SendMessageW(tooltipBox, BM_SETCHECK, showTooltip ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(hiddenBox, BM_SETCHECK, startHidden ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(startupBox, BM_SETCHECK, startupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
        EnableWindow(dpiCombo, FALSE);
        layout();
        return 0;
    }
    case WM_PAINT:
        paint();
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor(reinterpret_cast<HDC>(w), RGB(241, 242, 244));
        SetTextColor(reinterpret_cast<HDC>(w), RGB(48, 53, 59));
        return reinterpret_cast<LRESULT>(background);
    case WM_DPICHANGED:
        scale = HIWORD(w);
        layout();
        {
            auto r = reinterpret_cast<RECT *>(l);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER);
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE && !SendMessageW(dpiCombo, CB_GETDROPPEDSTATE, 0, 0) &&
            !SendMessageW(intervalCombo, CB_GETDROPPEDSTATE, 0, 0))
            hidePopup();
        return 0;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE)
            hidePopup();
        return 0;
    case WM_CLOSE:
        hidePopup();
        return 0;
    case WM_TIMER:
        if (w == 99)
            PostMessageW(hwnd, WM_COMMAND, 900, 0);
        return 0;
    case ResultMessage: {
        std::unique_ptr<Result> r(reinterpret_cast<Result *>(l));
        busy = false;
        current = *r;
        if (current.battery)
            GetLocalTime(&lastSuccess);
        if (!current.error.empty())
            logError(current.error);
        if (!current.dpiError.empty())
            logError(current.dpiError);
        SendMessageW(dpiCombo, CB_RESETCONTENT, 0, 0);
        if (current.dpi)
        {
            for (size_t i = 0; i < current.dpi->stages.size(); ++i)
            {
                auto s = current.dpi->stages[i];
                std::wstring label = L"第 " + std::to_wstring(i + 1) + L" 档 · " + std::to_wstring(s.x) +
                                     (s.x == s.y ? L"" : L" / " + std::to_wstring(s.y));
                SendMessageW(dpiCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            }
            SendMessageW(dpiCombo, CB_SETCURSEL, current.dpi->active, 0);
        }
        EnableWindow(dpiCombo, current.dpi.has_value());
        updateTray();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case TrayMessage: {
        auto event = LOWORD(l);
        if (event == NIN_SELECT || event == NIN_KEYSELECT)
        {
            if (IsWindowVisible(hwnd))
                hidePopup();
            else if (GetTickCount64() - lastHidden > 250)
                showPopup();
        }
        else if (event == WM_CONTEXTMENU)
        {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 901, L"电量与偏好");
            AppendMenuW(menu, MF_STRING, IdRefresh, L"立即刷新");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 900, L"退出 Miao");
            POINT p{};
            GetCursorPos(&p);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            PostMessageW(hwnd, WM_NULL, 0, 0);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == 900)
        {
            stopping = true;
            workChanged.notify_all();
            if (worker.joinable())
                worker.join();
            MSG msg{};
            while (PeekMessageW(&msg, hwnd, ResultMessage, ResultMessage, PM_REMOVE))
                delete reinterpret_cast<Result *>(msg.lParam);
            Shell_NotifyIconW(NIM_DELETE, &tray);
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == 901)
        {
            showPopup();
            return 0;
        }
        if (id == IdRefresh)
        {
            request();
            return 0;
        }
        if (id == IdDpi && HIWORD(w) == CBN_SELCHANGE)
        {
            auto index = SendMessageW(dpiCombo, CB_GETCURSEL, 0, 0);
            if (index >= 0)
                request(static_cast<int>(index));
            return 0;
        }
        if (id == IdInterval && HIWORD(w) == CBN_SELCHANGE)
        {
            auto i = SendMessageW(intervalCombo, CB_GETCURSEL, 0, 0);
            if (i >= 0)
            {
                {
                    std::lock_guard guard(workMutex);
                    interval = static_cast<int>(SendMessageW(intervalCombo, CB_GETITEMDATA, i, 0));
                }
                saveSettings();
                request();
            }
            return 0;
        }
        if (id == IdTooltip)
        {
            showTooltip = SendMessageW(tooltipBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            saveSettings();
            updateTray();
            return 0;
        }
        if (id == IdHidden)
        {
            startHidden = SendMessageW(hiddenBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            saveSettings();
            return 0;
        }
        if (id == IdStartup)
        {
            bool desired = SendMessageW(startupBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (!setStartup(desired))
            {
                SendMessageW(startupBox, BM_SETCHECK, startupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
                current.error = L"开机启动设置失败";
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR command, int)
{
    singleton = CreateMutexW(nullptr, FALSE, L"Local\\AngryMiaoBatteryTray");
    if (!singleton || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (singleton)
            CloseHandle(singleton);
        return 0;
    }
    wchar_t local[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, local)))
        return 1;
    std::wstring directory = std::wstring(local) + L"\\AngryMiaoBattery";
    CreateDirectoryW(directory.c_str(), nullptr);
    if (startupEnabled())
        setStartup(true);
    configPath = directory + L"\\native.ini";
    logPath = directory + L"\\native.log";
    interval = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"preferences", L"interval", 30, configPath.c_str())), 5,
        3600);
    showTooltip = GetPrivateProfileIntW(L"preferences", L"tooltip", 1, configPath.c_str()) != 0;
    startHidden = GetPrivateProfileIntW(L"preferences", L"hidden", 1, configPath.c_str()) != 0;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&gdiplus, &input, nullptr);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&common);
    WNDCLASSW cls{};
    cls.lpfnWndProc = procedure;
    cls.hInstance = instance;
    cls.lpszClassName = L"MiaoNativePopup";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.style = CS_DROPSHADOW;
    RegisterClassW(&cls);
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    windowHandle = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, cls.lpszClassName, L"Miao · 电量与偏好",
                                   WS_POPUP | WS_BORDER, 0, 0, 304, 426, nullptr, nullptr, instance, nullptr);
    if (!windowHandle)
        return 2;
    updateTray(true);
    worker = std::thread(runWorker);
    bool smoke = wcsstr(command, L"--smoke-test") != nullptr;
    if (!startHidden || smoke || wcsstr(command, L"--show"))
        showPopup();
    if (smoke)
        SetTimer(windowHandle, 99, 4000, nullptr);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(windowHandle, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (trayIcon)
        DestroyIcon(trayIcon);
    DeleteObject(font);
    DeleteObject(numberFont);
    DeleteObject(titleFont);
    DeleteObject(background);
    Gdiplus::GdiplusShutdown(gdiplus);
    CloseHandle(singleton);
    return 0;
}
