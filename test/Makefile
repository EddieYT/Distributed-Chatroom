TARGETS = proxy stresstest

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

stresstest: stresstest.o
	g++ $^ -o $@

proxy: proxy.o
	g++ $^ -o $@

clean::
	rm -fv $(TARGETS) *~ *.o
