#!/usr/local/bin/perl

# udp-banger.pl 
#
# Duane Wessels, Dec 1995
#
# Usage: udp-banger.pl [host [port]] < url-list
#
# Sends a continuous stream of ICP queries to a cache.  Stdin is a list of
# URLs to request.  Run N of these at the same time to simulate a heavy
# neighbor cache load.

require 'getopts.pl';

$|=1;

&Getopts('n');

$host=(shift || 'localhost') ;
$port=(shift || '3130') ;

# just copy this from src/proto.c
@CODES=(
    "ICP_INVALID",
    "ICP_QUERY",
    "UDP_HIT",
    "UDP_MISS",
    "ICP_ERR",
    "ICP_SEND",
    "ICP_SENDA",
    "ICP_DATABEG",
    "ICP_DATA",
    "ICP_DATAEND",
    "ICP_SECHO",
    "ICP_DECHO",
    "ICP_OP_UNUSED0",
    "ICP_OP_UNUSED1",
    "ICP_OP_UNUSED2",
    "ICP_OP_UNUSED3",
    "ICP_OP_UNUSED4",
    "ICP_OP_UNUSED5",
    "ICP_OP_UNUSED6",
    "ICP_OP_UNUSED7",
    "ICP_OP_UNUSED8",
    "UDP_RELOADING",
    "UDP_DENIED",
    "UDP_HIT_OBJ",
    "ICP_END"
);

require 'sys/socket.ph';

$sockaddr = 'S n a4 x8';
($name, $aliases, $proto) = getprotobyname("udp");
($fqdn, $aliases, $type, $len, $themaddr) = gethostbyname($host);
$thissock = pack($sockaddr, &AF_INET, 0, "\0\0\0\0");
$them = pack($sockaddr, &AF_INET, $port, $themaddr);

chop($me=`uname -a|cut -f2 -d' '`);
$myip=(gethostbyname($me))[4];

die "socket: $!\n" unless
	socket (SOCK, &AF_INET, &SOCK_DGRAM, $proto);

$flags = 0;
$flags |= 0x80000000;
$flags |= 0x40000000 if ($opt_n);
$flags = ~0;

while (<>) {
	chop;
	$request_template = 'CCnx4Nx4x4a4a' . length;
	$request = pack($request_template, 1, 2, 24 + length, $flags, $myip, $_);
	die "send: $!\n" unless
		send(SOCK, $request, 0, $them);
	die "recv: $!\n" unless
                $theiraddr = recv(SOCK, $reply, 1024, 0);
  	($junk, $junk, $sourceaddr, $junk) = unpack($sockaddr, $theiraddr);
  	@theirip = unpack('C4', $sourceaddr);
        ($type,$ver,$len,$flag,$p1,$p2,$payload) = unpack('CCnx4Nnnx4A', $reply);
        print join('.', @theirip) . ' ' . $CODES[$type] . " $_\n";
	print "hop = $p1\n" if ($opt_n);
	print "rtt = $p2\n" if ($opt_n);
}

