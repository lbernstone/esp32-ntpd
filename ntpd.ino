#include <WiFi.h>
#include <esp_sntp.h>

#define NTP_SRV "pool.ntp.org"
#define NTP_TIMEOUT 30 //sec
#define NTP_PORT 123
#define NTPD_DELAY 100 //msec
#define NTP_PACKET_SIZE 48
#define NTP_STRATUM 0b00000002
#define SECONDS_FROM_1900_TO_1970  2208988800UL

timeval last_ntpsync;

void buildNTPpacket(byte* packetBuffer)
{
  timeval now;
  gettimeofday(&now, NULL);
  // LI: 0, Version: 3, Mode: 4 (server)
  packetBuffer[0] = 0b00011100;
  // Stratum, or type of clock
  packetBuffer[1] = NTP_STRATUM;
  // Polling Interval
  packetBuffer[2] = 4;
  // Peer Clock Precision
  packetBuffer[3] = 0xF7;
  // root delay
  packetBuffer[4] = 0; 
  packetBuffer[5] = 0;
  packetBuffer[6] = 0; 
  packetBuffer[7] = 0;
  // root dispersion
  packetBuffer[8] = 0;
  packetBuffer[9] = 0;
  packetBuffer[10] = 0;
  packetBuffer[11] = 0x50;
  //time source (namestring)
  packetBuffer[12] = 69; //E
  packetBuffer[13] = 83; //S
  packetBuffer[14] = 80; //P
  packetBuffer[15] = 0;
  //reference time is last sync
  uint32_t rSec = last_ntpsync.tv_sec + SECONDS_FROM_1900_TO_1970;
  packetBuffer[16] = (rSec >> 24) & 0xFF;
  packetBuffer[17] = (rSec >> 16) & 0xFF;
  packetBuffer[18] = (rSec >> 8) & 0xFF;
  packetBuffer[19] = rSec & 0xFF;
  packetBuffer[20] = (last_ntpsync.tv_usec >> 24) & 0xFF;
  packetBuffer[21] = (last_ntpsync.tv_usec >> 16) & 0xFF;
  packetBuffer[22] = (last_ntpsync.tv_usec >> 8) & 0xFF;
  packetBuffer[23] = last_ntpsync.tv_usec & 0xFF;
  //copy client xmit time to originate 
  for (int i = 24; i < 32; i++) packetBuffer[i] = packetBuffer[i-8];
  //put initial time into receive
  rSec = now.tv_sec + SECONDS_FROM_1900_TO_1970;
  packetBuffer[32] = (rSec >> 24) & 0xFF;
  packetBuffer[33] = (rSec >> 16) & 0xFF;
  packetBuffer[34] = (rSec >> 8) & 0xFF;
  packetBuffer[35] = rSec & 0xFF;
  packetBuffer[36] = (now.tv_usec >> 24) & 0xFF;
  packetBuffer[37] = (now.tv_usec >> 16) & 0xFF;
  packetBuffer[38] = (now.tv_usec >> 8) & 0xFF;
  packetBuffer[39] = now.tv_usec & 0xFF;
  //put final time into xmit
  gettimeofday(&now, NULL);
  log_i("system time: %d.%d",now.tv_sec,now.tv_usec); 
  rSec = now.tv_sec + SECONDS_FROM_1900_TO_1970;
  packetBuffer[40] = (rSec >> 24) & 0xFF;
  packetBuffer[41] = (rSec >> 16) & 0xFF;
  packetBuffer[42] = (rSec >> 8) & 0xFF;
  packetBuffer[43] = rSec & 0xFF;
  packetBuffer[44] = (now.tv_usec >> 24) & 0xFF;
  packetBuffer[45] = (now.tv_usec >> 16) & 0xFF;
  packetBuffer[46] = (now.tv_usec >> 8) & 0xFF;
  packetBuffer[47] = now.tv_usec & 0xFF;
}

void ntpSrv (void * parameters) {
  WiFiUDP Udp;
  Udp.begin(NTP_PORT); // start udp server
  while(1) {
    delay(NTPD_DELAY);
    if (Udp.parsePacket()) { // we've got a packet 
        byte packetBuffer[NTP_PACKET_SIZE];
        Udp.read(packetBuffer, NTP_PACKET_SIZE);

        buildNTPpacket(packetBuffer);

        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, NTP_PACKET_SIZE);
        Udp.endPacket();
    }
  }
}

void timesync_cb(struct timeval *tv) {
  last_ntpsync.tv_sec = tv->tv_sec;
  last_ntpsync.tv_usec = tv->tv_usec;
  log_i("ntp sync at %d",last_ntpsync.tv_sec);
}

void setup() {
  Serial.begin(115200);
  delay(20);
  Serial.println();
  WiFi.begin("myssid","mypassword");
  if (!WiFi.waitForConnectResult()) {
    Serial.println("No WiFi network found!");
    ESP.restart();
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  delay(250);
  sntp_set_time_sync_notification_cb(timesync_cb);
  struct tm tm_now;
  configTime(0, 0, NTP_SRV); //UTC time
  if (!getLocalTime(&tm_now, NTP_TIMEOUT * 1000ULL)) {
    Serial.println("Unable to sync with NTP server");
    return;
  }
  Serial.println("NTP time updated");
  xTaskCreate(ntpSrv, "NTP daemon", 2048, NULL, tskIDLE_PRIORITY, NULL);
}

void loop() {
  delay(-1);
}
