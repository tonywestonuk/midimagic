#ifndef UI_H
#define UI_H

#include <Bounce2.h>
#include <M5GFX.h>
#include "FS.h"
#include "SD.h"
#include <Arduino.h>

extern void play_wav(const char* filename);

#define IP5306_REG_SYS_0 0x00
#define IP5306_REG_READ_4 0x78

class UI {

public:
  char songName[25];
  std::atomic<bool> songChanged{ false };

  UI() {
    // Wire.begin();

    Serial.println("Setup UI");

    buttonA.attach(39, INPUT_PULLUP);
    buttonB.attach(38, INPUT_PULLUP);
    buttonC.attach(37, INPUT_PULLUP);

    buttonA.interval(5);
    buttonB.interval(5);
    buttonC.interval(5);

    buttonA.setPressedState(LOW);
    buttonB.setPressedState(LOW);
    buttonC.setPressedState(LOW);


    tft.init();
    tft.setRotation(0);
    delay(300);

    drawTFTScreen();
    startTask();
  }


private:
  M5GFX tft;
  int lastBrightness = 0;
  long lastBrightnessChanged = 0;
  TaskHandle_t taskHandle = nullptr;

  Bounce2::Button buttonA = Bounce2::Button();
  Bounce2::Button buttonB = Bounce2::Button();
  Bounce2::Button buttonC = Bounce2::Button();

  void setBrightness(int brightness) {

    if ((brightness == lastBrightness) || (millis() - lastBrightnessChanged) < 100) return;
    tft.setBrightness(brightness);
    lastBrightness = brightness;
    lastBrightnessChanged = millis();
  }

  void startTask() {
    xTaskCreate(
      UITaskWrapper,
      "UITaskWrapper",
      5000,
      this,
      1,
      &taskHandle);
  }

  static void UITaskWrapper(void* params) {
    UI* instance = static_cast<UI*>(params);
    instance->runTask();
  }

  void runTask() {

    long ti = millis();
    int currentFileNum = -1;
    for (;;) {

      long dif = 10000 - (millis() - ti);
      int brightness = dif / 6;
      if (brightness < 0) brightness = 0;
      if (brightness > 127) brightness = 127;

      if (brightness == 0) {
        if (dimmed_screen() == 1) {
          play_wav("/thunderz.wav");
        }

        ti = millis();
      }

      setBrightness(brightness);

      buttonA.update();
      buttonB.update();
      buttonC.update();
      if (buttonA.pressed()) {
        Serial.println("A pressed");
        ti = millis();

        // PLay sound from SD card.
        play_wav("/thunderz.wav");
      }

      if (buttonB.pressed()) {
        currentFileNum--;
        if (currentFileNum < -1) currentFileNum = -1;
        selectFile(currentFileNum);
        ti = millis();
      }

      if (buttonC.pressed()) {
        currentFileNum++;
        if (currentFileNum > 1) currentFileNum = 1;
        selectFile(currentFileNum);
        ti = millis();
      }
      delay(10);
    }
  }

  void drawTFTScreen() {
    tft.fillScreen(TFT_BLUE);
    //uiBattery();
    tft.fillRect(0, 23, 240, 2);
    selectFile(-1);
  }

  void selectFile(int filenum) {
    Serial.println("Select file");
    File root = SD.open("/tracks/");

    File file = root.openNextFile();
    while (file && strncmp("._", file.name(), 2) == 0) {
      file.close();
      file = root.openNextFile();
    }
    int c = -1;
    while (file) {
      if (c == filenum) {
        tft.setColor(TFT_WHITE);
        tft.setTextColor(TFT_BLUE, TFT_WHITE);

        if (c == -1) {
          // Send off
          snprintf(songName, sizeof(songName), "");
        } else {
          snprintf(songName, sizeof(songName), file.name());
        }
        songChanged.store(true);


      } else {
        tft.setColor(TFT_BLUE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
      }
      tft.fillRect(0, c * 30 + 68, 240, 30);

      if (c == -1) {
        tft.drawString(" - OFF -", 20, c * 30 + 70, 4);
      } else {
        tft.drawString(file.name(), 20, c * 30 + 70, 4);
        file.close();
        file = root.openNextFile();
        while (file && strncmp("._", file.name(), 2) == 0) {
          file.close();
          file = root.openNextFile();
        }
      }
      c++;
    }
    root.close();
  }


  int dimmed_screen() {
    tft.setBrightness(0);
    for (;;) {
      buttonA.update();
      buttonB.update();
      buttonC.update();

      if (buttonA.pressed()) return 1;
      else if (buttonB.pressed()) return 2;
      else if (buttonC.pressed()) return 3;

      delay(10);
    }
  }

  // void uiBattery() {

  //   int reg_4 = ip5306_get_reg(IP5306_REG_READ_4);

  //   //Serial.println(reg_4);
  //   int power = ((reg_4 & 0x01 ? 0 : 25) + (reg_4 & 0x02 ? 0 : 25) + (reg_4 & 0x04 ? 0 : 25) + (reg_4 & 0x08 ? 0 : 25));


  //   int powerWidth = (38 * power) / 100;

  //   tft.setColor(TFT_WHITE);
  //   tft.drawRoundRect(190, 5, 40, 15, 3);
  //   tft.fillRect(230, 7, 3, 11);
  //   tft.fillRect(191, 6, powerWidth, 13);
  // }

  // int ip5306_get_reg(uint8_t reg) {
  //   Wire.beginTransmission(0x75);
  //   Wire.write(reg);
  //   if (Wire.endTransmission(false) == 0 && Wire.requestFrom(0x75, 1)) {
  //     return Wire.read();
  //   }
  //   return -1;
  // }

  // int ip5306_set_reg(uint8_t reg, uint8_t value) {
  //   Wire.beginTransmission(0x75);
  //   Wire.write(reg);
  //   Wire.write(value);
  //   if (Wire.endTransmission(true) == 0) {
  //     return 0;
  //   }
  //   return -1;
  // }
};



#endif