#include <Arduino.h>
#include <cstring>
#include <etag/core.h>
#include "etag_catalog.h"
#include "gpio_wire.h"
#ifdef ETAG_CARDPUTER_ADV
#include <M5Cardputer.h>
#include <SD.h>
#include "sd_card.h"
#include "display/App.hpp"
#include "display/KeyboardInput.hpp"
#include <Preferences.h>
#endif

using namespace etag_build;
namespace {
GpioDebugWire wire(kDD, kDC, kReset);
etag::CcDebugProbe probe(wire);
etag::LineBuffer serialLine;
size_t selected = 0;
etag::ProbeResult lastResult;
String lastProfile;
bool haveResult = false;
bool hostReady = true;
#ifdef ETAG_CARDPUTER_ADV
tagtinker::App displayApp;
bool displayMode = false;
Preferences consolePreferences;
bool sdReady = false;
String input;
bool inputOverflow = false;
String screen[7];

void redraw() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setCursor(2, 2);
    M5Cardputer.Display.print("etag / read-only diagnostics");
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    for (size_t i = 0; i < 7; ++i) {
        M5Cardputer.Display.setCursor(2, 18 + i * 13);
        M5Cardputer.Display.print(screen[i]);
    }
    M5Cardputer.Display.setCursor(2, 116);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.print("> " + input.substring(input.length() > 35 ? input.length() - 35 : 0));
}
#endif

void report(const String& line) {
    Serial.println(line);
#ifdef ETAG_CARDPUTER_ADV
    // Wrap long output to the tiny display; USB always receives the complete line.
    for (size_t offset = 0; offset < line.length() || offset == 0; offset += 38) {
        for (size_t row = 0; row < 6; ++row) screen[row] = screen[row + 1];
        screen[6] = line.substring(offset, offset + 38);
    }
    redraw();
#endif
}

void saveResult() {
#ifdef ETAG_CARDPUTER_ADV
    if (!hostReady || !sdReady || !haveResult) { report("Mount SD and probe before saving."); return; }
    if (!SD.exists("/etag") && !SD.mkdir("/etag")) { report("SD directory failed."); return; }
    char path[48];
    unsigned number = 1;
    do {
        snprintf(path, sizeof(path), "/etag/probe-%06u.json", number++);
        if (number > 1000000) { report("Probe log namespace full."); return; }
    } while (SD.exists(path));
    File file = SD.open(path, FILE_WRITE);
    if (!file) { report("SD open failed."); return; }
    const String payload = "{\"schema_version\":1,\"kind\":\"probe\",\"host\":\"" + String(kHostId) +
        "\",\"profile\":\"" + lastProfile + "\",\"chip_id\":" + String(lastResult.chipId) +
        ",\"status\":" + String(lastResult.status) + ",\"result\":\"" +
        etag::errorName(lastResult.error) + "\",\"uptime_ms\":" + String(millis()) + "}\n";
    const size_t written = file.print(payload);
    file.flush();
    file.close();
    if (written != payload.length()) { SD.remove(path); report("SD short write; log discarded."); return; }
    report(String("Saved ") + path);
#else
    report("SD logging is available on Cardputer Advance; capture this serial session on the host.");
#endif
}

void command(String line) {
    line.trim();
    if (!line.length()) return;
    if (line == "help") {
        report("about | pins | profiles | select <id>");
        report("probe confirmed | sd | save | display");
        report("Probe requires confirmed MCU, wiring, and target supply. No erase/write commands.");
    } else if (line == "display") {
#ifdef ETAG_CARDPUTER_ADV
        if (!hostReady) { report("Expected Cardputer Advance."); return; }
        serialLine = {};
        input = "";
        inputOverflow = false;
        displayMode = displayApp.begin();
        if (!displayMode) report("Display mode initialization failed.");
#else
        report("IR display editor requires Cardputer Advance.");
#endif
    } else if (line == "about") {
        report(String("etag 0.2.0 / ") + kHostName);
        report("Read-only probe scaffold. No hardware validation claimed.");
    } else if (line == "pins") {
        report("DD=" + String(kDD) + " DC=" + String(kDC) + " RESET=" + String(kReset));
        report("Outputs idle until explicit probe. Target power is external and is not measured.");
    } else if (line == "profiles") {
        for (const auto& p : kProfiles) report(String(p.id) + (p.verified ? " [verified]" : " [unverified]"));
        report(String("Selected: ") + kProfiles[selected].id);
    } else if (line.startsWith("select ")) {
        for (size_t i = 0; i < kProfileCount; ++i) {
            if (line.substring(7) == kProfiles[i].id) {
                selected = i;
#ifdef ETAG_CARDPUTER_ADV
                if (consolePreferences.putString("profile", kProfiles[i].id) == 0)
                    report("Selection active but could not be saved.");
#endif
                report(String("Selected: ") + kProfiles[i].id);
                return;
            }
        }
        report("Unknown profile. Run profiles.");
    } else if (line == "probe confirmed") {
        if (!hostReady) { report("Detected host does not match this firmware."); return; }
        report(String("Probing ") + kProfiles[selected].id);
        lastResult = probe.run(kProfiles[selected]);
        lastProfile = kProfiles[selected].id;
        haveResult = true;
        char result[96];
        snprintf(result, sizeof(result), "%s / id=%04X status=%02X",
                 etag::errorName(lastResult.error), lastResult.chipId, lastResult.status);
        report(result);
        if (lastResult.error == etag::Error::Ok) {
            report(lastResult.locked() ? "Debug locked. No erase attempted." : "Debug unlocked. Capacity still needs confirmation.");
        }
    } else if (line == "sd") {
#ifdef ETAG_CARDPUTER_ADV
        if (!hostReady) { report("Detected host does not match this firmware."); return; }
        sdReady = etag_host::mountSd();
        report(sdReady ? "SD mounted; save writes the last probe." : "SD mount failed; card was not formatted.");
#else
        report("This host has no configured SD adapter.");
#endif
    } else if (line == "save") {
        saveResult();
    } else {
        report("Unknown command. Run help.");
    }
}
}

void setup() {
    wire.idle();
    Serial.begin(115200);
#ifdef ETAG_CARDPUTER_ADV
    auto cfg = M5.config();
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg);
    hostReady = M5.getBoard() == m5::board_t::board_M5CardputerADV;
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextWrap(false);
    if (hostReady) {
        wire.idle();
        if (consolePreferences.begin("etag-console", false)) {
            const String remembered = consolePreferences.getString("profile", "");
            for (size_t i = 0; i < kProfileCount; ++i)
                if (remembered == kProfiles[i].id) selected = i;
        }
    }
#endif
    report(String("etag 0.2.0 / ") + kHostName);
    report("Type help. Probe pins are idle.");
    if (!hostReady) report("Expected Cardputer Advance. Probing disabled.");
#ifdef ETAG_CARDPUTER_ADV
    if (hostReady) command("display");
#endif
}

void loop() {
#ifdef ETAG_CARDPUTER_ADV
    if (displayMode) {
        displayApp.update();
        if (displayApp.consoleRequested()) {
            displayMode = false;
            serialLine = {};
            // Clear queued browser bytes at the ownership boundary.
            for (unsigned n = 0; n < 4096 && Serial.available(); ++n) Serial.read();
            report("Wired diagnostics. Type display to return.");
        }
        delay(2);
        return;
    }
#endif
    // Bound serial work to keep keyboard/display responsive under sustained input.
    for (unsigned n = 0; n < 64 && Serial.available(); ++n) {
        const auto event = serialLine.push(static_cast<char>(Serial.read()));
        if (event == etag::LineBuffer::Event::Ready) command(serialLine.line());
#ifdef ETAG_CARDPUTER_ADV
        if (displayMode) break;
#endif
        if (event == etag::LineBuffer::Event::Overflow) report("Command too long; discarded.");
    }
#ifdef ETAG_CARDPUTER_ADV
    if (displayMode) return;
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const auto& keys = M5Cardputer.Keyboard.keysState();
        for (char c : keys.word) {
            if (input.length() < 95 && !inputOverflow) input += c;
            else inputOverflow = true;
        }
        if (tagtinker::keyboard::consoleEraseRequested(keys) && input.length())
            input.remove(input.length() - 1);
        if (keys.enter) {
            const String completed = input;
            input = "";
            if (inputOverflow) report("Command too long; discarded.");
            else command(completed);
            inputOverflow = false;
        }
        if (!displayMode) redraw();
    }
#endif
    delay(2);
}
