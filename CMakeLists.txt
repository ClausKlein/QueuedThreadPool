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

# The name of our project is "ThreadPoolTests". CMakeLists files in this project can
# refer to the root source directory of the project as ${PROJECT_SOURCE_DIR}
# and to the root binary directory of the project as ${PROJECT_BINARY_DIR}.
#===========================
project(ThreadPoolTests CXX)
#===========================

message(STATUS "Configuring on/for ${CMAKE_SYSTEM_NAME}")

## require a C++ standard
set(CMAKE_CXX_STANDARD 20)
option(CMAKE_CXX_EXTENSIONS "" OFF)
option(CMAKE_CXX_STANDARD_REQUIRED "" ON)

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
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE
  )
  ### add_definitions(-D_DEBUG)
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
option(USE_ASIO "use the standalon asio lib too" ON)
set(ASIO_ROOT
    $ENV{ASIO_ROOT}
    CACHE PATH "path to install dir"
)
if(USE_ASIO)
  find_package(asio 1.14 CONFIG HINTS ${ASIO_ROOT})
endif()
# ----------------------------------------------------------------------

set(EXTRA_LIBS)
option(WARN_PEDANTIC "Compile with security and all extra warnings" ON)
if(CMAKE_COMPILER_IS_GNUCXX)
  add_definitions(-Wall -Wextra)
  if(WARN_PEDANTIC)
    message("WARN_PEDANTIC: -Wall -Wextra -Wpedantic!")
    add_definitions(-Wpedantic -Wno-unused-parameter -Wno-tautological-compare)
  endif()
endif()

option(DISABLE_LOGGING "if you do not want any logging output " ON)
if(DISABLE_LOGGING)
  set(WITH_LOGGING
      OFF
      CACHE BOOL "NO logging" FORCE
  )
  add_definitions(-D_NO_LOGGING -DNDEBUG)
else()
  set(WITH_LOGGING
      ON
      CACHE BOOL "Use logging" FORCE
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

option(USE_PTHREAD_LIBRARY "" OFF)
if(USE_PTHREAD_LIBRARY)
  find_library(PTHREAD_LIBRARY pthread)
  if(PTHREAD_LIBRARY)
    message(STATUS "pthread library found")
    list(APPEND EXTRA_LIBS ${PTHREAD_LIBRARY})
    #XXX add_definitions(-DPOSIX_THREADS)
  endif()
endif()

if(USE_BOOST OR BUILD_TESTING)
  enable_testing()

  if(Boost_FOUND)
    # additional single file examples from boost/thread and boost/asio
    set(noinst_PROGRAMS
        #C++14
        async_future
        #FIXME async_server
        #C++14
        ba_externallly_locked
        #C++14
        bank_account
        chrono_io_ex1
        daytime_client
        default_executor
        executor
        lockfree_spsc_queue
        make_future
        multi_thread_pass
        #C++14
        parallel_quick_sort
        #C++14
        perf_condition_variable
        perf_shared_mutex
        serial_executor
        shared_monitor
        shared_mutex
        shared_ptr
        stopwatch_reporter_example
        strict_lock
        synchronized_person
        test_atomic_counter
        test_shared_mutex
        thread_pool
        thread_tss_test
        trylock_test
        #FIXME user_scheduler
        volatile
    )

    foreach(program ${noinst_PROGRAMS})
      add_executable(${program} ${program}.cpp)
      set_target_properties(${program} PROPERTIES CXX_STANDARD 20)
      target_link_libraries(${program} ${Boost_LIBRARIES} ${EXTRA_LIBS})
      add_test(NAME ${program} COMMAND ${program})
    endforeach()

    if(USE_ASIO AND asio_FOUND)
      add_executable(test_priority_scheduler priority_scheduler.cpp)
      target_compile_definitions(test_priority_scheduler PRIVATE ASIO_NO_DEPRECATED)
      target_link_libraries(
        test_priority_scheduler asio::standalone ${Boost_LIBRARIES}
        ${EXTRA_LIBS}
      )
      add_test(NAME test_priority_scheduler COMMAND test_priority_scheduler)
    endif()

    add_executable(threads_test threads_test.cpp threadpool.cpp threadpool.hpp)
    target_link_libraries(
      threads_test Boost::unit_test_framework Boost::chrono Boost::thread
    )
    add_test(NAME threads_test COMMAND threads_test)

    add_executable(
      threads_test_possix threads_test.cpp possix/threadpool.cpp
                          possix/threadpool.hpp
    )
    target_link_libraries(
      threads_test_possix Boost::unit_test_framework Boost::chrono
      Boost::thread
    )
    target_compile_definitions(threads_test_possix PRIVATE USE_AGENTPP_CK)
    add_test(NAME threads_test_possix COMMAND threads_test_possix)

  endif()

endif()
