/*
 * utilities etc.
 */

#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include "util.h"

const char *
pad(int indent)
{
    static const char padding[] =
	"                                        "
	"                                        ";
    static const int len = (sizeof padding) - 1;
    return indent < len ? padding + len - indent : padding;
}

std::string
printableString(const char *data, int len)
{
    std::string s = "";

    for (int i = 0; i < len; i++) {
	char c = data[i];
	if (c >= 32 && c < 0x7f)  {
	    s += c;
	} else {
	    char buf[5];
	    snprintf(buf, 5, "<%2.2x>", c);
	    s += buf;
	}
    }
    return s;
}

Exception::Exception()
{
}

Exception::Exception(const Exception &rhs)
{
}

std::ostream &
operator <<(std::ostream &os, const Exception &ex)
{
    ex.describe(os);
    return os;
}

Exception::~Exception()
    throw()
{
}

UnixException::UnixException(int err, const char *sysCall)
    : uxError(err)
    , sysCall(sysCall)
{
}

UnixException::~UnixException()
    throw()
{
}

std::ostream &
UnixException::describe(std::ostream &os) const
{
    return os << sysCall << ": " << strerror(uxError);
}


FileOpenException::FileOpenException(const std::string &name, int error)
    : UnixException(error, "open")
    , fileName(name)
{
}

FileOpenException::~FileOpenException() throw()
{
}

std::ostream &
FileOpenException::describe(std::ostream &os) const
{
    os << "open: " << fileName << ": ";
    UnixException::describe(os);
    return os;
}

std::ostream &
IndexOutOfBoundsException::describe(std::ostream &os) const
{
    return os << "index out of range";
}

IndexOutOfBoundsException::IndexOutOfBoundsException()
{
}

IndexOutOfBoundsException::~IndexOutOfBoundsException()
    throw()
{
}
