# Introduction #

HTTP Digest Authentication is an alternative to HTTP Basic Authentication which avoids sending the password in clear-text. Later revisions of Digest Authentication include anti-replay countermeasures.

# Details #

Digest authentication involves:

  * support in Squid/Lusca for the negotiation phases
  * an external helper which returns the relevant MD5 nonces used as part of the authentication phase

# Building Lusca #

Building digest authentication requires enabling the digest auth handling and zero or more digest auth helpers.

```
  ./configure ... --enable-auth="digest" --enable-digest-auth-modules"[auth modules]"
```

# Configuration #

The following is an example which uses the digest\_pw\_auth helper.

First, build Lusca with the relevant helper.

```
  ./configure ... --enable-auth="digest" --enable-digest-auth-modules"password"
```

Then add the following to squid.conf to enable digest authentication:

```
# Authentication
auth_param digest program /Users/adrian/work/lusca/run/libexec/digest_pw_auth -c /Users/adrian/work/lusca/run/etc/digest-passwd
auth_param digest children 2
auth_param digest realm test
acl localusers proxy_auth REQUIRED
```

Next, use 'localusers' in http\_access where appropriate to force authentication.

Finally, create a password entry in digest-passwd using "digest\_passwd" in the Lusca source directory (LUSCA\_HEAD/helpers/digest\_auth/password):

```
./digest_passwd [user] [password] [realm]
```

For example:

```
./digest_passwd adrian password test
```

The realm must match the realm configured in the authentication section in squid.conf.

# Behind the scenes #

The wikipedia article below gives a good introduction into how HTTP digest authentication works.

The digest helper protocol involves passing the helper a string containing "username":"realm"; it then returns the "HA1" to use in authentication. Since HA1 is calculated by using the realm name, the realm in the password file AND the squid.conf must match.

The digest\_pw\_auth helper can calculate the MD5 HA1 string given a cleartext password. The "-c" option to digest\_pw\_auth tells the helper that the MD5 HA1 string value is already calculated. If cleartext passwords are required, "-c" can be omitted.

# Further Information #

  * http://en.wikipedia.org/wiki/Digest_access_authentication