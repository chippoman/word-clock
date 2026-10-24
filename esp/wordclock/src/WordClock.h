#ifndef WORDCLOCK_H
#define WORDCLOCK_H

#include <Arduino.h>
#include "ClockDisplayHAL.h"
#include "NetworkManager.h"
#include "GifPlayer.h"

class WordClock
{
public:
    WordClock(ClockDisplayHAL *clockDisplayHAL, ClockNetworkManager *networkManager, GifPlayer *gifPlayer);
    void setup();
    void displayTime();

private:
    int lastHour;
    String allLastHighlightedWords;
    ClockDisplayHAL *clockDisplayHAL;
    ClockNetworkManager *networkManager;
    GifPlayer *gifPlayer;
    bool gifDownloaded;

    void downloadGIF();
    void highlightWord(const String &word, uint32_t color = 0xFFFFFF);
    String getMinutesWord(int minute);
    uint32_t getRandomColor();
};

#endif
