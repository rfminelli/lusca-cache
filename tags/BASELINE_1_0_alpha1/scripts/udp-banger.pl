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


$|=1;

$host=(shift || 'localhost') ;
$port=(shift || '3130') ;

@CODES=('xxx', 'QUERY', 'HIT', 'MISS', 'ERROR');

require "$ENV{'HARVEST_HOME'}/lib/socket.ph";

$sockaddr = 'S n a4 x8';
($name, $aliases, $proto) = getprotobyname("udp");
($fqdn, $aliases, $type, $len, $themaddr) = gethostbyname($host);
$thissock = pack($sockaddr, &AF_INET, 0, "\0\0\0\0");
$them = pack($sockaddr, &AF_INET, $port, $themaddr);

chop($me=`uname -a|cut -f2 -d' '`);
$myip=(gethostbyname($me))[4];

die "socket: $!\n" unless
	socket (SOCK, &AF_INET, &SOCK_DGRAM, $proto);

while (<>) {
	chop;
	$request_template = 'CCnx4x8x4a4a' . length;
	$request = pack($request_template, 1, 1, 24 + length, $myip, $_);
	die "send: $!\n" unless
		send(SOCK, $request, 0, $them);
	die "recv: $!\n" unless
		recv(SOCK, $reply, 1024, 0);
	($type,$ver,$len,$payload) = unpack('CCnx4x8x4A', $reply);
	print $CODES[$type] . " $_\n";
}

