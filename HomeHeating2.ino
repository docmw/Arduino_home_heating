//Rooms includes

#include <avr/wdt.h>
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print(F(x))
#define DEBUG_PRINTDEC(x)  Serial.print(x, DEC)
#define DEBUG_PRINTLN(x)   Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#endif

#include "room.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 8
#define TEMPERATURE_PRECISION 11
#define BOILER_PIN A5
#define HEATER_POWER 3
#define BOILER_RT 32
#define RELAY_6 31
#define RELAY_4 33
#define RELAY_3 34
#define RELAY_2 35
#define RELAY_1 36
#define DS_VCC A6
#define DS_GND A7

#define BOILER_EPR_ADR 100

//RTC includes
#include <Wire.h>
#include "RTClib.h"


uint8_t conversion_status = 0;


Room kidroom (4, 0, 215, 210, 47, 1, false, false, 0, 3);
Room office (5, 10, 215, 210, 49, 1, false, false, 8, 4);
Room bathroom (34, 20, 215, 210, 40, 3, false, false);
Room bedroom (7, 30, 215, 210, 46, 1, false, true, (-8), 5);
Room door (30, 40, 215, 210, 48, 1, true, false);
Room salon (29, 50, 215, 210, 28, 1, true, true, (-9));
Room roof (37, 60, 215, 210, 41, 1, false, false);
Room workshop (6, 70, 215, 210, 26, 1, false, false, 4);
Room water (RELAY_6, 80, 280, 500, A2, 1, false, false, 0, 5);
Room* rooms[] = {&kidroom, &office, &bathroom, &bedroom, &door, &salon, &roof, &workshop, &water};
uint8_t rooms_size = sizeof(rooms) / sizeof(rooms[0]);
uint8_t tod = 0; //time of day, day = 0, night = 1,
bool heating = false;
bool heating_started = false;
enum heating_status {off, auto_h, day_h, night_h};


#include "network.h"


//RTC variables
DateTime now;
DS3231 rtc;

//Timers
unsigned long last_sens_read = 0;
unsigned long sensors_request_time = 0;
unsigned long last_sec_inc = 0;


void Temperature_control() {
  unsigned long current_time = millis();
  if ((current_time - last_sens_read) >= 10000) {
    switch (conversion_status) {
      case 0:
        Request_sensors();
        conversion_status = 1;
        sensors_request_time = current_time;
        break;
      case 1:
        if (Conversion_Complete()) {
          conversion_status = 2;
        }
        break;
      case 2:
        last_sens_read = millis();
        Read_sensors();
        Heaters_control();
        conversion_status = 0;
        //Temperature_copy();

        DEBUG_PRINT("Time:");
        DEBUG_PRINTDEC(now.hour());
        DEBUG_PRINT(":");
        if (now.minute() < 10) DEBUG_PRINT("0");
        DEBUG_PRINTDEC(now.minute());
        DEBUG_PRINTLN();
        Boiler_control();
        break;
    }
  }

}
void Rooms_init() {
  for (uint8_t i = 0; i < rooms_size; i++) {
    if (rooms[i]->Room_init()) {
      DEBUG_PRINT("Room ");
      DEBUG_PRINTDEC(i);
      DEBUG_PRINTLN(" initialized");
      wdt_reset();
    }
  }
}

void Request_sensors() {
  for (uint8_t i = 0; i < rooms_size; i++) {
    DEBUG_PRINT("Requesting sensor room  ");
    DEBUG_PRINTDEC(i);
    DEBUG_PRINTLN("");
    rooms[i]->Request_sensor();
    wdt_reset();
  }
}

bool Conversion_Complete() {
  unsigned long current_time = millis();
  if ((current_time - sensors_request_time) >= 1000) {
    return true;
  }
  return false;
}

void Read_sensors() {
  for (uint8_t i = 0; i < rooms_size; i++) {
    DEBUG_PRINT("Reading sensor room  ");
    DEBUG_PRINTDEC(i);
    DEBUG_PRINTLN("");
    rooms[i]->Read_sensor();
    wdt_reset();
  }
}

void Heaters_control() {
  for (uint8_t i = 0; i < rooms_size; i++) {
    rooms[i]->Control_temp(tod);
    wdt_reset();
  }
}


void Heaters_off() {
  for (uint8_t i = 0; i < rooms_size; i++) {
    rooms[i]->Heater_off();
  }
}

void Boiler_control() {
  bool heated = true;
  for (uint8_t i = 0; i < rooms_size; i++) {
    if (!(rooms[i]->Is_heated())) {
      if (!heating_started) {
        heating_started = true;
        EEPROM.update(BOILER_EPR_ADR, 1);
        DEBUG_PRINTLN(F("Heating started"));
        if (bedroom.Force_heating(tod)) DEBUG_PRINTLN(F("Bedroom heating forced"));
        if (workshop.Force_heating(tod)) DEBUG_PRINTLN(F("workshop heating forced"));
        if (kidroom.Force_heating(tod)) DEBUG_PRINTLN(F("kidroom heating forced"));
        if (office.Force_heating(tod)) DEBUG_PRINTLN(F("office heating forced"));
      }
      heated = false;
      break;
    }
    heated = true;

  }
  if (!heated) {
    digitalWrite(HEATER_POWER, HIGH);
    digitalWrite(BOILER_RT, HIGH);
  }
  else {
    digitalWrite(HEATER_POWER, LOW);
    digitalWrite(BOILER_RT, LOW);
    if (heating_started) {
      EEPROM.update(BOILER_EPR_ADR, 0);
      heating_started = false;
      DEBUG_PRINTLN(F("Heating stoped"));
    }
  }
}


void Eth_Received(char variableType, uint8_t variableIndex, String valueAsText) {

  int value = valueAsText.toInt();
  uint8_t room_index;
  uint8_t temp_index;
  now = rtc.now();
  uint8_t h = now.hour();
  uint8_t m = now.minute();
  uint8_t d = now.day();
  uint8_t mnt = now.month();
  uint16_t y = now.year();
  if (variableType == 'V') {
    if ((variableIndex < (rooms_size * 3 + 1)) && variableIndex > 0) {
      room_index = (variableIndex - 1) / 3;
      temp_index = (variableIndex - 1) % 3;
      rooms[room_index]->Set_temp(temp_index, (int)value);
      DEBUG_PRINT("update temperature - room ");
      DEBUG_PRINTDEC(room_index);
      DEBUG_PRINT(",");
      DEBUG_PRINTDEC(temp_index);
      DEBUG_PRINT(":");
      DEBUG_PRINTDEC(value);
    }
  }
  if (variableType == 'P') {
    switch (variableIndex) {
      case 1:
        h = value;
        break;
      case 2:
        m = value;
        break;
      case 3:
        d = value;
        break;
      case 4:
        mnt = value;
        break;
      case 5:
        y = value;
        break;
      default:
        break;
    }
    rtc.adjust(DateTime(y, mnt, d, h, m, 0));
  }


}

String Eth_Requested(char variableType, uint8_t variableIndex) {
  static bool reset_log = false;
  int value;
  uint8_t room_index;
  uint8_t temp_index;
  if (variableType == 'V') {
    if ((variableIndex < (rooms_size * 3 + 1)) && variableIndex > 0) {
      room_index = (variableIndex - 1) / 3;
      temp_index = (variableIndex - 1) % 3;
      value = rooms[room_index]->Read_temp(temp_index);
      return  String(value);
    }
  }
  if (variableType == 'P') {
    switch (variableIndex) {
      case 1:
        value = (int)now.hour();
        break;
      case 2:
        value = (int)now.minute();
        break;
      case 3:
        value = (int)now.day();
        break;
      case 4:
        value = (int)now.month();
        break;
      case 5:
        value = (int)now.year();
        break;
      case 6:
        ;
        if (reset_log == false) {
          value = 1;
          reset_log = true;
        }
        else {
          value = 0;
        }
        break;
      default:
        value = 0;
        break;
    }
    return String(value);
  }
  return "";
}

void setup()
{
  wdt_enable(WDTO_8S);
  pinMode(DS_VCC, OUTPUT);
  digitalWrite(DS_VCC, HIGH);
  pinMode(DS_GND, OUTPUT);
  digitalWrite(DS_GND, LOW);
  delay(100);
  Serial.begin(115200);
  DEBUG_PRINT("start setup\n\r");
  Wire.begin();
  rtc.begin();
  Rooms_init();
  pinMode(BOILER_PIN, INPUT_PULLUP);
  pinMode(HEATER_POWER, OUTPUT);
  pinMode(BOILER_RT, OUTPUT);
  uint8_t epr_boiler;
  epr_boiler = EEPROM.read(BOILER_EPR_ADR);
  if (epr_boiler > 0) {
    digitalWrite(HEATER_POWER, HIGH);
    digitalWrite(BOILER_RT, HIGH);
    DEBUG_PRINTLN("Boiler start on");
  }
  else {
    digitalWrite(HEATER_POWER, LOW);
    digitalWrite(BOILER_RT, LOW);
    DEBUG_PRINTLN("Boiler start off");
  }

  //inMode(RELAY_6, OUTPUT);
  pinMode(RELAY_4, OUTPUT);
  //digitalWrite(RELAY_6, HIGH);
  digitalWrite(RELAY_4, HIGH);
  pinMode(A8, OUTPUT);
  digitalWrite(A8, HIGH);
  Network_begin(Eth_Received, Eth_Requested);
  DEBUG_PRINT("setup end\n\r");

}


void loop() {
  wdt_reset();
  unsigned long current_time = millis();
  if ((current_time - last_sec_inc) >= 1000) {
    last_sec_inc = millis();

    now = rtc.now();
    if (now.hour() >= 23 || now.hour() < 6) {
      tod = 1;
    }
    else {
      tod = 0;
    }
    Temperature_control();

  }
  Virtuino_run();


}
