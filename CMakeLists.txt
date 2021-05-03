cmake_minimum_required(VERSION 3.14...3.20)

## use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
  message(STATUS "use ccache")
  set(CMAKE_CXX_COMPILER_LAUNCHER
      "${CCACHE_EXECUTABLE}"
      CACHE PATH "ccache" FORCE
  )
endif()

#================================
project(AgentPro LANGUAGES CXX C)
#================================

message(STATUS "Configuring on/for ${CMAKE_SYSTEM_NAME}")

include(cmake/CodeCoverage.cmake)

## require a C++ standard
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 98)
  option(CMAKE_CXX_EXTENSIONS "" OFF)
  option(CMAKE_CXX_STANDARD_REQUIRED "" ON)
endif()

message(STATUS "Is the C++ compiler loaded? ${CMAKE_CXX_COMPILER_LOADED}")
if(CMAKE_CXX_COMPILER_LOADED)
  message(STATUS "The C++ compiler ID is: ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "Is the C++ from GNU? ${CMAKE_COMPILER_IS_GNUCXX}")
  message(STATUS "The C++ compiler version is: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# ----------------------------------------------------------------------
# default cmake setup the build configuration for this project
# ----------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE
        STRING
        "supported values are: None Debug Release RelWithDebInfo MinSizeRel Coverage."
  )
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ----------------------------------------------------------------------
#
# general user options for ninja and cppcheck
#
# ----------------------------------------------------------------------
option(CMAKE_C_DEPFILE_EXTENSION_REPLACE
       "name depend files as main.d instead of main.c.d" ON
)
option(CMAKE_C_OUTPUT_EXTENSION_REPLACE
       "name object files as main.o instead of main.c.o" ON
)
option(CMAKE_CXX_DEPFILE_EXTENSION_REPLACE
       "name depend files as main.d instead of main.cpp.d" ON
)
option(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE
       "name object files as main.o instead of main.cpp.o" ON
)

option(CMAKE_DEPENDS_IN_PROJECT_ONLY "do not use system header files" ON)
if(CMAKE_DEPENDS_IN_PROJECT_ONLY)
  if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_DEPFILE_FLAGS_C
        "-MMD"
        CACHE STRING "dependency flag" FORCE
    )
    set(CMAKE_DEPFILE_FLAGS_CXX
        "-MMD"
        CACHE STRING "dependency flag" FORCE
    )
  elseif(GHSMULTI)
    set(CMAKE_DEPFILE_FLAGS_C
        "-MD"
        CACHE STRING "dependency flag" FORCE
    )
    set(CMAKE_DEPFILE_FLAGS_CXX
        "-MD"
        CACHE STRING "dependency flag" FORCE
    )
  endif()
endif()

option(USE_CXX_CPPCHECK "run cppcheck along with the compiler" OFF)
if(USE_CXX_CPPCHECK)
  find_program(CMAKE_CXX_CPPCHECK cppcheck REQUIRED)
endif()

# set this variable to specify a common place where CMake should put all
# libraries and executables (instead of CMAKE_CURRENT_BINARY_DIR)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)


# ----------------------------------------------------------------------
option(USE_ASIO "Test the standalon asio lib too" OFF)
set(ASIO_ROOT
    $ENV{ASIO_ROOT}
    CACHE PATH "path to install dir"
)
if(USE_ASIO)
  find_package(asio 1.14 CONFIG HINTS ${ASIO_ROOT})
endif()
# ----------------------------------------------------------------------

option(WARN_PEDANTIC "Compile with security and all extra warnings" ON)
if(WARN_PEDANTIC)
  include(cmake/WarningsAsErrors.cmake)
  add_compile_options(-Wno-unused-parameter -Wno-tautological-compare)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
  add_compile_options(-Wno-unused-parameter -Wno-tautological-compare)
endif()

option(DISABLE_LOGGING "if you do not want any logging output " ON)
if(DISABLE_LOGGING)
  set(WITH_LOGGING
      OFF
      CACHE BOOL "NO logging" FORCE
  )
  add_definitions(-D_NO_LOGGING)
else()
  set(WITH_LOGGING
      ON
      CACHE BOOL "Enable logging" FORCE
  )
  add_definitions(-DDEFAULT_LOG_PROFILE -DDEBUG)
endif()
mark_as_advanced(FORCE WITH_LOGGING)

option(USE_BOOST "build with BOOST support" ON)
if(USE_BOOST)
  option(Boost_DEBUG "find boost at verbose mode" OFF)
  option(Boost_USE_STATIC_LIBS "use static libs only" OFF)
  option(Boost_USE_MULTITHREADED "use -mt libs only" ON)

  find_package(
    Boost 1.71 CONFIG REQUIRED COMPONENTS atomic chrono filesystem thread
                                          unit_test_framework
  )
  if(Boost_FOUND)
    add_definitions(-DBOOST_ALL_NO_LIB)
    # starting with boost V1.69.0, there is no chrono/stopwatches!
    # It not longer part of boost/chrono; NOTE: use zip file! CK
    include_directories(include)
  endif()
endif()

find_package(Threads REQUIRED)

option(BUILD_TESTING "build other tests too" ON)
if(USE_BOOST OR BUILD_TESTING)
  enable_testing()

  option(USE_ThreadSanitizer "Compile with tool that detects data races" OFF)
  if(BUILD_TESTING AND NOT USE_ThreadSanitizer)
    # additional single file examples from boost/thread and boost/asio
    set(NOINST_PROGRAMS
        # #C++14 async_future
        # #C++14 ba_externallly_locked
        # #C++14 bank_account
        # #C++14 chrono_io_ex1
        # #C++14 executor
        # #C++14 multi_thread_pass
        # #C++14 parallel_quick_sort
        # #C++14 shared_monitor
        # #C++14 shared_ptr
        # #FIXME async_server
        # #FIXME user_scheduler
        # #SW lockfree_spsc_queue.cpp
        # #SW perf_condition_variable.cpp
        # #SW perf_shared_mutex.cpp
        # #SW shared_mutex.cpp
        # #SW stopwatch_reporter_example.cpp
        # #SW test_atomic_counter.cpp
        # #SW test_shared_mutex.cpp
        ###NO! daytime_client
        ###NO! make_future
        default_executor
        serial_executor
        strict_lock
        synchronized_person
        thread_tss_test
        volatile
        test_wait_for
    )

    foreach(program ${NOINST_PROGRAMS})
      add_executable(${program} ${program}.cpp)
      set_target_properties(${program} PROPERTIES CXX_STANDARD 98)
      target_link_libraries(${program} ${Boost_LIBRARIES} Threads::Threads)
      add_test(NAME ${program} COMMAND ${program})
    endforeach()

    if(USE_ASIO AND asio_FOUND)
      add_executable(test_priority_scheduler priority_scheduler.cpp)
      set_target_properties(test_priority_scheduler PROPERTIES CXX_STANDARD 17)
      target_compile_definitions(
        test_priority_scheduler PRIVATE ASIO_NO_DEPRECATED
      )
      target_link_libraries(
        test_priority_scheduler asio::standalone ${Boost_LIBRARIES}
        Threads::Threads
      )
      add_test(NAME test_priority_scheduler COMMAND test_priority_scheduler)
    endif()
  endif()

  # ----------------------------------------------------------------------
  # additional tests from me (CK)
  # ----------------------------------------------------------------------
  add_executable(test_mutexattr test_mutexattr.c)
  set_target_properties(test_mutexattr PROPERTIES CXX_STANDARD 99)
  target_link_libraries(test_mutexattr Threads::Threads)
  if(USE_ThreadSanitizer)
    target_compile_options(test_mutexattr PRIVATE -fsanitize=thread -O1)
    target_link_options(test_mutexattr PRIVATE -fsanitize=thread -O1)
  else()
    add_test(NAME test_mutexattr-errorcheck COMMAND test_mutexattr 1)
  endif()
  add_test(NAME test_mutexattr COMMAND test_mutexattr)
  add_test(NAME test_mutexattr-recursive COMMAND test_mutexattr -1)

  # ----------------------------------------------------------------------
  add_executable(test_trylock trylock_test.cpp)
  set_target_properties(test_trylock PROPERTIES CXX_STANDARD 98)
  target_link_libraries(test_trylock Boost::chrono Boost::thread)
  if(USE_ThreadSanitizer)
    target_compile_options(test_trylock PRIVATE -fsanitize=thread -O1)
    target_link_options(test_trylock PRIVATE -fsanitize=thread -O1)
  endif()
  add_test(NAME test_trylock COMMAND test_trylock)

  # ----------------------------------------------------------------------
  # TODO add_executable(threads_test threads_test.cpp threadpool.cpp threadpool.hpp)
  # ----------------------------------------------------------------------
  option(USE_AGENT_PP "Test the agent++ lib too" ON)
  if(USE_AGENT_PP)
    #XXX find_library(LIB_AGENT_PP agent++_static)
    find_library(LIB_AGENT_PP agent_pp REQUIRED)
    if(NOT ${LIB_AGENT_PP} STREQUAL LIB_AGENT_PP-NOTFOUND)
      message(STATUS "LIB_AGENT_PP: ${LIB_AGENT_PP}")
      find_library(LIB_SNMP_PP snmp_pp)
      add_executable(threads_test threads_test.cpp threads.cpp agent_pp/threads.h)
      target_compile_definitions(threads_test PRIVATE USE_AGENTPP)
      set_target_properties(threads_test PROPERTIES CXX_STANDARD 17)
      target_link_libraries(
        threads_test Boost::unit_test_framework Boost::chrono Boost::thread
        ${LIB_AGENT_PP} ${LIB_SNMP_PP} des
      )
      add_test(NAME threads_test COMMAND threads_test --log_level=warning)
      if(USE_ThreadSanitizer)
        target_compile_options(threads_test PRIVATE -fsanitize=thread -O1)
        target_link_options(threads_test PRIVATE -fsanitize=thread -O1)
      else()
        add_test(NAME threads_test-random COMMAND threads_test
                                                  --log_level=warning --random
        )
        add_test(NAME threads_test-thread
                 COMMAND threads_test --log_level=warning
                         --run_test=ThreadPool_test -25
        )
        add_test(NAME threads_test-queue
                 COMMAND threads_test --log_level=warning
                         --run_test=QueuedThreadPoolLoad_test -25
        )
      endif()
    endif()
  endif()
  # ----------------------------------------------------------------------

  add_library(threadpool posix/threadpool.cpp posix/threadpool.hpp)
  set_target_properties(threadpool PROPERTIES CXX_STANDARD 98)
  if(NOT DISABLE_LOGGING)
    target_compile_definitions(threadpool PRIVATE TRACE_VERBOSE)
  endif()
  target_include_directories(threadpool PUBLIC .)
  target_link_libraries(threadpool PUBLIC Boost::chrono Boost::thread)
  if(USE_ThreadSanitizer)
    target_compile_options(threadpool PUBLIC -fsanitize=thread -O1)
    target_link_options(threadpool PUBLIC -fsanitize=thread -O1)
  endif()

  # ----------------------------------------------------------------------
  add_executable(threads_test_posix threads_test.cpp)
  set_target_properties(threads_test_posix PROPERTIES CXX_STANDARD 17)
  target_link_libraries(
    threads_test_posix threadpool Boost::unit_test_framework
  )
  target_compile_definitions(threads_test_posix PRIVATE USE_AGENTPP_CK)
  add_test(NAME threads_test_posix COMMAND threads_test_posix
                                            --log_level=success
  )
  add_test(NAME threads_test_posix-throw
           COMMAND threads_test_posix --log_level=success
                   --run_test=ThreadTaskThrow_test -25
  )
  if(NOT USE_ThreadSanitizer AND NOT ${CMAKE_BUILD_TYPE} STREQUAL Coverage)
    add_test(NAME threads_test_posix-random
             COMMAND threads_test_posix --log_level=success --random
    )
    add_test(NAME threads_test_posix-thread
             COMMAND threads_test_posix --log_level=success
                     --run_test=ThreadPool_test -25
    )
    add_test(NAME threads_test_posix-queue
             COMMAND threads_test_posix --log_level=success
                     --run_test=QueuedThreadPoolLoad_test -25
    )
  endif()

  set(COVERAGE_EXCLUDES '/Applications/*' '/usr/*' '*/include/boost/*'
                        '${CURRENT_SOURCE_DIR}/include/boost/*'
  )
  setup_target_for_coverage(
    NAME lcov # New target name
    EXECUTABLE ctest DEPENDENCIES all # build this targets first
  )
endif()
