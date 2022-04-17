#include "network.h"
#include <avr/wdt.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};       // Set the ethernet shield mac address.
IPAddress ip(192, 168, 0, 150);

EthernetServer server(8000);
EthernetUDP Udp;
const char timeServer[] = "pl.pool.ntp.org"; // time.nist.gov NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE];
boolean debug = false;


VirtuinoCM virtuino;



void sendNTPpacket(const char * address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void Network_begin(void (*Receive_callback)(char,uint8_t,String),String (*Request_callback)(char,uint8_t)) {
  virtuino.begin(Receive_callback, Request_callback, 256);
  virtuino.key = "1234";
  Ethernet.begin(mac, ip);
  delay(1000);
  /*if (Ethernet.linkStatus() == 1) {

    Udp.begin(8888);
    sendNTPpacket(timeServer);
    // wait to see if a reply is available
    delay(1000);
    if (Udp.parsePacket()) {
      // We've received a packet, read the data from it
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      // the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, extract the two words:

      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Seconds since Jan 1 1900 = ");
      Serial.println(secsSince1900);

      // now convert NTP time into everyday time:
      Serial.print("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      // print Unix time:
      Serial.println(epoch);


      // print the hour, minute and second:
      Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
      Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
      Serial.print(':');
      if (((epoch % 3600) / 60) < 10) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
      Serial.print(':');
      if ((epoch % 60) < 10) {
        // In the first 10 seconds of each minute, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.println(epoch % 60); // print the second
    }
    // wait ten seconds before asking for the time again
    Ethernet.maintain();

  }*/
  server.begin();
}




void Virtuino_run() {
  unsigned long client_read_time = 0;
  EthernetClient client = server.available();
  if (client) {
    if (debug) Serial.println("Connected");
    virtuino.readBuffer = "";           // clear Virtuino input buffer. The inputBuffer stores the incoming characters
    if (client.connected()) {
      client_read_time = millis();
      while (client.available() > 0) {
        wdt_reset();
        char c = client.read();         // read the incoming data
        virtuino.readBuffer += c;       // add the incoming character to Virtuino input buffer
        if (debug) Serial.write(c);
        if ((c == '\n') || ((millis() - client_read_time) > 4000) ) {
          virtuino.readBuffer += '\0';
          break;
        }
       

      }
      client.flush();
      virtuino.getResponse();    // get the text that has to be sent to Virtuino as reply. The library will check the inptuBuffer and it will create the response text

      if (debug) Serial.println("\nResponse : " + virtuino.writeBuffer);
      delay(10);
      client.flush();
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println(virtuino.writeBuffer);
      client.stop();
    }
    if (debug) Serial.println("Disconnected");
  }
}
//============================================================== vDelay
void Virtuino_delay(int delayInMillis) {
  long t = millis() + delayInMillis;
  while (millis() < t) Virtuino_run();
}
