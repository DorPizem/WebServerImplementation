#include "Response.h"

const string Response::newLine = "\r\n";
const string Response::allowMethods = "GET, HEAD, PUT, OPTIONS, DELETE, TRACE, POST";

string convertResponseToString(Response& i_Response)
{
	time_t timer;
	time(&timer);
	string date = ctime(&timer);
	date.pop_back();

	string output = i_Response.HTTPVersion += " ";
	output += to_string((int)i_Response.code) + " ";
	output += eStatusCodeToString(i_Response.code) + Response::newLine;
	output += string("Date: ") + date + Response::newLine;
	output += string("Cache-Control: ") + i_Response.cacheControl + Response::newLine;
	output += string("Content-Type: ") + i_Response.contentType + Response::newLine;
	if (!i_Response.contentLocation.empty())
	{
		output += string("Content-Location: ") + i_Response.contentLocation + Response::newLine;
	}
	if (i_Response.contentLength >= 0)
	{
		output += string("Content-Length: ") + to_string(i_Response.contentLength) + Response::newLine;
	}
	if (!i_Response.allow.empty())
	{
		output += string("Allow: ") + i_Response.allow + Response::newLine;
	}
	output += Response::newLine;
	if (!i_Response.body.empty())
	{
		output += i_Response.body + Response::newLine;
	}
	return output;
}

string eStatusCodeToString(eStatusCode i_Code)
{
	string output;
	switch (i_Code)
	{
	case eStatusCode::Continue:
		output = "Continue";
		break;
	case eStatusCode::OK:
		output = "OK";
		break;
	case eStatusCode::NotFound:
		output = "Not Found";
		break;
	case eStatusCode::Created:
		output = "Created";
		break;
	case eStatusCode::Accepted:
		output = "Accepted";
		break;

	case eStatusCode::InternalServerError:
		output = "Internal Server Error";
		break;
	}
	return output;
}