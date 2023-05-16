#ifndef NETWORK_H
#define NETWORK_H


#include <SPI.h>
#include <EthernetENC.h>
/*#include <SuplaDevice.h>
#include <supla/control/relay.h>
#include <supla/condition.h>
#include <supla/sensor/thermometer.h>
#include <supla/network/ethernet_shield.h>*/

                  

#include "VirtuinoCM.h"
             
#define V_memory_count 32          // the size of V memory. You can change it to a number <=255)
#define P_memory_count 32          // the size of V memory. You can change it to a number <=255)
#define E_memory_count 32            // set this variable to false on the finale code to decrease the request time.
#define CLIENT_TIMEOUT 5000
void Network_begin(void (*Receive_callback)(char,uint8_t,String),String (*Request_callback)(char,uint8_t));
void Virtuino_run();
void Virtuino_delay(int);
bool Set_water_heater(uint8_t);
uint8_t Check_water_heater(uint8_t, int);
bool Set_night_light(uint8_t state);
uint8_t Check_light_status();



#endif
