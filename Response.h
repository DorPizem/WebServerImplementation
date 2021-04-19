#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <time.h>
#include "eStatusCode.h"
using namespace std;

struct Response
{
	string HTTPVersion = "HTTP/1.1";
	eStatusCode code = eStatusCode::OK;
	size_t contentLength = 0;
	string contentType = "text/html";
	string body;
	string allow;
	string cacheControl = "no-cache, private";
	string contentLocation;
	static const string newLine;
	static const string allowMethods;
};

string convertResponseToString(Response& response);
string eStatusCodeToString(eStatusCode code);
