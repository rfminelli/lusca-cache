# Async IO Support Notes #

## Outline ##

The Squid AIO implementation uses a pool of worker threads to implement potentially blocking disk-related syscalls - open(), read(), write(), close(), unlink(), stat(), truncate(). Operations are queued through a series of function calls which register callbacks that are called on completion of the request.

## History ##

It appeared sometime during Squid-1.2beta. It tied into the filedescriptor / disk code to try and provide a fully async disk interface for all disk IO - not just disk cache objects but logfile and swapfile writing.

## Current Implementation ##

There is one global thread pool and worker queue.

## Shortcomings ##

### Integration ###

### Load balancing / scheduling / shedding ###

## TODO list ##