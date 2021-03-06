#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <Adafruit_NeoPixel.h>

#define PIN            6
int r =0;
int g= 0;
int b=255;
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      4


// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int delayval = 5; // delay for half a second

#define EEPROM_CFG 1
#define EEPROM_FOLDER 2
#define EEPROM_TRACK 4

#define BUTTON_TOLERANCE 2
#define LONG_KEY_PRESS_TIME_MS 2000L
#define VOLUME_CHECK_INTERVAL_MS 200L
#define PLAY_DELAY_MS 500L
#define FADE_OUT_MS  500L * 60L
#define READ_RETRIES 3

#define PIN_KEY A3
#define PIN_VOLUME A2
#define PIN_VOLUME_INTERNAL A4

#define NO_FOLDERS 11

SoftwareSerial softSerial(9, 11); // RX, TX
DFRobotDFPlayerMini player;

enum {
  MODE_NORMAL, MODE_SET_TIMER
} mode = MODE_NORMAL;

boolean loopPlaylist = false;
boolean continuousPlayWithinPlaylist = false;
boolean restartLastTrackOnStart = false;
int maxVolume =20;
float volFade = 1.0;
int vol = 10;
int key = -1;
int Pause = 0;
unsigned long keyPressTimeMs = 0L;
unsigned long volumeHandledLastMs = 0L;
float sensorValue = 0;
unsigned long sleepAtMs = 0L;
unsigned long offAtMs = 0L;

unsigned long nowMs;

int16_t curFolder = -1;
int16_t curTrack = -1;
int16_t curTrackFileNumber = -1;

unsigned long startTrackAtMs = 0L;

int maxTracks[NO_FOLDERS];

void turnOff() {
  volFade = 0.0;
  player.volume(5);
  delay(50);
  player.stop();
  delay(50);
  player.outputDevice(DFPLAYER_DEVICE_SLEEP);
  delay(50);
  player.disableDAC();
  delay(50);
  player.sleep();
  delay(200);
pixels.clear();
pixels.show();
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
  restartLastTrackOnStart = cfg & 4;
}

void writeConfig() {
  uint8_t cfg = eeprom_read_byte((uint8_t*) EEPROM_CFG);

  cfg = (cfg & (0xff ^ 1)) | (loopPlaylist ? 1 : 0);
  cfg = (cfg & (0xff ^ 2)) | (continuousPlayWithinPlaylist ? 2 : 0);
  cfg = (cfg & (0xff ^ 4)) | (restartLastTrackOnStart ? 4 : 0);

  eeprom_update_byte((uint8_t*) EEPROM_CFG, cfg);
}

void readTrackInfo() {
  curFolder = (int16_t) eeprom_read_word((uint16_t*) EEPROM_FOLDER);
  if (curFolder < 0 || curFolder > NO_FOLDERS) {
    curFolder = -1;
    curTrack = -1;

    return;
  }

  curTrack = (int16_t) eeprom_read_word((uint16_t*) EEPROM_TRACK);
  if (curTrack < 0 || curTrack > maxTracks[curFolder - 1]) {
    curTrack = -1;
    curFolder = -1;
  }
}

void writeTrackInfo(int16_t folder, int16_t track) {
  eeprom_update_word((uint16_t*) EEPROM_FOLDER, (uint16_t) folder);
  eeprom_update_word((uint16_t*) EEPROM_TRACK, (uint16_t) track);
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
    ret = player.readCurrentFileNumber(DFPLAYER_DEVICE_SD);
  }
  return ret;
}

void playOrAdvertise(int fileNo) {
  Serial.println("ADVERTISE");
  Serial.println(player.readState());
  //int state = player.readState();
  //if (state >= 0) {
   // if ((state & 1) == 1) {
      player.playMp3Folder(fileNo);
   // } else {
    //  player.playMp3Folder(1);
  //  }
 // }
}

void handleleds(){

float sensorOld = sensorValue;





 sensorValue = analogRead(A1);

 
  // print out the value you read;
  Serial.print(sensorValue);
  Serial.print(" ");
 float sensorVolt = 5 * sensorValue ;
 sensorVolt =  sensorVolt /1023 ;
Serial.print("Volt: ");
Serial.println(sensorVolt); 

sensorValue = map(sensorValue, 695, 860, 1, 4);
 if (sensorOld != sensorValue){
pixels.clear();  
 }
  Serial.println(sensorValue);
  delay(1);        // delay in between reads for stability
  // For a set of NeoPixels the first NeoPixel is 0, second is 1, all the way up to the count of pixels minus one.

  for(int i=0;i<sensorValue;i++){

    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
 
    
    pixels.setPixelColor(i, pixels.Color(r,g,b)); // Moderately bright green color.

    pixels.show(); // This sends the updated pixel color to the hardware.

 //   delay(delayval); // Delay for a period of time (in milliseconds).

  }

  
}


void playFolderOrNextInFolder(int folder, boolean loop = true) {
  startTrackAtMs = 0;

  if (curFolder != folder) {
    curFolder = folder;
    curTrack = 1;
  } else {
    if (++curTrack > maxTracks[folder - 1]) {
      curTrack = 1;

      if (!loop)
      {
        curTrack = maxTracks[folder - 1];
        return;
      }
    }
  }
Serial.print("Folder= ");
Serial.print(curFolder);
Serial.print(" Track= ");
Serial.println(curTrack);

  startTrackAtMs = millis() + PLAY_DELAY_MS;
}

void setup() {
  pinMode(PIN_VOLUME, INPUT);
  pinMode(PIN_VOLUME_INTERNAL, INPUT);
  pinMode(PIN_KEY, INPUT_PULLUP);
pixels.setBrightness(10);
  pixels.begin(); // This initializes the NeoPixel library.
   rainbowCycle(1); 
  readConfig();

  delay(50);

  softSerial.begin(9600);
Serial.begin(9600);
  if (!player.begin(softSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));
 
  
 // player.begin(softSerial, false, false); // disable ACK to work with MH2024K-24SS chips

  initDFPlayer();
  handleVolume();
  player.volume(10);
  player.playMp3Folder(138);
    rainbowCycle(1); 
  Serial.println("Fertig");

  for (int i = 0; i < NO_FOLDERS; ++i) {
    maxTracks[i] = readPlayerFileCountsInFolder(i + 1, READ_RETRIES);
  }

  if (restartLastTrackOnStart) {
    readTrackInfo();
    if (curFolder != -1 && curTrack != -1) {
      startTrackAtMs = millis() + PLAY_DELAY_MS;
    }
  }
}

inline void handleSleepTimer() {
  Serial.print("Sleep Ziel: ");
  Serial.print(sleepAtMs);
  Serial.print(" aktuelle Zeit: ");
  Serial.println(nowMs);
  
  if (sleepAtMs != 0 && nowMs >= sleepAtMs) {
    Serial.println("TIMER ABGELAUFEN");
    volFade = 1.0 - (nowMs - sleepAtMs) / (float) (offAtMs - sleepAtMs);
    Serial.print("VolFADE: ");
    Serial.println(volFade);
    if (volFade <= 0.0) {
      turnOff();
    }
  }
}

inline void handleVolume() {
 
  if (nowMs > volumeHandledLastMs + VOLUME_CHECK_INTERVAL_MS) {
    volumeHandledLastMs = nowMs;

    int volCurrent = analogRead(PIN_VOLUME);
    //int volInternal = analogRead(PIN_VOLUME_INTERNAL);
   
    
    int volNew = (map(volCurrent, 0, 1023, 1,maxVolume));
    
    if (volNew != vol) {
      vol = volNew;
      //vol = 10;
      player.volume(vol);
     
    }
  }
}

inline void handleKeyPress() {
  int keyCurrent = analogRead(PIN_KEY);
Serial.print("Key Cururent: ");
Serial.println(keyCurrent);
  /* /////////////////////////////////////DEBUGGING TIME /////////////////////////////////
Serial.print(nowMs);
Serial.print("-");  
Serial.print(keyPressTimeMs);
Serial.print("=");
Serial.println(nowMs-keyPressTimeMs);


*/



  if (keyCurrent > 980 && key > 0) {
    switch (mode) {
      case MODE_NORMAL:
        if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 5) {
          mode = MODE_SET_TIMER;
          playOrAdvertise(100);
          delay(1000);
        } else if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 1) {
          continuousPlayWithinPlaylist = !continuousPlayWithinPlaylist;
          playOrAdvertise(continuousPlayWithinPlaylist ? 200 : 201);
          writeConfig();
          delay(500);
        } else if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 2) {
          loopPlaylist = !loopPlaylist;
          playOrAdvertise(loopPlaylist ? 300 : 301);
          writeConfig();
          delay(500);
        } else if (nowMs - keyPressTimeMs >= LONG_KEY_PRESS_TIME_MS && key == 3) {
          restartLastTrackOnStart = !restartLastTrackOnStart;
          playOrAdvertise(restartLastTrackOnStart ? 400 : 401);
          writeConfig();
          writeTrackInfo(curFolder, curTrack);
          delay(500);
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
  } else if (keyCurrent <= 980) {

    int keyOld = key;
Serial.println(keyCurrent);
    if (keyCurrent > 930 - BUTTON_TOLERANCE) {
      Serial.println(key);
      PausenHandler();
    
      delay(500);
      
    } else if (keyCurrent > 845 - BUTTON_TOLERANCE) {
      key = 9;
      r=0;
      g=0;
      b=255;
        Serial.println(key);
      delay(500);
    } else if (keyCurrent > 760 - BUTTON_TOLERANCE) {
      key = 6;
      r=255;
      g=0;
      b=0;
        Serial.println(key);
     delay(500);
      
    } else if (keyCurrent > 670 - BUTTON_TOLERANCE) {
      key = 3;
      r=200;
      g=200;
      b=200;
        Serial.println(key);
     delay(500);
    } else if (keyCurrent > 590 - BUTTON_TOLERANCE) {
      key = 2;
      r=0;
      g=255;
      b=0;
        Serial.println(key);
      delay(500);
    } else if (keyCurrent > 500 - BUTTON_TOLERANCE) {
      key = 5;
      r=0;
      g=0;
      b=255;
        Serial.println(key);
      delay(500);
    } else if (keyCurrent >  400 - BUTTON_TOLERANCE) {
      key = 8;
       r=255;
      g=255;
      b=0;
        Serial.println(key);
     delay(500);
    } else if (keyCurrent > 320 - BUTTON_TOLERANCE) {
      key = 11;
        Serial.println(key);
      delay(500);
    } else if (keyCurrent > 230 - BUTTON_TOLERANCE) {
      key = 7;
      r=255;
      g=0;
      b=0;
        Serial.println(key);
      delay(500);
    } else if (keyCurrent > 120 - BUTTON_TOLERANCE) {
      key = 4;
      r=255;
      g=255;
      b=0;
        Serial.println(key);
      delay(500); 
     } else if (keyCurrent > 10 - BUTTON_TOLERANCE) {
      key = 1;
      r=255;
      g=255;
      b=255;
        Serial.println(key);
      delay(500);
   /*  
    } else if (keyCurrent > 10) {
     Serial.print("Pause: ");
     Serial.println(Pause);
   PausenHandler();
        */
    
      
    }

    if (keyOld != key) {
      keyPressTimeMs = nowMs;
     // Serial.println(keyPressTimeMs);
    }
   
  }
}

void PausenHandler(){
  if (Pause < 1){
    player.pause();
    Pause = 1;
    Serial.println("Pause");
   
  }
  else if (Pause >0){
    player.start();
    Pause = 0;
    Serial.println("Play");
    
  }
  delay(100);
}



void loop() {
  nowMs = millis();

  handleSleepTimer();
  handleVolume();
  
  handleKeyPress();
  handleleds();

  if (startTrackAtMs != 0 and nowMs >= startTrackAtMs) {
    startTrackAtMs = 0;
    player.playLargeFolder(curFolder, curTrack);
  delay(200);
    if (restartLastTrackOnStart) {
      writeTrackInfo(curFolder, curTrack);
    }

    // Don't reduce the following delay. Otherwise player might not have started playing
    // the requested track, returning the file number of the previous file, thus breaking
    // continuous play list playing which relies on correct curTrackFileNumber.
    delay(500);
    curTrackFileNumber = readPlayerCurrentFileNumber(READ_RETRIES);
  }

  if (player.available()) {
   
    uint8_t type = player.readType();
    int value = player.read();
Serial.print("player.read: ");
Serial.println(value);

    if (type == DFPlayerPlayFinished && value == curTrackFileNumber) {
      int16_t oldTrack = curTrack;

      if (startTrackAtMs == 0 /* no user request pending */ && continuousPlayWithinPlaylist) {
        playFolderOrNextInFolder(curFolder, loopPlaylist);

        // Don't reduce the following delay. Callbacks that a track has finished
        // might occur multiple times within 1 second and you don't want to move
        // on more than exactly one track.
        delay(1000);
      }

      if (oldTrack == curTrack) {
        writeTrackInfo(-1, -1);
      }
    }
  }

  delay(50);
}



void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));
    }
    pixels.show();
    delay(wait);
  }
}



// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
