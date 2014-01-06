/*
 * Miscellaneous utilities.
 */

#ifndef util_h_guard
#define util_h_guard

#include <exception>
#include <iosfwd>
#include <string>

const char *pad(int indent);
std::string printableString(const char *data, int len);

class Exception : public std::exception {
protected:
    virtual std::ostream &describe(std::ostream &) const = 0;
    friend std::ostream &operator<<(std::ostream &, const Exception &);
public:
    Exception();
    Exception(const Exception &rhs);
    virtual ~Exception() throw();
};

std::ostream &operator<<(std::ostream &, const Exception &);

class UnixException : public Exception {
public:
    int uxError;
    std::string sysCall;
    std::ostream &describe(std::ostream &) const;
    UnixException(int error, const char *sysCall);
    ~UnixException() throw();
};

class IndexOutOfBoundsException : public Exception {
public:
    std::ostream&describe(std::ostream &) const;
    IndexOutOfBoundsException();
    ~IndexOutOfBoundsException() throw();
};

class FileOpenException : public UnixException {
    std::string fileName;
public:
    std::ostream& describe(std::ostream &) const;
    FileOpenException(const std::string &name, int error);
    ~FileOpenException() throw();
};

#endif
