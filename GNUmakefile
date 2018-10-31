BOOST_ROOT:=/usr/local
#TODO MT:=-mt

#NO! CK
### USE_AGENTPP:=1

#NO! CK
## CPPFLAGS+=-D_POSIX_C_SOURCE=200809L
CPPFLAGS+=-DBOOST_TEST_NO_LIB -DPOSIX_THREADS #NO! -DBOOST_THREAD_VERSION=4
CPPFLAGS+=-I$(BOOST_ROOT)/include
LDFLAGS+= -L$(BOOST_ROOT)/lib
LDLIBS:= -lboost_thread$(MT) -lboost_chrono$(MT) -lboost_system$(MT)


### CXXFLAGS+=-g
CXXFLAGS+=-O2 -DNDEBUG
CXXFLAGS+=-Wpedantic -Wextra -Wno-unused-parameter -Wno-c++11-long-long

.PHONY: all cmake ctest test clean distclean cppcheck format
all: default_executor shared_mutex lockfree_spsc_queue thread_tss_test # trylock_test ## cmake ### threads_test

cmake: build
	cd build && cmake --build .

build: CMakeLists.txt
	mkdir -p build
	cd build && cmake -G Ninja ..

ctest: cmake
	cd build && ctest -C debug

lockfree_spsc_queue: CXXFLAGS+=--std=c++03
lockfree_spsc_queue.o: lockfree_spsc_queue.cpp simple_stopwatch.hpp
lockfree_spsc_queue: lockfree_spsc_queue.o
	$(LINK.cc) $^ -o $@ $(LDLIBS)

default_executor: CXXFLAGS+=--std=c++03
default_executor: default_executor.cpp
	$(LINK.cc) $^ -o $@ $(LDLIBS)

shared_mutex: CXXFLAGS+=--std=c++03
shared_mutex: shared_mutex.cpp
	$(LINK.cc) $^ -o $@ $(LDLIBS)

threads_test.o: CXXFLAGS+=--std=c++14
threads_test.o: threads_test.cpp
threads_test.o: threadpool.hpp


threadpool.o: CPPFLAGS+=-D_NO_LOGGING
threadpool.o: threadpool.cpp
threadpool.o: threadpool.hpp

ifdef USE_AGENTPP
threads_test: CPPFLAGS+=-DUSE_AGENTPP
threads_test: LDLIBS+= -lboost_unit_test_framework$(MT)
threads_test: LDLIBS:= -lsnmp++ -lagent++ -lcrypto
threads_test: threads_test.o
else
threads_test: threads_test.o threadpool.o
endif
	$(LINK.cc) $^ -o $@ $(LDLIBS)


trylock_test: trylock_test.c


thread_tss_test: thread_tss_test.cpp


clean:
	$(RM) lockfree_spsc_queue default_executor threads_test thread_tss_test trylock_test *.o *.exe

distclean: clean
	$(RM) -r build *.bak *.orig *~ *.stackdump *.dSYM

test: lockfree_spsc_queue thread_tss_test default_executor shared_mutex threads_test
	#./threads_test -l message --random
	# ./threads_test --run_test=ThreadPool_test -25
	# ./threads_test --run_test=QueuedThreadPoolLoad_test -25
	#TODO ./threads_test --run_test=QueuedThreadPoolLoad_test -1000
	# ./trylock_test +1
	# ./trylock_test -1
	./thread_tss_test
	./default_executor
	./shared_mutex
	./lockfree_spsc_queue

#NOTE: bash for loop:
#	i=0 && while test $$i -lt 1000 && ./threads_test -t QueuedThreadPoolLoad_test ; do \
#	  echo $$i; i=$$(($$i+1)); done

cppcheck:
	cppcheck --enable=all --inconclusive --std=posix --force $(CPPFLAGS) -std=c++14 -j 2 thread*.cpp

format:
	clang-format -i -style=file *.cpp *.hpp *.c
