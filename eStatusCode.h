#pragma once

enum class eStatusCode
{
	Continue = 100,
	OK = 200,
	Created = 201,
	Accepted = 202,
	NotFound = 404,
	InternalServerError = 500
};