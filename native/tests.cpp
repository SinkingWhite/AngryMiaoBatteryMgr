// SPDX-License-Identifier: GPL-3.0-only
#include "hid.hpp"
#include <iostream>
#include <sstream>
int main(int argc, char **argv)
{
    try
    {
        std::atomic_bool stop = false;
        if (argc > 1 && std::string(argv[1]) == "--hardware")
        {
            miao::Hid hid(stop);
            auto b = hid.battery();
            auto d = hid.dpi();
            std::cout << "Mouse=" << b.mouse << " Dock=" << b.dock << " Stage=" << d.active + 1
                      << " X=" << d.stages[d.active].x << " Y=" << d.stages[d.active].y << '\n';
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "--switch-test")
        {
            miao::Hid hid(stop);
            auto original = hid.dpi();
            if (original.stages.size() < 2)
                throw std::runtime_error("Need two enabled stages");
            try
            {
                auto changed = hid.dpi((original.active + 1) % static_cast<int>(original.stages.size()));
                std::cout << "Switched and verified: " << changed.stages[changed.active].x << '\n';
            }
            catch (...)
            {
                hid.dpi(original.active);
                throw;
            }
            auto restored = hid.dpi(original.active);
            std::cout << "Restored and verified: " << restored.stages[restored.active].x << '\n';
            return 0;
        }
        int checks = 0;
        auto check = [&](bool ok, const char *name) {
            if (!ok)
                throw std::runtime_error(name);
            ++checks;
            std::cout << "PASS " << name << '\n';
        };
        auto fails = [&](auto action, const char *name) {
            bool failed = false;
            try
            {
                action();
            }
            catch (const std::exception &)
            {
                failed = true;
            }
            check(failed, name);
        };
        auto request = miao::packet(0xD4, 0, true);
        check(request[0] == 0 && request[1] == 0xD4 && request[8] == 0x2B, "official header checksum");
        auto r = miao::packet(0);
        r[0] = 5;
        r[3] = 72;
        r[11] = 100;
        auto battery = miao::parseBattery(r);
        check(battery.mouse == 72 && battery.dock == 100, "Windows battery offsets");
        // F7 response-ready status 01 retains valid battery telemetry after DPI forwarding.
        auto responseReady = r;
        responseReady[1] = 1;
        responseReady[3] = 92;
        responseReady[11] = 96;
        auto readyBattery = miao::parseBattery(responseReady);
        check(readyBattery.mouse == 92 && readyBattery.dock == 96,
              "response-ready F7 status keeps battery fields");
        auto emptyReady = miao::packet(0);
        emptyReady[0] = 5;
        emptyReady[1] = 1;
        fails([&] { miao::parseBattery(emptyReady); }, "empty response-ready status refused");
        auto badPadding = responseReady;
        badPadding[2] = 1;
        fails([&] { miao::parseBattery(badPadding); }, "response-ready invalid padding refused");
        auto bad = r;
        bad[0] = 0;
        fails([&] { miao::parseBattery(bad); }, "wrong report ID");
        bad = r;
        bad[1] = 2;
        fails([&] { miao::parseBattery(bad); }, "junk header");
        bad = r;
        bad[11] = 255;
        fails([&] { miao::parseBattery(bad); }, "missing spare");
        bad = r;
        bad[3] = 101;
        fails([&] { miao::parseBattery(bad); }, "invalid mouse percent");
        bad = {};
        bad[0] = 5;
        fails([&] { miao::parseBattery(bad); }, "empty telemetry");
        bad = r;
        bad[3] = 0;
        check(miao::parseBattery(bad).mouse == 0, "genuine zero");
        std::string hex = "00D40001040000002B90012003B0044006800C00191027307590012003B0044006800C001910273075"
                          "FF0000FF3200FFFF0000FF0000FFFF0000FFFF00FFFFFFFF";
        miao::Report dpi{};
        for (size_t i = 0; i < 65; ++i)
            dpi[i] = static_cast<unsigned char>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
        auto d = miao::parseDpi(dpi);
        check(d.active == 1 && d.stages.size() == 4 && d.stages[1].x == 800 && d.stages[1].y == 800,
              "captured DPI table");
        auto switched = miao::switchRequest(dpi, 2);
        check(switched[1] == 0x54 && switched[3] == 2, "stage command");
        check(switched[8] == 0xA5, "stage checksum");
        bool preserved = true;
        for (size_t i = 0; i < 65; ++i)
            if (i != 1 && i != 3 && i != 8 && switched[i] != dpi[i])
                preserved = false;
        check(preserved, "preserve all unrelated bytes");
        check(dpi[1] == 0xD4 && dpi[3] == 1, "source immutable");
        fails([&] { miao::switchRequest(dpi, 4); }, "disabled stage");
        fails([&] { miao::switchRequest(dpi, -1); }, "negative stage");
        auto after = dpi;
        after[3] = 2;
        miao::verifySwitch(dpi, after, 2);
        check(true, "confirmed readback");
        fails([&] { miao::verifySwitch(dpi, dpi, 2); }, "stale readback refused");
        after[9]++;
        fails([&] { miao::verifySwitch(dpi, after, 2); }, "concurrent DPI change refused");
        bad = dpi;
        bad[4] = 0;
        fails([&] { miao::parseDpi(bad); }, "empty DPI stages");
        bad = dpi;
        bad[3] = 8;
        fails([&] { miao::parseDpi(bad); }, "invalid active index");
        stop = true;
        fails([&] { miao::Hid hid(stop); }, "cancelled enumeration");
        std::cout << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
