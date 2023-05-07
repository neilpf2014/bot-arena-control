// MQTThandler.h

#ifndef _MQTTHANDLER_h
#define _MQTTHANDLER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif
#include <WiFi.h>
#include <IPAddress.h>
#include <Client.h>
#include <PubSubClient.h>
#include <functional>

class MQTThandler{
 protected:
	Client* ClWifi;
	PubSubClient MQTTClient;
	String S_message;
	String Inc_message; //contains the latest message
	String Out_topic;
	String InC_topic;
	String ConStatus; // connection status debug message
	byte* B_message; // will contain byte array for binary data
	unsigned int bufferSz;
	uint8_t mode; // 0 for string, 1 for binary
	String ClientName;
	uint8_t mailFlag;
	IPAddress brokerIP;
	// non blocking timer
	unsigned long CurTime;
	unsigned long pastTime;
	const long waitTime = 5000;
	void callback(char* topic, uint8_t* payload, unsigned int length);
	void CBbinMsg(char* topic, uint8_t* payload, unsigned int length);
	//std::function<void(char*, uint8_t*, unsigned int)> callback;
	void reconnect();

 public:
	
	// constuctor pass wifi and broker name ** don't use with ESP32 **
	MQTThandler(Client& _ClWifi, const char* _serverName);

	// constuctor pass wifi and broker IP
	MQTThandler(Client& _ClWifi, IPAddress& _brokerIP);
	MQTThandler(Client& _ClWifi, IPAddress& _brokerIP, uint8_t _mode, uint _bufferSz);

	// need to call in main loop
	int update();

	void setClientName(String _clientName);
	void setServerIP(IPAddress& _ServerIP);
	void subscribeIncomming(String topic);
	void subscribeOutgoing(String topic);
	int publish(String message);
	String GetMsg();
	String GetConStatus();
};


#endif

