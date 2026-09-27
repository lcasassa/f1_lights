// ESP-NOW receiver for the wireless game controller.
//
// Compiled in only when -DSCREEN_REMOTE_CONTROL=1 (see platformio.ini
// envs esp32-c3-screen-{tetris,catch,invaders}-remote*).

#include "controller_link.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace controller_link {

namespace {

// Wire format. Must stay byte-identical to the sender side
// (esp32-tictactoe/controller_link.cpp).
struct __attribute__((packed)) Packet {
  uint32_t magic;       // 'F1CT' = 0x46314354
  uint8_t  version;     // protocol version
  uint8_t  seq;         // wraps; for diagnostics only
  uint8_t  buttons;     // bit-mask of BTN_*
  uint8_t  reserved;
};

constexpr uint32_t kMagic        = 0x46314354;  // 'F1CT'
constexpr uint8_t  kVersion      = 1;
constexpr uint32_t kStaleMs      = 500;

volatile uint8_t   g_lastMask    = 0;
volatile uint32_t  g_lastRxMs    = 0;
volatile uint8_t   g_lastSeq     = 0;
bool               g_initialised = false;

#if ESP_IDF_VERSION_MAJOR >= 5
void onRecv(const esp_now_recv_info_t * /*info*/, const uint8_t *data, int len) {
#else
void onRecv(const uint8_t * /*mac*/, const uint8_t *data, int len) {
#endif
  if (len < (int)sizeof(Packet)) return;
  const Packet *p = reinterpret_cast<const Packet *>(data);
  if (p->magic != kMagic) return;
  if (p->version != kVersion) return;
  g_lastMask = p->buttons;
  g_lastSeq  = p->seq;
  g_lastRxMs = millis();
}

}  // namespace

void setup() {
  // ESP-NOW needs the radio in some WIFI mode. The screen build always
  // brings WIFI_STA up (for OTA), so we just attach here. If we're
  // called before WiFi is ready, force a sane mode anyway.
  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
  }

  if (g_initialised) {
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    g_initialised = false;
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("controller_link: esp_now_init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  g_initialised = true;

  uint8_t pri = 0; wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&pri, &sec);
  Serial.printf("controller_link: ESP-NOW receiver up (ch=%u)\n", pri);
}

uint8_t buttonMask() {
  if (!isFresh()) return 0;
  return g_lastMask;
}

bool isFresh() {
  if (g_lastRxMs == 0) return false;
  return (millis() - g_lastRxMs) <= kStaleMs;
}

uint32_t lastRxAgeMs() {
  if (g_lastRxMs == 0) return UINT32_MAX;
  return millis() - g_lastRxMs;
}

}  // namespace controller_link

