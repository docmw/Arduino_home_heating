#ifndef NETWORK_H
#define NETWORK_H


#include <SPI.h>
#include <UIPEthernet.h>
                  

#include "VirtuinoCM.h"
             
#define V_memory_count 32          // the size of V memory. You can change it to a number <=255)
#define P_memory_count 32          // the size of V memory. You can change it to a number <=255)
#define E_memory_count 32            // set this variable to false on the finale code to decrease the request time.

void Network_begin(void (*Receive_callback)(char,uint8_t,String),String (*Request_callback)(char,uint8_t));
void Virtuino_run();
void Virtuino_delay(int);



#endif
