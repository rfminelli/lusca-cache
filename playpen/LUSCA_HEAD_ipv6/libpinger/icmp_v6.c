#include "../include/config.h"

#include <stdio.h>
#include <stdlib.h>


#include "icmp_v6.h"

// Icmp6 OP-Codes
// see http://www.iana.org/assignments/icmpv6-parameters
// NP: LowPktStr is for codes 0-127
const char *icmp6LowPktStr[] = {
    "ICMP 0",                   // 0
    "Destination Unreachable",  // 1 - RFC2463
    "Packet Too Big",           // 2 - RFC2463
    "Time Exceeded",            // 3 - RFC2463
    "Parameter Problem",                // 4 - RFC2463
    "ICMP 5",                   // 5
    "ICMP 6",                   // 6
    "ICMP 7",                   // 7
    "ICMP 8",                   // 8
    "ICMP 9",                   // 9
    "ICMP 10"                   // 10
};

// NP: HighPktStr is for codes 128-255
const char *icmp6HighPktStr[] = {
    "Echo Request",                                     // 128 - RFC2463
    "Echo Reply",                                       // 129 - RFC2463
    "Multicast Listener Query",                 // 130 - RFC2710
    "Multicast Listener Report",                        // 131 - RFC2710
    "Multicast Listener Done",                  // 132 - RFC2710
    "Router Solicitation",                              // 133 - RFC4861
    "Router Advertisement",                             // 134 - RFC4861
    "Neighbor Solicitation",                    // 135 - RFC4861
    "Neighbor Advertisement",                   // 136 - RFC4861
    "Redirect Message",                         // 137 - RFC4861
    "Router Renumbering",                               // 138 - Crawford
    "ICMP Node Information Query",                      // 139 - RFC4620
    "ICMP Node Information Response",           // 140 - RFC4620
    "Inverse Neighbor Discovery Solicitation",  // 141 - RFC3122
    "Inverse Neighbor Discovery Advertisement", // 142 - RFC3122
    "Version 2 Multicast Listener Report",              // 143 - RFC3810
    "Home Agent Address Discovery Request",             // 144 - RFC3775
    "Home Agent Address Discovery Reply",               // 145 - RFC3775
    "Mobile Prefix Solicitation",                       // 146 - RFC3775
    "Mobile Prefix Advertisement",                      // 147 - RFC3775
    "Certification Path Solicitation",          // 148 - RFC3971
    "Certification Path Advertisement",         // 149 - RFC3971
    "ICMP Experimental (150)",                  // 150 - RFC4065
    "Multicast Router Advertisement",           // 151 - RFC4286
    "Multicast Router Solicitation",            // 152 - RFC4286
    "Multicast Router Termination",                     // 153 - [RFC4286]
    "ICMP 154",
    "ICMP 155",
    "ICMP 156",
    "ICMP 157",
    "ICMP 158",
    "ICMP 159",
    "ICMP 160"
};

