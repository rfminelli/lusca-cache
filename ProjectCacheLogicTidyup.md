# Overview #

The caching logic pathways are long, complicated and twisty. This project will aim to first unwind a bit of the twistiness and document what is going on. It won't try to rewrite or heavily modify things.

# Background #

The bulk of the caching logic actually sits inline with the client-side request and reply processing. Part of the hit processing pathway involves evaluating whether the stored object is still fresh; whether there's variant or ETags to worry about and handling IMS revalidation.

Some of the caching logic sits in the server-facing code (src/http.c.) httpCachableReply() determines whether the reply is at all cachable based on a few properties of the reply. A cachable reply on the server-side may then be made non-cachable by the client-side logic.

This all requires significant reorganisation and documentation. In particular, the process flow of request and reply handling in the client-side code requires a lot of attention.

# Progress #

There is another project currently in progress to tidy up and reorganise the client-side codebase as a whole. This is meeting the initial requirements for trying to unwind the various code paths.

Some documentation will likely appear once this project is completed enough.