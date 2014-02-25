OBJS += expatwrap.o xmlexpect.o connection.o util.o
VPATH=..
CXXFLAGS += -g -Wall -std=c++0x

all: test_expat test_treealgo

test_expat: $(OBJS) test_expat.o
	$(CXX) $(CXXFLAGS) -o $@ test_expat.o $(OBJS) -lexpat
test_treealgo: util.o expatwrap.o test_treealgo.o
	$(CXX) $(CXXFLAGS) -o $@ test_treealgo.o expatwrap.o util.o -lexpat
clean:
	rm -f $(OBJS) xmlexpect tags
