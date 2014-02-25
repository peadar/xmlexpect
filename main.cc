/*
 * XML expect mini-language commandline driver.
 */

#include "xmlexpect.h"
#include <iostream>
#include <unistd.h>

static int
usage()
{
    std::clog << "xmlexpect <file>" << std::endl;
    return -1;
}

int
main(int argc, char *argv[])
{
    std::map<std::string, std::string> variables;
    if (argc != 2)
	return usage();
    try {
        ExpectHandlers handlers;
	ExpatParser parser;
        parser.push(&handlers);
        parser.parseFile(argv[1]);
	ExpectProgram expect(1024, variables);
	int r = dup(0);
	int w = dup(1);
	expect.run(handlers.root(), r, w);
	std::clog << "completed" << std::endl;
	return 0;
    }
    catch (const Exception &ex) {
	std::clog << "ERROR: " << ex << std::endl;
    }
}

