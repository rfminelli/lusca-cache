# Introduction #

Welcome to Cacheboy. This is a fork of the Squid-2.HEAD sources from early April, 2008 with subsequent updates merged in as needed.

# What? #

This project has been created to continue the logical development of Squid-2.HEAD in directions which may clash with the current Squid development (focusing on Squid-3.)

# Why? #

The Squid project is focusing on Squid-3 development with specific forward proxy project goals in mind, including content adaptation and ICAP.

Squid-2 enjoys a very large install base and Squid-3 is not yet mature enough to replace it in many production environments.

This project plans on developing the mature Squid-2 codebase into a more modern application, taking advantage of hardware and software advances. The initial focus will be on non-invasive code refactoring and reorganisation to improve clarity, allow for unit-testing and to re-use once-core code as libraries for other related programs. The next focus will be on performance and structural modifications in preparation for IPv6, HTTP/1.1 and content compression. After that - well, whatever people want to code up and contribute!