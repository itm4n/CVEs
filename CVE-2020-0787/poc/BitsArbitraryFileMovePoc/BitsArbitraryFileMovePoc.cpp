
#include <iostream>
#include <Windows.h>
#include <strsafe.h>

#include "BitsArbitraryFileMove.h"

int main()
{
	BOOL bRes;
	BitsArbitraryFileMove bitsArbitraryFileMove;

	bRes = bitsArbitraryFileMove.Run(L"C:\\Windows\\System32\\FakeDll.dll");
}

