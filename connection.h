/*
 * Connection management.
 */
#ifndef connection_h_guard
#define connection_h_guard

class Connection {
protected:
    int facility;
public:
    Connection(int facility);
    virtual ~Connection();
    virtual int connect() const = 0;
};

class ResolverException : public Exception {
    int gaie;
    std::string function;
public:
    std::ostream &describe(std::ostream &) const;
    ResolverException(const std::string &function, int gaie);
    ~ResolverException() throw();
};

class ModemConnection : public Connection {
    std::string device;
    int speed;
    int bits;
    bool flowXonXoff;
    bool flowHard;
    bool parity;
    bool oddParity;
public:
    ModemConnection(const char **settings, int facility);
    ~ModemConnection();
    int connect() const;
};

class NetworkConnection : public Connection {
    std::string host;
    std::string service;
public:
    NetworkConnection(const char **settings, int facility);
    ~NetworkConnection();
    int connect() const;
};

class ListenConnection : public Connection {
    std::string host;
    std::string service;
public:
    ListenConnection(const char **settings, int facility);
    ~ListenConnection();
    int connect() const;
};

#endif
