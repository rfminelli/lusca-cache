#include "range.h"

/* fills "absent" positions in range specification based on response body size 
   returns true if the range is still valid
   range is valid if its intersection with [0,length-1] is not empty
*/
static rangeCanonizeSpec(ByteRangeSpec *spec, size_t length);

/* canonizes all range specs within a set
   merges overlapping ranges if possible
   preserves the order
   returns true if the set is still valid
   set is valid if (after canonization):
	- all range specs are valid AND
	- first_pos of all specs are in increasing order (Squid specific requirement!)
*/
static rangeCanonizeSet(ByteRangeSet *set, size_t length);

