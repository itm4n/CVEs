
#include "Utils.h"

#include <iostream>


Utils::Utils()
{
}


Utils::~Utils()
{
}

bool Utils::IsServiceRunning(const WCHAR* lpServiceName)
{
	SC_HANDLE schSCM;
	SC_HANDLE schSvc;
	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwBytesNeeded;
	BOOL bSuccess = FALSE;
	bool bResult = false;

	// Open Service Manager
	schSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (NULL == schSCM)
	{
		//std::wcout << "[-] Failed to open Service Manager." << std::endl;
		return false;
	}

	// Open service 
	schSvc = OpenService(schSCM, lpServiceName, SERVICE_QUERY_STATUS);
	if (NULL == schSvc)
	{
		//std::wcout << "[-] Failed to open service '" << lpServiceName << "'." << std::endl;
		CloseServiceHandle(schSCM);
		return false;
	}

	// Get service status 
	bSuccess = QueryServiceStatusEx(schSvc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssStatus), sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded);

	if (bSuccess != 0)
	{
		bResult = (ssStatus.dwCurrentState == SERVICE_RUNNING);
	}
	else
	{
		//std::wcout << "Failed to query service status ('" << lpServiceName << "'), status: " << GetLastError() << std::endl;
	}

	CloseServiceHandle(schSvc);
	CloseServiceHandle(schSCM);

	return bResult;
}

bool Utils::IsServiceDisabled(const WCHAR* lpServiceName)
{
	SC_HANDLE schSCM;
	SC_HANDLE schSvc;
	BOOL bSuccess = FALSE;
	LPQUERY_SERVICE_CONFIG lpServiceConfig = NULL;
	DWORD dwBytesNeeded, cbBufSize = 0, dwError;
	bool bResult = false;

	// Open Service Manager
	schSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (NULL == schSCM)
	{
		//std::wcout << "[-] Failed to open Service Manager." << std::endl;
		return false;
	}

	// Open service 
	schSvc = OpenService(schSCM, lpServiceName, SERVICE_QUERY_CONFIG);
	if (NULL == schSvc)
	{
		//std::wcout << "[-] Failed to open service '" << lpServiceName << "'." << std::endl;
		goto cleanup;
	}

	// Get LPQUERY_SERVICE_CONFIG size 
	if (!QueryServiceConfig(schSvc, NULL, 0, &dwBytesNeeded))
	{
		dwError = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == dwError)
		{
			cbBufSize = dwBytesNeeded;
			lpServiceConfig = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, cbBufSize);
		}
		else
		{
			//std::wcout << "[-] Failed to retrieve config of service '" << lpServiceName << "'" << std::endl;
			goto cleanup;
		}
	}

	// Get service config 
	if (!QueryServiceConfig(schSvc, lpServiceConfig, cbBufSize, &dwBytesNeeded))
	{
		//std::wcout << "[-] Failed to retrieve config of service '" << lpServiceName << "'" << std::endl;
		goto cleanup;
	}

	if (lpServiceConfig == NULL)
		return false;

	if (lpServiceConfig->dwStartType == SERVICE_DISABLED)
	{
		bResult = true;
	}

cleanup:
	if (schSvc != NULL)
		CloseServiceHandle(schSvc);
	CloseServiceHandle(schSCM);

	return bResult;
}

bool Utils::StartRasman()
{
	Utils utils;
	const WCHAR* lpServiceName = L"Rasman";
	SC_HANDLE schSCM;
	SC_HANDLE schSvc;
	bool bResult = false;

	schSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (NULL == schSCM)
	{
		//std::wcout << "[-] Failed to open Service Manager." << std::endl;
		return false;
	}

	schSvc = OpenService(schSCM, lpServiceName, SERVICE_START);
	if (NULL == schSvc)
	{
		//std::wcout << "[-] Failed to open service: '" << lpServiceName << "'" << std::endl;
		CloseServiceHandle(schSCM);
		return false;
	}

	if (!StartService(schSvc, 0, NULL))
	{
		//std::wcout << "[-] Failed to start service: '" << lpServiceName << "'" << std::endl;
		CloseServiceHandle(schSvc);
		CloseServiceHandle(schSCM);
		return false;
	}

	CloseServiceHandle(schSvc);
	CloseServiceHandle(schSCM);

	int iWait = 500;
	int iIter = TIMEOUT * 1000 / iWait;
	for (int i = 0; i < iIter; i++)
	{
		if (IsServiceRunning(L"Rasman"))
		{
			bResult = true;
			break;
		}
		Sleep(iWait);
	}

	return bResult;
}
