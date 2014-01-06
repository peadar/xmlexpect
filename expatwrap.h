/*
 * C++ wrapper for Expat, with stack based handlers.
 *
 * (c) Peter Edwards, March 2004.
 *
 * $Id: expatWrap.h,v 1.4 2004/06/19 13:03:53 petere Exp $
 */

#ifndef expatwrap_h_guard
#define expatwrap_h_guard
#include "util.h"

#ifdef __FreeBSD__
#include <bsdxml.h>
#else
#include <expat.h>
#endif

class ExpatInputStream {
public:
    virtual void moreData(void *, int maxLen, int &populatedLen, bool &final) = 0;
    static const int buffSize = 512;
};

class ExpatFileInputStream : public ExpatInputStream {
    int fd;
public:
    void moreData(void *, int maxLen, int &populatedLen, bool &final);
    ExpatFileInputStream(const char *fileName);
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
    UnknownElement(const char *str) : element(str) {}
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
    void parseFile(const char *fileName);
    int getCurrentLineNumber() { return XML_GetCurrentLineNumber(expatParser); }
};

class ExpatParserHandlers {
    friend class ExpatParser;
    friend void ExpatParserHandlers_endElementTrampoline(void *, const XML_Char *);
public:
    static const char *getAttribute(const char **atts, const char *name);
    static bool boolAttribute(const char *);
    ExpatParser *parser; // Parser we are handling events from
    virtual void startElement(const XML_Char *name, const XML_Char **atts);
    virtual void endElement(const XML_Char *name);
    virtual void characterData(const XML_Char *data, int len);
    virtual void processingInstruction(const XML_Char *target, const XML_Char *data);
    virtual void comment(const XML_Char *comment);
    virtual void startCdataSection();
    virtual void endCdataSection();
    virtual void defaultData(const XML_Char *data, int size);
    virtual void xmlDecl(const XML_Char *version, const XML_Char *encoding, bool standAlone);
    virtual void startDoctype(const XML_Char *docTypeName, const XML_Char *sysId, const XML_Char *pubId, bool hasInternalInternalSubset);
    virtual void endDoctype();
    virtual int externalEntityRef(const XML_Char *context,
				  const XML_Char *base,
				  const XML_Char *systemId,
				  const XML_Char *publicId);
    ExpatParserHandlers();
    virtual ~ExpatParserHandlers();
};

#endif

