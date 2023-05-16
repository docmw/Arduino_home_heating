#ifndef ROOM_H
#define ROOM_H
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS18B20.h>
#include <DHT.h>
#include <EEPROM.h>

#define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(x)     Serial.print(F(x))
  #define DEBUG_PRINTDEC(x)  Serial.print(x, DEC)
  #define DEBUG_PRINTHEX(x)  Serial.print(x, HEX)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
  #define DEBUG_PRINTF(x)   Serial.print(x)
  
#else
   #define DEBUG_PRINT(x)     
  #define DEBUG_PRINTDEC(x)  
  #define DEBUG_PRINTHEX(x)  
  #define DEBUG_PRINTLN(x)  
  #define DEBUG_PRINTF(x)   
#endif

#define DAY 1
#define NIGHT 2
#define DHTSENS 0
#define DS18B20SENS 1
#define LM35SENS 2
#define NOSENS 3
#define TEMPERATURE_PRECISION 11
#define UPDATE_INTERVAL 660000

#define HEATER_SET (*heater_port) |= heater_bit
#define HEATER_RESET (*heater_port) &= ~heater_bit

class Room
{
  public:
  
  Room(uint8_t hpin, uint16_t _eeprom_adr, float _temp_day, float _temp_night, uint8_t _pin, uint8_t _type, bool _trigger = false, bool _cb = true, int8_t _correction = 0, uint8_t _hys = 4);
  ~Room();
  
  bool Room_init();
  void Set_temp(uint8_t _type, int _temp_set);
  int Read_temp(uint8_t);
  int Read_hum();
  void Request_sensor();
  void Read_sensor();
  void Control_temp(uint8_t _hr);
  void Heater_off();
  void Is_bathroom(uint8_t _apin);
  bool Is_heated();
  uint8_t Error_count();
  bool Force_heating(uint8_t _dn);
  uint32_t schedule;
  
  private:
  uint8_t type;
  OneWire *bus = NULL;
  DS18B20 *sensor = NULL;
  byte address[8];
  //DallasTemperature *sensor;
  //DeviceAddress dev_adr;
  uint16_t eeprom_adr;
  uint8_t dht_pin;
  volatile uint8_t* heater_port;
  uint8_t heater_bit;
  int16_t temp_set[2];
  int16_t temp_read;
  int16_t temp_ext;
  int8_t correction;
  uint8_t error_cnt;
  uint8_t is_set;
  bool DHT_ok;
  bool heated;
  DHT* dht = NULL;
  void Heater_set();
  bool Search_address();
  bool trigger;
  bool control_boiler;
  bool sensor_ready;
  void Sensor_init(uint8_t);
  uint8_t pin;
  bool forced;
  int8_t hysteresis;
  unsigned long last_time_update;
  int16_t reading_buf;
  uint8_t reading_idx;
  
  
  
  
  
};
#endif
