#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_timer.h>
#include "RotaryEncoder.hpp"

#define A_PIN 32
#define B_PIN 33
#define Z_PIN 25

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

class EncoderListener : public RotaryEncoderListener
{
  virtual void turned(RotaryEncoder *source, int deltaClicks, int rpm);
  virtual void clicked(RotaryEncoder *source);
};

EncoderListener listener;
RotaryEncoder encoder(&listener, A_PIN, B_PIN, Z_PIN, 24);
int aPinDelta;
int encoder_rpm;
bool zPinTriggered;

volatile bool leftDisplayClearTriggered;
volatile bool centerDisplayClearTriggered;
volatile bool rightDisplayClearTriggered;
volatile bool rpmClearTriggered;

esp_timer_handle_t leftDisplayTimer;
esp_timer_handle_t centerDisplayTimer;
esp_timer_handle_t rightDisplayTimer;
esp_timer_handle_t rpmTimer;

void leftDisplayClear(void *);
void centerDisplayClear(void *);
void rightDisplayClear(void *);
void rpmClear(void *);

const int displayClearTime_us = 100000;
const int displaySide = 4;
const int leftDisplayX = (SCREEN_WIDTH / 2) + 2;
const int leftDisplayY = 18;
const int centerDisplayX = (SCREEN_WIDTH / 2) + ((SCREEN_WIDTH / 2) / 2) - 2;
const int centerDisplayY = 18;
const int rightDisplayX = SCREEN_WIDTH - 6;
const int rightDisplayY = 18;

esp_timer_create_args_t leftDisplayTimerArgs;
esp_timer_create_args_t centerDisplayTimerArgs;
esp_timer_create_args_t rightDisplayTimerArgs;
esp_timer_create_args_t rpmTimerArgs;

int indicatorX;
void updateIndicator(int dir);

void setup()
{
  Serial.begin(115200);
  Serial.printf("rotary-encoder-test  %s\n", __TIMESTAMP__);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3C for 128x32
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ; // Don't proceed, loop forever
  }

  encoder.init();

  indicatorX = (SCREEN_WIDTH / 2) - 2;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("rot-enc-test");
  display.setCursor(0, 8);
  display.printf("%s %s", __DATE__, __TIME__);
  display.setCursor(0, 16);
  display.printf("RPM: %4d", 0);
  display.drawRect(leftDisplayX - 2, leftDisplayY - 2, displaySide + 4, displaySide + 4, SSD1306_WHITE);
  display.drawRect(centerDisplayX - 2, centerDisplayY - 2, displaySide + 4, displaySide + 4, SSD1306_WHITE);
  display.drawRect(rightDisplayX - 2, rightDisplayY - 2, displaySide + 4, displaySide + 4, SSD1306_WHITE);
  display.drawRect(indicatorX, 24, 4, 8, SSD1306_WHITE);
  display.display();

  leftDisplayClearTriggered = false;
  centerDisplayClearTriggered = false;
  rightDisplayClearTriggered = false;
  rpmClearTriggered = false;

  leftDisplayTimerArgs.callback = &leftDisplayClear;
  esp_timer_create(&leftDisplayTimerArgs, &leftDisplayTimer);

  centerDisplayTimerArgs.callback = &centerDisplayClear;
  esp_timer_create(&centerDisplayTimerArgs, &centerDisplayTimer);

  rightDisplayTimerArgs.callback = &rightDisplayClear;
  esp_timer_create(&rightDisplayTimerArgs, &rightDisplayTimer);

  rpmTimerArgs.callback = &rpmClear;
  esp_timer_create(&rpmTimerArgs, &rpmTimer);
}

void loop()
{
  encoder.loop();

  bool displayStuffHappened = false;

  if (encoder_rpm != 0)
  {
    int temp_rpm = encoder_rpm;
    encoder_rpm = 0;
    display.fillRect(0, 16, SCREEN_WIDTH / 2, 8, SSD1306_BLACK);
    display.setCursor(0, 16);
    display.printf("RPM: %4d", temp_rpm);
    esp_timer_start_once(rpmTimer, displayClearTime_us);
    displayStuffHappened = true;
    Serial.printf("RPM: %3d\r\n", temp_rpm);
  }

  if (aPinDelta != 0)
  {
    int tempDelta = aPinDelta;
    aPinDelta = 0;

    Serial.printf("A triggered! Delta: %3d\r\n", tempDelta);

    if (tempDelta > 0)
    {
      display.fillRect(rightDisplayX, rightDisplayY, displaySide, displaySide, SSD1306_WHITE);
      rightDisplayClearTriggered = false;
      esp_timer_start_once(rightDisplayTimer, displayClearTime_us);
      updateIndicator(1);
      displayStuffHappened = true;
    }
    else if (tempDelta < 0)
    {
      display.fillRect(leftDisplayX, leftDisplayY, displaySide, displaySide, SSD1306_WHITE);
      leftDisplayClearTriggered = false;
      esp_timer_start_once(leftDisplayTimer, displayClearTime_us);
      updateIndicator(-1);
      displayStuffHappened = true;
    }
  }

  if (zPinTriggered)
  {
    zPinTriggered = false;

    Serial.println("Z triggered!");
    display.fillRect(centerDisplayX, centerDisplayY, displaySide, displaySide, SSD1306_WHITE);
    centerDisplayClearTriggered = false;
    esp_timer_start_once(centerDisplayTimer, displayClearTime_us);
    updateIndicator(0);
    displayStuffHappened = true;
  }

  if (leftDisplayClearTriggered)
  {
    leftDisplayClearTriggered = false;
    display.fillRect(leftDisplayX, leftDisplayY, displaySide, displaySide, SSD1306_BLACK);
    displayStuffHappened = true;
  }

  if (centerDisplayClearTriggered)
  {
    centerDisplayClearTriggered = false;
    display.fillRect(centerDisplayX, centerDisplayY, displaySide, displaySide, SSD1306_BLACK);
    displayStuffHappened = true;
  }

  if (rightDisplayClearTriggered)
  {
    rightDisplayClearTriggered = false;
    display.fillRect(rightDisplayX, rightDisplayY, displaySide, displaySide, SSD1306_BLACK);
    displayStuffHappened = true;
  }

  if (rpmClearTriggered)
  {
    rpmClearTriggered = false;
    display.fillRect(0, 16, SCREEN_WIDTH / 2, 8, SSD1306_BLACK);
    display.setCursor(0, 16);
    display.printf("RPM: %4d", 0);
    displayStuffHappened = true;
  }

  if (displayStuffHappened)
  {
    display.display();
  }
}

void EncoderListener::turned(RotaryEncoder *source, int deltaClicks, int rpm)
{
  aPinDelta = deltaClicks;
  encoder_rpm = rpm;
}

void EncoderListener::clicked(RotaryEncoder *source)
{
  zPinTriggered = true;
}

void leftDisplayClear(void *)
{
  leftDisplayClearTriggered = true;
}

void centerDisplayClear(void *)
{
  centerDisplayClearTriggered = true;
}

void rightDisplayClear(void *)
{
  rightDisplayClearTriggered = true;
}

void rpmClear(void *)
{
  rpmClearTriggered = true;
}

void updateIndicator(int dir)
{
  if (dir < 0)
  {
    if (indicatorX > 0)
    {
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_BLACK);
      indicatorX--;
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_WHITE);
    }
    else
    {
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_BLACK);
      indicatorX = SCREEN_WIDTH - 4;
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_WHITE);
    }
  }
  else if (dir > 0)
  {
    if (indicatorX < SCREEN_WIDTH - 4)
    {
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_BLACK);
      indicatorX++;
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_WHITE);
    }
    else
    {
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_BLACK);
      indicatorX = 0;
      display.drawRect(indicatorX, 24, 4, 8, SSD1306_WHITE);
    }
  }
  else
  {
    display.drawRect(indicatorX, 24, 4, 8, SSD1306_BLACK);
    indicatorX = (SCREEN_WIDTH / 2) - 2;
    display.drawRect(indicatorX, 24, 4, 8, SSD1306_WHITE);
  }
}