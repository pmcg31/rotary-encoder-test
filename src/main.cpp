#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_timer.h>

#define A_PIN 32
#define B_PIN 33
#define Z_PIN 25

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int debounceTime_us = 250;

volatile bool aPinPending;
volatile bool zPinPending;
volatile int aPinDelta;
volatile int aPinTotalTriggers;
volatile bool bPinValue;
volatile bool zPinTriggered;

esp_timer_handle_t aPinTimer;
esp_timer_handle_t zPinTimer;

void IRAM_ATTR APinHandler();
void IRAM_ATTR ZPinHandler();
void APinDebounce(void *);
void ZPinDebounce(void *);

volatile bool leftDisplayClearTriggered;
volatile bool centerDisplayClearTriggered;
volatile bool rightDisplayClearTriggered;

esp_timer_handle_t leftDisplayTimer;
esp_timer_handle_t centerDisplayTimer;
esp_timer_handle_t rightDisplayTimer;

void leftDisplayClear(void *);
void centerDisplayClear(void *);
void rightDisplayClear(void *);

const int displayClearTime_us = 100000;
const int displaySide = 4;
const int leftDisplayX = (SCREEN_WIDTH / 2) + 2;
const int leftDisplayY = 18;
const int centerDisplayX = (SCREEN_WIDTH / 2) + ((SCREEN_WIDTH / 2) / 2) - 2;
const int centerDisplayY = 18;
const int rightDisplayX = SCREEN_WIDTH - 6;
const int rightDisplayY = 18;

esp_timer_create_args_t aPinTimerArgs;
esp_timer_create_args_t zPinTimerArgs;
esp_timer_create_args_t leftDisplayTimerArgs;
esp_timer_create_args_t centerDisplayTimerArgs;
esp_timer_create_args_t rightDisplayTimerArgs;

int indicatorX;
void updateIndicator(int dir);

unsigned long lastRotaryTick_ms;

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

  lastRotaryTick_ms = 0;

  aPinPending = false;
  zPinPending = false;
  aPinDelta = 0;
  aPinTotalTriggers = 0;
  bPinValue = false;
  zPinTriggered = false;
  leftDisplayClearTriggered = false;
  centerDisplayClearTriggered = false;
  rightDisplayClearTriggered = false;

  pinMode(A_PIN, INPUT_PULLUP);
  pinMode(B_PIN, INPUT_PULLUP);
  pinMode(Z_PIN, INPUT_PULLUP);

  attachInterrupt(A_PIN, APinHandler, FALLING);
  attachInterrupt(Z_PIN, ZPinHandler, FALLING);

  aPinTimerArgs.callback = &APinDebounce;
  esp_timer_create(&aPinTimerArgs, &aPinTimer);

  zPinTimerArgs.callback = &ZPinDebounce;
  esp_timer_create(&zPinTimerArgs, &zPinTimer);

  leftDisplayTimerArgs.callback = &leftDisplayClear;
  esp_timer_create(&leftDisplayTimerArgs, &leftDisplayTimer);

  centerDisplayTimerArgs.callback = &centerDisplayClear;
  esp_timer_create(&centerDisplayTimerArgs, &centerDisplayTimer);

  rightDisplayTimerArgs.callback = &rightDisplayClear;
  esp_timer_create(&rightDisplayTimerArgs, &rightDisplayTimer);
}

void loop()
{
  bool displayStuffHappened = false;
  unsigned long now = millis();

  if ((now > lastRotaryTick_ms) && (now - lastRotaryTick_ms) > 50)
  {
    display.fillRect(0, 16, SCREEN_WIDTH / 2, 8, SSD1306_BLACK);
    display.setCursor(0, 16);
    display.printf("RPM: %4d", 0);
    displayStuffHappened = true;
  }

  if (aPinTotalTriggers != 0)
  {
    int tempTotalTriggers = aPinTotalTriggers;
    int tempDelta = aPinDelta;
    aPinTotalTriggers = 0;
    aPinDelta = 0;
    int tickTime_ms = now > lastRotaryTick_ms ? (now - lastRotaryTick_ms) / tempTotalTriggers : 50;
    lastRotaryTick_ms = now;

    int rpm = int(60000.0 / (double(tickTime_ms) * 24.0));
    display.fillRect(0, 16, SCREEN_WIDTH / 2, 8, SSD1306_BLACK);
    display.setCursor(0, 16);
    display.printf("RPM: %4d", rpm);
    displayStuffHappened = true;

    Serial.printf("A triggered! Count: %3d Delta: %3d Tick time: %4d\r\n", tempTotalTriggers, tempDelta, tickTime_ms);

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

  if (displayStuffHappened)
  {
    display.display();
  }
}

void IRAM_ATTR APinHandler()
{
  if (!aPinPending)
  {
    aPinPending = true;
    bPinValue = digitalRead(B_PIN) == LOW;
    esp_timer_start_once(aPinTimer, debounceTime_us);
  }
}

void IRAM_ATTR ZPinHandler()
{
  if (!zPinPending)
  {
    zPinPending = true;
    esp_timer_start_once(zPinTimer, debounceTime_us);
  }
}

void APinDebounce(void *)
{
  aPinPending = false;
  if (digitalRead(A_PIN) == LOW)
  {
    aPinTotalTriggers++;
    if (bPinValue)
    {
      aPinDelta++;
    }
    else
    {
      aPinDelta--;
    }
  }
}

void ZPinDebounce(void *)
{
  zPinPending = false;
  if (digitalRead(Z_PIN) == LOW)
  {
    zPinTriggered = true;
  }
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