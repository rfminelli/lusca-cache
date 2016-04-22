# Packet capture #

Sent:
```
GET /tbm.php?s=972950002&_U=21bc4d852a671f65928e9effe341729e&_u=http%3a%2f%2fsearch.yahoo.com%2fsearch%3fp%3dghetto%2btube%26fr%3dyfp-t-501-s%26toggle%3d1%26cop%3dmss%26ei%3dUTF-8&hurl=c046cf550cace5fb&rurl=e6e398ee16bb0c0f&pc=grpj&intl=us&ldt=5016&t_resp=110&dnst=72.30.186.249&dnsr=10.18.0.6&ns=10.22.62.222&t=27385828 HTTP/1.0
Host: 98.136.60.161
User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.0.11) Gecko/2009060310 Ubuntu/8.10 (intrepid) Firefox/3.0.11
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: en-us,en;q=0.5
Accept-Encoding: gzip,deflate
Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
Via: 1.1 cindy.cacheboy.net:3128 (Lusca/LUSCA_HEAD)
X-Forwarded-For: 192.168.1.8
Cache-Control: max-age=259200
Connection: keep-alive
```

Received from origin:

```
HTTP/1.1 204 No Content
Date: Tue, 30 Jun 2009 01:19:59 GMT
Set-Cookie: BX=akrhrf154iq1v&b=3&s=6g; expires=Tue, 02-Jun-2037 20:00:00 GMT; path=/; domain=.60.161
P3P: policyref="http://info.yahoo.com/w3c/p3p.xml", CP="CAO DSP COR CUR ADM DEV TAI PSA PSD IVAi IVDi CONi TELo OTPi OUR DELi SAMi OTRi UNRi PUBi IND PHY ONL UNI PUR FIN COM NAV INT DEM CNT STA POL HEA PRE LOC GOV"
Cache-Control: max-age=0, no-cache, no-store, private
Pragma: no-cache
Expires: Wed Dec 31 16:00:00 1969
Connection: close
Content-Type: text/html; charset=utf-8
Content-Encoding: gzip
Content-Length: 98

...........Q..U(I.5.K....+.0..L....K..U............T...<.c...C+CK+SK.w...#..K.]];...........d.I...
```

## Issue ##

A 204 reply must not have a reply body. Lusca correctly handles this. [Issue 35](https://code.google.com/p/lusca-cache/issues/detail?id=35) attempts to clearly log this particular condition separate from "there is more data in the reply body than the Content-Length said."

## Status ##

  * Working on clarifying the logging in Lusca - [Issue 35](https://code.google.com/p/lusca-cache/issues/detail?id=35)
  * Yahoo! contacted about this