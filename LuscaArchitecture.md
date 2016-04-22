# Introduction #

Lusca is a single process application which processes network traffic in a (now) traditional callback-based event notification style framework.

It uses POSIX threads for disk IO and external processes for a variety of tasks (logfile writing, ACL helpers, authentication helpers, URL rewriting, etc.)

This architecture overview is not designed to be completely thorough; it is meant to introduce the general structure and data flow of Lusca. The aim is to document the various APIs using HeaderDoc in the source code itself; this documentation just acts as a higher level introduction to what goes where.

# Sections #

## Core framework ##

  * [LuscaArchitectureEventOverview](LuscaArchitectureEventOverview.md) - the event-driven callback framework overview
  * [LuscaArchitectureNetwork](LuscaArchitectureNetwork.md) - the network framework
  * [LuscaArchitectureCallbackData](LuscaArchitectureCallbackData.md) - the callback data registry and reference counting
  * [LuscaArchitectureMemoryManagement](LuscaArchitectureMemoryManagement.md) - basic memory management
  * [LuscaArchitectureDiskIO](LuscaArchitectureDiskIO.md) - the legacy disk IO mechanism
  * [LuscaArchitectureAsyncDiskIO](LuscaArchitectureAsyncDiskIO.md) - the thread-based async disk IO mechanism
  * [LuscaArchitectureHelperIPC](LuscaArchitectureHelperIPC.md) - how external helper IPC is implemented
  * [LuscaArchitectureHelpers](LuscaArchitectureHelpers.md) - the generic "helper" process framework
  * [LuscaArchitectureDebugging](LuscaArchitectureDebugging.md) - the debugging framework

## Storage framework ##

  * [LuscaArchitectureStorageLayer](LuscaArchitectureStorageLayer.md) - the store layer
  * [LuscaArchitectureStoreVary](LuscaArchitectureStoreVary.md) - supporting Vary objects
  * [LuscaArchitectureStoreIndex](LuscaArchitectureStoreIndex.md) - maintaining the store index
  * [LuscaArchitectureStoreClientLayer](LuscaArchitectureStoreClientLayer.md) - client-side storage interface
  * [LuscaArchitectureStoreServerLayer](LuscaArchitectureStoreServerLayer.md) - the server-side storage interface
  * [LuscaArchitectureStoreMemory](LuscaArchitectureStoreMemory.md) - the store memory layer, and how it is used as a "hot object cache"
  * [LuscaArchitectureStoreDisk](LuscaArchitectureStoreDisk.md) - the store disk layer

  * [LuscaArchitectureStoreShortcomings](LuscaArchitectureStoreShortcomings.md) - shortcomings in the current storage layer
  * [LuscaArchitectureStoreRebuilding](LuscaArchitectureStoreRebuilding.md) - how Lusca rebuilds the store index at startup

## Network framework ##

  * [LuscaArchitectureNetworkIntroduction](LuscaArchitectureNetworkIntroduction.md) - the basic network and communication overview
  * [LuscaArchitectureNetworkTransparentInterception](LuscaArchitectureNetworkTransparentInterception.md) - transparent interception related changes
  * [LuscaArchitectureNetworkReadingWriting](LuscaArchitectureNetworkReadingWriting.md) - reading and writing network data
  * [LuscaArchitectureNetworkCloseHandlers](LuscaArchitectureNetworkCloseHandlers.md) - the processing which occurs on socket and filedescriptor close
  * [LuscaArchitectureNetworkConnectingToRemoteHosts](LuscaArchitectureNetworkConnectingToRemoteHosts.md) - the "Squid way" of asynchronously connecting to a remote host

## HTTP processing framework ##

  * [LuscaArchitectureClientSide](LuscaArchitectureClientSide.md) - the client-side path, handling client requests
  * [LuscaArchitectureForwarding](LuscaArchitectureForwarding.md) - request forwarding
  * [LuscaArchitecturePeerSelection](LuscaArchitecturePeerSelection.md) - various peer selection methods
  * [LuscaArchitectureHttpServerSide](LuscaArchitectureHttpServerSide.md) - the HTTP server-side path, communicating to peers and servers
  * [LuscaArchitectureFtpServerSide](LuscaArchitectureFtpServerSide.md) - the FTP server-side path, communicating to FTP servers

## Unsorted Entries ##

  * [LuscaArchitectureRangeRequests](LuscaArchitectureRangeRequests.md) - how Range requests and responses are handled, processed and cached
