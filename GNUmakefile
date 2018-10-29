BOOST_ROOT:=/opt/local

### USE_AGENTPP:=1

## CPPFLAGS+=-D_NO_LOGGING
CPPFLAGS+=-D_POSIX_C_SOURCE=200809L -DBOOST_TEST_NO_LIB -DPOSIX_THREADS
CPPFLAGS+=-I$(BOOST_ROOT)/include ## -isystem agent++/include
LDFLAGS+= -L$(BOOST_ROOT)/lib


CXXFLAGS+=-g
### CXXFLAGS+=-O2 -DNDEBUG
CXXFLAGS+=-Wextra -Wno-unused-parameter
#XXX CXXFLAGS+=--std=c++03
CXXFLAGS+=--std=c++14

.PHONY: all ctest test clean distclean cppcheck
all: build ### threads_test trylock_test thread_tss_test
	cd build && cmake --build .

build: CMakeLists.txt
	mkdir -p build
	cd build && cmake -G Ninja ..

ctest: build
	cd build && ctest -C debug

threads_test.o: threads_test.cpp
threads_test.o: threadpool.hpp

threadpool.o: threadpool.cpp
threadpool.o: threadpool.hpp

#NO!
ifdef USE_AGENTPP
threads_test: CPPFLAGS+=-DUSE_AGENTPP
threads_test: LDLIBS:= -lboost_unit_test_framework -lboost_system
threads_test: LDLIBS:= -lsnmp++ -lagent++ -lcrypto
threads_test: threads_test.o
else
threads_test: threads_test.o threadpool.o
endif
	$(LINK.cc) $^ -o $@ $(LDLIBS)


trylock_test: trylock_test.c


thread_tss_test: LDLIBS:= -lboost_thread -l boost_system
thread_tss_test: thread_tss_test.cpp


clean:
	$(RM) threads_test thread_tss_test trylock_test *.o *.exe

distclean: clean
	$(RM) -r build *.bak *.orig *~ *.stackdump *.dSYM

test: ctest threads_test
	./threads_test -l message --random
	./threads_test --run_test=ThreadPool_test -25
	./threads_test --run_test=QueuedThreadPoolLoad_test -25
	#TODO ./threads_test --run_test=QueuedThreadPoolLoad_test -1000
	# ./trylock_test +1
	# ./trylock_test -1

#NOTE: bash for loop:
#	i=0 && while test $$i -lt 1000 && ./threads_test -t QueuedThreadPoolLoad_test ; do \
#	  echo $$i; i=$$(($$i+1)); done

cppcheck:
	cppcheck --enable=all --inconclusive --std=posix --force $(CPPFLAGS) -std=c++14 -j 2 thread*.cpp
