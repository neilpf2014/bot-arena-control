// Neil Flynn
// 1/28/2019
// V-0.9.5

#include "MQTThandler.h"

// MQTThandler
// wrapper class for the pub sub client class, use to hide callback from main.
// return text as strings
// alow for binary data to be passed
// Will add some of my own code in here to extend pub sub client

void MQTThandler::callback(char* topic, uint8_t* payload, unsigned int length)
{
	
	char* somearray = new char[length + 1];
	for (unsigned int i = 0; i < length; i++) {
		somearray[i] = ((char)payload[i]);
	}
	somearray[length] = '\0'; // null termination
	Inc_message = somearray;
	mailFlag = true;
}

// Treat message as binary
void MQTThandler::CBbinMsg(char* topic, uint8_t* payload, unsigned int length)
{
	
	for (unsigned int i = 0; i < length; i++) {
		B_message[i] = payload[i];
	}
	mailFlag = true;
}

// private function called to maintain the server connection
// the mess that is being hidden from main
void MQTThandler::reconnect(){
	CurTime = millis();	
	while ((!MQTTClient.connected())&&(pastTime < (CurTime - waitTime))){
		ConStatus = "Attempting MQTT connection...\n";  // debug
		// Attempt to connect
		if (MQTTClient.connect(ClientName.c_str())) {
			ConStatus = ConStatus + "connected";
			MQTTClient.subscribe(InC_topic.c_str());
		}
		else {
			ConStatus = ConStatus + "failed, rc="; //debug
			ConStatus = ConStatus + String(MQTTClient.state());
			ConStatus = ConStatus + "\ntry again in 5 seconds"; //debug
			// Wait 5 seconds before retrying
			pastTime = CurTime;
		}
	}
}

//Constructor; need ESP wifi client pointer and broker domain

MQTThandler::MQTThandler(Client& _ClWifi, const char* _serverName){
	ClWifi = &_ClWifi;
	MQTTClient.setClient(_ClWifi);
	MQTTClient.setServer(_serverName, 1883);
	MQTTClient.setCallback([this](char* topic, byte* payload, unsigned int length) { this->callback(topic, payload, length); });
	mode = 0;
	mailFlag = false;
}
//Constructor; need ESP wifi client pointer and broker IP address

MQTThandler::MQTThandler(Client& _ClWifi, IPAddress& _brokerIP){
	ClWifi = &_ClWifi;
	brokerIP = _brokerIP;
	MQTTClient.setClient(_ClWifi);
	MQTTClient.setServer(_brokerIP, 1883);
	MQTTClient.setCallback([this](char* topic, byte* payload, unsigned int length) { this->callback(topic, payload, length); });
	mode = 0;
	mailFlag = false;
}

//Constructor for binary or text payload, mode 0 is string, 1 binary 
// need ESP wifi client pointer and broker IP address

MQTThandler::MQTThandler(Client & _ClWifi, IPAddress& _brokerIP, uint8_t _mode, unsigned int _bufferSz)
{
	ClWifi = &_ClWifi;
	brokerIP = _brokerIP;
	MQTTClient.setClient(_ClWifi);
	MQTTClient.setServer(_brokerIP, 1883);
	mode = _mode;
	bufferSz = _bufferSz;
	if (_mode == (0))
		MQTTClient.setCallback([this](char* topic, byte* payload, unsigned int length) { this->callback(topic, payload, length); });
	else {
		MQTTClient.setCallback([this](char* topic, byte* payload, unsigned int length) { this->CBbinMsg(topic, payload, length); });
		B_message = new byte[bufferSz];
	}
	mailFlag = false;
}

// update will return true if a message has been recieved
// this needs to be called evertime in main.cpp loop

int MQTThandler::update(){
	if(!MQTTClient.connected())
		reconnect();
	MQTTClient.loop();
	return mailFlag;
}

// set client Name

void MQTThandler::setClientName(String _clientName){
	ClientName = _clientName;
}

//set server IP, if it differs from what is passed in const

void  MQTThandler::setServerIP(IPAddress& _ServerIP){
		MQTTClient.setServer(_ServerIP, 1883);
		brokerIP = _ServerIP;
}

// set topic for incomming messages

void MQTThandler::subscribeIncomming(String topic){
	InC_topic = topic;
}

// set topic for outgoing messages

void MQTThandler::subscribeOutgoing(String topic){
	Out_topic = topic;
}

// publish an outgoing message

int MQTThandler::publish(String message){
	if (!MQTTClient.connected()) {
		reconnect();
	}
	MQTTClient.publish(Out_topic.c_str(),message.c_str());
	return 0;
}

// return any recived messages

String MQTThandler::GetMsg(){
	String RetVal;
	if (mailFlag){
		RetVal = Inc_message;
		mailFlag = false;
	}
	else
		RetVal = "";
	return RetVal;
}

// Get connection status message, for debug mostly

String MQTThandler::GetConStatus(){
	return ConStatus;
}