/*
 * C++ wrapper for Expat, with stack based handlers.
 */

#ifndef expatwrap_h_guard
#define expatwrap_h_guard
#include "util.h"

#include <map>

#ifdef __FreeBSD__
#include <bsdxml.h>
#else
#include <expat.h>
#endif

typedef std::map<std::string, std::string> Attributes;

class ExpatInputStream {
public:
    virtual void moreData(void *, int maxLen, int &populatedLen, bool &final) = 0;
    static const int buffSize = 512;
};

class ExpatFileInputStream : public ExpatInputStream {
    int fd;
public:
    void moreData(void *, int maxLen, int &populatedLen, bool &final);
    ExpatFileInputStream(const std::string &fileName);
    virtual ~ExpatFileInputStream();
};

class SocketInputStream : public ExpatInputStream {
private:
    int fd;
public:
    SocketInputStream(int fd);
    virtual void moreData(void *, int maxLen, int &populatedLen, bool &final);
};

class XMLException : public Exception {
public:
    ~XMLException() throw() {}
};

class UnknownElement : public XMLException {
    std::string element;
protected:
    std::ostream &describe(std::ostream &) const;
public:
    UnknownElement(const std::string &str) : element(str) {}
    ~UnknownElement() throw() {}

};

class ExpatParser;
class ParseException : public XMLException {
protected:
    std::ostream &describe(std::ostream &) const;
public:
    XML_Error code;
    int line;
    std::string reason;
    ParseException(const ExpatParser &, std::string);
    ~ParseException() throw();
};

class ExpatParserHandlers;

extern "C" {
    // these two are friends, so must be declared up front. The rest are just static inline in the source.
    void ExpatParserHandlers_startElementTrampoline(void *userData, const XML_Char *name, const XML_Char **attribs);
    void ExpatParserHandlers_endElementTrampoline(void *userData, const XML_Char *name);
}

class ExpatParser {
    const XML_Char *encoding;
    XML_Char separator;
    XML_Parser expatParser;
    friend class ParseException;
    int depth;
    friend void ExpatParserHandlers_startElementTrampoline(void *, const XML_Char *, const XML_Char **);
    friend void ExpatParserHandlers_endElementTrampoline(void *, const XML_Char *);
    std::string reason;
public:
    bool stop;
    ExpatParserHandlers &handlers;
    ExpatParser(ExpatParserHandlers &handlers, const XML_Char *encoding = "UTF-8", XML_Char sep = ':');
    virtual ~ExpatParser();
    void parse(ExpatInputStream &is);
    void parseSome(ExpatInputStream &, bool &final);
    void parseFile(const std::string &fileName);
    int getCurrentLineNumber() { return XML_GetCurrentLineNumber(expatParser); }
};

class ExpatParserHandlers {
    friend class ExpatParser;
    friend void ExpatParserHandlers_endElementTrampoline(void *, const XML_Char *);
public:
    ExpatParser *parser; // Parser we are handling events from
    virtual void startElement(const std::string &name, const Attributes &);
    virtual void endElement(const std::string &);
    virtual void characterData(const std::string &data);
    virtual void processingInstruction(const std::string &target, const std::string &data);
    virtual void comment(const std::string &comment);
    virtual void startCdataSection();
    virtual void endCdataSection();
    virtual void defaultData(const std::string &);
    virtual void xmlDecl(const std::string &version, const std::string &encoding, bool standAlone);
    virtual void startDoctype(const std::string &docTypeName, const std::string &sysId, const std::string &pubId, bool hasInternalInternalSubset);
    virtual void endDoctype();
    virtual int externalEntityRef(const std::string & context,
				  const std::string & base,
				  const std::string & systemId,
				  const std::string & publicId);
    ExpatParserHandlers();
    virtual ~ExpatParserHandlers();
};

#endif

