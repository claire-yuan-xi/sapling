// Sapling: tree pet firmware for the generic Tamagotchi board (v1)
// Board: Seeed XIAO ESP32-C3 | Display: 0.96" SSD1306 I2C OLED (SDA=D4, SCL=D5)
// Buttons: SW1=D0 (water), SW2=D1 (shoo pests), SW3=D2 (compost), active LOW
// Untested design-submission firmware.
// Libraries: Adafruit SSD1306, Adafruit GFX

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_sleep.h>

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

const int BTN_WATER = D0, BTN_PESTS = D1, BTN_SOIL = D2;
const uint64_t TICK_US = 5ULL * 60 * 1000000;  // background update every 5 min
const uint32_t IDLE_MS = 20000;                // sleep after 20 s with no presses

enum Stage : uint8_t { SEED, SPROUT, SAPLING, YOUNG_TREE, OLD_TREE };
const char* STAGE_NAMES[] = {"seed", "sprout", "sapling", "young tree", "old tree"};

struct Pet {
  uint32_t magic;
  int8_t water, soil, happy;
  uint8_t pests;
  Stage stage;
  uint32_t ageMin;
};
RTC_DATA_ATTR Pet pet;

int8_t clamp(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

void tick(uint32_t minutes) {
  pet.water = clamp(pet.water - minutes / 6);
  pet.soil  = clamp(pet.soil - minutes / 30);
  if (random(100) < 4 && pet.pests < 4) pet.pests++;
  pet.happy = clamp((pet.water + pet.soil) / 2 - pet.pests * 10);
  pet.ageMin += minutes;
  if (pet.happy > 50) {
    uint32_t d = pet.ageMin / 1440;
    Stage s = d >= 30 ? OLD_TREE : d >= 14 ? YOUNG_TREE : d >= 5 ? SAPLING : d >= 1 ? SPROUT : SEED;
    if (s > pet.stage) pet.stage = s;
  }
}

void bar(int y, const char* label, int v) {
  oled.setCursor(0, y); oled.print(label);
  oled.drawRect(36, y, 40, 7, SSD1306_WHITE);
  oled.fillRect(36, y, v * 40 / 100, 7, SSD1306_WHITE);
}

void draw() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0); oled.print(STAGE_NAMES[pet.stage]);
  bar(14, "H2O", pet.water);
  bar(26, "SOIL", pet.soil);
  bar(38, "MOOD", pet.happy);
  // the tree: trunk grows with stage, canopy appears after the seed
  int h = 6 + pet.stage * 8;
  oled.fillRect(104, 60 - h, 4, h, SSD1306_WHITE);
  if (pet.stage > SEED) oled.fillCircle(106, 60 - h, 4 + pet.stage * 3, SSD1306_WHITE);
  oled.drawLine(90, 60, 124, 60, SSD1306_WHITE);   // ground
  for (int i = 0; i < pet.pests; i++) oled.fillCircle(92 + i * 8, 63, 1, SSD1306_WHITE);
  oled.display();
}

void play() {
  uint32_t last = millis();
  while (millis() - last < IDLE_MS) {
    bool hit = false;
    if (!digitalRead(BTN_WATER)) { pet.water = clamp(pet.water + 20); hit = true; }
    if (!digitalRead(BTN_PESTS) && pet.pests) { pet.pests--; hit = true; }
    if (!digitalRead(BTN_SOIL))  { pet.soil = clamp(pet.soil + 25); hit = true; }
    if (hit) { tick(0); draw(); last = millis(); delay(200); }
    delay(20);
  }
}

void setup() {
  pinMode(BTN_WATER, INPUT_PULLUP);
  pinMode(BTN_PESTS, INPUT_PULLUP);
  pinMode(BTN_SOIL, INPUT_PULLUP);
  Wire.begin();
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  if (pet.magic != 0x5A91u) pet = {0x5A91u, 70, 70, 70, 0, SEED, 0};  // plant the seed

  auto why = esp_sleep_get_wakeup_cause();
  tick(why == ESP_SLEEP_WAKEUP_TIMER ? 5 : 0);
  draw();
  if (why != ESP_SLEEP_WAKEUP_TIMER) play();

  oled.ssd1306_command(SSD1306_DISPLAYOFF);  // OLED off while asleep
  esp_sleep_enable_timer_wakeup(TICK_US);
  esp_deep_sleep_enable_gpio_wakeup((1ULL << 2) | (1ULL << 3) | (1ULL << 4),  // D0-D2 = GPIO2-4
                                    ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void loop() {}
