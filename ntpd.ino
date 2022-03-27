#include <WiFi.h>
#include <esp_sntp.h>

#define NTP_SRV "pool.ntp.org"
#define NTP_TIMEOUT 30 //sec
#define NTP_SYNC_INTERVAL 3600UL //sec
#define NTP_PORT 123
#define NTPD_DELAY 100 //msec
#define NTP_PACKET_SIZE 48
#define NTP_STRATUM 2
#define SECONDS_FROM_1900_TO_1970  2208988800UL

timeval last_ntpsync;

void fillTimeval(byte* buff, uint32_t secs, uint32_t usecs, uint8_t startpt) {
  buff[startpt] = (secs >> 24) & 0xFF;
  buff[startpt+1] = (secs >> 16) & 0xFF;
  buff[startpt+2] = (secs >> 8) & 0xFF;
  buff[startpt+3] = secs & 0xFF;
  buff[startpt+4] = (usecs >> 24) & 0xFF;
  buff[startpt+5] = (usecs >> 16) & 0xFF;
  buff[startpt+6] = (usecs >> 8) & 0xFF;
  buff[startpt+7] = usecs & 0xFF;  
}

void buildNTPpacket(byte* packet)
{
  timeval now;
  gettimeofday(&now, NULL);
  
  packet[0] = 0b00011100; // LI: 0, version: 3, mode: 4
  packet[1] = NTP_STRATUM;  // stratum
  packet[2] = 4; // polling interval
  packet[3] = 0xF7; // clock precision
  
  packet[4] = 0; // root delay
  packet[5] = 0;
  packet[6] = 0;
  packet[7] = 0;
  
  packet[8] = 0; // root dispersion
  packet[9] = 0;
  packet[10] = 0;
  packet[11] = 0x50;
  
  packet[12] = 69; //E //time source (namestring)
  packet[13] = 83; //S
  packet[14] = 80; //P
  packet[15] = 0;
  
  //reference time is last sync
  uint32_t rSec = last_ntpsync.tv_sec + SECONDS_FROM_1900_TO_1970;
  fillTimeval(packet, rSec, last_ntpsync.tv_usec, 16);
  
  //copy client xmit time to originate
  for (int i = 24; i < 32; i++) packet[i] = packet[i+16];
  
  //put initial time into receive
  rSec = now.tv_sec + SECONDS_FROM_1900_TO_1970;
  fillTimeval(packet, rSec, now.tv_usec, 32);
  
  //put final time into xmit
  gettimeofday(&now, NULL);
  rSec = now.tv_sec + SECONDS_FROM_1900_TO_1970;
  fillTimeval(packet, rSec, now.tv_usec, 40);
  log_d("sent time: %d.%d",now.tv_sec,now.tv_usec); 
}

void ntpSrv (void * parameters) {
  WiFiUDP Udp;
  Udp.begin(NTP_PORT); // start udp listener
  while(1) {
    delay(NTPD_DELAY);
    if (Udp.parsePacket()) { // we've got a packet 
        byte packetBuffer[NTP_PACKET_SIZE];
        if (Udp.read(packetBuffer, NTP_PACKET_SIZE) == NTP_PACKET_SIZE) {
          log_d("NTP request from %d.%d.%d.%d", 
                 Udp.remoteIP()[0], Udp.remoteIP()[1], Udp.remoteIP()[2], Udp.remoteIP()[3]);
          buildNTPpacket(packetBuffer);
    
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
          Udp.write(packetBuffer, NTP_PACKET_SIZE);
          Udp.endPacket();
          Udp.flush(); // Only one customer per loop to avoid DoS or crosstalk
        }
    }
  }
}

void timesync_cb(struct timeval *tv) {
  last_ntpsync.tv_sec = tv->tv_sec;
  last_ntpsync.tv_usec = tv->tv_usec;
  log_d("ntp sync at %d",last_ntpsync.tv_sec);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  WiFi.begin();
  if (!WiFi.waitForConnectResult()) {
    Serial.println("No WiFi network found!");
    ESP.restart();
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  delay(250);
  sntp_set_sync_interval(NTP_SYNC_INTERVAL * 1000UL);
  sntp_set_time_sync_notification_cb(timesync_cb);
  struct tm tm_now;
  configTime(0, 0, NTP_SRV); //UTC time
  if (!getLocalTime(&tm_now, NTP_TIMEOUT * 1000UL)) {
    Serial.println("Unable to sync with NTP server");
    return;
  }
  xTaskCreate(ntpSrv, "NTP daemon", 2048, NULL, tskIDLE_PRIORITY, NULL);
}

void loop() {
  delay(-1);
}
