
/*
 * $Id$
 *
 * DEBUG: section 9     File Transfer Protocol (FTP)
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  
 */

#include "squid.h"

static const char *const crlf = "\r\n";
static char cbuf[1024];

typedef enum {
    BEGIN,
    SENT_USER,
    SENT_PASS,
    SENT_TYPE,
    SENT_MDTM,
    SENT_SIZE,
    SENT_PORT,
    SENT_PASV,
    SENT_CWD,
    SENT_LIST,
    SENT_NLST,
    SENT_REST,
    SENT_RETR,
    SENT_STOR,
    SENT_QUIT,
    READING_DATA,
    WRITING_DATA,
    SENT_MKDIR
} ftp_state_t;

struct _ftp_flags {
    int isdir:1;
    int pasv_supported:1;
    int skip_whitespace:1;
    int rest_supported:1;
    int pasv_only:1;
    int authenticated:1;
    int http_header_sent:1;
    int tried_nlst:1;
    int use_base:1;
    int root_dir:1;
    int no_dotdot:1;
    int html_header_sent:1;
    int binary:1;
    int try_slash_hack:1;
    int put:1;
    int put_mkdir:1;
    int listformat_unknown:1;
    int datachannel_hack:1;
};

typedef struct _Ftpdata {
    StoreEntry *entry;
    request_t *request;
    char user[MAX_URL];
    char password[MAX_URL];
    char *reply_hdr;
    int reply_hdr_state;
    char *title_url;
    int conn_att;
    int login_att;
    ftp_state_t state;
    time_t mdtm;
    int size;
    wordlist *pathcomps;
    char *filepath;
    int restart_offset;
    int rest_att;
    char *proxy_host;
    size_t list_width;
    wordlist *cwd_message;
    char *old_request;
    char *old_reply;
    char *old_filepath;
    char typecode;
    struct {
	int fd;
	char *buf;
	size_t size;
	off_t offset;
	FREE *freefunc;
	wordlist *message;
	char *last_command;
	char *last_reply;
	int replycode;
    } ctrl;
    struct {
	int fd;
	char *buf;
	size_t size;
	off_t offset;
	FREE *freefunc;
	char *host;
	u_short port;
    } data;
    struct _ftp_flags flags;
} FtpStateData;

typedef struct {
    char type;
    int size;
    char *date;
    char *name;
    char *showname;
    char *link;
} ftpListParts;

typedef void (FTPSM) (FtpStateData *);

/* Local functions */
static CNCB ftpPasvCallback;
static PF ftpDataRead;
static PF ftpStateFree;
static PF ftpTimeout;
static PF ftpReadControlReply;
static CWCB ftpWriteCommandCallback;
static void ftpLoginParser(const char *, FtpStateData *);
static wordlist *ftpParseControlReply(char *, size_t, int *, int *);
static void ftpAppendSuccessHeader(FtpStateData * ftpState);
static void ftpAuthRequired(HttpReply * reply, request_t * request, const char *realm);
static void ftpHackShortcut(FtpStateData * ftpState, FTPSM * nextState);
static void ftpPutStart(FtpStateData *);
static CWCB ftpPutTransferDone;
static void ftpUnhack(FtpStateData * ftpState);
static void ftpScheduleReadControlReply(FtpStateData *, int);
static void ftpHandleControlReply(FtpStateData *);
static char *ftpHtmlifyListEntry(char *line, FtpStateData * ftpState);

/* State machine functions
 * send == state transition
 * read == wait for response, and select next state transition
 * other == Transition logic
 */
static FTPSM ftpReadWelcome;
static FTPSM ftpSendUser;
static FTPSM ftpReadUser;
static FTPSM ftpSendPass;
static FTPSM ftpReadPass;
static FTPSM ftpSendType;
static FTPSM ftpReadType;
static FTPSM ftpSendMdtm;
static FTPSM ftpReadMdtm;
static FTPSM ftpSendSize;
static FTPSM ftpReadSize;
static FTPSM ftpSendPort;
static FTPSM ftpReadPort;
static FTPSM ftpSendPasv;
static FTPSM ftpReadPasv;
static FTPSM ftpTraverseDirectory;
static FTPSM ftpListDir;
static FTPSM ftpGetFile;
static FTPSM ftpSendCwd;
static FTPSM ftpReadCwd;
static FTPSM ftpSendList;
static FTPSM ftpSendNlst;
static FTPSM ftpReadList;
static FTPSM ftpSendRest;
static FTPSM ftpReadRest;
static FTPSM ftpSendRetr;
static FTPSM ftpReadRetr;
static FTPSM ftpReadTransferDone;
static FTPSM ftpSendQuit;
static FTPSM ftpReadQuit;
static FTPSM ftpFail;
static FTPSM ftpDataTransferDone;
static FTPSM ftpRestOrList;
static FTPSM ftpSendStor;
static FTPSM ftpReadStor;
static FTPSM ftpSendReply;
static FTPSM ftpTryMkdir;
static FTPSM ftpReadMkdir;
/************************************************
** State Machine Description (excluding hacks) **
*************************************************
From			To
---------------------------------------
Welcome			User
User			Pass
Pass			Type
Type			TraverseDirectory / GetFile
TraverseDirectory	Cwd / GetFile / ListDir
Cwd			TraverseDirectory
GetFile			Mdtm
Mdtm			Size
Size			Pasv
ListDir			Pasv
Pasv			RestOrList
RestOrList		Rest / Retr / Nlst / List
Rest			Retr
Retr / Nlst / List	(ftpDataRead on datachannel)
(ftpDataRead)		ReadTransferDone
ReadTransferDone	DataTransferDone
DataTransferDone	Quit
Quit			-
************************************************/

FTPSM *FTP_SM_FUNCS[] =
{
    ftpReadWelcome,
    ftpReadUser,
    ftpReadPass,
    ftpReadType,
    ftpReadMdtm,
    ftpReadSize,
    ftpReadPort,
    ftpReadPasv,
    ftpReadCwd,
    ftpReadList,		/* SENT_LIST */
    ftpReadList,		/* SENT_NLST */
    ftpReadRest,
    ftpReadRetr,
    ftpReadStor,
    ftpReadQuit,
    ftpReadTransferDone,
    ftpSendReply,
    ftpReadMkdir
};

static void
ftpStateFree(int fdnotused, void *data)
{
    FtpStateData *ftpState = data;
    if (ftpState == NULL)
	return;
    debug(9, 3) ("ftpStateFree: %s\n", storeUrl(ftpState->entry));
    storeUnregisterAbort(ftpState->entry);
    storeUnlockObject(ftpState->entry);
    if (ftpState->reply_hdr) {
	memFree(MEM_8K_BUF, ftpState->reply_hdr);
	ftpState->reply_hdr = NULL;
    }
    requestUnlink(ftpState->request);
    if (ftpState->ctrl.buf)
	ftpState->ctrl.freefunc(ftpState->ctrl.buf);
    if (ftpState->data.buf)
	ftpState->data.freefunc(ftpState->data.buf);
    if (ftpState->pathcomps)
	wordlistDestroy(&ftpState->pathcomps);
    if (ftpState->ctrl.message)
	wordlistDestroy(&ftpState->ctrl.message);
    if (ftpState->cwd_message)
	wordlistDestroy(&ftpState->cwd_message);
    safe_free(ftpState->ctrl.last_reply);
    safe_free(ftpState->ctrl.last_command);
    safe_free(ftpState->old_request);
    safe_free(ftpState->old_reply);
    safe_free(ftpState->old_filepath);
    safe_free(ftpState->title_url);
    safe_free(ftpState->filepath);
    safe_free(ftpState->data.host);
    if (ftpState->data.fd > -1) {
	comm_close(ftpState->data.fd);
	ftpState->data.fd = -1;
    }
    cbdataFree(ftpState);
}

static void
ftpLoginParser(const char *login, FtpStateData * ftpState)
{
    char *s = NULL;
    xstrncpy(ftpState->user, login, MAX_URL);
    if ((s = strchr(ftpState->user, ':'))) {
	*s = 0;
	xstrncpy(ftpState->password, s + 1, MAX_URL);
	rfc1738_unescape(ftpState->password);
    } else {
	xstrncpy(ftpState->password, null_string, MAX_URL);
    }
    rfc1738_unescape(ftpState->user);
    if (ftpState->user[0] || ftpState->password[0])
	return;
    xstrncpy(ftpState->user, "anonymous", MAX_URL);
    xstrncpy(ftpState->password, Config.Ftp.anon_user, MAX_URL);
}

static void
ftpTimeout(int fd, void *data)
{
    FtpStateData *ftpState = data;
    StoreEntry *entry = ftpState->entry;
    ErrorState *err;
    debug(9, 4) ("ftpTimeout: FD %d: '%s'\n", fd, storeUrl(entry));
    if (entry->store_status == STORE_PENDING) {
	if (entry->mem_obj->inmem_hi == 0) {
	    err = errorCon(ERR_READ_TIMEOUT, HTTP_GATEWAY_TIMEOUT);
	    err->request = requestLink(ftpState->request);
	    errorAppendEntry(entry, err);
	} else {
	    storeAbort(entry, 0);
	}
    }
    if (ftpState->data.fd > -1) {
	comm_close(ftpState->data.fd);
	ftpState->data.fd = -1;
    }
    comm_close(ftpState->ctrl.fd);
    /* don't modify ftpState here, it has been freed */
}

static void
ftpListingStart(FtpStateData * ftpState)
{
    StoreEntry *e = ftpState->entry;
    wordlist *w;
    char *dirup;
    storeBuffer(e);
    storeAppendPrintf(e, "<!-- HTML listing generated by Squid %s -->\n",
	version_string);
    storeAppendPrintf(e, "<!-- %s -->\n", mkrfc1123(squid_curtime));
    storeAppendPrintf(e, "<HTML><HEAD><TITLE>\n");
    storeAppendPrintf(e, "FTP Directory: %s\n",
	ftpState->title_url);
    storeAppendPrintf(e, "</TITLE>\n");
    if (ftpState->flags.use_base)
	storeAppendPrintf(e, "<BASE HREF=\"%s\">\n",
	    ftpState->title_url);
    storeAppendPrintf(e, "</HEAD><BODY>\n");
    if (ftpState->cwd_message) {
	storeAppendPrintf(e, "<PRE>\n");
	for (w = ftpState->cwd_message; w; w = w->next)
	    storeAppendPrintf(e, "%s\n", w->key);
	storeAppendPrintf(e, "</PRE>\n");
	storeAppendPrintf(e, "<HR>\n");
	wordlistDestroy(&ftpState->cwd_message);
    }
    storeAppendPrintf(e, "<H2>\n");
    storeAppendPrintf(e, "FTP Directory: %s\n", ftpState->title_url);
    storeAppendPrintf(e, "</H2>\n");
    storeAppendPrintf(e, "<PRE>\n");
    dirup = ftpHtmlifyListEntry("<internal-dirup>", ftpState);
    storeAppend(e, dirup, strlen(dirup));
    storeBufferFlush(e);
    ftpState->flags.html_header_sent = 1;
}

static void
ftpListingFinish(FtpStateData * ftpState)
{
    StoreEntry *e = ftpState->entry;
    storeBuffer(e);
    storeAppendPrintf(e, "</PRE>\n");
    if (ftpState->flags.listformat_unknown && !ftpState->flags.tried_nlst) {
	storeAppendPrintf(e, "<A HREF=\"./;type=d\">[As plain directory]</A>\n");
    } else if (ftpState->typecode == 'D') {
	storeAppendPrintf(e, "<A HREF=\"./\">[As extended directory]</A>\n");
    }
    storeAppendPrintf(e, "<HR>\n");
    storeAppendPrintf(e, "<ADDRESS>\n");
    storeAppendPrintf(e, "Generated %s by %s (<a href=\"http://squid.nlanr.net/Squid/\">%s</a>)\n",
	mkrfc1123(squid_curtime),
	getMyHostname(),
	full_appname_string,
	version_string);
    storeAppendPrintf(e, "</ADDRESS></BODY></HTML>\n");
    storeBufferFlush(e);
}

static const char *Month[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int
is_month(const char *buf)
{
    int i;
    for (i = 0; i < 12; i++)
	if (!strcasecmp(buf, Month[i]))
	    return 1;
    return 0;
}


static void
ftpListPartsFree(ftpListParts ** parts)
{
    safe_free((*parts)->date);
    safe_free((*parts)->name);
    safe_free((*parts)->showname);
    safe_free((*parts)->link);
    safe_free(*parts);
}

#define MAX_TOKENS 64

#define SCAN_FTP1       "%[0123456789]"
#define SCAN_FTP2       "%[0123456789:]"
#define SCAN_FTP3       "%[0123456789]-%[0123456789]-%[0123456789]"
#define SCAN_FTP4       "%[0123456789]:%[0123456789]%[AaPp]%[Mm]"

static ftpListParts *
ftpListParseParts(const char *buf, struct _ftp_flags flags)
{
    ftpListParts *p = NULL;
    char *t = NULL;
    const char *ct = NULL;
    char *tokens[MAX_TOKENS];
    int i;
    int n_tokens;
    static char sbuf[128];
    char *xbuf = NULL;
    if (buf == NULL)
	return NULL;
    if (*buf == '\0')
	return NULL;
    p = xcalloc(1, sizeof(ftpListParts));
    n_tokens = 0;
    for (i = 0; i < MAX_TOKENS; i++)
	tokens[i] = (char *) NULL;
    xbuf = xstrdup(buf);
    if (flags.tried_nlst) {
	/* Machine readable format, one name per line */
	p->name = xbuf;
	p->type = '\0';
	return p;
    }
    for (t = strtok(xbuf, w_space); t && n_tokens < MAX_TOKENS; t = strtok(NULL, w_space))
	tokens[n_tokens++] = xstrdup(t);
    xfree(xbuf);
    /* locate the Month field */
    for (i = 3; i < n_tokens - 3; i++) {
	if (!is_month(tokens[i]))	/* Month */
	    continue;
	if (!sscanf(tokens[i - 1], SCAN_FTP1, sbuf))	/* Size */
	    continue;
	if (!sscanf(tokens[i + 1], SCAN_FTP1, sbuf))	/* Day */
	    continue;
	if (!sscanf(tokens[i + 2], SCAN_FTP2, sbuf))	/* Yr | hh:mm */
	    continue;
	p->type = *tokens[0];
	p->size = atoi(tokens[i - 1]);
	snprintf(sbuf, 128, "%s %2s %5s",
	    tokens[i], tokens[i + 1], tokens[i + 2]);
	if (!strstr(buf, sbuf))
	    snprintf(sbuf, 128, "%s %2s %-5s",
		tokens[i], tokens[i + 1], tokens[i + 2]);
	if ((t = strstr(buf, sbuf))) {
	    p->date = xstrdup(sbuf);
	    if (flags.skip_whitespace) {
		t += strlen(sbuf);
		while (strchr(w_space, *t))
		    t++;
	    } else {
		/* XXX assumes a single space between date and filename
		 * suggested by:  Nathan.Bailey@cc.monash.edu.au and
		 * Mike Battersby <mike@starbug.bofh.asn.au> */
		t += strlen(sbuf) + 1;
	    }
	    p->name = xstrdup(t);
	    if ((t = strstr(p->name, " -> "))) {
		*t = '\0';
		p->link = xstrdup(t + 4);
	    }
	}
	break;
    }
    /* try it as a DOS listing */
    if (n_tokens > 3 && p->name == NULL &&
	sscanf(tokens[0], SCAN_FTP3, sbuf, sbuf, sbuf) == 3 &&
    /* 04-05-70 */
	sscanf(tokens[1], SCAN_FTP4, sbuf, sbuf, sbuf, sbuf) == 4) {
	/* 09:33PM */
	if (!strcasecmp(tokens[2], "<dir>")) {
	    p->type = 'd';
	} else {
	    p->type = '-';
	    p->size = atoi(tokens[2]);
	}
	snprintf(sbuf, 128, "%s %s", tokens[0], tokens[1]);
	p->date = xstrdup(sbuf);
	p->name = xstrdup(tokens[3]);
    }
    /* Try EPLF format; carson@lehman.com */
    if (p->name == NULL && buf[0] == '+') {
	ct = buf + 1;
	p->type = 0;
	while (ct && *ct) {
	    switch (*ct) {
	    case '\t':
		sscanf(ct + 1, "%[^,]", sbuf);
		p->name = xstrdup(sbuf);
		break;
	    case 's':
		sscanf(ct + 1, "%d", &(p->size));
		break;
	    case 'm':
		sscanf(ct + 1, "%d", &i);
		p->date = xstrdup(ctime((time_t *) & i));
		*(strstr(p->date, "\n")) = '\0';
		break;
	    case '/':
		p->type = 'd';
		break;
	    case 'r':
		p->type = '-';
		break;
	    case 'i':
		break;
	    default:
		break;
	    }
	    ct = strstr(ct, ",");
	    if (ct) {
		ct++;
	    }
	}
	if (p->type == 0) {
	    p->type = '-';
	}
    }
    for (i = 0; i < n_tokens; i++)
	xfree(tokens[i]);
    if (p->name == NULL)
	ftpListPartsFree(&p);
    return p;
}

static const char *
dots_fill(size_t len)
{
    static char buf[256];
    int i = 0;
    if (len > Config.Ftp.list_width) {
	memset(buf, ' ', 256);
	buf[0] = '\n';
	buf[Config.Ftp.list_width + 4] = '\0';
	return buf;
    }
    for (i = (int) len; i < Config.Ftp.list_width; i++)
	buf[i - len] = (i % 2) ? '.' : ' ';
    buf[i - len] = '\0';
    return buf;
}

static char *
ftpHtmlifyListEntry(char *line, FtpStateData * ftpState)
{
    LOCAL_ARRAY(char, link, 2048 + 40);
    LOCAL_ARRAY(char, link2, 2048 + 40);
    LOCAL_ARRAY(char, icon, 2048);
    LOCAL_ARRAY(char, html, 8192);
    size_t width = Config.Ftp.list_width;
    ftpListParts *parts;
    if ((int) strlen(line) > 1024) {
	snprintf(html, 8192, "%s\n", line);
	return html;
    }
    /* Handle builtin <dirup> */
    if (!strcmp(line, "<internal-dirup>")) {
	snprintf(icon, 2048, "<IMG BORDER=0 SRC=\"%s\" ALT=\"%-6s\">",
	    mimeGetIconURL("internal-dirup"),
	    "[DIRUP]");
	if (!ftpState->flags.no_dotdot && !ftpState->flags.root_dir) {
	    /* Normal directory */
	    snprintf(link, 2048, "<A HREF=\"%s\">%s</A>",
		"../",
		"Parent Directory");
	} else if (!ftpState->flags.no_dotdot && ftpState->flags.root_dir) {
	    /* "Top level" directory */
	    snprintf(link, 2048, "<A HREF=\"%s\">%s</A> (<A HREF=\"%s\">%s</A>)",
		"%2e%2e/",
		"Parent Directory",
		"%2f/",
		"Root Directory");
	} else if (ftpState->flags.no_dotdot && !ftpState->flags.root_dir) {
	    /* Normal directory where last component is / or ..  */
	    snprintf(link, 2048, "<A HREF=\"%s\">%s</A> (<A HREF=\"%s\">%s</A>)",
		"%2e%2e/",
		"Parent Directory",
		"../",
		"Up");
	} else {		/* NO_DOTDOT && ROOT_DIR */
	    /* "UNIX Root" directory */
	    snprintf(link, 2048, "<A HREF=\"%s\">%s</A>",
		"../",
		"Home Directory");
	}
	snprintf(html, 8192, "%s %s\n", icon, link);
	return html;
    }
    if ((parts = ftpListParseParts(line, ftpState->flags)) == NULL) {
	char *p;
	snprintf(html, 8192, "%s\n", line);
	for (p = line; *p && isspace(*p); p++);
	if (*p && !isspace(*p))
	    ftpState->flags.listformat_unknown = 1;
	return html;
    }
    if (!strcmp(parts->name, ".") || !strcmp(parts->name, "..")) {
	*html = '\0';
	ftpListPartsFree(&parts);
	return html;
    }
    parts->size += 1023;
    parts->size >>= 10;
    parts->showname = xstrdup(parts->name);
    if (!Config.Ftp.list_wrap) {
	if (strlen(parts->showname) > width - 1) {
	    *(parts->showname + width - 1) = '>';
	    *(parts->showname + width - 0) = '\0';
	}
    }
    switch (parts->type) {
    case 'd':
	snprintf(icon, 2048, "<IMG SRC=\"%s\" ALT=\"%-6s\">",
	    mimeGetIconURL("internal-dir"),
	    "[DIR]");
	snprintf(link, 2048, "<A HREF=\"%s/\">%s</A>%s",
	    rfc1738_escape(parts->name),
	    parts->showname,
	    dots_fill(strlen(parts->showname)));
	snprintf(html, 8192, "%s %s  [%s]\n",
	    icon,
	    link,
	    parts->date);
	break;
    case 'l':
	snprintf(icon, 2048, "<IMG SRC=\"%s\" ALT=\"%-6s\">",
	    mimeGetIconURL("internal-link"),
	    "[LINK]");
	snprintf(link, 2048, "<A HREF=\"%s\">%s</A>%s",
	    rfc1738_escape(parts->name),
	    parts->showname,
	    dots_fill(strlen(parts->showname)));
	/* sometimes there is an 'l' flag, but no "->" link */
	if (parts->link)
	    snprintf(link2, 2048, "<A HREF=\"%s\">%s</A>",
		rfc1738_escape(parts->link),
		parts->link);
	snprintf(html, 8192, "%s %s  [%s] -> %s\n",
	    icon,
	    link,
	    parts->date,
	    link2);
	break;
    case '\0':
	snprintf(icon, 2048, "<IMG SRC=\"%s\" ALT=\"%-6s\">",
	    mimeGetIconURL(parts->name),
	    "[UNKNOWN]");
	snprintf(link, 2048, "<A HREF=\"%s\">%s</A>",
	    rfc1738_escape(parts->name),
	    parts->name);
	snprintf(link2, 2048, "(<A HREF=\"%s/;type=d\">chdir</A>)",
	    rfc1738_escape(parts->name));
	snprintf(html, 8192, "%s %s %s\n",
	    icon,
	    link,
	    link2);
	break;
    case '-':
    default:
	snprintf(icon, 2048, "<IMG SRC=\"%s\" ALT=\"%-6s\">",
	    mimeGetIconURL(parts->name),
	    "[FILE]");
	snprintf(link, 2048, "<A HREF=\"%s\">%s</A>%s",
	    rfc1738_escape(parts->name),
	    parts->showname,
	    dots_fill(strlen(parts->showname)));
	snprintf(html, 8192, "%s %s  [%s] %6dk\n",
	    icon,
	    link,
	    parts->date,
	    parts->size);
	break;
    }
    ftpListPartsFree(&parts);
    return html;
}

static void
ftpParseListing(FtpStateData * ftpState)
{
    char *buf = ftpState->data.buf;
    char *sbuf;			/* NULL-terminated copy of buf */
    char *end;
    char *line;
    char *s;
    char *t;
    size_t linelen;
    size_t usable;
    StoreEntry *e = ftpState->entry;
    int len = ftpState->data.offset;
    /*
     * We need a NULL-terminated buffer for scanning, ick
     */
    sbuf = xmalloc(len + 1);
    xstrncpy(sbuf, buf, len + 1);
    end = sbuf + len - 1;
    while (*end != '\r' && *end != '\n' && end > sbuf)
	end--;
    usable = end - sbuf;
    debug(9, 3) ("ftpParseListing: usable = %d\n", usable);
    if (usable == 0) {
	debug(9, 3) ("ftpParseListing: didn't find end for %s\n", storeUrl(e));
	xfree(sbuf);
	return;
    }
    debug(9, 3) ("ftpParseListing: %d bytes to play with\n", len);
    line = memAllocate(MEM_4K_BUF);
    end++;
    storeBuffer(e);
    s = sbuf;
    s += strspn(s, crlf);
    for (; s < end; s += strcspn(s, crlf), s += strspn(s, crlf)) {
	debug(9, 3) ("ftpParseListing: s = {%s}\n", s);
	linelen = strcspn(s, crlf) + 1;
	if (linelen < 2)
	    break;
	if (linelen > 4096)
	    linelen = 4096;
	xstrncpy(line, s, linelen);
	debug(9, 7) ("ftpParseListing: {%s}\n", line);
	if (!strncmp(line, "total", 5))
	    continue;
	t = ftpHtmlifyListEntry(line, ftpState);
	assert(t != NULL);
	storeAppend(e, t, strlen(t));
    }
    storeBufferFlush(e);
    assert(usable <= len);
    if (usable < len) {
	/* must copy partial line to beginning of buf */
	linelen = len - usable;
	if (linelen > 4096)
	    linelen = 4096;
	xstrncpy(line, end, linelen);
	xstrncpy(ftpState->data.buf, line, ftpState->data.size);
	ftpState->data.offset = strlen(ftpState->data.buf);
    }
    memFree(MEM_4K_BUF, line);
    xfree(sbuf);
}

static void
ftpReadComplete(FtpStateData * ftpState)
{
    debug(9, 3) ("ftpReadComplete\n");
    /* Connection closed; retrieval done. */
    if (ftpState->flags.html_header_sent)
	ftpListingFinish(ftpState);
    if (!ftpState->flags.put) {
	storeTimestampsSet(ftpState->entry);
	storeComplete(ftpState->entry);
    }
    /* expect the "transfer complete" message on the control socket */
    ftpScheduleReadControlReply(ftpState, 1);
}

static void
ftpDataRead(int fd, void *data)
{
    FtpStateData *ftpState = data;
    int len;
    int j;
    int bin;
    StoreEntry *entry = ftpState->entry;
    MemObject *mem = entry->mem_obj;
    assert(fd == ftpState->data.fd);
    if (fwdAbortFetch(entry)) {
	storeAbort(entry, 0);
	ftpDataTransferDone(ftpState);
	return;
    }
    errno = 0;
    memset(ftpState->data.buf + ftpState->data.offset, '\0',
	ftpState->data.size - ftpState->data.offset);
    len = read(fd,
	ftpState->data.buf + ftpState->data.offset,
	ftpState->data.size - ftpState->data.offset);
    if (len > 0) {
	fd_bytes(fd, len, FD_READ);
	kb_incr(&Counter.server.all.kbytes_in, len);
	kb_incr(&Counter.server.ftp.kbytes_in, len);
	ftpState->data.offset += len;
    }
    debug(9, 5) ("ftpDataRead: FD %d, Read %d bytes\n", fd, len);
    if (len > 0) {
	IOStats.Ftp.reads++;
	for (j = len - 1, bin = 0; j; bin++)
	    j >>= 1;
	IOStats.Ftp.read_hist[bin]++;
    }
    if (ftpState->flags.isdir && !ftpState->flags.html_header_sent && len >= 0) {
	ftpListingStart(ftpState);
    }
    if (len < 0) {
	debug(50, 1) ("ftpDataRead: read error: %s\n", xstrerror());
	if (ignoreErrno(errno)) {
	    commSetSelect(fd,
		COMM_SELECT_READ,
		ftpDataRead,
		data,
		Config.Timeout.read);
	} else {
	    assert(mem->inmem_hi > 0);
	    storeAbort(entry, 0);
	    ftpDataTransferDone(ftpState);
	}
    } else if (len == 0) {
	ftpReadComplete(ftpState);
    } else {
	if (ftpState->flags.isdir) {
	    ftpParseListing(ftpState);
	} else {
	    storeAppend(entry, ftpState->data.buf, len);
	    ftpState->data.offset = 0;
	}
	if (ftpState->size > 0 && mem->inmem_hi >= ftpState->size + mem->reply->hdr_sz)
	    ftpReadComplete(ftpState);
	else
	    commSetSelect(fd,
		COMM_SELECT_READ,
		ftpDataRead,
		data,
		Config.Timeout.read);
    }
}

/*
 * ftpCheckAuth
 *
 * Return 1 if we have everything needed to complete this request.
 * Return 0 if something is missing.
 */
static int
ftpCheckAuth(FtpStateData * ftpState, const HttpHeader * req_hdr)
{
    char *orig_user;
    const char *auth;
    ftpLoginParser(ftpState->request->login, ftpState);
    if (ftpState->user[0] && ftpState->password[0])
	return 1;		/* name and passwd both in URL */
    if (!ftpState->user[0] && !ftpState->password[0])
	return 1;		/* no name or passwd */
    if (ftpState->password[0])
	return 1;		/* passwd with no name? */
    /* URL has name, but no passwd */
    if (!(auth = httpHeaderGetAuth(req_hdr, HDR_AUTHORIZATION, "Basic")))
	return 0;		/* need auth header */
    orig_user = xstrdup(ftpState->user);
    ftpLoginParser(auth, ftpState);
    if (!strcmp(orig_user, ftpState->user)) {
	xfree(orig_user);
	return 1;		/* same username */
    }
    strcpy(ftpState->user, orig_user);
    xfree(orig_user);
    return 0;			/* different username */
}

static void
ftpCheckUrlpath(FtpStateData * ftpState)
{
    request_t *request = ftpState->request;
    int l;
    const char *t;
    if ((t = strRChr(request->urlpath, ';')) != NULL) {
	if (strncasecmp(t + 1, "type=", 5) == 0) {
	    ftpState->typecode = (char) toupper((int) *(t + 6));
	    strSet(request->urlpath, t, '\0');
	}
    }
    l = strLen(request->urlpath);
    ftpState->flags.use_base = 1;
    /* check for null path */
    if (!l) {
	stringReset(&request->urlpath, "/");
	ftpState->flags.isdir = 1;
	ftpState->flags.root_dir = 1;
    } else if (!strCmp(request->urlpath, "/%2f/")) {
	ftpState->flags.isdir = 1;
	ftpState->flags.root_dir = 1;
    } else if ((l >= 1) && (*(strBuf(request->urlpath) + l - 1) == '/')) {
	ftpState->flags.isdir = 1;
	ftpState->flags.use_base = 0;
	if (l == 1)
	    ftpState->flags.root_dir = 1;
    }
}

static void
ftpBuildTitleUrl(FtpStateData * ftpState)
{
    request_t *request = ftpState->request;
    size_t len;
    char *t;
    len = 64
	+ strlen(ftpState->user)
	+ strlen(ftpState->password)
	+ strlen(request->host)
	+ strLen(request->urlpath);
    t = ftpState->title_url = xcalloc(len, 1);
    strcat(t, "ftp://");
    if (strcmp(ftpState->user, "anonymous")) {
	strcat(t, ftpState->user);
	strcat(t, "@");
    }
    strcat(t, request->host);
    if (request->port != urlDefaultPort(PROTO_FTP))
	snprintf(&t[strlen(t)], len - strlen(t), ":%d", request->port);
    strcat(t, strBuf(request->urlpath));
}

void
ftpStart(request_t * request, StoreEntry * entry, int fd)
{
    LOCAL_ARRAY(char, realm, 8192);
    const char *url = storeUrl(entry);
    FtpStateData *ftpState = xcalloc(1, sizeof(FtpStateData));
    HttpReply *reply;
    cbdataAdd(ftpState, MEM_NONE);
    debug(9, 3) ("FtpStart: '%s'\n", url);
    Counter.server.all.requests++;
    Counter.server.ftp.requests++;
    storeLockObject(entry);
    ftpState->entry = entry;
    ftpState->request = requestLink(request);
    ftpState->ctrl.fd = fd;
    ftpState->data.fd = -1;
    ftpState->size = -1;
    ftpState->flags.pasv_supported = 1;
    ftpState->flags.rest_supported = 1;
    if (ftpState->request->method == METHOD_PUT)
	ftpState->flags.put = 1;
    if (!ftpCheckAuth(ftpState, &request->header)) {
	/* This request is not fully authenticated */
	if (request->port == 21) {
	    snprintf(realm, 8192, "ftp %s", ftpState->user);
	} else {
	    snprintf(realm, 8192, "ftp %s port %d",
		ftpState->user, request->port);
	}
	/* create reply */
	reply = entry->mem_obj->reply;
	assert(reply != NULL);
	/* create appropriate reply */
	ftpAuthRequired(reply, request, realm);
	httpReplySwapOut(reply, entry);
	storeComplete(entry);
	ftpStateFree(-1, ftpState);
	return;
    }
    ftpCheckUrlpath(ftpState);
    ftpBuildTitleUrl(ftpState);
    debug(9, 5) ("FtpStart: host=%s, path=%s, user=%s, passwd=%s\n",
	ftpState->request->host, strBuf(ftpState->request->urlpath),
	ftpState->user, ftpState->password);
    comm_add_close_handler(fd, ftpStateFree, ftpState);
    ftpState->state = BEGIN;
    ftpState->ctrl.buf = memAllocate(MEM_4K_BUF);
    ftpState->ctrl.freefunc = memFree4K;
    ftpState->ctrl.size = 4096;
    ftpState->ctrl.offset = 0;
    ftpState->data.buf = xmalloc(SQUID_TCP_SO_RCVBUF);
    ftpState->data.size = SQUID_TCP_SO_RCVBUF;
    ftpState->data.freefunc = xfree;
    ftpScheduleReadControlReply(ftpState, 0);
    commSetTimeout(fd, Config.Timeout.read, ftpTimeout, ftpState);
}

/* ====================================================================== */

static void
ftpWriteCommand(const char *buf, FtpStateData * ftpState)
{
    debug(9, 5) ("ftpWriteCommand: %s\n", buf);
    safe_free(ftpState->ctrl.last_command);
    ftpState->ctrl.last_command = xstrdup(buf);
    comm_write(ftpState->ctrl.fd,
	xstrdup(buf),
	strlen(buf),
	ftpWriteCommandCallback,
	ftpState,
	xfree);
    ftpScheduleReadControlReply(ftpState, 0);
}

static void
ftpWriteCommandCallback(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    FtpStateData *ftpState = data;
    StoreEntry *entry = ftpState->entry;
    ErrorState *err;
    debug(9, 7) ("ftpWriteCommandCallback: wrote %d bytes\n", size);
    if (size > 0) {
	fd_bytes(fd, size, FD_WRITE);
	kb_incr(&Counter.server.all.kbytes_out, size);
	kb_incr(&Counter.server.ftp.kbytes_out, size);
    }
    if (errflag == COMM_ERR_CLOSING)
	return;
    if (errflag) {
	debug(50, 1) ("ftpWriteCommandCallback: FD %d: %s\n", fd, xstrerror());
	if (entry->mem_obj->inmem_hi == 0) {
	    err = errorCon(ERR_WRITE_ERROR, HTTP_SERVICE_UNAVAILABLE);
	    err->xerrno = errno;
	    err->request = requestLink(ftpState->request);
	    errorAppendEntry(entry, err);
	}
	if (entry->store_status == STORE_PENDING)
	    storeAbort(entry, 0);
	comm_close(ftpState->ctrl.fd);
    }
}

static wordlist *
ftpParseControlReply(char *buf, size_t len, int *codep, int *used)
{
    char *s;
    char *sbuf;
    char *end;
    int usable;
    int complete = 0;
    wordlist *head = NULL;
    wordlist *list;
    wordlist **tail = &head;
    off_t offset;
    size_t linelen;
    int code = -1;
    debug(9, 5) ("ftpParseControlReply\n");
    /*
     * We need a NULL-terminated buffer for scanning, ick
     */
    sbuf = xmalloc(len + 1);
    xstrncpy(sbuf, buf, len + 1);
    end = sbuf + len - 1;
    while (*end != '\r' && *end != '\n' && end > sbuf)
	end--;
    usable = end - sbuf;
    debug(9, 3) ("ftpParseControlReply: usable = %d\n", usable);
    if (usable == 0) {
	debug(9, 3) ("ftpParseControlReply: didn't find end of line\n");
	xfree(sbuf);
	return NULL;
    }
    debug(9, 3) ("ftpParseControlReply: %d bytes to play with\n", len);
    end++;
    s = sbuf;
    s += strspn(s, crlf);
    for (; s < end; s += strcspn(s, crlf), s += strspn(s, crlf)) {
	if (complete)
	    break;
	debug(9, 3) ("ftpParseControlReply: s = {%s}\n", s);
	linelen = strcspn(s, crlf) + 1;
	if (linelen < 2)
	    break;
	if (linelen > 3)
	    complete = (*s >= '0' && *s <= '9' && *(s + 3) == ' ');
	if (complete)
	    code = atoi(s);
	offset = 0;
	if (linelen > 3)
	    if (*s >= '0' && *s <= '9' && (*(s + 3) == '-' || *(s + 3) == ' '))
		offset = 4;
	list = xcalloc(1, sizeof(wordlist));
	list->key = xmalloc(linelen - offset);
	xstrncpy(list->key, s + offset, linelen - offset);
	debug(9, 7) ("%d %s\n", code, list->key);
	*tail = list;
	tail = &list->next;
    }
    *used = (int) (s - sbuf);
    if (!complete)
	wordlistDestroy(&head);
    if (codep)
	*codep = code;
    return head;
}

static void
ftpScheduleReadControlReply(FtpStateData * ftpState, int buffered_ok)
{
    debug(9, 3) ("ftpScheduleReadControlReply: FD %d\n", ftpState->ctrl.fd);
    if (buffered_ok && ftpState->ctrl.offset > 0) {
	/* We've already read some reply data */
	ftpHandleControlReply(ftpState);
    } else {
	commSetSelect(ftpState->ctrl.fd,
	    COMM_SELECT_READ,
	    ftpReadControlReply,
	    ftpState,
	    Config.Timeout.read);
    }
}

static void
ftpReadControlReply(int fd, void *data)
{
    FtpStateData *ftpState = data;
    StoreEntry *entry = ftpState->entry;
    int len;
    ErrorState *err;
    debug(9, 5) ("ftpReadControlReply\n");
    assert(ftpState->ctrl.offset < ftpState->ctrl.size);
    len = read(fd,
	ftpState->ctrl.buf + ftpState->ctrl.offset,
	ftpState->ctrl.size - ftpState->ctrl.offset);
    if (len > 0) {
	fd_bytes(fd, len, FD_READ);
	kb_incr(&Counter.server.all.kbytes_in, len);
	kb_incr(&Counter.server.ftp.kbytes_in, len);
    }
    debug(9, 5) ("ftpReadControlReply: FD %d, Read %d bytes\n", fd, len);
    if (len < 0) {
	debug(50, 1) ("ftpReadControlReply: read error: %s\n", xstrerror());
	if (ignoreErrno(errno)) {
	    ftpScheduleReadControlReply(ftpState, 0);
	} else {
	    if (entry->mem_obj->inmem_hi == 0) {
		err = errorCon(ERR_READ_ERROR, HTTP_INTERNAL_SERVER_ERROR);
		err->xerrno = errno;
		err->request = requestLink(ftpState->request);
		errorAppendEntry(entry, err);
	    }
	    if (entry->store_status == STORE_PENDING)
		storeAbort(entry, 0);
	    comm_close(ftpState->ctrl.fd);
	}
	return;
    }
    if (len == 0) {
	if (entry->store_status == STORE_PENDING) {
	    storeReleaseRequest(entry);
	    if (entry->mem_obj->inmem_hi == 0) {
		err = errorCon(ERR_FTP_FAILURE, HTTP_INTERNAL_SERVER_ERROR);
		err->xerrno = 0;
		err->request = requestLink(ftpState->request);
		err->ftp_server_msg = ftpState->ctrl.message;
		errorAppendEntry(entry, err);
	    }
	}
	comm_close(ftpState->ctrl.fd);
	return;
    }
    len += ftpState->ctrl.offset;
    ftpState->ctrl.offset = len;
    assert(len <= ftpState->ctrl.size);
    ftpHandleControlReply(ftpState);
}

static void
ftpHandleControlReply(FtpStateData * ftpState)
{
    char *oldbuf;
    wordlist **W;
    int bytes_used = 0;
    wordlistDestroy(&ftpState->ctrl.message);
    ftpState->ctrl.message = ftpParseControlReply(ftpState->ctrl.buf,
	ftpState->ctrl.offset, &ftpState->ctrl.replycode, &bytes_used);
    if (ftpState->ctrl.message == NULL) {
	/* didn't get complete reply yet */
	if (ftpState->ctrl.offset == ftpState->ctrl.size) {
	    oldbuf = ftpState->ctrl.buf;
	    ftpState->ctrl.buf = xcalloc(ftpState->ctrl.size << 1, 1);
	    xmemcpy(ftpState->ctrl.buf, oldbuf, ftpState->ctrl.size);
	    ftpState->ctrl.size <<= 1;
	    ftpState->ctrl.freefunc(oldbuf);
	    ftpState->ctrl.freefunc = xfree;
	}
	ftpScheduleReadControlReply(ftpState, 0);
	return;
    } else if (ftpState->ctrl.offset == bytes_used) {
	/* used it all up */
	ftpState->ctrl.offset = 0;
    } else {
	/* Got some data past the complete reply */
	assert(bytes_used < ftpState->ctrl.offset);
	ftpState->ctrl.offset -= bytes_used;
	xmemmove(ftpState->ctrl.buf, ftpState->ctrl.buf + bytes_used,
	    ftpState->ctrl.offset);
    }
    for (W = &ftpState->ctrl.message; *W && (*W)->next; W = &(*W)->next);
    safe_free(ftpState->ctrl.last_reply);
    ftpState->ctrl.last_reply = (*W)->key;
    safe_free(*W);
    debug(9, 8) ("ftpReadControlReply: state=%d, code=%d\n", ftpState->state,
	ftpState->ctrl.replycode);
    FTP_SM_FUNCS[ftpState->state] (ftpState);
}

/* ====================================================================== */

static void
ftpReadWelcome(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("ftpReadWelcome\n");
    if (ftpState->flags.pasv_only)
	ftpState->login_att++;
    if (code == 220) {
	if (ftpState->ctrl.message)
	    if (strstr(ftpState->ctrl.message->key, "NetWare"))
		ftpState->flags.skip_whitespace = 1;
	ftpSendUser(ftpState);
    } else if (code == 120) {
	if (NULL != ftpState->ctrl.message)
	    debug(9, 3) ("FTP server is busy: %s\n",
		ftpState->ctrl.message->key);
	return;
    } else {
	ftpFail(ftpState);
    }
}

static void
ftpSendUser(FtpStateData * ftpState)
{
    if (ftpState->proxy_host != NULL)
	snprintf(cbuf, 1024, "USER %s@%s\r\n",
	    ftpState->user,
	    ftpState->request->host);
    else
	snprintf(cbuf, 1024, "USER %s\r\n", ftpState->user);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_USER;
}

static void
ftpReadUser(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("ftpReadUser\n");
    if (code == 230) {
	ftpReadPass(ftpState);
    } else if (code == 331) {
	ftpSendPass(ftpState);
    } else {
	ftpFail(ftpState);
    }
}

static void
ftpSendPass(FtpStateData * ftpState)
{
    snprintf(cbuf, 1024, "PASS %s\r\n", ftpState->password);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_PASS;
}

static void
ftpReadPass(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("ftpReadPass\n");
    if (code == 230) {
	ftpSendType(ftpState);
    } else {
	ftpFail(ftpState);
    }
}

static void
ftpSendType(FtpStateData * ftpState)
{
    const char *t;
    const char *filename;
    char mode;
    /*
     * Ref section 3.2.2 of RFC 1738
     */
    switch (mode = ftpState->typecode) {
    case 'D':
	mode = 'A';
	break;
    case 'A':
    case 'I':
	break;
    default:
	if (ftpState->flags.isdir) {
	    mode = 'A';
	} else {
	    t = strRChr(ftpState->request->urlpath, '/');
	    filename = t ? t + 1 : strBuf(ftpState->request->urlpath);
	    mode = mimeGetTransferMode(filename);
	}
	break;
    }
    if (mode == 'I')
	ftpState->flags.binary = 1;
    snprintf(cbuf, 1024, "TYPE %c\r\n", mode);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_TYPE;
}

static void
ftpReadType(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    wordlist *w;
    wordlist **T;
    char *path;
    char *d;
    debug(9, 3) ("This is ftpReadType\n");
    if (code == 200) {
	path = xstrdup(strBuf(ftpState->request->urlpath));
	T = &ftpState->pathcomps;
	for (d = strtok(path, "/"); d; d = strtok(NULL, "/")) {
	    rfc1738_unescape(d);
	    w = xcalloc(1, sizeof(wordlist));
	    w->key = xstrdup(d);
	    *T = w;
	    T = &w->next;
	}
	xfree(path);
	if (ftpState->pathcomps)
	    ftpTraverseDirectory(ftpState);
	else
	    ftpListDir(ftpState);
    } else {
	ftpFail(ftpState);
    }
}

static void
ftpTraverseDirectory(FtpStateData * ftpState)
{
    wordlist *w;
    debug(9, 4) ("ftpTraverseDirectory %s\n",
	ftpState->filepath ? ftpState->filepath : "<NULL>");

    safe_free(ftpState->filepath);
    /* Done? */
    if (ftpState->pathcomps == NULL) {
	debug(9, 3) ("the final component was a directory\n");
	ftpListDir(ftpState);
	return;
    }
    /* Go to next path component */
    w = ftpState->pathcomps;
    ftpState->filepath = w->key;
    ftpState->pathcomps = w->next;
    xfree(w);
    /* Check if we are to CWD or RETR */
    if (ftpState->pathcomps != NULL || ftpState->flags.isdir) {
	ftpSendCwd(ftpState);
    } else {
	debug(9, 3) ("final component is probably a file\n");
	ftpGetFile(ftpState);
	return;
    }
}

static void
ftpSendCwd(FtpStateData * ftpState)
{
    char *path = ftpState->filepath;
    debug(9, 3) ("ftpSendCwd\n");
    if (!strcmp(path, "..") || !strcmp(path, "/")) {
	ftpState->flags.no_dotdot = 1;
    } else {
	ftpState->flags.no_dotdot = 0;
    }
    snprintf(cbuf, 1024, "CWD %s\r\n", path);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_CWD;
}

static void
ftpReadCwd(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadCwd\n");
    if (code >= 200 && code < 300) {
	/* CWD OK */
	ftpUnhack(ftpState);
	if (ftpState->cwd_message)
	    wordlistDestroy(&ftpState->cwd_message);
	ftpState->cwd_message = ftpState->ctrl.message;
	ftpState->ctrl.message = NULL;
	/* Continue to traverse the path */
	ftpTraverseDirectory(ftpState);
    } else {
	/* CWD FAILED */
	if (!ftpState->flags.put)
	    ftpFail(ftpState);
	else
	    ftpTryMkdir(ftpState);
    }
}

static void
ftpTryMkdir(FtpStateData * ftpState)
{
    char *path = ftpState->filepath;
    debug(9, 3) ("ftpTryMkdir: with path=%s\n", path);
    snprintf(cbuf, 1024, "MKD %s\r\n", path);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_MKDIR;
}

static void
ftpReadMkdir(FtpStateData * ftpState)
{
    char *path = ftpState->filepath;
    int code = ftpState->ctrl.replycode;

    debug(9, 3) ("ftpReadMkdir: path %s, code %d\n", path, code);
    if (code == 257) {		/* success */
	ftpSendCwd(ftpState);
    } else if (code == 550) {	/* dir exists */
	if (ftpState->flags.put_mkdir) {
	    ftpState->flags.put_mkdir = 1;
	    ftpSendCwd(ftpState);
	} else
	    ftpSendReply(ftpState);
    } else
	ftpSendReply(ftpState);
}

static void
ftpGetFile(FtpStateData * ftpState)
{
    assert(*ftpState->filepath != '\0');
    ftpState->flags.isdir = 0;
    ftpSendMdtm(ftpState);
}

static void
ftpListDir(FtpStateData * ftpState)
{
    if (!ftpState->flags.isdir) {
	debug(9, 3) ("Directory path did not end in /\n");
	strcat(ftpState->title_url, "/");
	ftpState->flags.isdir = 1;
	ftpState->flags.use_base = 1;
    }
    ftpSendPasv(ftpState);
}

static void
ftpSendMdtm(FtpStateData * ftpState)
{
    assert(*ftpState->filepath != '\0');
    snprintf(cbuf, 1024, "MDTM %s\r\n", ftpState->filepath);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_MDTM;
}

static void
ftpReadMdtm(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadMdtm\n");
    if (code == 213) {
	ftpState->mdtm = parse_iso3307_time(ftpState->ctrl.last_reply);
	ftpUnhack(ftpState);
    } else if (code < 0) {
	ftpFail(ftpState);
    }
    ftpSendSize(ftpState);
}

static void
ftpSendSize(FtpStateData * ftpState)
{
    /* Only send SIZE for binary transfers. The returned size
     * is useless on ASCII transfers */
    if (ftpState->flags.binary) {
	assert(ftpState->filepath != NULL);
	assert(*ftpState->filepath != '\0');
	snprintf(cbuf, 1024, "SIZE %s\r\n", ftpState->filepath);
	ftpWriteCommand(cbuf, ftpState);
	ftpState->state = SENT_SIZE;
    } else
	/* Skip to next state no non-binary transfers */
	ftpSendPasv(ftpState);
}

static void
ftpReadSize(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadSize\n");
    if (code == 213) {
	ftpUnhack(ftpState);
	ftpState->size = atoi(ftpState->ctrl.last_reply);
    } else if (code < 0) {
	ftpFail(ftpState);
    }
    ftpSendPasv(ftpState);
}

static void
ftpSendPasv(FtpStateData * ftpState)
{
    int fd;
    struct sockaddr_in addr;
    int addr_len;
    if (ftpState->data.fd >= 0) {
	if (!ftpState->flags.datachannel_hack) {
	    /* We are already connected, reuse this connection. */
	    ftpRestOrList(ftpState);
	    return;
	} else {
	    /* Close old connection */
	    comm_close(ftpState->data.fd);
	    ftpState->data.fd = -1;
	}
    }
    if (!ftpState->flags.pasv_supported) {
	ftpSendPort(ftpState);
	return;
    }
    addr_len = sizeof(addr);
    if (getsockname(ftpState->ctrl.fd, (struct sockaddr *) &addr, &addr_len)) {
	debug(9, 0) ("ftpSendPasv: getsockname(%d,..): %s\n",
	    ftpState->ctrl.fd, xstrerror());
	addr.sin_addr = Config.Addrs.tcp_outgoing;
    }
    /* Open data channel with the same local address as control channel */
    fd = comm_open(SOCK_STREAM,
	0,
	addr.sin_addr,
	0,
	COMM_NONBLOCKING,
	storeUrl(ftpState->entry));
    debug(9, 3) ("ftpSendPasv: Unconnected data socket created on FD %d\n", fd);
    if (fd < 0) {
	ftpFail(ftpState);
	return;
    }
    /*
     * No comm_add_close_handler() here.  If we have both ctrl and
     * data FD's call ftpStateFree() upon close, then we have
     * to delete the close handler which did NOT get called
     * to prevent ftpStateFree() getting called twice.
     * Instead we'll always call comm_close() on the ctrl FD.
     */
    ftpState->data.fd = fd;
    snprintf(cbuf, 1024, "PASV\r\n");
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_PASV;
}

static void
ftpReadPasv(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    int h1, h2, h3, h4;
    int p1, p2;
    int n;
    u_short port;
    int fd = ftpState->data.fd;
    char *buf = ftpState->ctrl.last_reply;
    LOCAL_ARRAY(char, junk, 1024);
    debug(9, 3) ("This is ftpReadPasv\n");
    if (code != 227) {
	debug(9, 3) ("PASV not supported by remote end\n");
	comm_close(ftpState->data.fd);
	ftpState->data.fd = -1;
	ftpSendPort(ftpState);
	return;
    }
    if ((int) strlen(buf) > 1024) {
	debug(9, 1) ("ftpReadPasv: Avoiding potential buffer overflow\n");
	ftpSendPort(ftpState);
	return;
    }
    /*  227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).  */
    /*  ANSI sez [^0-9] is undefined, it breaks on Watcom cc */
    debug(9, 5) ("scanning: %s\n", buf);
    n = sscanf(buf, "%[^0123456789]%d,%d,%d,%d,%d,%d",
	junk, &h1, &h2, &h3, &h4, &p1, &p2);
    if (n != 7 || p1 < 0 || p2 < 0 || p1 > 255 || p2 > 255) {
	debug(9, 3) ("Bad 227 reply\n");
	debug(9, 3) ("n=%d, p1=%d, p2=%d\n", n, p1, p2);
	ftpSendPort(ftpState);
	return;
    }
    snprintf(junk, 1024, "%d.%d.%d.%d", h1, h2, h3, h4);
    if (!safe_inet_addr(junk, NULL)) {
	debug(9, 1) ("unsafe address (%s)\n", junk);
	ftpSendPort(ftpState);
	return;
    }
    port = ((p1 << 8) + p2);
    if (0 == port) {
	debug(9, 1) ("ftpReadPasv: Invalid PASV reply: %s\n", buf);
	ftpSendPort(ftpState);
	return;
    }
    debug(9, 5) ("ftpReadPasv: connecting to %s, port %d\n", junk, port);
    ftpState->data.port = port;
    ftpState->data.host = xstrdup(junk);
    commConnectStart(fd, junk, port, ftpPasvCallback, ftpState);
}

static void
ftpPasvCallback(int fd, int status, void *data)
{
    FtpStateData *ftpState = data;
    request_t *request = ftpState->request;
    ErrorState *err;
    debug(9, 3) ("ftpPasvCallback\n");
    if (status != COMM_OK) {
	err = errorCon(ERR_CONNECT_FAIL, HTTP_SERVICE_UNAVAILABLE);
	err->xerrno = errno;
	err->host = xstrdup(ftpState->data.host);
	err->port = ftpState->data.port;
	err->request = requestLink(request);
	errorAppendEntry(ftpState->entry, err);
	comm_close(ftpState->ctrl.fd);
	return;
    }
    ftpRestOrList(ftpState);
}

static int
ftpOpenListenSocket(FtpStateData * ftpState, int fallback)
{
    int fd;
    struct sockaddr_in addr;
    int addr_len;
    int on = 1;
    u_short port = 0;
    /*
     * Set up a listen socket on the same local address as the
     * control connection.
     */
    addr_len = sizeof(addr);
    if (getsockname(ftpState->ctrl.fd, (struct sockaddr *) &addr, &addr_len)) {
	debug(9, 0) ("ftpOpenListenSocket: getsockname(%d,..): %s\n",
	    ftpState->ctrl.fd, xstrerror());
	return -1;
    }
    /*
     * REUSEADDR is needed in fallback mode, since the same port is
     * used for both control and data.
     */
    if (fallback) {
	setsockopt(ftpState->ctrl.fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
	port = ntohs(addr.sin_port);
    }
    fd = comm_open(SOCK_STREAM,
	0,
	addr.sin_addr,
	port,
	COMM_NONBLOCKING | (fallback ? COMM_REUSEADDR : 0),
	storeUrl(ftpState->entry));
    debug(9, 3) ("ftpOpenListenSocket: Unconnected data socket created on FD %d\n", fd);
    if (fd < 0) {
	debug(9, 0) ("ftpOpenListenSocket: comm_open failed\n");
	return -1;
    }
    if (comm_listen(fd) < 0) {
	comm_close(fd);
	return -1;
    }
    ftpState->data.fd = fd;
    ftpState->data.port = comm_local_port(fd);;
    ftpState->data.host = NULL;
    return fd;
}

static void
ftpSendPort(FtpStateData * ftpState)
{
    int fd;
    struct sockaddr_in addr;
    int addr_len;
    unsigned char *addrptr;
    unsigned char *portptr;
    debug(9, 3) ("This is ftpSendPort\n");
    ftpState->flags.pasv_supported = 0;
    fd = ftpOpenListenSocket(ftpState, 0);
    addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *) &addr, &addr_len)) {
	debug(9, 0) ("ftpSendPort: getsockname(%d,..): %s\n", fd, xstrerror());
	/* XXX Need to set error message */
	ftpFail(ftpState);
	return;
    }
    addrptr = (unsigned char *) &addr.sin_addr.s_addr;
    portptr = (unsigned char *) &addr.sin_port;
    snprintf(cbuf, 1024, "PORT %d,%d,%d,%d,%d,%d\r\n",
	addrptr[0], addrptr[1], addrptr[2], addrptr[3],
	portptr[0], portptr[1]);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_PORT;
}

static void
ftpReadPort(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadPort\n");
    if (code != 200) {
	/* Fall back on using the same port as the control connection */
	debug(9, 3) ("PORT not supported by remote end\n");
	comm_close(ftpState->data.fd);
	ftpOpenListenSocket(ftpState, 1);
    }
    ftpRestOrList(ftpState);
}

/* "read" handler to accept data connection */
static void
ftpAcceptDataConnection(int fd, void *data)
{
    FtpStateData *ftpState = data;
    struct sockaddr_in peer, me;
    debug(9, 3) ("ftpAcceptDataConnection\n");

    fd = comm_accept(fd, &peer, &me);
    if (fd < 0) {
	debug(9, 1) ("ftpHandleDataAccept: comm_accept(%d): %s", fd, xstrerror());
	/* XXX Need to set error message */
	ftpFail(ftpState);
	return;
    }
    /* Replace the Listen socket with the accepted data socket */
    comm_close(ftpState->data.fd);
    debug(9, 3) ("ftpAcceptDataConnection: Connected data socket on FD %d\n", fd);
    ftpState->data.fd = fd;
    ftpState->data.port = ntohs(peer.sin_port);
    ftpState->data.host = xstrdup(inet_ntoa(peer.sin_addr));
    commSetTimeout(ftpState->data.fd, Config.Timeout.read, ftpTimeout,
	ftpState);
    /* XXX We should have a flag to track connect state...
     *    host NULL -> not connected, port == local port
     *    host set  -> connected, port == remote port
     */
    /* Restart state (SENT_NLST/LIST/RETR) */
    FTP_SM_FUNCS[ftpState->state] (ftpState);
}

static void
ftpRestOrList(FtpStateData * ftpState)
{

    debug(9, 3) ("This is ftpRestOrList\n");
    if (ftpState->flags.put) {
	debug(9, 3) ("ftpRestOrList: Sending STOR request...\n");
	ftpSendStor(ftpState);
    } else if (ftpState->typecode == 'D') {
	/* XXX This should NOT be here */
	ftpSendNlst(ftpState);	/* sec 3.2.2 of RFC 1738 */
	ftpState->flags.isdir = 1;
	ftpState->flags.use_base = 1;
    } else if (ftpState->flags.isdir)
	ftpSendList(ftpState);
    else if (ftpState->restart_offset > 0)
	ftpSendRest(ftpState);
    else
	ftpSendRetr(ftpState);
}

static void
ftpSendStor(FtpStateData * ftpState)
{
    assert(ftpState->filepath != NULL);
    snprintf(cbuf, 1024, "STOR %s\r\n", ftpState->filepath);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_STOR;
}

static void
ftpReadStor(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadStor\n");
    if (code >= 100 && code < 200) {
	/*
	 * Cancel the timeout on the Control socket, pumpStart will
	 * establish one on the data socket.
	 */
	commSetTimeout(ftpState->ctrl.fd, -1, NULL, NULL);
	ftpPutStart(ftpState);
	debug(9, 3) ("ftpReadStor: writing data channel\n");
	ftpState->state = WRITING_DATA;
    } else if (code == 553) {
	/* directory does not exist, have to create, sigh */
#if WORK_IN_PROGRESS
	ftpTraverseDirectory(ftpState);
#endif
	ftpSendReply(ftpState);
    } else {
	debug(9, 3) ("ftpReadStor: that's all folks\n");
	ftpSendReply(ftpState);
    }
}

static void
ftpSendRest(FtpStateData * ftpState)
{
    snprintf(cbuf, 1024, "REST %d\r\n", ftpState->restart_offset);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_REST;
}

static void
ftpReadRest(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadRest\n");
    assert(ftpState->restart_offset > 0);
    if (code == 350) {
	ftpSendRetr(ftpState);
    } else if (code > 0) {
	debug(9, 3) ("ftpReadRest: REST not supported\n");
	ftpState->flags.rest_supported = 0;
    } else {
	ftpFail(ftpState);
    }
}

static void
ftpSendList(FtpStateData * ftpState)
{
    if (ftpState->filepath) {
	ftpState->flags.use_base = 1;
	snprintf(cbuf, 1024, "LIST %s\r\n", ftpState->filepath);
    } else {
	snprintf(cbuf, 1024, "LIST\r\n");
    }
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_LIST;
}

static void
ftpSendNlst(FtpStateData * ftpState)
{
    ftpState->flags.tried_nlst = 1;
    if (ftpState->filepath) {
	ftpState->flags.use_base = 1;
	snprintf(cbuf, 1024, "NLST %s\r\n", ftpState->filepath);
    } else {
	snprintf(cbuf, 1024, "NLST\r\n");
    }
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_NLST;
}

static void
ftpReadList(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadList\n");
    if (code == 125 || (code == 150 && ftpState->data.host)) {
	/* Begin data transfer */
	ftpAppendSuccessHeader(ftpState);
	commSetSelect(ftpState->data.fd,
	    COMM_SELECT_READ,
	    ftpDataRead,
	    ftpState,
	    Config.Timeout.read);
	commSetDefer(ftpState->data.fd, fwdCheckDeferRead, ftpState->entry);
	ftpState->state = READING_DATA;
	/*
	 * Cancel the timeout on the Control socket and establish one
	 * on the data socket
	 */
	commSetTimeout(ftpState->ctrl.fd, -1, NULL, NULL);
	commSetTimeout(ftpState->data.fd, Config.Timeout.read, ftpTimeout, ftpState);
	return;
    } else if (code == 150) {
	/* Accept data channel */
	commSetSelect(ftpState->data.fd,
	    COMM_SELECT_READ,
	    ftpAcceptDataConnection,
	    ftpState,
	    0);
	/*
	 * Cancel the timeout on the Control socket and establish one
	 * on the data socket
	 */
	commSetTimeout(ftpState->ctrl.fd, -1, NULL, NULL);
	commSetTimeout(ftpState->data.fd, Config.Timeout.read, ftpTimeout, ftpState);
	return;
    } else if (!ftpState->flags.tried_nlst && code > 300) {
	ftpSendNlst(ftpState);
    } else {
	ftpFail(ftpState);
	return;
    }
}

static void
ftpSendRetr(FtpStateData * ftpState)
{
    assert(ftpState->filepath != NULL);
    snprintf(cbuf, 1024, "RETR %s\r\n", ftpState->filepath);
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_RETR;
}

static void
ftpReadRetr(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadRetr\n");
    if (code == 125 || (code == 150 && ftpState->data.host)) {
	/* Begin data transfer */
	debug(9, 3) ("ftpReadRetr: reading data channel\n");
	ftpAppendSuccessHeader(ftpState);
	commSetSelect(ftpState->data.fd,
	    COMM_SELECT_READ,
	    ftpDataRead,
	    ftpState,
	    Config.Timeout.read);
	commSetDefer(ftpState->data.fd, fwdCheckDeferRead, ftpState->entry);
	ftpState->state = READING_DATA;
	/*
	 * Cancel the timeout on the Control socket and establish one
	 * on the data socket
	 */
	commSetTimeout(ftpState->ctrl.fd, -1, NULL, NULL);
	commSetTimeout(ftpState->data.fd, Config.Timeout.read, ftpTimeout,
	    ftpState);
    } else if (code == 150) {
	/* Accept data channel */
	commSetSelect(ftpState->data.fd,
	    COMM_SELECT_READ,
	    ftpAcceptDataConnection,
	    ftpState,
	    0);
	/*
	 * Cancel the timeout on the Control socket and establish one
	 * on the data socket
	 */
	commSetTimeout(ftpState->ctrl.fd, -1, NULL, NULL);
	commSetTimeout(ftpState->data.fd, Config.Timeout.read, ftpTimeout,
	    ftpState);
    } else if (code >= 300) {
	if (!ftpState->flags.try_slash_hack) {
	    /* Try this as a directory missing trailing slash... */
	    ftpHackShortcut(ftpState, ftpSendCwd);
	} else {
	    ftpFail(ftpState);
	}
    } else {
	ftpFail(ftpState);
    }
}

static void
ftpReadTransferDone(FtpStateData * ftpState)
{
    int code = ftpState->ctrl.replycode;
    debug(9, 3) ("This is ftpReadTransferDone\n");
    if (code != 226) {
	debug(9, 1) ("ftpReadTransferDone: Got code %d after reading data\n",
	    code);
	debug(9, 1) ("--> releasing '%s'\n", storeUrl(ftpState->entry));
	storeReleaseRequest(ftpState->entry);
    }
    ftpDataTransferDone(ftpState);
}

static void
ftpDataTransferDone(FtpStateData * ftpState)
{
    debug(9, 3) ("This is ftpDataTransferDone\n");
    if (ftpState->data.fd > -1) {
	comm_close(ftpState->data.fd);
	ftpState->data.fd = -1;
    }
    ftpSendQuit(ftpState);
}

static void
ftpSendQuit(FtpStateData * ftpState)
{
    assert(ftpState->ctrl.fd > -1);
    snprintf(cbuf, 1024, "QUIT\r\n");
    ftpWriteCommand(cbuf, ftpState);
    ftpState->state = SENT_QUIT;
}

static void
ftpReadQuit(FtpStateData * ftpState)
{
    comm_close(ftpState->ctrl.fd);
}

static void
ftpTrySlashHack(FtpStateData * ftpState)
{
    char *path;
    ftpState->flags.try_slash_hack = 1;
    /* Free old paths */
    if (ftpState->pathcomps)
	wordlistDestroy(&ftpState->pathcomps);
    safe_free(ftpState->filepath);
    /* Build the new path (urlpath begins with /) */
    path = xstrdup(strBuf(ftpState->request->urlpath));
    rfc1738_unescape(path);
    ftpState->filepath = path;
    /* And off we go */
    ftpGetFile(ftpState);
}

static void
ftpTryDatachannelHack(FtpStateData * ftpState)
{
    ftpState->flags.datachannel_hack = 1;
    /* we have to undo some of the slash hack... */
    if (ftpState->old_filepath != NULL) {
	ftpState->flags.try_slash_hack = 0;
	safe_free(ftpState->filepath);
	ftpState->filepath = ftpState->old_filepath;
	ftpState->old_filepath = NULL;
    }
    ftpState->flags.tried_nlst = 0;
    /* And off we go */
    if (ftpState->flags.isdir) {
	safe_free(ftpState->filepath);
	ftpListDir(ftpState);
    } else {
	ftpGetFile(ftpState);
    }
    return;
}

/* Forget hack status. Next error is shown to the user */
static void
ftpUnhack(FtpStateData * ftpState)
{
    if (ftpState->old_request != NULL) {
	safe_free(ftpState->old_request);
	safe_free(ftpState->old_reply);
    }
}

static void
ftpHackShortcut(FtpStateData * ftpState, FTPSM * nextState)
{
    /* Save old error message & some state info */
    if (ftpState->old_request == NULL) {
	ftpState->old_request = ftpState->ctrl.last_command;
	ftpState->ctrl.last_command = NULL;
	ftpState->old_reply = ftpState->ctrl.last_reply;
	ftpState->ctrl.last_reply = NULL;
	if (ftpState->pathcomps == NULL && ftpState->filepath != NULL)
	    ftpState->old_filepath = xstrdup(ftpState->filepath);
    }
    /* Jump to the "hack" state */
    nextState(ftpState);
}

static void
ftpFail(FtpStateData * ftpState)
{
    ErrorState *err;
    debug(9, 3) ("ftpFail\n");
    /* Try the / hack to support "Netscape" FTP URL's for retreiving files */
    if (!ftpState->flags.isdir &&
	!ftpState->flags.try_slash_hack) {
	switch (ftpState->state) {
	case SENT_CWD:
	case SENT_RETR:
	    /* Try the / hack */
	    ftpHackShortcut(ftpState, ftpTrySlashHack);
	    return;
	default:
	    break;
	}
    }
    /* Try to reopen datachannel */
    if (!ftpState->flags.datachannel_hack &&
	ftpState->pathcomps == NULL) {
	switch (ftpState->state) {
	case SENT_RETR:
	case SENT_LIST:
	case SENT_NLST:
	    /* Try to reopen datachannel */
	    ftpHackShortcut(ftpState, ftpTryDatachannelHack);
	    return;
	default:
	    break;
	}
    }
    err = errorCon(ERR_FTP_FAILURE, HTTP_INTERNAL_SERVER_ERROR);
    err->request = requestLink(ftpState->request);
    err->ftp_server_msg = ftpState->ctrl.message;
    if (ftpState->old_request)
	err->ftp.request = ftpState->old_request;
    else
	err->ftp.request = ftpState->ctrl.last_command;
    if (ftpState->old_reply)
	err->ftp.reply = ftpState->old_reply;
    else
	err->ftp.reply = ftpState->ctrl.last_reply;
    errorAppendEntry(ftpState->entry, err);
    comm_close(ftpState->ctrl.fd);
}

static void
ftpPutStart(FtpStateData * ftpState)
{
    debug(9, 3) ("ftpPutStart\n");
    pumpStart(ftpState->data.fd, ftpState->entry,
	ftpState->request, ftpPutTransferDone, ftpState);
}

static void
ftpPutTransferDone(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    FtpStateData *ftpState = data;
    if (ftpState->data.fd >= 0) {
	comm_close(ftpState->data.fd);
	ftpState->data.fd = -1;
    }
    ftpReadComplete(ftpState);
}

static void
ftpSendReply(FtpStateData * ftpState)
{
    ErrorState *err;
    int code = ftpState->ctrl.replycode;
    int http_code;
    int err_code = ERR_NONE;
    debug(9, 5) ("ftpSendReply: %s, code %d\n",
	storeUrl(ftpState->entry), code);
    if (cbdataValid(ftpState))
	debug(9, 5) ("ftpSendReply: ftpState (%p) is valid!\n", ftpState);
    if (code == 226) {
	err_code = (ftpState->mdtm > 0) ? ERR_FTP_PUT_MODIFIED : ERR_FTP_PUT_CREATED;
	http_code = (ftpState->mdtm > 0) ? HTTP_ACCEPTED : HTTP_CREATED;
    } else {
	err_code = ERR_FTP_PUT_ERROR;
	http_code = HTTP_INTERNAL_SERVER_ERROR;
    }
    err = errorCon(err_code, http_code);
    err->request = requestLink(ftpState->request);
    if (ftpState->old_request)
	err->ftp.request = ftpState->old_request;
    else
	err->ftp.request = ftpState->ctrl.last_command;
    if (ftpState->old_reply)
	err->ftp.reply = ftpState->old_reply;
    else
	err->ftp.reply = ftpState->ctrl.last_reply;
    errorAppendEntry(ftpState->entry, err);
    storeBufferFlush(ftpState->entry);
    comm_close(ftpState->ctrl.fd);
}

static void
ftpAppendSuccessHeader(FtpStateData * ftpState)
{
    char *mime_type = NULL;
    char *mime_enc = NULL;
    String urlpath = ftpState->request->urlpath;
    const char *filename = NULL;
    const char *t = NULL;
    StoreEntry *e = ftpState->entry;
    http_reply *reply = e->mem_obj->reply;
    if (ftpState->flags.http_header_sent)
	return;
    ftpState->flags.http_header_sent = 1;
    assert(e->mem_obj->inmem_hi == 0);
    filename = (t = strRChr(urlpath, '/')) ? t + 1 : strBuf(urlpath);
    if (ftpState->flags.isdir) {
	mime_type = "text/html";
    } else {
	switch (ftpState->typecode) {
	case 'I':
	    mime_type = "application/octet-stream";
	    mime_enc = mimeGetContentEncoding(filename);
	    break;
	case 'A':
	    mime_type = "text/plain";
	    break;
	default:
	    mime_type = mimeGetContentType(filename);
	    mime_enc = mimeGetContentEncoding(filename);
	    break;
	}
    }
    storeBuffer(e);
    httpReplyReset(reply);
    /* set standard stuff */
    httpReplySetHeaders(reply, 1.0, HTTP_OK, "Gatewaying",
	mime_type, ftpState->size, ftpState->mdtm, -2);
    /* additional info */
    if (mime_enc)
	httpHeaderPutStr(&reply->header, HDR_CONTENT_ENCODING, mime_enc);
    httpReplySwapOut(reply, e);
    storeBufferFlush(e);
    reply->hdr_sz = e->mem_obj->inmem_hi;
    storeTimestampsSet(e);
    storeSetPublicKey(e);
}

static void
ftpAuthRequired(HttpReply * old_reply, request_t * request, const char *realm)
{
    ErrorState *err = errorCon(ERR_ACCESS_DENIED, HTTP_UNAUTHORIZED);
    HttpReply *rep;
    err->request = requestLink(request);
    rep = errorBuildReply(err);
    errorStateFree(err);
    /* add Authenticate header */
    httpHeaderPutAuth(&rep->header, "Basic", realm);
    /* move new reply to the old one */
    httpReplyAbsorb(old_reply, rep);
}

char *
ftpUrlWith2f(const request_t * request)
{
    LOCAL_ARRAY(char, buf, MAX_URL);
    LOCAL_ARRAY(char, loginbuf, MAX_LOGIN_SZ + 1);
    LOCAL_ARRAY(char, portbuf, 32);
    char *t;
    portbuf[0] = '\0';
    if (request->protocol != PROTO_FTP)
	return NULL;
    if (request->port != urlDefaultPort(request->protocol))
	snprintf(portbuf, 32, ":%d", request->port);
    loginbuf[0] = '\0';
    if ((int) strlen(request->login) > 0) {
	strcpy(loginbuf, request->login);
	if ((t = strchr(loginbuf, ':')))
	    *t = '\0';
	strcat(loginbuf, "@");
    }
    snprintf(buf, MAX_URL, "%s://%s%s%s%s%s",
	ProtocolStr[request->protocol],
	loginbuf,
	request->host,
	portbuf,
	"/%2f",
	strBuf(request->urlpath));
    if ((t = strchr(buf, '?')))
	*t = '\0';
    return buf;
}
