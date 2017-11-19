#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

#define EEPROM_CFG 1
#define BUTTON_TOLERANCE 25
#define LONG_KEY_PRESS_TIME_MS 2000L
#define FADE_OUT_MS 3L * 1000L * 60L
#define READ_RETRIES 3

#define PIN_KEY A3
#define PIN_VOLUME A2
#define PIN_VOLUME_INTERNAL A1

SoftwareSerial softSerial(0, 1); // RX, TX 0,1
DFRobotDFPlayerMini player;

enum {
  MODE_NORMAL, MODE_SET_TIMER
} mode = MODE_NORMAL;

boolean loopPlaylist = false;
boolean continuousPlayWithinPlaylist = false;

float volFade = 1.0;
int vol = -1;
int key = -1;
unsigned long keyPressTimeMs = 0L;
unsigned long sleepAtMs = 0L;
unsigned long offAtMs = 0L;

int curFolder = -1;
int curFolderMaxTracks = -1;
int curTrack = -1;
int curTrackFileNumber = -1;

void turnOff() {
  volFade = 0.0;
  player.volume(0);
  delay(50);
  player.stop();
  delay(50);
  player.outputDevice(DFPLAYER_DEVICE_SLEEP);
  delay(50);
  player.disableDAC();
  delay(50);
  player.sleep();
  delay(200);

  while (1) {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode();
    delay(5000);
  }
}

void initDFPlayer(boolean reset = false) {
  if (reset) {
    player.setTimeOut(6000);
    player.reset();
    player.waitAvailable();
  }
  player.setTimeOut(1000);

  delay(50);

  player.EQ(DFPLAYER_EQ_NORMAL);
  player.outputDevice(DFPLAYER_DEVICE_SD);
  player.enableDAC();
}

void readConfig() {
  uint8_t cfg = eeprom_read_byte((uint8_t*) EEPROM_CFG);
  loopPlaylist = cfg & 1;
  continuousPlayWithinPlaylist = cfg & 2;
}

int readPlayerState(int retries) {
  int ret = -1;
  while (--retries >= 0 && ret == -1) {
    ret = player.readState();
  }
  return ret;
}

int readPlayerFileCountsInFolder(int folderNumber, int retries) {
  int ret = -1;
  while (--retries >= 0 && ret == -1) {
    ret = player.readFileCountsInFolder(folderNumber);
  }
  return ret;
}

int readPlayerCurrentFileNumber(int retries) {
  int ret = -1;
  while (--retries >= 0 && ret == -1) {
    ret = player.readCurrentFileNumber();
  }
  return ret;
}

void writeConfig() {
  uint8_t cfg = eeprom_read_byte((uint8_t*) EEPROM_CFG);
  cfg = (cfg & (0xff ^ 1)) | (loopPlaylist ? 1 : 0);
  cfg = (cfg & (0xff ^ 2)) | (continuousPlayWithinPlaylist ? 2 : 0);

  eeprom_update_byte((uint8_t*) EEPROM_CFG, cfg);
}

void playOrAdvertise(int fileNo) {
  int state = readPlayerState(READ_RETRIES);
  if (state >= 0) {
    if ((state & 1) == 1) {
      player.advertise(fileNo);
    } else {
      player.playMp3Folder(fileNo);
    }
  }
}

void playFolderOrNextInFolder(int folder, boolean loopPlayList = true) {
  if (curFolder != folder) {
    curFolder = folder;
    curTrack = 1;
    curFolderMaxTracks = readPlayerFileCountsInFolder(folder, READ_RETRIES);
  } else {
    if (++curTrack > curFolderMaxTracks) {
      curTrack = 1;

      if (!loopPlayList)
      {
        curTrack = curFolderMaxTracks;
        return;
      }
    }
  }
  player.playFolder(curFolder, curTrack);
  delay(125);
  curTrackFileNumber = readPlayerCurrentFileNumber(READ_RETRIES);
}

void setup() {
  pinMode(PIN_VOLUME, INPUT);
  pinMode(PIN_VOLUME_INTERNAL, INPUT);
  pinMode(PIN_KEY, INPUT_PULLUP);

  readConfig();

  delay(50);

  softSerial.begin(9600);

  while (!player.begin(softSerial)) {
    delay(200);
  }

  initDFPlayer();
}

void loop() {
  if (volFade <= 0.0) {
    turnOff();
  }

  unsigned long nowMs = millis();

  if (sleepAtMs != 0 && nowMs >= sleepAtMs) {
    volFade = 1.0 - (nowMs - sleepAtMs) / (float) (offAtMs - sleepAtMs);
    if (volFade <= 0.0) {
      turnOff();
    }
  }

  int volCurrent = analogRead(PIN_VOLUME);
  int volInternal = analogRead(PIN_VOLUME_INTERNAL);
  int volNew = (map(volCurrent, 0, 1023, 1,
                    31 - map(volInternal, 1023, 0, 1, 30))) * volFade;
  if (volNew != vol) {
    vol = volNew;
    player.volume(vol);
  }

  int keyCurrent = analogRead(PIN_KEY);

  if (keyCurrent > 1000 && key > 0) {
    switch (mode) {
      case MODE_NORMAL:
        if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 11) {
          mode = MODE_SET_TIMER;
          playOrAdvertise(100);
          delay(1000);
        } else if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 1) {
          continuousPlayWithinPlaylist = !continuousPlayWithinPlaylist;
          playOrAdvertise(continuousPlayWithinPlaylist ? 200 : 201);
          writeConfig();
          delay(1000);
        } else if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 2) {
          loopPlaylist = !loopPlaylist;
          playOrAdvertise(loopPlaylist ? 300 : 301);
          writeConfig();
          delay(1000);
        } else {
          playFolderOrNextInFolder(key);
        }
        break;

      case MODE_SET_TIMER:
        playOrAdvertise(key);

        if (key == 1) {
          sleepAtMs = 0;
          offAtMs = 0;
          volFade = 1.0;
        } else {
          sleepAtMs = nowMs + (key - 1) * 1000L * 60L * 5L;
          offAtMs = sleepAtMs + FADE_OUT_MS;
        }

        delay(1000);
        mode = MODE_NORMAL;
        break;

    }

    key = -1;
  } else if (keyCurrent <= 1000) {
    int keyOld = key;

    if (keyCurrent > 949 - BUTTON_TOLERANCE) {
      key = 11;
    } else if (keyCurrent > 894 - BUTTON_TOLERANCE) {
      key = 9;
    } else if (keyCurrent > 847 - BUTTON_TOLERANCE) {
      key = 6;
    } else if (keyCurrent > 801 - BUTTON_TOLERANCE) {
      key = 3;
    } else if (keyCurrent > 754 - BUTTON_TOLERANCE) {
      key = 2;
    } else if (keyCurrent > 701 - BUTTON_TOLERANCE) {
      key = 5;
    } else if (keyCurrent > 635 - BUTTON_TOLERANCE) {
      key = 8;
    } else if (keyCurrent > 553 - BUTTON_TOLERANCE) {
      key = 10;
    } else if (keyCurrent > 444 - BUTTON_TOLERANCE) {
      key = 7;
    } else if (keyCurrent > 286 - BUTTON_TOLERANCE) {
      key = 4;
    } else if (keyCurrent > 0) {
      key = 1;
    }

    if (keyOld != key) {
      keyPressTimeMs = nowMs;
    }
  }

  if (player.available()) {
    uint8_t type = player.readType();
    int value = player.read();

    if (type == DFPlayerPlayFinished && value == curTrackFileNumber) {
      if (continuousPlayWithinPlaylist) {
        delay(1000);
        playFolderOrNextInFolder(curFolder, loopPlaylist);
      }
    }
  }

  delay(125);
}
