OBJS += expatwrap.o main.o xmlexpect.o connection.o util.o
CXXFLAGS += -g -Wall

all: xmlexpect

xmlexpect: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lexpat
clean:
	rm -f $(OBJS) xmlexpect tags
