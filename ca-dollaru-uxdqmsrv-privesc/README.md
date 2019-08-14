#  CVE-2018-??? - CA Dollar Universe 5.3.3 'uxdqmsrv' - Privilege escalation via a vulnerable setuid binary 

For more information: https://itm4n.github.io/ca-dollaru-uxdqmsrv-privesc/

<p align="center">
  <img src="/ca-dollaru-uxdqmsrv-privesc/screenshots/ca-dollaru-uxdqmsrv-privesc.gif">
</p>

## Remediation  
At the time of writing, Dollar Universe 5.3.3 is reaching its end of life. Therefore, no patch has been developped on this version.

However, a workaround exists:
- Remove the setuid bit. 
- Create a new entry in _/etc/sudoers_ to enable a specific user to run it as root. 

Alternatively, upgrade to Dollar Universe 6. 

## Credits 
The shared object was taken from the following exploit: https://www.exploit-db.com/exploits/40768/

## Disclosure timeline 
2018-06-06 - Vulnerability discovery  
2018-06-07 - Being redirected to the Product Manager  
2018-06-26 - Report (+demonstration video) sent to vendor  
2018-07-11 - Reminder sent to vendor  
2018-07-12 - Vendor acknowledges vulnerability  
2018-07-12 - Suggested a workaround  
2018-08-02 - Reminder sent to vendor  
2018-08-03 - Workaround accepted by vendor  
2018-08-31 - Vulnerability disclosed  

