/*
 * XML Expect dialog mini-language implementation
 *
 * (c) Peter Edwards, March 2004.
 *
 * $Id: xmlExpect.cc,v 1.18 2004/08/29 11:36:03 petere Exp $
 */

#include <poll.h>
#include <arpa/telnet.h>
#include <regex.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <typeinfo>

#include "xmlexpect.h"
#include "connection.h"
#include "util.h"

/*
 * Classes
 */

class ExpectNetwork : public ExpectElement {
    NetworkConnection net;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectNetwork(const char **);
};

class ExpectListen : public ExpectElement {
    ListenConnection net;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectListen(const char **);
};

class ExpectVariable : public ExpectCharacterData {
    std::string key;
    std::string def;
public:
    virtual void write(ExpectProgram &, std::ostream &) const;
    ExpectVariable(const char **attributes);
};

class ExpectModem : public ExpectElement {
    ModemConnection modem;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectModem(const char **);
};

class ExpectSleep : public ExpectElement {
    int delay;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectSleep(const char **attributes);
};

class ExpectSend : public ExpectElement {
public:
    virtual void execute(ExpectProgram &program) const;
};

class ExpectLog : public ExpectElement {
     std::string message;
     int level;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectLog(const char **attributes);
    ~ExpectLog();
};

class ExpectDrip : public ExpectElement {
     bool rate;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectDrip(const char **attributes);
    ~ExpectDrip();
};

class ExpectComment : public ExpectElement {
    int level;
public:
    virtual void execute(ExpectProgram &program)  const {}
};

class ExpectTimeout : public ExpectElement {
    int value;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectTimeout(const char **attribs);
};

class ExpectChoose : public ExpectControlElement {
public:
    ExpectChoose();
    virtual void execute(ExpectProgram &program) const;
};

class ExpectIf : public ExpectControlElement {
public:
    ExpectIf(const char **);
    virtual void execute(ExpectProgram &program) const;
};

class ExpectStrlen : public ExpectCharacterData {
public:
    ExpectStrlen(const char **);
    virtual void write(ExpectProgram &, std::ostream &) const;
};

class ExpectStrcat : public ExpectCharacterData {
public:
    ExpectStrcat(const char **);
    virtual void write(ExpectProgram &, std::ostream &) const;
};

class ExpectStreq : public ExpectCharacterData {
public:
    ExpectStreq(const char **);
    virtual void write(ExpectProgram &, std::ostream &) const;
};

class ExpectThen : public ExpectElement {
public:
    ExpectThen(const char **);
};

class ExpectElse : public ExpectElement {
public:
    ExpectElse(const char **);
};

class ExpectPrint : public ExpectElement {
public:
    ExpectPrint(const char **);
    virtual void execute(ExpectProgram &program) const;
};

class ExpectRawCharacterData : public ExpectCharacterData {
    char *data;
    int len;
public:
    ExpectRawCharacterData(const char *data, int len, bool stripCtrl);
    ~ExpectRawCharacterData();
    void write(ExpectProgram &, std::ostream &) const;
};

class ExpectCtrl : public ExpectCharacterData {
    char character;
public:
    ExpectCtrl(const char **attributes);
    void write(ExpectProgram &, std::ostream &) const;
};

class Vt100EscapeCodes : public std::map<std::string, std::string> {
public:
    Vt100EscapeCodes();
};

class ExpectVt100 : public ExpectCharacterData {
    std::string output;
    static Vt100EscapeCodes escapes;

public:
    ExpectVt100(const char **attributes);
    void write(ExpectProgram &, std::ostream &) const;
};

class ExpectExpect : public ExpectElement {
public:
    ExpectExpect();
    void execute(ExpectProgram &) const;
    int match(ExpectProgram &program) const;
};

class ExpectDo : public ExpectElement {
    std::string status;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectDo(const char **);
};

class ExpectOnError : public ExpectElement {
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectOnError(const char **);
};

class ExpectDefaultElement : public ExpectElement {
private:
public:
    ExpectDefaultElement(const char *, const char **attributes);
};


/*
 * Static members
 */

Vt100EscapeCodes ExpectVt100::escapes;

/*
 * Static utility routines.
 */

static const char *
telnetCommand(int command)
{
    switch (command) {
    case DO: return "DO";
    case DONT: return "DONT";
    case WILL: return "WILL";
    case WONT: return "WONT";
    default: return "(unknown)";
    }
}

/* 
 * Class Implementations
 */

ExpectHandlers::ExpectHandlers()
    : sp(0)
{
}

ExpectHandlers::~ExpectHandlers()
{
}

ExpectNode *
ExpectHandlers::root()
{
    return stack[0];
}

void
ExpectHandlers::characterData(const char *data, int len)
{
    if (sp > 0 && dynamic_cast<ExpectControlElement *>(stack[sp - 1]))
	return;
    addNode(new ExpectRawCharacterData(data, len, true));
}

void
ExpectHandlers::addNode(ExpectNode *node)
{
    node->lineNumber = parser->getCurrentLineNumber();
    if (stack[sp])
	stack[sp]->nextSibling = node;
    else if (sp != 0)
	stack[sp-1]->firstChild = node;
    stack[sp] = node;
}

ExpectNode *
ExpectHandlers::getNode(const char *name, const char **attributes)
{
    if (!strcmp(name, "get"))
	return new ExpectVariable(attributes);
    if (!strcmp(name, "template"))
	return new ExpectTemplate(attributes);
    if (!strcmp(name, "listen"))
	return new ExpectListen(attributes);
    if (!strcmp(name, "network"))
	return new ExpectNetwork(attributes);
    if (!strcmp(name, "modem"))
	return new ExpectModem(attributes);
    if (!strcmp(name, "choose"))
	return new ExpectChoose();
    if (!strcmp(name, "expect") || !strcmp(name, "e"))
	return new ExpectExpect();
    if (!strcmp(name, "send") || !strcmp(name, "s"))
	return new ExpectSend();
    if (!strcmp(name, "do"))
	return new ExpectDo(attributes);
    if (!strcmp(name, "timeout"))
	return new ExpectTimeout(attributes);
    if (!strcmp(name, "br"))
	return new ExpectRawCharacterData("\r\n", 2, false);
    if (!strcmp(name, "comment"))
	return new ExpectComment();
    if (!strcmp(name, "ctrl"))
	return new ExpectCtrl(attributes);
    if (!strcmp(name, "vt100"))
	return new ExpectVt100(attributes);
    if (!strcmp(name, "log"))
	return new ExpectLog(attributes);
    if (!strcmp(name, "cr"))
	return new ExpectRawCharacterData("\r", 1, false);
    if (!strcmp(name, "lf"))
	return new ExpectRawCharacterData("\n", 1, false);
    if (!strcmp(name, "crlf"))
	return new ExpectRawCharacterData("\r\n", 2, false);
    if (!strcmp(name, "if"))
	return new ExpectIf(attributes);
    if (!strcmp(name, "then"))
	return new ExpectThen(attributes);
    if (!strcmp(name, "else"))
	return new ExpectElse(attributes);
    if (!strcmp(name, "print"))
	return new ExpectPrint(attributes);
    if (!strcmp(name, "sleep"))
	return new ExpectSleep(attributes);
    if (!strcmp(name, "strlen"))
	return new ExpectStrlen(attributes);
    if (!strcmp(name, "strcat"))
	return new ExpectStrcat(attributes);
    if (!strcmp(name, "streq"))
	return new ExpectStreq(attributes);
    if (!strcmp(name, "onerror"))
	return new ExpectOnError(attributes);
    if (!strcmp(name, "drip"))
	return new ExpectDrip(attributes);

    if (!strcmp(name, "include")) {
	const char *filename = 0, **cpp;
	for (cpp = attributes; cpp[0]; cpp++) {
	    if (!strcmp(cpp[0], "file"))
		filename = cpp[1];
	}
	if (filename == 0)
	    abort();
	ExpectHandlers handlers;
	ExpatParser parser(handlers, "UTF-8");
	parser.parseFile(filename);
	return handlers.root();
    }
    throw UnknownElement(name);
    abort();
}

std::ostream &
UnknownElement::describe(std::ostream &os) const
{
    return os << "unknown element \"" << element << "\"";
}

void
ExpectHandlers::startElement(const char *name, const char **attributes)
{
    ExpectNode *el = getNode(name, attributes);
    addNode(el);
    stack[++sp] = 0;
}

void
ExpectHandlers::endElement(const char *name)
{
    sp--;
}

ExpectNode::ExpectNode()
    : nextSibling(0)
    , firstChild(0)
    , lineNumber(-1)
{
}

void
ExpectNode::executeChildren(ExpectProgram &program) const
{
    for (ExpectNode *c = firstChild; c; c = c->nextSibling)
	program.execute(c);
}

void
ExpectNode::execute(ExpectProgram &program) const
{
    executeChildren(program);
}

ExpectNode::~ExpectNode()
{
    ExpectNode *c;
    while ((c = firstChild) != 0) {
	firstChild = c->nextSibling;
	delete c;
    }
}

ExpectCtrl::ExpectCtrl(const char **attributes)
{
    for (const char **cpp = attributes; cpp[0]; cpp += 2)
	if (!strcmp(cpp[0], "char"))
	    character = toupper(cpp[1][0]) - 'A' + 1;
	else if (!strcmp(cpp[0], "code"))
	    character = atoi(cpp[1]);
}

void
ExpectCtrl::write(ExpectProgram &, std::ostream &os) const
{
    os << character;
}

ExpectVt100::ExpectVt100(const char **attributes)
{
    for (const char **cpp = attributes; cpp[0]; cpp += 2)
	if (!strcmp(cpp[0], "key"))
	    output = escapes[cpp[1]];
}

void
ExpectVt100::write(ExpectProgram &, std::ostream &os) const
{
    os << output;
}

ExpectLog::ExpectLog(const char **attributes)
{
    const char **cpp;

    level = 1;
    message = "";
    for (cpp = attributes; cpp[0]; cpp += 2) {
	    if (!strcmp(cpp[0], "level")) {
		level = atoi(cpp[1]);
	    } else if (!strcmp(cpp[0], "message")) {
		message = cpp[1];
	    }
    }
}

void
ExpectLog::execute(ExpectProgram &program) const
{
    std::clog << message;
}

ExpectLog::~ExpectLog()
{
}

ExpectChoose::ExpectChoose()
{
}

void
ExpectChoose::execute(ExpectProgram &program) const
{
    int offset = -1;

    try {
	for (;;) {
	    ExpectExpect *expected;
	    ExpectNode *action;
	    for (expected = dynamic_cast<ExpectExpect *>(firstChild);
		    expected;
		    expected = dynamic_cast<ExpectExpect *>(action->nextSibling)) {
		action = expected->nextSibling;
		if ((offset = expected->match(program)) != -1) {
		    program.execute(action);
		    break;
		}
	    }
	    if (offset != -1)
		break;
	    program.receive();
	}
    }
    catch (const UnixException &ux) {
	if (ux.uxError == ETIMEDOUT)
	    throw ExpectTimeoutException(program.matching, program.status,
	    std::string(program.receiveData, program.receiveOffset));
    }
}

ExpectExpect::ExpectExpect()
{
}

ExpectSleep::ExpectSleep(const char **attributes)
{
    const char **cpp;
    delay = 500;
    for (cpp = attributes; cpp[0]; cpp++) {
	if (strcmp(cpp[0], "msec") == 0)
	    delay = atoi(cpp[1]) * 1000;
	else if (strcmp(cpp[0], "usec") == 0)
	    delay = atoi(cpp[1]);
	else if (strcmp(cpp[0], "sec") == 0)
	    delay = atoi(cpp[1]) * 1000000;
    }
}

void
ExpectSleep::execute(ExpectProgram &program) const
{
    program.flush();
    usleep(delay);
}

int
ExpectExpect::match(ExpectProgram &program) const
{
    std::stringstream strm;
    for (const ExpectNode *c = firstChild; c; c = c->nextSibling)
	dynamic_cast<const ExpectCharacterData &>(*c).write(program, strm);
    return program.match(strm.str());
}

void
ExpectExpect::execute(ExpectProgram &program) const
{
    try {
	while (match(program) == -1)
	    program.receive();
    }
    catch (const UnixException &ux) {
	if (ux.uxError == ETIMEDOUT)
	    throw ExpectTimeoutException(program.matching, program.status,
	    std::string(program.receiveData, program.receiveOffset));
    }
}

ExpectTimeoutException::ExpectTimeoutException(std::string waitingFor, std::string status, std::string currentData)
    : waitingFor(waitingFor)
    , status(status)
    , currentData(currentData)
{
}

std::ostream &
ExpectTimeoutException::describe(std::ostream &output) const
{
    return output << "timeout waiting for \"" << waitingFor << "\" while " << status
    <<". current data: " << currentData;
}

ExpectDefaultElement::ExpectDefaultElement(const char *name, const char **attributes)
{
}

ExpectSyntaxException::ExpectSyntaxException(std::string reason)
    : reason(reason)
{
}

std::ostream &
ExpectSyntaxException::describe(std::ostream &output) const
{
    return output << "syntax error in template: " << reason;
}


void
ExpectSend::execute(ExpectProgram &program) const
{
    // XXX: Implement output stream for fds to avoid intermediate string buffer
    std::stringstream strm;
    for (const ExpectNode *cdp = firstChild; cdp; cdp = cdp->nextSibling)
	dynamic_cast <const ExpectCharacterData &>(*cdp).write(program, strm);
    std::string s = strm.str();
    program.send(s.data(), s.size());

}

ExpectRawCharacterData::ExpectRawCharacterData(const char *newData, int newLen, bool stripCtrl)
    : data(newData ? new char[newLen]: 0)
    , len(0)
{
    if (newData) {
        // Remove control characters from data.
        for (int i = 0; i < newLen; ++i) {
            if (!stripCtrl || newData[i] >= 32)
                data[len++] = newData[i];
        }
    }
}

ExpectRawCharacterData::~ExpectRawCharacterData()
{
    delete [] data;
}

void
ExpectRawCharacterData::write(ExpectProgram &, std::ostream &os) const
{
    os.write(data, len);
}

void
ExpectCharacterData::execute(ExpectProgram &program) const
{ }

ExpectProgram::ExpectProgram(int maxBuf, std::map<std::string, std::string> &variables)
    : dripRate(0)
    , variables(variables)
    , lastVisited(0)
    , readFd(-1)
    , writeFd(-1)
    , timeout(2000)
    , expectDelay(50)
    , receiveSize(maxBuf - 1)
    , receiveData(new char[maxBuf])
    , receiveOffset(0)
    , sendSize(maxBuf - 1)
    , sendData(new char[maxBuf])
    , sendOffset(0)
    , logFacility(0)
{
}

void
ExpectProgram::statusUpdate(std::string)
{
}

int
ExpectProgram::match(std::string s)
{
    regex_t re;

    matching = s;

    receiveData[receiveOffset] = 0;
    const char *cp = s.c_str();
    // XXX: Really need to cache the regex compile in the expect or chardata node if
    // not dynamic.
    if (regcomp(&re, cp, REG_NOSUB) == 0) {
	if (regexec(&re, receiveData, 0, 0, 0) == 0) {
	    receiveOffset = 0; // discard any data already received.
	    regfree(&re);
	    return 0;
	}
    }
    regfree(&re);
    return -1;
}

void
ExpectProgram::sendRaw(const char *data, int len)
{
    int offset = 0;
    do {
	int avail = std::min<int>(len, sendSize - sendOffset);
	memcpy(sendData + sendOffset, data + offset, avail);
	offset += avail;
	sendOffset += avail;
	if (sendOffset == sendSize)
	    flush();
    } while (offset < len);
}

void
ExpectProgram::flush()
{
    int sent;

    for (int total = 0; total < sendOffset; total += sent) {
	if (dripRate != 0) {
	    sent = ::write(writeFd, sendData + total, 1);
	    usleep(dripRate * 1000);
	} else {
	    sent = ::write(writeFd, sendData + total, sendOffset - total);
	}
	switch (sent) {
	case -1:
	    throw UnixException(errno, "write");
	case 0:
	    throw UnixException(0, "write");
	}
    }
    sendOffset = 0;
}

void
ExpectProgram::send(const char *data, int len)
{
    std::clog << "SEND " << printableString(data, len) << std::endl;
    sendRaw(data, len);
}

void
ExpectProgram::receiveRaw()
{
    flush(); // Don't have any outstanding unsent data.
    if (expectDelay) // The sleep makes it more likely that a single transaction will read more data.
	usleep(expectDelay * 1000);

    // Make sure we have at least 1/8th of the receive buffer free.
    struct pollfd pfd;
    pfd.fd = readFd;
    pfd.events = POLLIN|POLLPRI;

    if (poll(&pfd, 1, timeout) == 0) {
	if (poll(&pfd, 1, 0) == 1)
	    abort();
	throw UnixException(ETIMEDOUT, "poll");
    }

    int received = ::read(readFd, receiveData + receiveOffset, receiveSize - receiveOffset);

    switch (received) {
    case 0:
    case -1:
	throw UnixException(errno, "read");
    default:
	receiveOffset += received;
	break;
    }
}

void
ExpectProgram::receive()
{
    int origOffset = receiveOffset;
    do {
	int minFree = receiveSize / 8;
	if ((receiveSize - receiveOffset) < minFree) {
	    memmove(receiveData, receiveData + minFree, receiveOffset - minFree);
	    receiveOffset -= minFree;
	    origOffset = std::max(0, origOffset - minFree);
	}
	receiveRaw();
	stripTelnet();
    } while (receiveOffset == 0);

    std::clog << "RECV " << printableString(receiveData + origOffset, receiveOffset - origOffset) << std::endl;
}

void
ExpectProgram::need(int size)
{
    while (receiveOffset < size)
	receiveRaw();
}

void
ExpectProgram::stripTelnet()
{
    unsigned char c;
    int i, j;
    unsigned char response[1024];
    int responseSize = 0;

    for (i = j = 0; i < receiveOffset; i++) {
	switch (c = (unsigned char)receiveData[i]) {
	case IAC:
	    need(++i);
	    switch (c = receiveData[i]) {
	    case DO:
	    case DONT:
	    case WILL:
	    case WONT:
		response[responseSize++] = IAC;
		response[responseSize++] = (c == DO || c == DONT) ? WONT : DONT;
		need(++i);
		std::clog << "receive " << telnetCommand(c) << " " << int(receiveData[i]) << std::endl;
		response[responseSize++] = receiveData[i];
		break;
	    default:
		std::clog << "unknown telnet command " << int(c) << std::endl;
                break;
	    }
	    break;

	case '\0':
	    break; /* Discard nulls */

	default:
	    receiveData[j++] = c;
	    break;
	}
    }
    if (responseSize != 0)
	sendRaw((const char *)response, responseSize);
    receiveOffset = j;
}

void
ExpectProgram::execute(const ExpectNode *node)
{
    lastVisited = node;
    node->execute(*this);
}

void
ExpectProgram::closeFds()
{
    if (writeFd != -1) {
	flush();
	::close(writeFd);
    }
    if (readFd != -1 && readFd != writeFd)
	::close(readFd);
    readFd = writeFd = -1;
}

void
ExpectProgram::run(const ExpectNode *code, int r, int w)
{
    receiveOffset = sendOffset = 0;
    exceptionHandler = 0;

    try {
	readFd = r;
	writeFd = w;
	execute(code);
	closeFds();
    }
    catch (...) {
	closeFds();
	throw;
    }
}

ExpectProgram::~ExpectProgram()
{
    closeFds();
    delete[] receiveData;
    delete[] sendData;
}

ExpectTimeout::ExpectTimeout(const char **attribs)
{
    const char *to = ExpatParserHandlers::getAttribute(attribs, "msec");
    value = to ? atoi(to) : 2000;

    for (const char **cpp = attribs; cpp[0]; cpp += 2) {
	int count = atoi(cpp[1]);
	if (!strcmp(cpp[0], "msec"))
	    value = count;
	else if (!strcmp(cpp[0], "sec"))
	    value = count * 1000;
    }
}

void
ExpectTimeout::execute(ExpectProgram &prog) const
{
    prog.timeout = value;
}

ExpectVariable::ExpectVariable(const char **attributes)
{
    const char *v = ExpatParserHandlers::getAttribute(attributes, "key");
    key = v ? v : "";
    v = ExpatParserHandlers::getAttribute(attributes, "default");
    def = v ? v : "";
}

void
ExpectVariable::write(ExpectProgram &program, std::ostream &os) const
{
    os << program.variables[key];
}

ExpectTemplate::ExpectTemplate(const char **attributes)
{
    const char *cname = ExpatParserHandlers::getAttribute(attributes, "name");
    name = cname ? cname : "unnamed";
}

ExpectListen::ExpectListen(const char **attribs)
    : net(attribs, 0)
{
}

void
ExpectListen::execute(ExpectProgram &program) const
{
    program.closeFds();
    program.readFd = program.writeFd = net.connect();
}


ExpectNetwork::ExpectNetwork(const char **attribs)
    : net(attribs, 0)
{
}
void
ExpectNetwork::execute(ExpectProgram &program) const
{
    program.closeFds();
    program.readFd = program.writeFd = net.connect();
}

ExpectModem::ExpectModem(const char **attribs)
    : modem(attribs, 0)
{
}

void
ExpectModem::execute(ExpectProgram &program) const
{
    program.closeFds();
    program.readFd = program.writeFd = modem.connect();
}

ExpectDo::ExpectDo(const char **attributes)
{
    const char *p = ExpatParserHandlers::getAttribute(attributes, "status");
    status = p ? p : "";
}

void
ExpectDo::execute(ExpectProgram &program) const
{
    std::string oldStatus = program.status;
    if (status != "")
	program.status = status;
    const ExpectNode *oldExceptionHandler = program.exceptionHandler;
    try {
	executeChildren(program);
    } catch (...) {
	for (const ExpectNode *n = program.exceptionHandler; n; n = n->nextSibling)
	    program.execute(n);
	program.exceptionHandler = oldExceptionHandler; // Restore this, but not status.
	throw;
    }
    program.status = oldStatus;
    program.exceptionHandler = oldExceptionHandler;
}

ExpectOnError::ExpectOnError(const char **attribs)
    : ExpectElement()
{
}

void
ExpectOnError::execute(ExpectProgram &program) const
{
    program.exceptionHandler = firstChild;
}

ExpectIf::ExpectIf(const char **)
{
}

void
ExpectIf::execute(ExpectProgram &program) const
{
    const ExpectCharacterData *cond = dynamic_cast<const ExpectCharacterData *>(firstChild);
    if (!cond)
	throw ExpectSyntaxException("if condition is not a string");
    const ExpectThen *thn = cond->nextSibling ? dynamic_cast<const ExpectThen *>(cond->nextSibling) : 0;
    if (!thn)
	throw ExpectSyntaxException("no then in if");
    const ExpectElse *els = thn->nextSibling ? dynamic_cast<const ExpectElse *>(cond->nextSibling) : 0;
    std::stringstream expr;
    cond->write(program, expr);
    std::string str = expr.str();
    if (atoi(str.c_str()))
	thn->execute(program);
    else if (els)
	els->execute(program);
}

ExpectThen::ExpectThen(const char **)
{
}

ExpectElse::ExpectElse(const char **)
{
}

ExpectPrint::ExpectPrint(const char **)
{
}

void
ExpectPrint::execute(ExpectProgram &program) const
{
    for (const ExpectCharacterData *data = dynamic_cast<const ExpectCharacterData *>(firstChild); data; data = dynamic_cast<const ExpectCharacterData *>(data->nextSibling))
	data->write(program, std::cout);
}

ExpectStrlen::ExpectStrlen(const char **)
{
}

void
ExpectStrlen::write(ExpectProgram &program, std::ostream &os) const
{
    std::stringstream sub;
    for (const ExpectCharacterData *data = dynamic_cast<const ExpectCharacterData *>(firstChild); data; data = dynamic_cast<const ExpectCharacterData *>(data->nextSibling))
	data->write(program, sub);
    os << sub.str().size();
}

ExpectStreq::ExpectStreq(const char **)
{
}

ExpectDrip::ExpectDrip(const char **attributes)
    : rate(0)
{
    const char *p = ExpatParserHandlers::getAttribute(attributes, "rate");
    if (p)
	rate = atoi(p);
}

void
ExpectDrip::execute(ExpectProgram &program) const
{
    program.dripRate = rate;
}

ExpectDrip::~ExpectDrip()
{
}

void
ExpectStreq::write(ExpectProgram &program, std::ostream &os) const
{
    std::stringstream ls, rs;
    const ExpectCharacterData *l, *r;
    if (!(l = dynamic_cast<const ExpectCharacterData *>(firstChild)))
	throw ExpectSyntaxException("streq first argument must be a string");
    if (!(r = dynamic_cast<const ExpectCharacterData *>(l->nextSibling)))
	throw ExpectSyntaxException("streq second argument must be a string");
    l->write(program, ls);
    r->write(program, rs);
    os << (ls.str() == rs.str());
}

ExpectStrcat::ExpectStrcat(const char **)
{
}

void
ExpectStrcat::write(ExpectProgram &program, std::ostream &os) const
{
    for (const ExpectNode *child = firstChild; child; child = child->nextSibling) {
        const ExpectCharacterData *chars = dynamic_cast<const ExpectCharacterData *>(child);
        if (chars)
            chars->write(program, os);
    }
}


Vt100EscapeCodes::Vt100EscapeCodes()
{
    std::map<std::string, std::string> &me = *this;

    me["up"]      = "\033[A";
    me["down"]    = "\033[B";
    me["right"]   = "\033[C";
    me["left"]    = "\033[D";
    me["f1"]      = "\033OP";
    me["f2"]      = "\033OQ";
    me["f3"]      = "\033OR";
    me["f4"]      = "\033OS";
    // TODO XXX Finish me.
}

const ExpectNode *
ExpectNodeFilter::search(const ExpectNode *node)
{
    switch (visit(node)) {
	case Found:
	    return node;
	case Skip:
	    return 0;
	case Descend:
	    for (ExpectNode *c = node->firstChild; c; c = c->nextSibling) {
		const ExpectNode *found;
		if ((found = search(c)) != 0)
		    return found;
	    }
	    return 0;
        default:
            abort();
            return 0;
    }
}
