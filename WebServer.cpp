#include "WebServer.h"

bool addSocket(Server& server, SOCKET id, eSocketStatus what)
{
	unsigned long flag = 1;
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (server.sockets[i].recv == eSocketStatus::EMPTY)
		{
			server.sockets[i].id = id;
			server.sockets[i].recv = what;
			server.sockets[i].send = eSocketStatus::IDLE;
			server.sockets[i].len = 0;
			server.socketsCount++;
			if (ioctlsocket(server.sockets[i].id, FIONBIO, &flag) != 0)
			{
				cout << "Web Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
			}
			return true;
		}
	}
	return false;
}

void removeSocket(Server& server, int index)
{
	server.sockets[index].recv = eSocketStatus::EMPTY;
	server.sockets[index].send = eSocketStatus::EMPTY;
	server.socketsCount--;
	server.sockets[index].timer = 0;
}

void acceptConnection(Server& server, int index)
{
	SOCKET id = server.sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*) & from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Web Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Web Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	if (addSocket(server, msgSocket, eSocketStatus::RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(Server& server, int index)
{
	SOCKET msgSocket = server.sockets[index].id;
	server.sockets[index].timer = time(0);
	int len = server.sockets[index].len;
	int bytesRecv = recv(msgSocket, &server.sockets[index].buffer[len], sizeof(server.sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Web Server: Error at recv(): " << WSAGetLastError() << endl;
		terminateSocket(msgSocket, server, index);
		return;
	}

	if (bytesRecv == 0)
	{
		terminateSocket(msgSocket, server, index);
		return;
	}
	else
	{
		server.sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Web Server: Recieved: " << bytesRecv << " bytes of \"" << &server.sockets[index].buffer[len] << "\" message.\n";
		server.sockets[index].len += bytesRecv;
		messageHandler(server, index);
	}
}

void messageHandler(Server& server, int index)
{
	SOCKET msgSocket = server.sockets[index].id;
	if (server.sockets[index].len > 0)
	{
		parseResponse(server, index);
		if (server.sockets[index].request.find("Method") != server.sockets[index].request.end())
		{
			server.sockets[index].send = eSocketStatus::SEND;
		}
		//we will never get here
		else if (strncmp(server.sockets[index].buffer, "Exit", 4) == 0)
		{
			closesocket(msgSocket);
			removeSocket(server, index);
			return;
		}
	}
}

void terminateSocket(SOCKET& socket, Server& server, int index)
{
	closesocket(socket);
	removeSocket(server, index);
}

void sendMessage(Server& server, int index)
{
	int bytesSent = 0;
	string sendBuff;
	Response response;
	SOCKET msgSocket = server.sockets[index].id;
	string method = server.sockets[index].request["Method"];
	string continueTo = server.sockets[index].continueTo;
	if (method == "GET")
	{
		response = generateGetResponse(server, index);
	}
	else if (method == "POST")
	{
		response = generatePostResponse(server, index);
	}
	else if (method == "HEAD")
	{
		response = generateHeadResponse(server, index);
	}
	else if (method == "PUT" || continueTo == "PUT")
	{
		response = generatePutResponse(server, index);
	}
	else if (method == "DELETE")
	{
		response = generateDeleteResponse(server, index);
	}
	else if (method == "OPTIONS")
	{
		response = generateOptionsResponse(server, index);
	}
	else if (method == "TRACE")
	{
		response = generateTraceResponse(server, index);
	}
	else
	{
		//method not allowd  - send 404
		response.code = eStatusCode::NotFound;
		response.contentLength = 0;
	}
	sendBuff = convertResponseToString(response);
	bytesSent = send(msgSocket, sendBuff.c_str(), (int)sendBuff.size(), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Web Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Web Server: Sent: " << bytesSent << "\\" << sendBuff.size() << " bytes of \"" << sendBuff << "\" message.\n";

	server.sockets[index].send = eSocketStatus::IDLE;
}

void initWinsock()
{
	WSAData wsaData;
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Web Server: Error at WSAStartup()\n";
		return;
	}
}

void run(Server& server)
{
	initWinsock();

	if (!initListenSocket(server))
	{
		return;
	}

	if (!initServerSide(server))
	{
		return;
	}

	addSocket(server, server.listenSocket, eSocketStatus::LISTEN);
	while (true)
	{
		fd_set waitRecv;
		fd_set waitSend;
		initWaitRecvSet(server, waitRecv);
		initWaitSendSet(server, waitSend);

		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (server.sockets[i].send == eSocketStatus::SEND)
			{
				FD_SET(server.sockets[i].id, &waitSend);
			}
		}

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Web Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		acceptAndRecieveMsg(server, nfd, waitRecv);
		sendAllMessages(server, nfd, waitSend);
	}

	// Closing connections and Winsock.
	cout << "Web Server: Closing Connection.\n";
	closesocket(server.listenSocket);
	WSACleanup();
}

void sendAllMessages(Server& server, int nfd, fd_set& waitSend)
{
	for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
	{
		if (FD_ISSET(server.sockets[i].id, &waitSend))
		{
			nfd--;
			switch (server.sockets[i].send)
			{
			case eSocketStatus::SEND:
				sendMessage(server, i);
				break;
			}
		}
	}
}

void acceptAndRecieveMsg(Server& server, int nfd, fd_set& waitRecv)
{
	for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
	{
		if (FD_ISSET(server.sockets[i].id, &waitRecv))
		{
			nfd--;
			if (server.sockets[i].recv == eSocketStatus::LISTEN)
			{
				acceptConnection(server, i);
			}
			else if (server.sockets[i].recv == eSocketStatus::RECEIVE)
			{
				receiveMessage(server, i);
			}
		}
	}
}

void initWaitRecvSet(Server& server, fd_set& waitRecv)
{
	FD_ZERO(&waitRecv);
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if ((server.sockets[i].recv == eSocketStatus::LISTEN)
			|| (server.sockets[i].recv == eSocketStatus::RECEIVE))
		{
			FD_SET(server.sockets[i].id, &waitRecv);
		}
	}
}

void initWaitSendSet(Server& server, fd_set& waitSend)
{
	FD_ZERO(&waitSend);
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (server.sockets[i].send == eSocketStatus::SEND)
		{
			FD_SET(server.sockets[i].id, &waitSend);
		}
	}
}

bool initListenSocket(Server& server)
{
	server.listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == server.listenSocket)
	{
		cout << "Web Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return false;
	}
	return true;
}

bool initServerSide(Server& server)
{
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(TIME_PORT);
	if (SOCKET_ERROR == bind(server.listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Web Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(server.listenSocket);
		WSACleanup();
		return false;
	}

	if (SOCKET_ERROR == listen(server.listenSocket, 5))
	{
		cout << "Web Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(server.listenSocket);
		WSACleanup();
		return false;
	}

	return true;
}

void getSubType(Server& server, int index)
{
	string buffer = server.sockets[index].buffer;
	size_t found = buffer.find('?');
	if (found != string::npos)
	{
		server.sockets[index].isQuary = true;
		server.sockets[index].quary = buffer.substr(found + 6, 2);
	}
}

string htmlToString(ifstream& htmlFile)
{
	string temp;
	string output;
	while (getline(htmlFile, temp))
	{
		output += temp += Response::newLine;
	}

	return output;
}

Response generateGetResponse(Server& server, int index)
{
	getSubType(server, index);
	ifstream htmlFile;
	string htmlPath = server.sockets[index].request["Path"];
	if (htmlPath.empty())
	{
		return Response();
	}

	htmlPath.erase(0, 1);
	string fileAsString;
	Response output;
	if (server.sockets[index].isQuary)
	{
		if (server.sockets[index].quary == "he")
		{
			htmlPath = "index-he.html";
		}
		else if (server.sockets[index].quary == "en")
		{
			htmlPath = "index-en.html";
		}
	}

	if (htmlPath == "")
	{
		htmlPath = "index-en.html";
	}

	htmlFile.open(htmlPath);
	if (htmlFile.is_open())
	{
		fileAsString = htmlToString(htmlFile);
		output.code = eStatusCode::OK;
		output.contentLength = fileAsString.size();
		output.body = fileAsString;
		htmlFile.close();
	}
	else
	{
		output.code = eStatusCode::NotFound;
		output.contentLength = 0;
	}

	return output;
}

Response generatePostResponse(Server& server, int index)
{
	Response output = generateGetResponse(server, index);
	string path = "data.txt";
	string body = getBody(server.sockets[index].buffer);
	body += "\n";
	/*ofstream fileToWrite; // NEED TO BE WRITTEN TO THIS FILE?
	fileToWrite.open(path, ios::app);
	if (fileToWrite.is_open())
	{
		fileToWrite << body;
		output.code = eStatusCode::OK;
	}
	else
	{
		fileToWrite << body;
		output.code = eStatusCode::Created;
	}*/
	printBodyParameters(server, index);

	return output;
}

Response generateHeadResponse(Server& server, int index)
{
	Response output = generateGetResponse(server, index);
	output.body.clear();
	return output;
}

Response generatePutResponse(Server& server, int index)
{
	string buffer = server.sockets[index].buffer;
	Response output;
	string& continueTo = server.sockets[index].continueTo;
	string& prevPath = server.sockets[index].prevPath;


	if (server.sockets[index].request["Expect"] == "100-continue")
	{
		continueTo = "PUT";
		prevPath = server.sockets[index].request["Path"];
		output.code = eStatusCode::Continue;
	}

	if (isBodyExist(buffer))
	{
		// move body to file
		prevPath.erase(0, 1);
		ofstream fileToWrite;
		fileToWrite.open(prevPath, ios::trunc);
		if (fileToWrite.is_open())
		{
			string body = getBody(server.sockets[index].buffer);
			fileToWrite << body;
			output.code = eStatusCode::Created;
			output.body = body;
			output.contentLength = body.size();
			output.contentLocation = prevPath;
			continueTo.clear();
			fileToWrite.close();
		}
		else
		{
			output.code = eStatusCode::InternalServerError;
		}

	}

	return output;
}

Response generateDeleteResponse(Server& server, int index)
{
	Response output;
	string path = server.sockets[index].request["Path"];
	path.erase(0, 1);
	if (remove(path.c_str()) == 0)
	{
		output.code = eStatusCode::OK;
	}
	else
	{
		output.code = eStatusCode::NotFound;
	}
	return output;
}

Response generateOptionsResponse(Server& server, int index)
{
	Response output = generateGetResponse(server, index);
	output.allow = Response::allowMethods;
	return output;
}

Response generateTraceResponse(Server& server, int index)
{
	Response output;
	string buffer = server.sockets[index].buffer;
	output.body = buffer;
	output.contentLength = buffer.size();
	output.contentType = "message/http";
	output.code = eStatusCode::OK;
	return output;
}

void printBodyParameters(Server& server, int index)
{
	string buffer = server.sockets[index].buffer;
	if (isBodyExist(buffer))
	{
		string body = getBody(buffer);
		size_t pos = body.find('=');
		if (pos != string::npos)
		{
			string entityBody = body.substr(0, pos);;
			string octet = body.substr(pos + 1, body.size());
			cout << "POST Response Body = entity-body: " << entityBody << " *OCTET: " << octet << endl;
		}
	}
}

void parseResponse(Server& server, int index)
{
	string buffer = server.sockets[index].buffer;
	map<string, string>& request = server.sockets[index].request;
	string method = GetSubHeader(buffer, " ", 0);
	mapInsert(request, "Method", method);
	string path = GetSubHeader(buffer, " ", 0);
	mapInsert(request, "Path", path);
	string version = GetSubHeader(buffer, "\r", 2);
	mapInsert(request, "Version", version);

	while (buffer.size() > 1 && buffer[0] != '\r')
	{
		string key = GetSubHeader(buffer, ":", 2);
		if (!key.empty())
		{
			string value = GetSubHeader(buffer, "\r", 2);
			if (!value.empty())
			{
				mapInsert(request, key, value);
			}
		}
	}
}

string GetSubHeader(string& buffer, string lookFor, int offset)
{
	deleteBegingSpaces(buffer);
	size_t pos = buffer.find(lookFor);
	string result;
	if (pos != string::npos)
	{
		result = buffer.substr(0, pos);
		buffer.erase(0, pos + offset);
	}
	return result;
}

void deleteBegingSpaces(string& i_Input)
{
	size_t pos = i_Input.find_first_not_of(" ");
	if (pos != string::npos)
	{
		i_Input = i_Input.substr(pos);
	}
}

void mapInsert(map<string, string>& i_Request, string i_Key, string i_Value)
{
	map<string, string>::iterator it;
	it = i_Request.find(i_Key);
	i_Request[i_Key] = i_Value;
}

bool isBodyExist(string i_buffer)
{
	size_t pos = i_buffer.find("\r\n\r\n");
	return (i_buffer.size() >= pos + 5);
}

string getBody(string i_Buffer)
{
	size_t pos = i_Buffer.find("\r\n\r\n");
	string res;
	if (pos != string::npos)
	{
		res = i_Buffer.substr(pos + 4, i_Buffer.size());
		pos = i_Buffer.find("\r");
		if (pos != string::npos)
		{
			res = res.substr(0, pos);
		}
	}

	return res;
}

void isTimeOut(Server& server)
{
	size_t now = time(0);
	for (int i = 1; i < MAX_SOCKETS; i++)
	{
		if (now - server.sockets[i].timer > MAXTIMEOUT)
		{
			removeSocket(server, i);
		}
	}
}