/* Currently only byte ranges are supported */

/* Essentially, there are three types of byte ranges:
	1) first-byte-pos "-" last-byte-pos  // range
	2) first-byte-pos "-"                // trailer
	3)                "-" suffix-length  // suffix (last length bytes)
   We code an "absent" part as ((size_t)-1)
   Warning: last_pos is interpreted depending on first_byte_pos value
   Note: when response length becomes known, we convert range spec into type one.
*/
struct _ByteRangeSpec {
	size_t first;
	size_t last;
};

/* There may be more than one byte range specified in the request.
   This structure holds all range specs in order of their appearence
   in the request because we SHOULD preserve that order.
*/
struct _ByteRangeSet {
	u_char count;
	struct _ByteRangeSpec *specs;
};

typedef struct _ByteRangeSpec ByteRangeSpec;
typedef struct _ByteRangeSet ByteRangeSet;

/*
 * returns true if ranges are valid fills corresponding entries in the request
 * structure
 */
extern int rangeParseClient(ByteRangeSet *set, char *header);

/*
 * returns offset of the first (left most) byte, adjusted for page_size
 */
extern off_t rangeStart(const ByteRangeSet *set, size_t page_size);
