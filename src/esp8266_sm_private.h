/*
 * esp8266_sm_private.h
 *
 * Created: Wednesday 08. March 2017 - 20:29:15
 *  Author: Tom
 */ 
#ifndef ESP8266_SM_PRIVATE_H_
#define ESP8266_SM_PRIVATE_H_

#include <stdio.h>
#include "fsm.h"

/******************************************************************************
 * STATEMACHINE EVENTS
 */
typedef enum
{
    esp8266_eStart = 0,
    esp8266_eFailed,
    esp8266_eTimeout,
    esp8266_eDisconnect,
    esp8266_eEspUp,
    esp8266_eConnect,
    esp8266_eSucceed
}esp8266_events_t;

/******************************************************************************
 * STATEMACHINE STATES
 */
typedef enum
{
    esp8266_eStateINIT = 0,
    esp8266_eStateInitESP,
    esp8266_eStateReadyNotConnected,
    esp8266_eStateWaitTillConnected,
    esp8266_eStateConnected,
    esp8266_eStateTransmitting,
}esp8266_states_t;

/******************************************************************************
 * STATEMACHINE GUARDS
 */
int8_t esp8266_ConnectReceived(void *p);
int8_t esp8266_MoreInit(void *p);

 /******************************************************************************
 * STATEMACHINE ACTIONS
 */
void esp8266_OpenConnection(void *p);
void esp8266_ResetESP(void *p);
void esp8266_ResetInit(void *p);
void esp8266_SendNextCmd(void *p);
void esp8266_SetConnected(void *p);
void esp8266_SetDisconnected(void *p);
void esp8266_SetReadyToSend(void *p);
 
 /******************************************************************************
 * STATEMACHINE TABLE
 */
static const FSMTransition_t esp8266_StateINIT[] = {
	{	esp8266_eStart, 	esp8266_eStateInitESP,
		{0, (FSM_fpGuard_t []){NULL} },
		{2, (FSM_fpAction_t[]){esp8266_SetDisconnected, esp8266_ResetESP} }
	},
};

static const FSMTransition_t esp8266_StateInitESP[] = {
	{	esp8266_eEspUp, 	esp8266_eStateInitESP,
		{0, (FSM_fpGuard_t []){NULL} },
		{2, (FSM_fpAction_t[]){esp8266_ResetInit, esp8266_SendNextCmd} }
	},
	{	esp8266_eSucceed, 	esp8266_eStateInitESP,
		{1, (FSM_fpGuard_t []){esp8266_MoreInit} },
		{1, (FSM_fpAction_t[]){esp8266_SendNextCmd} }
	},
	{	esp8266_eSucceed, 	esp8266_eStateWaitTillConnected,
		{1, (FSM_fpGuard_t []){esp8266_ConnectReceived} },
		{1, (FSM_fpAction_t[]){esp8266_OpenConnection} }
	},
	{	esp8266_eSucceed, 	esp8266_eStateReadyNotConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{0, (FSM_fpAction_t[]){NULL} }
	},
	{	esp8266_eFailed, 	esp8266_eStateInitESP,
		{0, (FSM_fpGuard_t []){NULL} },
		{2, (FSM_fpAction_t[]){esp8266_ResetInit, esp8266_SendNextCmd} }
	},
	{	esp8266_eTimeout, 	esp8266_eStateInitESP,
		{0, (FSM_fpGuard_t []){NULL} },
		{2, (FSM_fpAction_t[]){esp8266_ResetInit, esp8266_SendNextCmd} }
	},
};

static const FSMTransition_t esp8266_StateReadyNotConnected[] = {
	{	esp8266_eConnect, 	esp8266_eStateWaitTillConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{1, (FSM_fpAction_t[]){esp8266_OpenConnection} }
	},
};

static const FSMTransition_t esp8266_StateWaitTillConnected[] = {
	{	esp8266_eSucceed, 	esp8266_eStateConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{1, (FSM_fpAction_t[]){esp8266_SetConnected} }
	},
	{	esp8266_eFailed, 	esp8266_eStateReadyNotConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{0, (FSM_fpAction_t[]){NULL} }
	},
};

static const FSMTransition_t esp8266_StateConnected[] = {
	{	esp8266_eDisconnect, 	esp8266_eStateReadyNotConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{0, (FSM_fpAction_t[]){NULL} }
	},
	{	esp8266_eSucceed, 	esp8266_eStateConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{1, (FSM_fpAction_t[]){esp8266_SetReadyToSend} }
	},
	{	esp8266_eFailed, 	esp8266_eStateConnected,
		{0, (FSM_fpGuard_t []){NULL} },
		{1, (FSM_fpAction_t[]){esp8266_SetReadyToSend} }
	},
};

static const FSMTransition_t esp8266_StateTransmitting[] = {
};



/******************************************************************************
 * STATEMACHINE TABLE
 */
static const FSMState_t esp8266_table[] = { FSM_ADDSTATE(esp8266_StateINIT), FSM_ADDSTATE(esp8266_StateInitESP), FSM_ADDSTATE(esp8266_StateReadyNotConnected), FSM_ADDSTATE(esp8266_StateWaitTillConnected), FSM_ADDSTATE(esp8266_StateConnected), FSM_ADDSTATE(esp8266_StateTransmitting) };
static FSMTable_t esp8266_tableData = {esp8266_eStateINIT, FSM_ADDTABLE(esp8266_table)};

/******************************************************************************
 * PRIVATE VARIABLE
 */
static tableIdx_t esp8266_tableIdx;

/******************************************************************************
 * FUNCTIONS
 */
static inline void esp8266_statemachine_init(void)
{
    esp8266_tableIdx = FSM_Add(&esp8266_tableData, NULL /*void *pData*/);
}

static inline void esp8266_Event(esp8266_events_t event)
{
    FSM_AddEvent(esp8266_tableIdx, event);
}

#endif /* ESP8266_SM_PRIVATE_H_ */