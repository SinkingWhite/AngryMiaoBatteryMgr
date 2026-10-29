// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#include "protocol.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
namespace miao
{
struct Disconnected : std::runtime_error
{
    Disconnected() : runtime_error("Device disconnected")
    {
    }
};
struct Cancelled : std::runtime_error
{
    Cancelled() : runtime_error("Cancelled")
    {
    }
};
struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h) : value(h)
    {
    }
    ~Handle()
    {
        if (value != INVALID_HANDLE_VALUE && value)
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
};
class Hid
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::atomic_bool &stopping;
    void pause(int ms)
    {
        for (int elapsed = 0; elapsed < ms; elapsed += 5)
        {
            if (stopping)
                throw Cancelled();
            Sleep(static_cast<DWORD>(std::min(5, ms - elapsed)));
        }
    }
    Report send(const Report &request, bool response = true)
    {
        if (stopping)
            throw Cancelled();
        auto data = request;
        if (!HidD_SetFeature(handle, data.data(), 65))
            throw std::runtime_error("HID SetFeature failed");
        pause(response ? 30 : 5);
        Report result{};
        if (response && !HidD_GetFeature(handle, result.data(), 65))
            throw std::runtime_error("HID GetFeature failed");
        return result;
    }
    Report wait(const std::function<bool(const Report &)> &condition, int timeout)
    {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        do
        {
            auto r = send(packet(0xF7));
            if (condition(r))
                return r;
            pause(100);
        } while (std::chrono::steady_clock::now() < end);
        throw std::runtime_error("Receiver forwarding timeout");
    }
    static bool ready(const Report &r)
    {
        return r[6] == 1 || (r[1] == 0 && r[5] == 0 && r[7] == 2);
    }
    Report forward(const Report &request, bool response = true)
    {
        send(packet(0xF6, 5), false);
        auto r = wait(ready, 3000);
        if (r[5] == 1)
            throw Disconnected();
        send(packet(0xFE, 64), false);
        send(request, false);
        if (!response)
        {
            wait(ready, 3000);
            return {};
        }
        wait([&](const auto &b) { return b[1] == 1 || b[1] == request[1]; }, 5000);
        pause(10);
        return send(packet(0xFC));
    }

  public:
    explicit Hid(std::atomic_bool &stop) : stopping(stop)
    {
        GUID guid{};
        HidD_GetHidGuid(&guid);
        HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (set == INVALID_HANDLE_VALUE)
            throw std::runtime_error("SetupAPI failed");
        try
        {
            for (DWORD i = 0;; ++i)
            {
                if (stopping)
                    throw Cancelled();
                SP_DEVICE_INTERFACE_DATA info{sizeof(info)};
                if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &info))
                {
                    if (GetLastError() == ERROR_NO_MORE_ITEMS)
                        break;
                    throw std::runtime_error("SetupAPI enumeration failed");
                }
                DWORD size = 0;
                SetupDiGetDeviceInterfaceDetailW(set, &info, nullptr, 0, &size, nullptr);
                std::vector<unsigned char> storage(size);
                auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
                detail->cbSize = sizeof(*detail);
                if (!SetupDiGetDeviceInterfaceDetailW(set, &info, detail, size, nullptr, nullptr))
                    continue;
                std::wstring path = detail->DevicePath;
                std::transform(path.begin(), path.end(), path.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
                if (path.find(L"vid_3151&pid_5007") == path.npos || path.find(L"mi_02") == path.npos)
                    continue;
                Handle candidate(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                                             nullptr));
                if (candidate.value == INVALID_HANDLE_VALUE)
                    throw std::runtime_error("Cannot open mouse HID");
                HIDD_ATTRIBUTES a{sizeof(a)};
                if (!HidD_GetAttributes(candidate.value, &a) || a.VendorID != 0x3151 || a.ProductID != 0x5007)
                    continue;
                PHIDP_PREPARSED_DATA pp = nullptr;
                if (!HidD_GetPreparsedData(candidate.value, &pp))
                    throw std::runtime_error("HID descriptor unavailable");
                HIDP_CAPS caps{};
                auto status = HidP_GetCaps(pp, &caps);
                HidD_FreePreparsedData(pp);
                if (status != HIDP_STATUS_SUCCESS || caps.UsagePage != 0xFFFF || caps.Usage != 2 ||
                    caps.FeatureReportByteLength != 65)
                    throw std::runtime_error("Unsupported HID capabilities");
                handle = candidate.value;
                candidate.value = INVALID_HANDLE_VALUE;
                break;
            }
        }
        catch (...)
        {
            SetupDiDestroyDeviceInfoList(set);
            throw;
        }
        SetupDiDestroyDeviceInfoList(set);
        if (handle == INVALID_HANDLE_VALUE)
            throw Disconnected();
    }
    ~Hid()
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
    Hid(const Hid &) = delete;
    Battery battery()
    {
        auto request = packet(0xF7);
        if (!HidD_SetFeature(handle, request.data(), 65))
            throw std::runtime_error("Battery request failed");
        pause(50);
        Report result{};
        result[0] = 5;
        if (!HidD_GetFeature(handle, result.data(), 65))
            throw std::runtime_error("Battery read failed");
        return parseBattery(result);
    }
    Dpi dpi(int selected = -1)
    {
        struct Cleanup
        {
            HANDLE h;
            ~Cleanup()
            {
                auto r = packet(0xF6);
                HidD_SetFeature(h, r.data(), 65);
            }
        } cleanup{handle};
        auto profile = forward(packet(0x85, 0, true));
        if (profile[1] != 0x85 || profile[2] > 7)
            throw std::runtime_error("Invalid profile response");
        auto before = forward(packet(0xD4, profile[2], true));
        auto state = parseDpi(before);
        if (state.profile != profile[2])
            throw std::runtime_error("Profile mismatch");
        if (selected >= 0 && selected != state.active)
        {
            auto request = switchRequest(before, selected);
            forward(request, false);
            pause(80);
            auto after = forward(packet(0xD4, profile[2], true));
            for (int retry = 0; parseDpi(after).active != selected && retry < 4; ++retry)
            {
                pause(200);
                after = forward(packet(0xD4, profile[2], true));
            }
            verifySwitch(before, after, selected);
            return parseDpi(after);
        }
        return state;
    }
};
} // namespace miao
