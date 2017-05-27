/*
 * esp8266_sm.h
 *
 * Created: Sunday 26. February 2017 - 14:01:14
 *  Author: Tom
 */ 
#ifndef ESP8266_SM_H_
#define ESP8266_SM_H_

#include <stdint.h>
#include "Client.h"

typedef void (*cbReceivedMsg)(uint8_t *data, uint8_t n, int16_t handle);

extern const Client_t espClientSock;

/******************************************************************************
 * API
 */
void    esp8266_Init        ();
int     esp8266_Connect     (const char *host, uint16_t port);
int     esp8266_Connect_noParam(void);
uint8_t esp8266_Connected   (void);
size_t  esp8266_Write       (const uint8_t *data, size_t length);
void    esp8266_Receive     (uint8_t *data, uint8_t length, int16_t handle);


#endif /* ESP8266_SM_H_ */
