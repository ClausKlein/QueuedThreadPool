#=====================
#   configure part
BOOST_ROOT?=/usr/local
MT?=-mt
CXXFLAGS+=-O2 -DNDEBUG
## CXXFLAGS+=-g
#=====================

#NO! CK
### USE_AGENTPP:=1

CPPFLAGS+=-MD
CPPFLAGS+=-DBOOST_ALL_NO_LIB
CPPFLAGS+=-DPOSIX_THREADS
CPPFLAGS+=-I$(BOOST_ROOT)/include
LDFLAGS+= -L$(BOOST_ROOT)/lib
LDLIBS:= -lboost_chrono$(MT) -lboost_thread$(MT) -lboost_system$(MT)

CXXFLAGS+=-Wpedantic -Wextra -Wno-unused-parameter -Wno-c++11-long-long

PROGRAMS:= \
alarm_cond \
async_server \
chrono_io_ex1 \
daytime_client \
default_executor \
enable_shared_from_this \
lockfree_spsc_queue \
perf_shared_mutex \
shared_mutex \
shared_ptr \
stopwatch_reporter_example \
thread_tss_test \
trylock_test \
volatile \
threads_test


.PHONY: all cmake ctest test clean distclean cppcheck format
all: $(PROGRAMS)

cmake: build
	cd build && cmake --build .

build: CMakeLists.txt
	mkdir -p build
	cd build && cmake -G Ninja -DBOOST_ROOT=${BOOST_ROOT} ..

ctest: cmake
	cd build && ctest -C debug

# examples using boost libs
lockfree_spsc_queue: CXXFLAGS+=--std=c++03
lockfree_spsc_queue.o: lockfree_spsc_queue.cpp simple_stopwatch.hpp
lockfree_spsc_queue: lockfree_spsc_queue.o
	$(LINK.cc) $^ -o $@ $(LDLIBS)

async_server: CXXFLAGS+=--std=c++03
async_server: async_server.cpp

daytime_client: CXXFLAGS+=--std=c++03
daytime_client: daytime_client.cpp

default_executor: CXXFLAGS+=--std=c++03
default_executor: default_executor.cpp

shared_mutex: CXXFLAGS+=--std=c++03
shared_mutex: shared_mutex.cpp

chrono_io_ex1: CXXFLAGS+=--std=c++03
chrono_io_ex1: chrono_io_ex1.cpp

stopwatch_reporter_example: CXXFLAGS+=--std=c++03
stopwatch_reporter_example: stopwatch_reporter_example.cpp

thread_tss_test: CXXFLAGS+=--std=c++03
thread_tss_test: thread_tss_test.cpp

perf_shared_mutex: CXXFLAGS+=--std=c++03
perf_shared_mutex: perf_shared_mutex.cpp

volatile: CXXFLAGS+=--std=c++03
volatile: volatile.cpp

alarm_cond: CXXFLAGS+=--std=c++03
alarm_cond: alarm_cond.cpp

enable_shared_from_this: CXXFLAGS+=--std=c++03
enable_shared_from_this: enable_shared_from_this.cpp


# test using boost unit test framework
threads_test.o: CXXFLAGS+=--std=c++14
threads_test.o: threads_test.cpp
threads_test.o: threadpool.hpp

###XXX threadpool.o: CPPFLAGS+=-D_NO_LOGGING
threadpool.o: CXXFLAGS+=--std=c++03
threadpool.o: threadpool.cpp
threadpool.o: threadpool.hpp

ifdef USE_AGENTPP
threads_test: CPPFLAGS+=-DUSE_AGENTPP
threads_test: LDLIBS+= -lboost_unit_test_framework$(MT)
threads_test: LDLIBS:= -lsnmp++ -lagent++ -lcrypto
threads_test: threads_test.o
else
threads_test: threadpool.o threads_test.o
endif
	$(LINK.cc) $^ -o $@ $(LDLIBS)


# plain old posix not longer used!
trylock_test: trylock_test.cpp
	$(LINK.cc) $^ -o $@ $(LDLIBS)


clean:
	$(RM) $(PROGRAMS) *.o *.exe

distclean: clean
	$(RM) -r build *.d *.bak *.orig *~ *.stackdump *.dSYM

test: $(PROGRAMS)
	#./threads_test -l message --random
	# ./threads_test --run_test=ThreadPool_test -25
	# ./threads_test --run_test=QueuedThreadPoolLoad_test -25
	#TODO ./threads_test --run_test=QueuedThreadPoolLoad_test -1000
	./chrono_io_ex1
	./default_executor
	./lockfree_spsc_queue
	./perf_shared_mutex
	./shared_mutex
	./shared_ptr
	./stopwatch_reporter_example
	./thread_tss_test
	# ./trylock_test +1
	# ./trylock_test -1
	./volatile
	cat alarm_cond.txt | ./alarm_cond --wait

#NOTE: bash for loop:
#	i=0 && while test $$i -lt 1000 && ./threads_test -t QueuedThreadPoolLoad_test ; do \
#	  echo $$i; i=$$(($$i+1)); done

cppcheck:
	cppcheck --enable=all --inconclusive --std=posix --force $(CPPFLAGS) -std=c++14 -j 2 thread*.cpp

format:
	clang-format -i -style=file *.cpp *.hpp *.c
