#pragma once

#include <Windows.h>

#define TIMEOUT 15

class Utils
{
public:
	Utils();
	~Utils();

public:
	bool IsServiceRunning(const WCHAR* lpServiceName);
	bool IsServiceDisabled(const WCHAR* lpServiceName);
	bool StartRasman();
};

