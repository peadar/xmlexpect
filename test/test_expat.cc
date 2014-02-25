#include "../expatwrap.h"
#include <iostream>


class TestHandler : public ExpatParserHandlers {
    void startElement(const std::string &name, const Attributes &attrs) override;
    void endElement(const std::string &name) override;
    void pop() override;
};

void
TestHandler::startElement(const std::string &name, const Attributes &attrs)
{
    std::clog << this << ": start element " << name << std::endl;
    if (name == "new") {
        auto h = new TestHandler();
        parser->push(h);
        std::clog << this << ": new handlers " << h << " pushed" << std::endl;
    }

}

void
TestHandler::endElement(const std::string &name)
{
    std::clog << this << ": end element " << name << std::endl;

}


void
TestHandler::pop()
{
    std::clog << this << ": pop handlers " << this << std::endl;
}

int
main(int argc, char *argv[])
{

    ExpatParser p;
    TestHandler h;

    p.push(&h);

    p.parseFile(argv[1]);
}
