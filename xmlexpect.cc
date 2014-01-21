/*
 * XML Expect dialog mini-language implementation
 */

#include <poll.h>
#include <regex.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <arpa/telnet.h>

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

struct Parameter {
    int idx;
    std::string name;
    std::string dflt;
    bool required;

    Parameter(std::map<std::string, Parameter *> &params, const std::string &&name_, const std::string &&dflt_)
        : name(name_)
        , dflt(dflt_)
    {
        params[name] = this;
        required = false;
    }

    Parameter(std::map<std::string, Parameter *> &params, const std::string &&name_)
        : name(name_)
    {
        params[name] = this;
        required = true;
    }
};

class ExpectFactory {
public:
    ExpectFactory(const std::string &name_) : name(name_) {}
    static ExpectNode *create(ExpectNode *parent, const std::string &name, const Attributes &attrs);
    Parameter *getParameter(const std::string &name) const {
        auto i = params.find(name);
        return i == params.end() ? i->second : nullptr;
    }
protected:
    virtual ExpectNode *createNode(const Attributes &) = 0;
    std::map<std::string, Parameter *> params;
private:
    typedef  std::map<std::string, ExpectFactory *> Factories;
    static Factories allFactories;
    std::string name;
};

ExpectFactory::Factories ExpectFactory::allFactories;

template <typename T> struct DfltExpectFactory : public ExpectFactory {
    ExpectNode *createNode(const Attributes &attrs) { return new T(attrs); }
    DfltExpectFactory(const std::string &name_) : ExpectFactory(name_) { }
};

class ExpectNetwork : public ExpectElement {
    NetworkConnection net;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectNetwork(const Attributes &);
};

struct ExpectNetworkFactory : public DfltExpectFactory<ExpectNetwork> {
    Parameter host{params, "host", "localhost"};
    Parameter port{params, "port"};
    ExpectNetworkFactory() : DfltExpectFactory("network") {}
};
ExpectNetworkFactory network;

class ExpectListen : public ExpectElement {
    ListenConnection net;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectListen(const Attributes &);
};
struct ExpectListenFactory : public DfltExpectFactory<ExpectListen> {
    Parameter port{params, "port"};
    ExpectListenFactory() : DfltExpectFactory("listen") {}
};
ExpectListenFactory listen;

class ExpectVariable : public ExpectCharacterData {
    std::string key;
    std::string def;
public:
    virtual void write(ExpectProgram &, std::ostream &) const;
    ExpectVariable(const Attributes &);
};
struct ExpectVariableFactory : public DfltExpectFactory<ExpectVariable> {
    Parameter port{params, "get"};
    ExpectVariableFactory() : DfltExpectFactory("variable") {}
};
ExpectVariableFactory variable;


class ExpectModem : public ExpectElement {
    ModemConnection connection;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectModem(const Attributes &attrs);
};

struct ExpectModemFactory : public DfltExpectFactory<ExpectModem> {
    Parameter device{params, "device"};
    Parameter speed{params, "speed","9600"};
    Parameter bits{params, "bits", "8"};
    Parameter flowXonXoff{params, "xonxoff", "false"};
    Parameter flowHard{params, "rtscts", "true"};
    Parameter parity{params, "parity", "none"};
    ExpectModemFactory() : DfltExpectFactory("modem") {}
};
ExpectModemFactory modem;

class ExpectSleep : public ExpectElement {
    int delayUsec;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectSleep(const Attributes &attributes);
};

struct ExpectSleepFactory : public DfltExpectFactory<ExpectSleep> {
    Parameter seconds{params, "seconds"};
    ExpectSleepFactory() : DfltExpectFactory("sleep") {}
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
    ExpectLog(const Attributes &attributes);
    ~ExpectLog();
};

class ExpectDrip : public ExpectElement {
    unsigned rate;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectDrip(const Attributes &attributes);
    ~ExpectDrip();
};

class ExpectComment : public ExpectElement {
    int level;
public:
    virtual void execute(ExpectProgram &program)  const {}
};

class ExpectTimeout : public ExpectElement {
    int usecs;
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectTimeout(const Attributes &);
};

class ExpectChoose : public ExpectControlElement {
public:
    ExpectChoose();
    virtual void execute(ExpectProgram &program) const;
};

class ExpectIf : public ExpectControlElement {
public:
    ExpectIf(const Attributes &);
    virtual void execute(ExpectProgram &program) const;
};

class ExpectStrlen : public ExpectCharacterData {
public:
    ExpectStrlen(const Attributes &);
    virtual void write(ExpectProgram &, std::ostream &) const;
};

class ExpectStrcat : public ExpectCharacterData {
public:
    ExpectStrcat(const Attributes &);
    virtual void write(ExpectProgram &, std::ostream &) const;
};

class ExpectStreq : public ExpectCharacterData {
public:
    ExpectStreq(const Attributes &);
    virtual void write(ExpectProgram &, std::ostream &) const;
};

class ExpectThen : public ExpectElement {
public:
    ExpectThen(const Attributes &);
};

class ExpectElse : public ExpectElement {
public:
    ExpectElse(const Attributes &);
};

class ExpectPrint : public ExpectElement {
public:
    ExpectPrint(const Attributes &);
    virtual void execute(ExpectProgram &program) const;
};

class ExpectRawCharacterData : public ExpectCharacterData {
    std::string data;
public:
    ExpectRawCharacterData(const std::string &data, bool stripCtrl);
    ~ExpectRawCharacterData();
    void write(ExpectProgram &, std::ostream &) const;
};

class ExpectCtrl : public ExpectCharacterData {
    char character;
public:
    ExpectCtrl(const Attributes &);
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
    ExpectVt100(const Attributes &);
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
    ExpectDo(const Attributes &);
};

class ExpectOnError : public ExpectElement {
public:
    virtual void execute(ExpectProgram &program) const;
    ExpectOnError(const Attributes &);
};

class ExpectDefaultElement : public ExpectElement {
private:
public:
    ExpectDefaultElement(const std::string &, const Attributes &);
};

template <class T> bool
set(T &value, const Attributes &attrs, const std::string &name)
{
    Attributes::const_iterator i = attrs.find(name);
    if (i == attrs.end())
        return false;
    std::istringstream is;
    is.str(i->second);
    is >> value;
    return true;
}

template <> bool
set(ModemConnection::Parity &value, const Attributes &attrs, const std::string &name)
{
    std::string s;
    if (!set(s, attrs, name))
        return false;
    if (s == "even")
        value = ModemConnection::even;
    else if (s == "odd")
        value = ModemConnection::odd;
    return true;
}

template <class T> bool
set(T &value, const Attributes &attrs, const std::string &name, const T &&dfltValue)
{
    if (set(value, attrs, name))
        return true;
    value = dfltValue;
    return false;
}


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
ExpectHandlers::characterData(const std::string &data)
{
    if (sp > 0 && dynamic_cast<ExpectControlElement *>(stack[sp - 1]))
	return;
    addNode(new ExpectRawCharacterData(data, true));
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
ExpectHandlers::getNode(const std::string &name, const Attributes &attributes)
{
    if (name == "get")
	return new ExpectVariable(attributes);
    if (name == "listen")
	return new ExpectListen(attributes);
    if (name == "network")
	return new ExpectNetwork(attributes);
    if (name == "modem")
	return new ExpectModem(attributes);
    if (name == "choose")
	return new ExpectChoose();
    if (name == "expect" || name == "e")
	return new ExpectExpect();
    if (name == "send" || name == "s")
	return new ExpectSend();
    if (name == "do")
	return new ExpectDo(attributes);
    if (name == "timeout")
	return new ExpectTimeout(attributes);
    if (name == "br")
	return new ExpectRawCharacterData("\r\n", false);
    if (name == "comment")
	return new ExpectComment();
    if (name == "ctrl")
	return new ExpectCtrl(attributes);
    if (name == "vt100")
	return new ExpectVt100(attributes);
    if (name == "log")
	return new ExpectLog(attributes);
    if (name == "cr")
	return new ExpectRawCharacterData("\r", false);
    if (name == "lf")
	return new ExpectRawCharacterData("\n", false);
    if (name == "crlf")
	return new ExpectRawCharacterData("\r\n", false);
    if (name == "if")
	return new ExpectIf(attributes);
    if (name == "then")
	return new ExpectThen(attributes);
    if (name == "else")
	return new ExpectElse(attributes);
    if (name == "print")
	return new ExpectPrint(attributes);
    if (name == "sleep")
	return new ExpectSleep(attributes);
    if (name == "strlen")
	return new ExpectStrlen(attributes);
    if (name == "strcat")
	return new ExpectStrcat(attributes);
    if (name == "streq")
	return new ExpectStreq(attributes);
    if (name == "onerror")
	return new ExpectOnError(attributes);
    if (name == "drip")
	return new ExpectDrip(attributes);

    if (name == "include") {
        std::string filename;
        set(filename, attributes, "file");
	ExpectHandlers handlers;
	ExpatParser parser(handlers, "UTF-8");
	parser.parseFile(filename);
	return handlers.root();
    }
    throw UnknownElement(name);
}

std::ostream &
UnknownElement::describe(std::ostream &os) const
{
    return os << "unknown element \"" << element << "\"";
}

void
ExpectHandlers::startElement(const std::string &name, const Attributes &attributes)
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

ExpectCtrl::ExpectCtrl(const Attributes &attributes)
{
    std::string chr;
    if (set(chr, attributes, "char"))
        character = toupper(chr[0] - 'A' + 1);
    else if (set(chr, attributes, "code"))
        character = strtol(chr.c_str(), 0, 0);
}

void
ExpectCtrl::write(ExpectProgram &, std::ostream &os) const
{
    os << character;
}

ExpectVt100::ExpectVt100(const Attributes &attributes)
{
    std::string key;
    if (set(key, attributes, "key"))
        output = escapes[key];
}

void
ExpectVt100::write(ExpectProgram &, std::ostream &os) const
{
    os << output;
}

ExpectLog::ExpectLog(const Attributes &attributes)
{
    set(level, attributes, "level", 1);
    set(message, attributes, "message");
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

ExpectSleep::ExpectSleep(const Attributes &attributes)
{
    if (set(delayUsec, attributes, "usec"))
        ;
    else if (set(delayUsec, attributes, "msec"))
        delayUsec *= 1000;
    else if (set(delayUsec, attributes, "sec"))
        delayUsec *= 1000000;
    else
        delayUsec = 5000000;
}

void
ExpectSleep::execute(ExpectProgram &program) const
{
    program.flush();
    ::usleep(delayUsec);
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

ExpectDefaultElement::ExpectDefaultElement(const std::string &name,
            const Attributes &attributes)
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

ExpectRawCharacterData::ExpectRawCharacterData(const std::string &newData, bool stripCtrl)
{
    // Remove control characters from data.
    for (char c : newData)
        if (!stripCtrl || c >= 32)
            data.push_back(c);
}

ExpectRawCharacterData::~ExpectRawCharacterData() { }

void
ExpectRawCharacterData::write(ExpectProgram &, std::ostream &os) const
{
    os << data;
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
    , timeoutUsec(2000000)
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

    if (poll(&pfd, 1, timeoutUsec / 1000) == 0) {
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

ExpectTimeout::ExpectTimeout(const Attributes &attribs)
{
    if (set(usecs, attribs, "usec"))
        ;
    else if (set(usecs, attribs, "msec"))
        usecs *= 1000;
    else if (set(usecs, attribs, "msec"))
        usecs *= 1000000;
}

void
ExpectTimeout::execute(ExpectProgram &prog) const
{
    prog.timeoutUsec = usecs;
}

ExpectVariable::ExpectVariable(const Attributes &attributes)
{
    set(key, attributes, "key");
    set(def, attributes, "default");
}

void
ExpectVariable::write(ExpectProgram &program, std::ostream &os) const
{
    os << program.variables[key];
}

ExpectListen::ExpectListen(const Attributes &attribs)
    : net()
{
    set(net.host, attribs, "host");
    set(net.service, attribs, "port");
}

void
ExpectListen::execute(ExpectProgram &program) const
{
    program.closeFds();
    program.readFd = program.writeFd = net.connect();
}

ExpectNetwork::ExpectNetwork(const Attributes &attribs)
    : net()
{
}

void
ExpectNetwork::execute(ExpectProgram &program) const
{
    program.closeFds();
    program.readFd = program.writeFd = net.connect();
}

ExpectModem::ExpectModem(const Attributes &settings)
{
    set(connection.device, settings, "device");
    set(connection.speed, settings, "speed");
    set(connection.bits, settings, "bits");
    set(connection.flowXonXoff, settings, "xonxoff");
    set(connection.flowHard, settings, "rtscts");
    set(connection.parity, settings, "parity");
}

void
ExpectModem::execute(ExpectProgram &program) const
{
    program.closeFds();
    program.readFd = program.writeFd = connection.connect();
}

ExpectDo::ExpectDo(const Attributes &attributes)
{
    set(status, attributes, "status");
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

ExpectOnError::ExpectOnError(const Attributes &attribs)
    : ExpectElement()
{
}

void
ExpectOnError::execute(ExpectProgram &program) const
{
    program.exceptionHandler = firstChild;
}

ExpectIf::ExpectIf(const Attributes &)
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

ExpectThen::ExpectThen(const Attributes &)
{
}

ExpectElse::ExpectElse(const Attributes &)
{
}

ExpectPrint::ExpectPrint(const Attributes &)
{
}

void
ExpectPrint::execute(ExpectProgram &program) const
{
    for (const ExpectCharacterData *data = dynamic_cast<const ExpectCharacterData *>(firstChild); data; data = dynamic_cast<const ExpectCharacterData *>(data->nextSibling))
	data->write(program, std::cout);
}

ExpectStrlen::ExpectStrlen(const Attributes &)
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

ExpectStreq::ExpectStreq(const Attributes &)
{
}

ExpectDrip::ExpectDrip(const Attributes &attributes)
{
    set(rate, attributes, "rate", 0U);
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

ExpectStrcat::ExpectStrcat(const Attributes &)
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

ExpectNode *
ExpectFactory::create(ExpectNode *parent, const std::string &name, const Attributes &attrs)
{
    auto fac = allFactories.find(name);
    if (fac == allFactories.end()) {
        // no element factory - maybe its a parameter of current element
        if (parent && parent->type && parent->type->getParameter(name))
            return new ExpectStrcat(attrs);
        throw UnknownElement(name);
    }
    ExpectNode *node = fac->second->createNode(attrs);
    node->type = fac->second;
    return node;
}
