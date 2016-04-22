# Introduction #

Refresh patterns are a method for controlling response cachability. It has a variety of options which allow modification, overriding or ignoring various cache control and revalidation methods.

# Details #

Refresh patterns are regular expressions which match on the requested URL. They can modify both the request and response cache control/revalidation methods.

The refresh patterns are evaluated in the order they are found in the configuration file until a match is found.

# Options #

The refresh pattern options are documented in the squid.conf.default file.

# Examples #

[TODO](TODO.md)

# Implementation Details #

[TODO](TODO.md)

# Differences to Squid #

  * Lusca has an extra flag - "ignore-no-store" - which ignores "Cache-Control: no-store" in the response.

# Notes #

  * The regular expression matching may cause a large CPU hit. Please limit the use of refresh patterns and order them appropriately to try and minimise the CPU spent on matching.