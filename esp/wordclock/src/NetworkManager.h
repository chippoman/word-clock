#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <Preferences.h>

class ClockNetworkManager
{
public:
    ClockNetworkManager();
    void setup();
    void update();
    struct tm getLocalTimeStruct();
    bool downloadGIF(const char *gifUrl);
    uint8_t *getGifBuffer();
    size_t getGifBufferSize();

private:
    Preferences preferences;
    String timezoneSetting;
    unsigned long lastSyncTime;
    const unsigned long syncInterval = 86400000;
    unsigned long setupButtonPressedSince;
    uint8_t *gifBuffer = nullptr;
    size_t gifBufferSize = 0;

    void runSetupPortal(bool onDemand);
    void checkSetupButton();
    void loadTimezone();
    void saveTimezone(const String &tz);
    String buildTimezoneSelectHtml();
    void syncTimeWithNTP();
    uint8_t *handleDownloadGIFResponse(HTTPClient &http, int gifSize);
};

#endif
