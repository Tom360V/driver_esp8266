/*
 * esp8266_sm.c
 *
 * Created: Sunday 26. February 2017 - 14:01:14
 *  Author: Tom
 */ 
#include <stdint.h>
#include "fsm.h"
#include "uart.h"
#include "esp8266_sm.h"
#include "esp8266_sm_private.h"
#include "util/atomic.h"
#define SINGLE_CONNECTION_MODE (1)

//static char myName[] = "esp8266";
//static char serverIp[] = "192.168.0.104";

/* * * * CONFIGURE ESP8266 * * *
 * You can do this once using ftdi cable and some terminal
 * The ESP8266 will remember his last used accesspoint
 * AT+UART_DEF=19200,8,1,0,0        //Change uart settings
 * AT+RST
 * AT+CWMODE=1                      //Set in 'station' mode
 * AT+CWJAP="[SSID]","[password]"   //Connect to accesspoint
 * AT+CIFSR                         //Check your ip-, and mac-address
 */

const uint8_t msg_AT[]                  = "AT";
const uint8_t msg_RESET[]               = "AT+RST";       //Reset
#ifdef SINGLE_CONNECTION_MODE
    const uint8_t msg_ConnectToServer[] = "AT+CIPSTART=\"TCP\",\"192.168.1.50\",1883";
    const uint8_t msg_CIPSEND[]         = "AT+CIPSEND=";
    const uint8_t msg_CIPMODE[]         = "AT+CIPMODE=0";   //0: normal mode, 1:unvarnished(?)
    const uint8_t msg_CIPMUX[]          = "AT+CIPMUX=0";    //Single Connection
#else
    const uint8_t msg_ConnectToServer[] = "AT+CIPSTART=0,\"TCP\",\"192.168.1.50\",1883";
    const uint8_t msg_CIPSEND[]         = "AT+CIPSEND=0,";
    const uint8_t msg_CIPMODE[]         = "AT+CIPMODE=0";   //0: normal mode, 1:unvarnished(?)
    const uint8_t msg_CIPMUX[]          = "AT+CIPMUX=1";    //Allow multiple connections
#endif
const uint8_t endOfMsg[2]               = "\r\n";

const uint8_t msg_ATE0[]                = "ATE0";           //Echo off
const uint8_t msg_CWMODE[]              = "AT+CWMODE=1";    //Set Station-mode, node can connect to an accesspoint

#define NOF_INIT_CMDS   (4)

uint8_t ConnectReceived = 0;

static cbReceivedMsg callBackReceive;

static int8_t   esp8266_WriteCommand(uint8_t *cmd, int16_t handle);
static int      esp8266_available(void);
static void     esp8266_Stop(void);
static int      esp8266_readbyte(void);
static void esp8266_reset(void);

const Client_t espClientSock =
{
    NULL, //&clientSocket_connectIP,
    &esp8266_Connect,
    &esp8266_Connected,
    NULL, //&clientSocket_write,
    &esp8266_Write,
    &esp8266_available,
    &esp8266_readbyte,
    NULL, //&clientSocket_readMulti,
    NULL, //&clientSocket_peek,
    NULL, //&clientSocket_flush,
    &esp8266_Stop
};


static void doSleep(void)
{
    uint16_t sleep;
    for(sleep=0; sleep<64000; sleep++)
    {
        asm("nop");
        asm("nop");
    }
}

static int8_t esp8266_WriteCommand(uint8_t *cmd, int16_t handle)
{
    UART_WriteStr((uint8_t*)cmd, (uint8_t)strlen((char*)cmd)    ,handle);
    UART_WriteStr((uint8_t*)endOfMsg ,2               ,handle);
    doSleep();
    return 0;
}

static void esp8266_Stop(void)
{
    
}


/******************************************************************************
 * LOCAL VARIABLE AND STRUCTS
 */
static int8_t idx_sendCommand;
static int8_t esp8266_connected = 0;
static int8_t esp8266_readyToSend = 0;

static void esp_reverseStr(uint8_t *str, uint8_t cnt)
{
    uint8_t idx;
    uint8_t temp;
    for(idx=0; idx<cnt; idx++)
    {
        cnt--;
        temp= str[idx];
        str[idx] = str[cnt];
        str[cnt] = temp;
    }
}

static uint8_t esp_dec2str(uint8_t value, size_t *str)
{
    uint8_t *p = str;
    uint8_t cnt=0;
    do
    {
        *p = value%10 + '0';
        value/=10;
        p++;
        cnt++;
    } while(value>0);
    *p = '\0';
    esp_reverseStr(str, cnt);
    return cnt;
}

/*******************************************************************************
 * GUARD
 */
int8_t esp8266_ConnectReceived(void *p)
{
    (void)p;
    return (ConnectReceived==1)?FSM_TRUE:FSM_FALSE;
}

/*******************************************************************************
 * GUARD
 */
int8_t esp8266_MoreInit(void *p)
{
    (void)p;
    if(idx_sendCommand < NOF_INIT_CMDS)
    {
        return FSM_TRUE;
    }
    return FSM_FALSE;
}

/*******************************************************************************
 * ACTION
 */
void esp8266_OpenConnection(void *p)
{
    (void)p;
    esp8266_WriteCommand((uint8_t *)msg_ConnectToServer,     0);
}

/*******************************************************************************
 * ACTION
 */
void esp8266_ResetESP(void *p)
{
    (void)p;

    //Send AT command before we send the RESET command
    // Possible during startup of the ATmage, the TX pin was in a undefined state and
    // could cause some garbage on the line.
    // To make a defined start, send a command which may fail before we send the RESET
    // This to make sure the REST is currently executed!
    
    esp8266_WriteCommand((uint8_t *)msg_AT,    0);  //May fail!  :)
    
    esp8266_WriteCommand((uint8_t *)msg_RESET,    0);   // May not fail!!
    
    UART_Flush();
    esp8266_reset();
}

/*******************************************************************************
 * ACTION
 */
void esp8266_ResetInit(void *p)
{
    (void)p;
    idx_sendCommand = 0;
}

/*******************************************************************************
 * ACTION
 */
void esp8266_SendNextCmd(void *p)
{
    (void)p;

    switch(idx_sendCommand)
    {
        case 0: esp8266_WriteCommand((uint8_t *)msg_ATE0,      0);  break;
        case 1: esp8266_WriteCommand((uint8_t *)msg_CWMODE,    0);  break;
        case 2: esp8266_WriteCommand((uint8_t *)msg_CIPMODE,   0);  break;
        case 3: esp8266_WriteCommand((uint8_t *)msg_CIPMUX,    0);  break;
    }
    idx_sendCommand++;
}

/*******************************************************************************
 * ACTION
 */
void esp8266_SetConnected(void *p)
{
    (void)p;
    esp8266_connected = 1;
}

/*******************************************************************************
 * ACTION
 */
void esp8266_SetDisconnected(void *p)
{
    (void)p;
    esp8266_connected = 0;
}

/*******************************************************************************
 * ACTION
 */
void esp8266_SetReadyToSend(void *p)
{
    (void)p;
    esp8266_readyToSend = 1;
}


//To protocol / communication layer!
static inline void callbackReceivedMessage(uint8_t *data, uint8_t n, int16_t handle)
{
    if( 0 != callBackReceive)
    {
        (callBackReceive)(data, n, handle);
    }
}


/******************************************************************************
 * API
 */
void esp8266_Init()
{
    ConnectReceived = 0;
    esp8266_statemachine_init();
    esp8266_Event(esp8266_eStart);    //Send FirstReset
}

size_t  esp8266_Write(const uint8_t *data, size_t length)
{
    uint8_t cLength[4] = {0};
    uint8_t iLength;

/*    if(esp8266_connected == 0)
    {
        esp8266_Connect_noParam();
        while(esp8266_connected == 0)
        {
            FSM_Feed();
        }
    }*/

    esp8266_readyToSend = 0;
    iLength = esp_dec2str(length, cLength);

#ifdef SINGLE_CONNECTION_MODE
    UART_WriteStr((uint8_t*)msg_CIPSEND    ,11         ,0);
#else
    UART_WriteStr((uint8_t*)msg_CIPSEND    ,13         ,0);
#endif
    UART_WriteStr((uint8_t*)cLength        ,iLength    ,0);
    UART_WriteStr((uint8_t*)endOfMsg       ,2          ,0);

    doSleep();

    UART_WriteStr((uint8_t*)data           ,length     ,0);
    //UART_WriteStr((uint8_t*)endOfMsg       ,2          ,0);
    doSleep();

    while( (esp8266_readyToSend == 0) &&
           (esp8266_connected == 1) )
    {
        FSM_Feed();
    }

    return length;
}
int esp8266_Connect(const char *host, uint16_t port)
{
    (void)host;
    (void)port;
    return esp8266_Connect_noParam();
}

int esp8266_Connect_noParam(void)
{
    ConnectReceived =1 ;
    esp8266_Event(esp8266_eConnect);

    while(esp8266_connected == 0)
    {
        FSM_Feed();
    }

    return 1;
}

uint8_t esp8266_Connected()
{
    return (esp8266_connected > 0);
}


#define BUFFER_SIZE      (200)
uint8_t buffer[BUFFER_SIZE] = {0};
uint8_t buf_idx_write;
uint8_t buf_idx_read;
uint8_t buf_used;
uint8_t buf_MAX = 0;

static void esp8266_storebyte(uint8_t *byte)
{
    if(BUFFER_SIZE > buf_used)
    {
        buffer[buf_idx_write] = *byte;
        ATOMIC_BLOCK(ATOMIC_FORCEON)
        {
            buf_idx_write++;
            buf_idx_write %= BUFFER_SIZE;
            buf_used++;
        }
        if(buf_MAX < buf_used)
        {
            buf_MAX = buf_used;
        }
    }
    else
    {
            
        while(1)
        {
            __asm("nop");
        }
    }
}

static int esp8266_readbyte(void)
{
    int retVal = -1;
    if(buf_used>0)
    {
        retVal = buffer[buf_idx_read];
        ATOMIC_BLOCK(ATOMIC_FORCEON)
        {
            buf_idx_read++;
            buf_idx_read %= BUFFER_SIZE;
            buf_used--;
        }
    }
    return retVal;
}

static int esp8266_available()
{
    return (buf_used>0);
}

static void esp8266_reset()
{
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        buf_idx_write = 0;
        buf_idx_read = 0;
        buf_used = 0;
    }
}

static uint8_t msgState = 0;
static uint8_t msgIdx = 0;
static uint8_t msgLength = 0;
static esp8266_events_t eEvent;

const uint8_t cmd_ReceiveData[] = "+IPD,";
const uint8_t cmd_OK[]          = "OK\r\n";
const uint8_t cmd_ERROR[]       = "ERROR\r\n";
const uint8_t cmd_WIFIGOTIP[]   = "WIFI GOT IP\r\n";

#ifdef SINGLE_CONNECTION_MODE
const uint8_t cmd_CLOSED[]      = "CLOSED\r\n";
const uint8_t cmd_CONNECT[]     = "CONNECT\r\n";
#else
const uint8_t cmd_CLOSED[]      = "0,CLOSED\r\n";
const uint8_t cmd_CONNECT[]     = "0,CONNECT\r\n";
#endif

uint8_t *pCmd = NULL;
void esp8266_Receive(uint8_t *pMsg, uint8_t length, int16_t handle)
{
    switch(msgState)
    {
        case 0: //Validate first char of the buffer
        {
            msgIdx = 0;
            switch(*pMsg)
            {
            //Receive incommig message!!!
            case '+': pCmd = cmd_ReceiveData; msgState = 1;  msgLength = 0;                 break;  /*'+'IDP,x,y" */
            case 'C': msgState = 20; break;
            case 'O': pCmd = cmd_OK;          msgState = 10; eEvent = esp8266_eSucceed;     break;  /* OK */              
            case 'E': pCmd = cmd_ERROR;       msgState = 10; eEvent = esp8266_eFailed;      break;  /* ERROR */           
            case 'W': pCmd = cmd_WIFIGOTIP;   msgState = 10; eEvent = esp8266_eEspUp;       break;  /* WIFI GOT IP\r\n" */
            }
            break;
        }
        case 20:
/*        {
            if(*pCmd==0x0A)//Expect ',', but got 0x0a, always.... 
            {
                msgState++;
            }
            else
            {
                msgState = 0;
            }
            break;
        }
        case 21:
        {   
            switch(*pMsg)
            {
                case 'C': msgState++; msg_idx; break;  // 0,CLOSED  or 0,CONNECTED
                default: msgState = 0;
            }
            break;
        }
        case 22:*/
        {   
            switch(*pMsg)
            {
                case 'L': pCmd = &cmd_CLOSED[1];  msgState = 10; eEvent = esp8266_eDisconnect;  break;  /* 0,CLOSED    */
//                case 'O': pCmd = cmd_Disconnect;  msgState = 10; eEvent = esp8266_eDisconnect;  break;  /* 0,CONNECTED */
                default: msgState = 0;
            }
            break;
        }
        case 1:
        {
            pCmd++;
            if(*pCmd==*pMsg)
            {
                if(*pCmd==',')
                {
                    msgState++;
                }
            }
            else
            {
                msgState = 0;
            }
            break;
        }            
        case 2:
/*        {
            msgState++;
            break;
        }
        case 3:
        {
            if(*pCmd==',')
            {
                msgState++;
            } 
            else
            {
                msgState = 0;
            }
            break;     
        }                                
        case 4:*/
        {
            if(*pMsg == ':')
            {
                msgState=5;
                break;
            }
            msgLength *= 10;
            msgLength += ((*pMsg)-0x30);
            break;
        }            
        case 5:
        {
            esp8266_storebyte(pMsg);
            if((--msgLength) == 0)
            {
                msgState = 0;
            }
            break;
        }
        
        case 10:
        {
            pCmd++;
            if(*pCmd==*pMsg)
            {
                if(*pCmd=='\n')
                {
                    esp8266_Event(eEvent);
                    msgState = 0;
                }
            }
            else 
            {
                msgState = 0;
            }
            break;
        }
    }
}