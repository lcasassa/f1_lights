// ESP-NOW transmitter for the wireless game controller (compiled in
// only when -DTICTACTOE_CONTROLLER=1).
//
// We broadcast the button mask: no pairing, no MAC discovery, the
// receiver just listens for our magic header. Both ends MUST agree on
// the WiFi channel — easiest way to guarantee that is for the
// transmitter to also start a (silent) SoftAP on a fixed channel and
// the receiver to follow it via the WIFI_STA association it already
// has. In practice though both boards default to channel 1 in pure
// STA mode, which is what we use here.

#include "controller_link.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace controller_link {

namespace {

struct __attribute__((packed)) Packet {
  uint32_t magic;
  uint8_t  version;
  uint8_t  seq;
  uint8_t  buttons;
  uint8_t  reserved;
};

constexpr uint32_t kMagic    = 0x46314354;  // 'F1CT'
constexpr uint8_t  kVersion  = 1;

uint8_t g_seq = 0;
bool    g_initialised = false;

constexpr uint8_t BCAST_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

}  // namespace

void setup() {
  if (g_initialised) return;

  // Bring the radio up in pure STA mode. We don't actually connect to
  // an AP — ESP-NOW just needs the radio initialised so it can pick a
  // channel. Both sides default to channel 1, which is the ESP-IDF
  // default for STA before association.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);   // make sure we're not auto-connecting
  delay(50);

  if (esp_now_init() != ESP_OK) {
    Serial.println("controller_link: esp_now_init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST_ADDR, 6);
  peer.channel = 0;       // 0 = use the current WiFi channel
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("controller_link: esp_now_add_peer failed");
    return;
  }

  g_initialised = true;
  uint8_t pri = 0; wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&pri, &sec);
  Serial.printf("controller_link: ESP-NOW transmitter up (ch=%u)\n", pri);
}

void sendButtons(uint8_t mask) {
  if (!g_initialised) return;
  Packet p;
  p.magic    = kMagic;
  p.version  = kVersion;
  p.seq      = g_seq++;
  p.buttons  = mask;
  p.reserved = 0;
  esp_now_send(BCAST_ADDR, reinterpret_cast<uint8_t *>(&p), sizeof(p));
}

}  // namespace controller_link

