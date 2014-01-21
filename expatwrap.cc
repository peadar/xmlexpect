/*
 * Expat C++ wrapper
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sstream>
#include "util.h"
#include "expatwrap.h"

/*
 * Trampolines: These provide the jump from C linkage, non-member functions to
 * C++ virtual members, by converting the "userdata" passed to Expat callbacks
 * into a reference to the C++ ExpatParser object.
 */


void
ExpatParserHandlers_startElementTrampoline(void *userData, const XML_Char *name, const XML_Char **attribs)
{
    ExpatParser *parser = static_cast<ExpatParser *>(userData);
    if (parser->stop)
	return;
    Attributes cppattrs;
    for (size_t i = 0; attribs[i]; i += 2)
        cppattrs[attribs[i]] = attribs[i+1];
    try {
	parser->handlers.startElement(name, cppattrs);
    }
    catch (const Exception &ex) {
	std::stringstream ss;
	ss << ex;
	parser->reason = ss.str();
	parser->stop = true;
    }
}

void
ExpatParserHandlers_endElementTrampoline(void *userData, const XML_Char *name)
{
    ExpatParser *parser = static_cast<ExpatParser *>(userData);
    parser->handlers.endElement(name);
}

extern "C" {
static void
characterDataTrampoline(void *userData, const XML_Char *data, int len)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.characterData(std::string(data, len));
}

static void
processingInstructionTrampoline(void *userData, const XML_Char *target, const XML_Char *data)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.processingInstruction(target, data);
}

static void
commentTrampoline(void *userData, const XML_Char *cmnt)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.comment(cmnt);
}

static void
startCdataSectionTrampoline(void *userData)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.startCdataSection();
}

static void
endCdataSectionTrampoline(void *userData)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.endCdataSection();
}

static void
defaultDataTrampoline(void *userData, const XML_Char *data, int len)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.defaultData(std::string(data, len));
}

static void
xmlDeclTrampoline(void *userData, const XML_Char *version, const XML_Char *encoding, int standAlone)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.xmlDecl(version, encoding, standAlone);
}

static void
startDoctypeTrampoline(void *userData, const XML_Char *docTypeName, const XML_Char *sysId, const XML_Char *pubId, int hasInternalSubset)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.startDoctype(docTypeName, sysId, pubId, hasInternalSubset);
}

static void
endDoctypeTrampoline(void *userData)
{
    ExpatParser *p = static_cast<ExpatParser *>(userData);
    if (!p->stop)
	p->handlers.endDoctype();
}

static int
externalEntityRefTrampoline(XML_Parser parser,
					const XML_Char *context,
					const XML_Char *base,
					const XML_Char *systemId,
					const XML_Char *publicId)
{
    ExpatParser *p = static_cast<ExpatParser *>(XML_GetUserData(parser));
    if (!p->stop)
	return p->handlers.externalEntityRef(context, base, systemId, publicId);
    else
	return -1;
}

}

/*
 * ExpatParser
 */
ExpatParser::ExpatParser(ExpatParserHandlers &handlers_, const XML_Char *encoding_, XML_Char sep_)
    : handlers(handlers_)
{
    handlers.parser = this;
    encoding = strdup(encoding_);
    separator = sep_;
    expatParser = XML_ParserCreateNS(encoding, separator);
    stop = false;

    // Setup trampolines from C-style callbacks to virtual methods
    XML_SetStartElementHandler(expatParser, ExpatParserHandlers_startElementTrampoline);
    XML_SetEndElementHandler(expatParser, ExpatParserHandlers_endElementTrampoline);
    XML_SetCharacterDataHandler(expatParser, characterDataTrampoline);
    XML_SetProcessingInstructionHandler(expatParser, processingInstructionTrampoline);
    XML_SetCommentHandler(expatParser, commentTrampoline);
    XML_SetStartCdataSectionHandler(expatParser, startCdataSectionTrampoline);
    XML_SetEndCdataSectionHandler(expatParser, endCdataSectionTrampoline);
    XML_SetDefaultHandler(expatParser, defaultDataTrampoline);
    XML_SetExternalEntityRefHandler(expatParser, externalEntityRefTrampoline);
    XML_SetXmlDeclHandler(expatParser, xmlDeclTrampoline);
    XML_SetStartDoctypeDeclHandler(expatParser, startDoctypeTrampoline);
    XML_SetEndDoctypeDeclHandler(expatParser, endDoctypeTrampoline);
    XML_SetParamEntityParsing(expatParser, XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);

    // Give trampolines a pointer to "this", so they can do their work
    XML_SetUserData(expatParser, this);


#if 0
    /*
     * These are callbacks that are not yet implemented in C++, due to time
     * constriaints and finger pain. Note that the bulk of these make up the
     * DTD handling part of expat
     */
    XML_SetDefaultHandlerExpand
    XML_SetSkippedEntityHandler
    XML_SetUnknownEncodingHandler
    XML_SetStartNamespaceDeclHandler
    XML_SetEndNamespaceDeclHandler
    XML_SetNamespaceDeclHandler
    XML_SetElementDeclHandler
    XML_SetAttlistDeclHandler
    XML_SetEntityDeclHandler
    XML_SetUnparsedEntityDeclHandler
    XML_SetNotationDeclHandler
    XML_SetNotStandaloneHandler
#endif
}

ExpatParser::~ExpatParser()
{
    XML_ParserFree(expatParser);
    free(const_cast<char *>(encoding));
}

ExpatParserHandlers::ExpatParserHandlers()
{
    parser = 0;
}

ExpatParserHandlers::~ExpatParserHandlers()
{
}

void
ExpatParserHandlers::startElement(const std::string &name, const Attributes &)
{
}

void
ExpatParserHandlers::endElement(const std::string &)
{
}

void
ExpatParserHandlers::characterData(const std::string &data)
{
}

void
ExpatParserHandlers::processingInstruction(const std::string &, const std::string &data)
{
}

void
ExpatParserHandlers::comment(const std::string &comment)
{
}

void
ExpatParserHandlers::startCdataSection()
{
}

void
ExpatParserHandlers::endCdataSection()
{
}

void
ExpatParserHandlers::defaultData(const std::string &)
{
}

void
ExpatParserHandlers::xmlDecl(const std::string &version, const std::string &encoding, bool standAlone)
{
}

void
ExpatParserHandlers::startDoctype(const std::string &docTypeName, const std::string &sysId, const std::string &pubId, bool hasInternalSubset)
{
}

void
ExpatParserHandlers::endDoctype()
{
}

int
ExpatParserHandlers::externalEntityRef(
				const std::string &context,
				const std::string &base,
				const std::string &systemId,
				const std::string &publicId)
{
    return 0;
}

void
ExpatParser::parseSome(ExpatInputStream &is, bool &final)
{
    int receivedSize;
    void *b = XML_GetBuffer(expatParser, ExpatInputStream::buffSize);

    is.moreData(b, ExpatInputStream::buffSize, receivedSize, final);
    int rc = XML_ParseBuffer(expatParser, receivedSize, final ? 1 : 0);
    if (stop)
	throw ParseException(*this, reason);
    switch (rc) {
	case XML_STATUS_OK:
	    return;
	default:
	    throw ParseException(*this, "parse error");
    }
}

void
ExpatParser::parse(ExpatInputStream &is)
{
    bool final;
    for (final = false; !final;)
	parseSome(is, final);
}

void
ExpatParser::parseFile(const std::string &fileName)
{
    ExpatFileInputStream s(fileName);
    parse(s);
}

ExpatFileInputStream::ExpatFileInputStream(const std::string &fileName)
{
    if ((fd = open(fileName.c_str(), O_RDONLY, 0666)) == -1)
	throw FileOpenException(fileName, errno);
}

void ExpatFileInputStream::moreData(void *p, int maxLen, int &receivedLen, bool &final)
{
    receivedLen = ::read(fd, p, maxLen);
    if (receivedLen == -1)
	throw UnixException(errno, "read");

    if (receivedLen == 0)
	final = true;
}

ExpatFileInputStream::~ExpatFileInputStream()
{
    close(fd);
}

void
SocketInputStream::moreData(void *ptr, int maxLen, int &populatedLen, bool &final)
{
retry:
    switch (populatedLen = read(fd, ptr, maxLen)) {
    case 0:
	final = true;
	break;
    case -1:
	if (errno == EINTR)
	    goto retry;
        throw UnixException(errno, "read");
	break;
    default:
	final = false;
	break;
    }
}

SocketInputStream::SocketInputStream(int fd)
    : fd(fd)
{
}

ParseException::ParseException(const ExpatParser &prs, std::string ss)
{
    code = XML_GetErrorCode(prs.expatParser);
    line = XML_GetCurrentLineNumber(prs.expatParser);
    reason = ss;
}

std::ostream &
ParseException::describe(std::ostream &os) const
{
    return os << "parse error at line " << line << " : " << (reason != "" ? reason : XML_ErrorString(code));
    
}

ParseException::~ParseException()
    throw()
{
}
