#include "HttpReply.h"
#include "HttpRequest.h"
#include "HttpConn.h"

extern void engineProcessRequest(HttpRequest *req);
extern void engineProcessReply(HttpReply *rep);
