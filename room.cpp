#include "room.h"
#include "Arduino.h"

Room::Room(uint8_t _hpin, uint16_t _eeprom_adr, float _temp_day, float _temp_night, uint8_t _pin, uint8_t _type, bool _trigger = false, bool _cb = true, int8_t _correction = 0, uint8_t _hys = 4) {
  correction = _correction;
  heated = true;
  temp_read = 225;
  uint8_t port = digitalPinToPort(_hpin);
  heater_port = portOutputRegister(port);
  heater_bit = digitalPinToBitMask(_hpin);
  Heater_set();
  pinMode(_hpin, OUTPUT);
  type = _type;
  eeprom_adr = _eeprom_adr;
  trigger = _trigger;
  control_boiler = _cb;
  sensor_ready = false;
  pin = _pin;
  Sensor_init(pin);
  is_set = 0;
  Set_temp(DAY, _temp_day);
  Set_temp(NIGHT, _temp_night);  
  is_set = 1;
  forced = false;
  hysteresis = _hys;
  last_time_update = 0 - UPDATE_INTERVAL;
  reading_idx = 0;
  reading_buf = 0;
}



Room::~Room() {
  if (dht != NULL) {
    delete dht;
  }
  if (sensor != NULL) {
    delete sensor;
    delete bus;
  }
}

bool Room::Room_init() {
  Sensor_init(pin);
  return sensor_ready;
}

void Room::Sensor_init(uint8_t pin) {
  if (type == DHTSENS) {
    dht_pin = pin;
    dht = new DHT(pin, DHT22);
    dht->begin();
    sensor_ready = true;
    DEBUG_PRINTLN("DHT started");
  }
  else if (type == DS18B20SENS) {
    bus = new OneWire(pin);
    if (Search_address()) {
      sensor = new DS18B20(bus);
      sensor->begin();
      sensor_ready = true;
      DEBUG_PRINTLN("DS18B20 configured");
    }
    else {
      error_cnt++;
      DEBUG_PRINTLN("DS18B20 error");
      if (Search_address()) {
        sensor = new DS18B20(bus);
        sensor->begin();
        sensor_ready = true;
      }
    }
    if (address[0] != 0x28) {
      sensor_ready = false;
    }
  }
}

void Room::Set_temp(uint8_t _type, int _temp_set) {
  uint8_t dn;
  if (_type == 0) {
    temp_ext = _temp_set;
    last_time_update = millis();
    DEBUG_PRINT("ext temp updated\n\r");
  }
  else {
    dn = _type - 1;
    if (is_set > 0) {
      if (_temp_set != temp_set[dn]) {
        temp_set[dn] = _temp_set;
        EEPROM.put(eeprom_adr + (dn * 2), temp_set[dn]);
      }
    }
    else {
      EEPROM.get(eeprom_adr + (dn * 2), temp_set[dn]);
      if ((temp_set[dn] < 100) ||  (temp_set[dn] > 400)) {
        temp_set[dn] = _temp_set;
      }
    }
  }
}

int Room::Read_temp(uint8_t _temp_index) {
  switch (_temp_index) {
    case 0:
      return temp_read;
      break;
    case 1:
      return temp_set[0];
      break;
    case 2:
      return temp_set[1];
      break;
    default:
      return 0;
      break;
  }
}

int Room::Read_hum() {
  if (type == DHTSENS) {
    return (int)dht->readHumidity();
  }
  else {
    return 0;
  }
}

void Room::Request_sensor() {
  if (type == DS18B20SENS) {
    if (sensor_ready == true) {
      sensor->request(address);
    }
    else {
      temp_read = 220;
      Sensor_init(pin);
      if (sensor_ready == true) {
        sensor->request(address);
      }
    }
  }
}

void Room::Read_sensor() {
  int16_t temp_read_old = temp_read;
  int16_t temp_sensor;
  if (type == DS18B20SENS) {
    if (sensor->available()) {
      temp_sensor = (int16_t)(sensor->readTemperature(address) * 10);

      DEBUG_PRINT("temperature ds:");
#ifdef DEBUG
      for (int i = 0; i < 8; i++) {
        DEBUG_PRINT("0x");
        DEBUG_PRINTHEX(address[i]);
        DEBUG_PRINT(":");
      }

#endif
      DEBUG_PRINTF(((float)temp_sensor) / 10);
      DEBUG_PRINTLN();
    }
  }
  else if (type == DHTSENS) {
    float readDHT = dht->readTemperature();
    float readDHTH = dht->readHumidity();
    if (!isnan(readDHT)) {
      temp_sensor = (int16_t)(readDHT * 10);
      DEBUG_PRINT("Temperature dht:");
      DEBUG_PRINTF(((float)temp_read) / 10);
      DEBUG_PRINTLN();
      DEBUG_PRINT("humidity dht:");
      DEBUG_PRINTF(readDHTH);
      DEBUG_PRINTLN();
      DHT_ok = true;
    }
    else {
      DHT_ok = false;
      DEBUG_PRINTLN("DHT error");
      error_cnt++;
      return;
    }
  }
  else if (type == LM35SENS) {
    uint32_t m_voltage = (analogRead(dht_pin) * 4888) / 1000;
    temp_sensor = (uint16_t)m_voltage;

  }
  else {
    return;
  }
  if (temp_sensor < -700 || temp_sensor == 0) {
    temp_sensor = temp_read_old;
    error_cnt++;
    DEBUG_PRINT("error temp:");
    DEBUG_PRINTDEC(error_cnt);
    DEBUG_PRINTLN();
    DEBUG_PRINTDEC(temp_read);
    DEBUG_PRINTLN();
    return;
  }
  temp_sensor += (int16_t)correction;
  reading_buf += temp_sensor;
  if(is_set == 1) {
    temp_read = temp_sensor;
    is_set = 2;
  }
  reading_idx++;
  if (reading_idx == 10) {
    temp_read = reading_buf / 10;
    if (reading_buf % 10 >= 5) {
      temp_read = temp_read + 1;
    }
    reading_idx = 0;
    reading_buf = 0;
    DEBUG_PRINT("Update temp:");
    DEBUG_PRINTDEC(temp_read);
    DEBUG_PRINTLN();
  }

}

void Room::Control_temp(uint8_t _dn) {
  int16_t control_temp;
  unsigned long current_time = millis();
  if (current_time - last_time_update < UPDATE_INTERVAL) {
    DEBUG_PRINT("control using temp_ext\n\r");
    control_temp = max(temp_ext, temp_read);
  }
  else {
    control_temp = temp_read;
    DEBUG_PRINT("control using sensor\n\r");
    last_time_update = current_time - UPDATE_INTERVAL;
  }
  int16_t temp_delta = temp_set[_dn] - control_temp;
  DEBUG_PRINTF(((float)temp_set[_dn]) / 10);
  DEBUG_PRINTLN();
  if (temp_delta >= 1 && heated) {
    heated = false;

    DEBUG_PRINTLN("not heated");
  }
  else if ((temp_delta <= ((-1) * (hysteresis))) && !heated) {
    heated = true;
    forced = false;
    DEBUG_PRINT("heated\n\r");
  }
  Heater_set();

}

void Room::Heater_off() {
  if (!trigger) HEATER_SET;
  else HEATER_RESET;
}

bool Room::Is_heated() {
  if (control_boiler == true | forced == true) return heated;
  else return true;
}

void Room::Heater_set () {
  if (heated == false) {
    if (!trigger) HEATER_RESET;
    else HEATER_SET;
    //delay(1000);
  }
  else if (heated == true) {
    forced = false;
    if (!trigger) {
      HEATER_SET;
    }
    else {
      HEATER_RESET;
    }

  }
}

bool Room::Search_address() {
  uint32_t beginResetTimeout = millis();
  uint32_t elapsedResetTimeout;
  DEBUG_PRINT("searching\n\r");
  /*while (bus->reset())
    {
    elapsedResetTimeout = millis() - beginResetTimeout;
    DEBUG_PRINT("BUS RESET");
    DEBUG_PRINTLN(millis());
    if (elapsedResetTimeout > 1000)
      return false;
    }*/
  bus->reset_search();
  beginResetTimeout = millis();
  while (bus->search(address))
  {
    DEBUG_PRINT("BUS ADDRESS\n\r");
    if (address[0] != 0x28)
      continue;

    if (OneWire::crc8(address, 7) != address[7])
    {
      return false;
    }

  }
  return true;
}

uint8_t Room::Error_count() {
  return error_cnt;
}

bool Room::Force_heating(uint8_t _dn) {
  int16_t control_temp;
  unsigned long current_time = millis();
  if (current_time - last_time_update < UPDATE_INTERVAL) {
    DEBUG_PRINT("force using temp_ext:\n\r");
    control_temp = max(temp_ext, temp_read);
  }
  else {
    control_temp = temp_read;
    DEBUG_PRINT("force using sensor:\n\r");

  }
  int16_t temp_delta = temp_set[_dn] - control_temp;
  if ((temp_delta >= ((-1)*1))) {
    heated = false;
    forced = true;
    return true;
  }
  return false;
}
