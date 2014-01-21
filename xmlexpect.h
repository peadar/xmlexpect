/*
 * An XML mini-language for holding ASCII dialogs with things expecting human
 * interaction
 */

#ifndef xmlexpect_h_guard
#define xmlexpect_h_guard
#include <map>
#include <string>
#include "util.h"
#include "expatwrap.h"

class ExpectNode;
class ExpectFactory;

class ExpectProgram {
    void stripTelnet();
    void receiveRaw();
    void sendRaw(const char *data, int len);
public:
    const ExpectNode *exceptionHandler;
    unsigned dripRate;
    std::map<std::string, std::string> variables;
    std::string status;
    std::string matching;
    const ExpectNode *lastVisited;
    int readFd;
    int writeFd;
    int timeoutUsec;
    int expectDelay;
    int receiveSize;
    char *receiveData;
    int receiveOffset;
    int sendSize;
    char *sendData;
    int sendOffset;
    int logFacility;
    ExpectProgram(int maxBuf, std::map<std::string, std::string> &);
    int match(std::string);
    void send(const char *data, int len);
    void flush();
    void receive();
    void need(int);
    void connect(const std::string &host, const std::string &service);
    virtual void run(const ExpectNode *node, int readFd, int writeFd);
    virtual ~ExpectProgram();
    void execute(const ExpectNode *node);
    void closeFds();
    virtual void statusUpdate(std::string); // Virtual callback for applications.
};

class ExpectException : public Exception {
public:
    ~ExpectException() throw () {}
};

class ExpectTimeoutException : public ExpectException {
public:
    std::string waitingFor;
    std::string status;
    std::string currentData;
    ExpectTimeoutException(std::string waitingFor, std::string, std::string current);
    virtual std::ostream &describe(std::ostream &) const;
    ~ExpectTimeoutException() throw () {}
};

class ExpectSyntaxException : public ExpectException {
    std::string reason;
public:
    ExpectSyntaxException(std::string reason);
    virtual std::ostream &describe(std::ostream &) const;
    ~ExpectSyntaxException() throw () {}
};

class ExpectNodeFilter;

class ExpectNode {
public:
    ExpectNode *nextSibling;
    ExpectNode *firstChild;
    int lineNumber;
    ExpectFactory *type;
    virtual void execute(ExpectProgram &program) const;
    void executeChildren(ExpectProgram &program) const;
    ExpectNode();
    virtual ~ExpectNode();
};

struct ExpectNodeFilter {
    enum FilterResult
    { Skip
    , Descend
    , Found };

    virtual FilterResult visit(const ExpectNode *node) = 0;
    const ExpectNode *search(const ExpectNode *node);

};

class ExpectCharacterData : public ExpectNode {
public:
    virtual void write(ExpectProgram &, std::ostream &) const = 0;
    void execute(ExpectProgram &program) const;
};

class ExpectElement : public ExpectNode {
public:
    Attributes attributes;
};

class ExpectControlElement : public ExpectNode {
    // Special element that will not receive "chardata" children.
};

struct ExpectHandlers : public ExpatParserHandlers {
    ExpectNode *stack[1024];
    int sp;
    void addNode(ExpectNode *);
protected:
    void startElement(const std::string &name, const Attributes &attributes);
    void characterData(const std::string &data);
    void endElement(const char *name);
    virtual ExpectNode *getNode(const std::string &name,
            const Attributes &attributes); // allows user to add extra commands.
public:
    ExpectNode *root();
    ExpectHandlers();
    ~ExpectHandlers();
};

#endif
