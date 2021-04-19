#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream> 
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string>
#include <time.h>
#include <map>
#include <queue>
#include "Response.h"
//#include "Request.h"
#define NUM_REQUESTS 7
#define MAXTIMEOUT 180

const int TIME_PORT = 80;
const int MAX_SOCKETS = 60;


enum class eSocketStatus
{
	EMPTY,
	LISTEN,
	RECEIVE,
	IDLE,
	SEND
};

typedef struct socketState
{
	SOCKET id;			
	eSocketStatus recv;	
	eSocketStatus send;	
	int sendSubType;
	bool isQuary;
	string quary;
	char buffer[1024];
	int len;
	map<string, string> request;
	string continueTo;
	string prevPath;
	time_t timer;
}SocketState;

typedef struct server
{
	SocketState sockets[MAX_SOCKETS] = { 0 };
	int socketsCount = 0;
	SOCKET listenSocket;
}Server;


bool addSocket(Server& server, SOCKET id, eSocketStatus currentStatus);
void removeSocket(Server& server, int index);
void acceptConnection(Server& server, int index);
void receiveMessage(Server& server, int index);
void sendMessage(Server& server, int index);

void run(Server& server);
bool initListenSocket(Server& server);
bool initServerSide(Server& server);
void getSubType(Server& server, int index);

Response generateGetResponse(Server& server, int index);
Response generatePostResponse(Server& server, int index);
Response generateHeadResponse(Server& server, int index);
Response generatePutResponse(Server& server, int index);
Response generateDeleteResponse(Server& server, int index);
Response generateOptionsResponse(Server& server, int index);
Response generateTraceResponse(Server& server, int index);
string htmlToString(ifstream& htmlFile);

void printBodyParameters(Server& server, int index);
void parseResponse(Server& server, int index);
void deleteBegingSpaces(string& i_Input);
string GetSubHeader(string& buffer, string lookFor, int offset);
bool isBodyExist(string i_buffer);
void mapInsert(map<string, string>& i_Request, string i_Key, string i_Value);
string getBody(string i_Buffer);
void terminateSocket(SOCKET& socket, Server& server, int index);
void messageHandler(Server& server, int index);
void isTimeOut(Server& server);
void initWaitRecvSet(Server& server, fd_set& waitRecv);
void initWaitSendSet(Server& server, fd_set& waitSend);
void acceptAndRecieveMsg(Server& server, int nfd, fd_set& waitRecv);
void sendAllMessages(Server& server, int nfd, fd_set& waitSend);