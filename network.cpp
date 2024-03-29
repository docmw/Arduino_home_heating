#include "network.h"
#include <avr/wdt.h>
//SUPLA includes
//#include <SuplaDevice.h>

int temperature_sup;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};       // Set the ethernet shield mac address.
//uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 0, 150);
IPAddress myDns(192, 168, 0, 1);
IPAddress water_module (192, 168, 0, 211);   //IP address of water heater relay module
IPAddress light_module (192, 168, 0, 121); 
//Supla::EthernetShield ethernet(mac, ip);
EthernetServer server(8000);
//EthernetUDP Udp;
const char timeServer[] = "pl.pool.ntp.org"; // time.nist.gov NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE];
boolean debug = false;


VirtuinoCM virtuino;


double get_temperature(int channelNumber, double last_val) {

  return (((double)(temperature_sup)) / 10.0);
}

/*void sendNTPpacket(const char * address) {
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
  }*/

void Network_begin(void (*Receive_callback)(char, uint8_t, String), String (*Request_callback)(char, uint8_t)) {
  virtuino.begin(Receive_callback, Request_callback, 256);
  virtuino.key = "1234";
  Ethernet.begin(mac, ip, myDns);


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
  //SuplaDevice.iterate();
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

bool Set_water_heater(uint8_t state) {
  server.end();
  EthernetClient client;
  wdt_reset();
  if (client.connect(water_module, 8000)) {
    client.flush();
    client.print(F("GET /1234!V00="));
    client.print(state, DEC);
    client.println('$');
    client.flush();
    client.stop();
    Serial.println(F("Water set success"));
    server.begin();
    return true;
  }
  client.stop();
  Serial.println(F("Water set error"));
  server.begin();
  return false;
}

uint8_t Check_water_heater(uint8_t control_state, int temperature) {
  server.end();
  EthernetClient client;
  char read_buffer[30] = {0};
  uint8_t i = 0;
  uint8_t heater_status = 0;
  wdt_reset();
  if (client.connect(water_module, 8000)) {
    client.flush();
    client.print(F("GET /1234!V00=?$!V02="));
    client.print(control_state, DEC);
    client.print(F("$!V03="));
    client.print(temperature, DEC);
    client.println("$");
    
  }
  else {
    Serial.println(F("Cannot connect"));
    client.stop();
    server.begin();
    return 2;
  }
  unsigned long start_time = millis();
  wdt_reset();

  while (!client.available() && (millis() - start_time < CLIENT_TIMEOUT)) {
    delay(1);
    wdt_reset();
  }
  if ((millis() - start_time >= CLIENT_TIMEOUT)) {
    Serial.println("timeout");
    client.flush();
    client.stop();
    server.begin();
    return 2;
  }
  while (client.available() > 0 && i <= 28) {
    read_buffer[i] = client.read();         // read the incoming data
    i++;
  }
  read_buffer[i] = '\n';
  client.flush();
  client.stop();
  char* start_Pos = strchr(read_buffer, '!');
  while (start_Pos != NULL) {
    if ((strncmp((start_Pos + 1), "V0=", 3)) == 0) {
      heater_status = atoi(start_Pos + 4);
      server.begin();
      return heater_status;
    }
    else {
      start_Pos = strchr((start_Pos + 1), '!');
    }
  }
  Serial.println(F("Connected but no answer"));
  server.begin();
  return 2;

}

bool Set_night_light(uint8_t state) {
  server.end();
  EthernetClient client;
  wdt_reset();
  if (client.connect(light_module, 8000)) {
    client.flush();
    client.print(F("GET /1234!V30="));
    client.print(state, DEC);
    client.println('$');
    client.flush();
    client.stop();
    Serial.println(F("Light set success"));
    server.begin();
    return true;
  }
  client.stop();
  Serial.println(F("Light set error"));
  server.begin();
  return false;
}

uint8_t Check_light_status() {
  server.end();
  EthernetClient client;
  char read_buffer[30] = {0};
  uint8_t i = 0;
  uint8_t light_status = 0;
  wdt_reset();
  if (client.connect(light_module, 8000)) {
    client.flush();
    client.println(F("GET /1234!V30=?$"));    
  }
  else {
    Serial.println(F("Cannot connect"));
    client.stop();
    server.begin();
    return 3;
  }
  unsigned long start_time = millis();
  wdt_reset();

  while (!client.available() && (millis() - start_time < CLIENT_TIMEOUT)) {
    delay(1);
    wdt_reset();
  }
  if ((millis() - start_time >= CLIENT_TIMEOUT)) {
    Serial.println("timeout");
    client.flush();
    client.stop();
    server.begin();
    return 3;
  }
  while (client.available() > 0 && i <= 28) {
    read_buffer[i] = client.read();         // read the incoming data
    i++;
  }
  read_buffer[i] = '\0';
  client.flush();
  client.stop();
  char* start_Pos = strchr(read_buffer, '!');
  while (start_Pos != NULL) {
    if ((strncmp((start_Pos + 1), "V30=", 4)) == 0) {
      light_status = atoi(start_Pos + 5);
      server.begin();
      return light_status;
    }
    else {
      start_Pos = strchr((start_Pos + 1), '!');
    }
  }
  Serial.println(F("Connected but no answer"));
  server.begin();
  return 3;

}
