#include "NetworkManager.h"
#include "SerialHelper.h"
#include <WiFiManager.h>

#define MAX_GIF_SIZE 32768 // 32KB limit

// Defaults below can be overridden in config.h
#ifndef SETUP_AP_NAME
#define SETUP_AP_NAME "WordClock-Setup"
#endif

// Hold this button for 3 seconds while the clock is running to reopen the setup portal.
// GPIO0 is the BOOT button on most ESP32 dev boards.
#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN 0
#endif

#ifndef DEFAULT_TIMEZONE
#define DEFAULT_TIMEZONE "EST5EDT,M3.2.0,M11.1.0"
#endif

#define SETUP_BUTTON_HOLD_MS 3000
#define SETUP_PORTAL_TIMEOUT_SEC 300
#define WIFI_CONNECT_TIMEOUT_SEC 30
#define TIMEZONE_MAX_LEN 64

struct TimezoneOption
{
    const char *name;
    const char *posix;
};

// POSIX TZ strings handle daylight saving time automatically
static const TimezoneOption TIMEZONES[] = {
    {"Newfoundland (St. John's)", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"Atlantic (Halifax)", "AST4ADT,M3.2.0,M11.1.0"},
    {"Eastern (Toronto, New York)", "EST5EDT,M3.2.0,M11.1.0"},
    {"Central (Winnipeg, Chicago)", "CST6CDT,M3.2.0,M11.1.0"},
    {"Saskatchewan (Regina)", "CST6"},
    {"Mountain (Edmonton, Denver)", "MST7MDT,M3.2.0,M11.1.0"},
    {"Arizona (Phoenix)", "MST7"},
    {"Pacific (Vancouver, Los Angeles)", "PST8PDT,M3.2.0,M11.1.0"},
    {"Alaska (Anchorage)", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Hawaii (Honolulu)", "HST10"},
    {"UTC", "UTC0"},
    {"UK & Ireland (London, Dublin)", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Central Europe (Paris, Berlin, Warsaw)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Eastern Europe (Athens, Kyiv)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"India (Kolkata)", "IST-5:30"},
    {"China (Shanghai)", "CST-8"},
    {"Japan (Tokyo)", "JST-9"},
    {"Australia East (Sydney, Melbourne)", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"New Zealand (Auckland)", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};

NetworkManager::NetworkManager()
    : lastSyncTime(0), setupButtonPressedSince(0), gifBuffer(nullptr), gifBufferSize(0) {}

void NetworkManager::setup()
{
    pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
    loadTimezone();

    // Start WiFi first so the saved-credentials check below can read them
    WiFi.mode(WIFI_STA);

    // Connects with saved credentials, or opens the setup portal if there are none / they fail
    runSetupPortal(false);
    SERIAL_PRINTLN("WiFi connected");

    syncTimeWithNTP();
}

void NetworkManager::update()
{
    checkSetupButton();

    unsigned long currentMillis = millis();
    if (currentMillis - lastSyncTime >= syncInterval)
    {
        syncTimeWithNTP();
    }
}

void NetworkManager::checkSetupButton()
{
    if (digitalRead(SETUP_BUTTON_PIN) != LOW)
    {
        setupButtonPressedSince = 0;
        return;
    }

    if (setupButtonPressedSince == 0)
    {
        setupButtonPressedSince = millis();
    }
    else if (millis() - setupButtonPressedSince >= SETUP_BUTTON_HOLD_MS)
    {
        setupButtonPressedSince = 0;
        SERIAL_PRINTLN("Setup button held, opening setup portal");
        runSetupPortal(true);
        syncTimeWithNTP();
    }
}

void NetworkManager::runSetupPortal(bool onDemand)
{
    WiFiManager wifiManager;
    wifiManager.setDebugOutput(USE_SERIAL);
    wifiManager.setTitle("Word Clock");
    wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);

    // No firmware update page: the setup network is open, so anyone nearby could reach it
    std::vector<const char *> menu = {"wifi", "info", "restart", "exit"};
    wifiManager.setMenu(menu);

    // With saved credentials, give up on the portal after a while and retry them
    // (e.g. the router was slower than the clock to come back after a power cut)
    if (onDemand || wifiManager.getWiFiIsSaved())
    {
        wifiManager.setConfigPortalTimeout(SETUP_PORTAL_TIMEOUT_SEC);
    }

    // When reopened from the button, close the portal on Save even if the WiFi fields
    // were left blank (to change only the timezone)
    wifiManager.setBreakAfterConfig(onDemand);

    String selectHtml = buildTimezoneSelectHtml();
    WiFiManagerParameter timezoneSelect(selectHtml.c_str());
    WiFiManagerParameter timezoneField("tz", "Timezone (POSIX TZ string)", timezoneSetting.c_str(), TIMEZONE_MAX_LEN);
    wifiManager.addParameter(&timezoneSelect);
    wifiManager.addParameter(&timezoneField);

    bool connected = onDemand
                         ? wifiManager.startConfigPortal(SETUP_AP_NAME)
                         : wifiManager.autoConnect(SETUP_AP_NAME);

    String newTimezone = timezoneField.getValue();
    newTimezone.trim();
    if (newTimezone.length() > 0 && newTimezone != timezoneSetting)
    {
        saveTimezone(newTimezone);
    }

    if (!connected && WiFi.status() != WL_CONNECTED)
    {
        SERIAL_PRINTLN("WiFi not connected, restarting");
        delay(1000);
        ESP.restart();
    }
}

String NetworkManager::buildTimezoneSelectHtml()
{
    String html = "<label for='tzsel'>Timezone</label>"
                  "<select id='tzsel' onchange=\"if(this.value)document.getElementById('tz').value=this.value\">";
    bool matched = false;
    for (const TimezoneOption &option : TIMEZONES)
    {
        bool selected = timezoneSetting.equals(option.posix);
        matched = matched || selected;
        html += "<option value='" + String(option.posix) + "'" + (selected ? " selected" : "") + ">" + option.name + "</option>";
    }
    html += String("<option value=''") + (matched ? "" : " selected") + ">Other (enter below)</option>";
    html += "</select><br/><br/>";
    return html;
}

void NetworkManager::loadTimezone()
{
    preferences.begin("wordclock", true);
    timezoneSetting = preferences.getString("tz", DEFAULT_TIMEZONE);
    preferences.end();
}

void NetworkManager::saveTimezone(const String &tz)
{
    timezoneSetting = tz;
    preferences.begin("wordclock", false);
    preferences.putString("tz", timezoneSetting);
    preferences.end();
    SERIAL_PRINTLN("Timezone saved: " + timezoneSetting);
}

void NetworkManager::syncTimeWithNTP()
{
    configTzTime(timezoneSetting.c_str(), "pool.ntp.org");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        SERIAL_PRINTLN("Failed to obtain time");
    }
    lastSyncTime = millis();
}

struct tm NetworkManager::getLocalTimeStruct()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        return timeinfo;
    }
    else
    {
        memset(&timeinfo, 0, sizeof(struct tm));
        SERIAL_PRINTLN("Failed to obtain local time");
        return timeinfo;
    }
}

bool NetworkManager::downloadGIF(const char *gifUrl)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(gifUrl);

        int httpResponseCode = http.GET();
        if (httpResponseCode == HTTP_CODE_OK)
        {
            int gifSize = http.getSize();

            if (gifSize > MAX_GIF_SIZE)
            {
                SERIAL_PRINTLN("GIF is too large. Max size allowed is 32KB.");
                http.end();
                return false;
            }

            gifBuffer = handleDownloadGIFResponse(http, gifSize);
            gifBufferSize = gifSize;
            http.end();
            return gifBuffer != nullptr;
        }
        else
        {
            SERIAL_PRINTLN("Failed to download GIF");
            http.end();
            return false;
        }
    }
    else
    {
        SERIAL_PRINTLN("WiFi not connected");
        return false;
    }
}

uint8_t *NetworkManager::handleDownloadGIFResponse(HTTPClient &http, int gifSize)
{
    WiFiClient *stream = http.getStreamPtr();

    if (gifSize > 0)
    {
        SERIAL_PRINTLN("Downloading GIF...");

        uint8_t *buffer = (uint8_t *)malloc(gifSize);
        if (buffer == nullptr)
        {
            SERIAL_PRINTLN("Memory allocation failed for GIF");
            return nullptr;
        }

        int bytesRead = 0;
        while (http.connected() && stream->available() > 0 && bytesRead < gifSize)
        {
            int byte = stream->read();
            buffer[bytesRead++] = byte;
        }

        SERIAL_PRINTLN("GIF downloaded and stored in memory");
        return buffer;
    }
    else
    {
        SERIAL_PRINTLN("No data available for GIF");
        return nullptr;
    }
}

uint8_t *NetworkManager::getGifBuffer()
{
    return gifBuffer;
}

size_t NetworkManager::getGifBufferSize()
{
    return gifBufferSize;
}
