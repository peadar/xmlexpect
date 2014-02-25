#include "../expatwrap.h"
#include <iostream>

using namespace std;

struct Node {
    string name;
    Node *firstChild;
    Node *nextSib;
    void print(int depth = 0) {
        std::cout << pad(depth) << name << std::endl;
        for (auto child = firstChild; child; child = child->nextSib)
            child->print(depth + 4);
    }
};

struct TestHandler : public ExpatParserHandlers {
    void startElement(const std::string &name, const Attributes &attrs) override;
    void endElement(const std::string &name) override;
    Node *cur;
    Node **next;
    TestHandler();
};

TestHandler::TestHandler()
    : cur(0)
    , next(&cur)
{
}

void
TestHandler::startElement(const std::string &name, const Attributes &attrs)
{
    std::clog << this << ": start element " << name << std::endl;
    Node *n = new Node();
    n->nextSib = cur; // back-link to parent.
    cur = *next = n;
    next = &n->firstChild;
    n->name = name;
}

void
TestHandler::endElement(const std::string &name)
{
    *next = 0;
    next = &cur->nextSib;
    if (cur->nextSib)
        cur = cur->nextSib;
}


int
main(int argc, char *argv[])
{

    ExpatParser p;
    TestHandler h;

    p.push(&h);
    p.parseFile(argv[1]);

    h.cur->print();
}
