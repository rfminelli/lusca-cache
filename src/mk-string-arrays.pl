#******************************************************************************
# $Id$
#
# File:		mk-strs.pl
#
# Author:	Max Okumoto <okumoto@ucsd.edu>
#
# Abstract:	This perl script parses enums and builds an array of
#		printable strings.
#
# Warning:	The parser is very simplistic, and will prob not work for
#		things other than squid.
#******************************************************************************

$pat{'err_type'} = "err_type_str";
$pat{'icp_opcode'} = "icp_opcode_str";

$state = 0;	# start state
while (<>) {
	if ($state == 0) {
		# Looking for start of typedef
		if (/^typedef enum /) {
			$count = 0;	# enum index
			$state = 1;
		}
		next;

	} elsif ($state == 1) {
		# Looking for end of typedef
		if (/^} /) {
			($b, $t) = split(/[ \t;]/, $_);
			if (defined($pat{$t})) {
				print "const char *$pat{$t}[] = \n";
				print "{\n";
				for ($i = 0; $i < $count; $i++) {
					printf "\t\"%s\"%s\n",
						$ea[$i],
						$i == $count - 1 ? '' : ',';
				}
				print "};\n";
				print "\n";
			}
			$state = 0;
		} else {
			($e) = split(' ', $_);
			$e =~ s/,//;
			$ea[$count] = $e;
			$count++;
		}
		next;
	}
}

exit 0;
