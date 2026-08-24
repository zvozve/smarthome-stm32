#ifndef __DHT11_H
#define __DHT11_H

#include "stdint.h"

typedef struct
{
    uint8_t hum_int;
    uint8_t hum_dec;
    uint8_t temp_int;
    uint8_t temp_dec;
    uint8_t checksum;
    uint8_t is_valid;
} DHT11_Data_t;

void DHT11_Init(void);
uint8_t DHT11_Read(DHT11_Data_t *pData);

#endif