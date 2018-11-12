cmake_minimum_required(VERSION 3.9.1)

## use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
    message(STATUS "use ccache")
    find_program(GXX_EXECUTABLE "g++" HINTS /usr/local/bin /opt/local/bin)
    if(GXX_EXECUTABLE)
      message(STATUS "GXX_EXECUTABLE FOUND")
      set(CMAKE_COMPILER_IS_GNUCXX ON)
      set(CMAKE_CXX_COMPILER "${CCACHE_EXECUTABLE}" CACHE PATH "ccache" FORCE)
      set(CMAKE_CXX_COMPILER_ARG1 ${GXX_EXECUTABLE} CACHE PATH "the real g++" FORCE)
    endif()
endif()

# The name of our project is "ThreadPoolTests". CMakeLists files in this project can
# refer to the root source directory of the project as ${PROJECT_SOURCE_DIR}
# and to the root binary directory of the project as ${PROJECT_BINARY_DIR}.
#====================
project(ThreadPoolTests CXX)
#====================

# ----------------------------------------------------------------------
# default cmake setup the build configuration for this project
# ----------------------------------------------------------------------
if( NOT CMAKE_BUILD_TYPE )
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE )
  add_definitions(-D_DEBUG)
endif()

# ----------------------------------------------------------------------
#
# general user options for ninja and cppcheck
#
# ----------------------------------------------------------------------
option(CMAKE_C_DEPFILE_EXTENSION_REPLACE "name depend files as main.d instead of main.c.d" ON)
option(CMAKE_C_OUTPUT_EXTENSION_REPLACE "name object files as main.o instead of main.c.o" ON)
option(CMAKE_CXX_DEPFILE_EXTENSION_REPLACE "name depend files as main.d instead of main.cpp.d" ON)
option(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE "name object files as main.o instead of main.cpp.o" ON)


option(CMAKE_DEPENDS_IN_PROJECT_ONLY "do not use system header files" ON)
if(CMAKE_DEPENDS_IN_PROJECT_ONLY)
  if(GXX_EXECUTABLE)
    set(CMAKE_DEPFILE_FLAGS_C "-MMD" CACHE STRING "dependency flag" FORCE)
    set(CMAKE_DEPFILE_FLAGS_CXX "-MMD" CACHE STRING "dependency flag" FORCE)
  elseif(GHSMULTI)
    set(CMAKE_DEPFILE_FLAGS_C "-MD" CACHE STRING "dependency flag" FORCE)
    set(CMAKE_DEPFILE_FLAGS_CXX "-MD" CACHE STRING "dependency flag" FORCE)
  endif()
endif()


option(USE_CXX_CPPCHECK "run clang-tidy along with the compiler and report any problems" ON)
if(USE_CXX_CPPCHECK)
  find_program(CMAKE_CXX_CPPCHECK cppcheck HINTS /usr/local/bin /opt/local/bin "C:/Program Files/Cppcheck" REQUIRED)
endif()


# ----------------------------------------------------------------------
# Note: do not use WIN32, it is not unique! ck
# from http://www.cmake.org/Wiki/CMake_Useful_Variables#System_.26_Compiler_Information
# UNIX : is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
# WIN32: is TRUE on Windows, including CygWin
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
if( NOT UNIX )
  # This variable is a special case of the more-general
  # CMAKE_<CONFIG>_POSTFIX variable for the DEBUG configuration.
  # See variable CMAKE_<CONFIG>_POSTFIX.
  # Default filename postfix for libraries only under configuration <CONFIG>.
  set( CMAKE_DEBUG_POSTFIX "D" CACHE STRING "Postfix added to the debug version of libraries and executables" )
  # Install path prefix, prepended onto install directories.
  # NOTE: can't be an OPTION; options are true/flase values only! ck
  set(CMAKE_INSTALL_PREFIX "C:/usr" CACHE STRING "Install path prefix")
  message( "NOTE: CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

  option(LINK_STATIC_RUNTIME "Link the runtime libs static" OFF)
  if( NOT BUILD_SHARED_LIBS AND LINK_STATIC_RUNTIME)
    message( "MSV: Force cmake to link MSVCR.dll statically!" )
    foreach(flag_var
      CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
      )
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        message( "${flag_var}:'${${flag_var}}'" )
        set(${flag_var} "${${flag_var}}" CACHE STRING "compiler flags used" FORCE)
      endif()
    endforeach(flag_var)
  endif()

  # Disable incremental linking for debug only!
  #
  # Example from OpenSceneGraph:
  # http://www.openscenegraph.org/svn/osg/OpenSceneGraph/tags/OpenSceneGraph-2.6.0/CMakeLists.txt
  #
  # TBD: Also remove /EDITANDCONTINUE, which is incompatible with /INCREMENTAL:NO.
  foreach (flag_type EXE MODULE SHARED)
    set(flag_var CMAKE_${flag_type}_LINKER_FLAGS_DEBUG)
    string (REGEX REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" flag_tmp "${${flag_var}}")
    ### string (REGEX REPLACE "/EDITANDCONTINUE" "" flag_tmp ${flag_tmp})
    set (${flag_var} ${flag_tmp} CACHE STRING "Overriding default debug linker flags." FORCE)
    mark_as_advanced(FORCE ${flag_var})
    message( "${flag_var}:'${${flag_var}}'" )
  endforeach ()

endif()
# ----------------------------------------------------------------------


# set this variable to specify a common place where CMake should put all
# libraries and executables (instead of CMAKE_CURRENT_BINARY_DIR)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Make sure the linker can find our the library once it is built.
link_directories(${LIBRARY_OUTPUT_PATH})


# ----------------------------------------------------------------------
set(ASIO_ROOT /usr/local CACHE PATH "path to install dir")
find_package(asio 1.11 CONFIG HINTS ${ASIO_ROOT})
if(asio_FOUND)
  # TODO: how to use the imported asio_INCLUDE_DIR? CK
  # NO! include_directories(BEFORE SYSTEM ${ASIO_ROOT}/include)
  set(asio_INCLUDE_DIR ${ASIO_ROOT}/include)
endif()
# ----------------------------------------------------------------------


set(EXTRA_LIBS)
option(WARN_PEDANTIC "Compile with security and all extra warnings" ON)
if(MSVC)
  ###NO! add_definitions(-DWIN32)
  if(NOT WARN_PEDANTIC)
    message("MSVC: no secure warnings!")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  endif()
  if(MSVC_VERSION GREATER 1399)
    # If using Visual C++ 2005 (MSVC80) and greater (MSVC_VERSION=1400)
    # Disable Visual C++ Security Enhancements in the CRT
    # See http://msdn.microsoft.com/en-us/library/8ef0s5kh%28VS.100%29.aspx
    # This is non standard and Microsoft specific
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_OBSOLETE_NO_WARNINGS)
    message("MSVC: no obsolete warnings!")
  endif()
  set(EXTRA_LIBS ${EXTRA_LIBS} ws2_32 CACHE PATH "extra libraries used")
  mark_as_advanced(FORCE EXTRA_LIB)

  ###TODO: set(CMAKE_STATIC_LIBRARY_PREFIX lib)  # libboost_chrono-vc141-mt-x32-1_67.lib
  ## set(CMAKE_FIND_LIBRARY_PREFIXES "lib" CACHE STRING "" FORCE)
  ## set(CMAKE_STATIC_LIBRARY_PREFIX "lib" CACHE STRING "" FORCE)

else()
  if(GXX_EXECUTABLE)
    # each error message will appear on a single line.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0")

    ###FIXME Run the Clang Static Analyzer only.
    ###FIXME add_definitions(--analyze)

    add_definitions(-Wall)
    add_definitions(-Wextra)
    if(WARN_PEDANTIC)
      message("WARN_PEDANTIC: -Wall -Wextra -Wpedantic!")
      add_definitions(-Wpedantic -Wno-unused-parameter -Wno-tautological-compare )
      add_definitions(-Wunknown-pragmas -Woverloaded-virtual -Wtautological-compare )
    else()
      add_definitions(-Wno-unused-parameter -Wno-unknown-pragmas -Wno-tautological-compare)
    endif()
  endif()
endif()


if(MSYS OR MINGW)
  set(EXTRA_LIBS ${EXTRA_LIBS} ws2_32 CACHE PATH "extra libraries used")
  mark_as_advanced(FORCE EXTRA_LIBS)
  message(WARNING "crosscompile for windows, is this indented?")
endif()


option(DISABLE_LOGGING "if you do not want any logging output " OFF)
if(DISABLE_LOGGING)
  set(WITH_LOGGING OFF CACHE BOOL "NO logging" FORCE)
  add_definitions(-D_NO_LOGGING)
else()
  set(WITH_LOGGING ON CACHE BOOL "Use logging" FORCE)
  add_definitions(-DDEFAULT_LOG_PROFILE)
endif()
mark_as_advanced(FORCE WITH_LOGGING)


option(USE_BOOST "build with BOOST support" ON)
if(USE_BOOST)
  option(Boost_DEBUG "find boost at verbose mode"      ON)
  option(Boost_USE_STATIC_LIBS "use static libs only" OFF)
  option(Boost_USE_MULTITHREADED "use -mt libs only"   ON)
  set(Boost_NO_BOOST_CMAKE        ON)

  if( NOT $ENV{BOOST_ROOT} STREQUAL "")
    set(BOOST_ROOT $ENV{BOOST_ROOT} CACHE PATH "path to boost install dir")
  else()
    set(BOOST_ROOT /usr/local CACHE PATH "path to boost install dir")
  endif()

  if(IS_DIRECTORY ${BOOST_ROOT}/stage/lib)
    set(BOOST_LIBRARYDIR ${BOOST_ROOT}/stage/lib CACHE PATH "path to boost stage lib dir")
    set(BOOST_INCLUDEDIR ${BOOST_ROOT} CACHE PATH "path to boost source dir")
  endif()

  find_package(Boost 1.67 COMPONENTS chrono atomic thread system
          regex unit_test_framework ## log ### HINTS /usr/local /opt/local
  )   # NOTE: Not REQUIRED! CK
  if(Boost_FOUND)
    include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIR})
  elseif(IS_DIRECTORY ${BOOST_ROOT}/include)
    message(WARNING "BOOST header only usable!")
    add_definitions(-DBOOST_ALL_NO_LIB)
    set(Boost_INCLUDE_DIR ${BOOST_ROOT}/include CACHE PATH "path to boost include dir")
    include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIR})
  endif()
endif()


find_library(PTHREAD_LIBRARY pthread)
if(PTHREAD_LIBRARY)
  message(STATUS "pthread library found")
  list(APPEND EXTRA_LIBS ${PTHREAD_LIBRARY})
  add_definitions(-DPOSIX_THREADS)
endif()


option(USE_CTEST "build with unittests added by CK" ON)
if(USE_BOOST OR USE_CTEST)

  # enable ctest
  enable_testing()

  if(Boost_FOUND)
    # additional single file examples from boost/thread and boost/asio
    set(noinst_PROGRAMS
        async_future
        async_server
        ba_externallly_locked
        bank_account
        chrono_io_ex1
        daytime_client
        default_executor
        executor
        lockfree_spsc_queue
        make_future
        multi_thread_pass
        parallel_quick_sort
        perf_condition_variable
        perf_shared_mutex
        serial_executor
        shared_monitor
        shared_mutex
        shared_ptr
        stopwatch_reporter_example
        strict_lock
        synchronized_person
        thread_pool
        thread_tss_test
        trylock_test
        volatile
    )

    foreach(program ${noinst_PROGRAMS})
        add_executable(${program} ${program}.cpp)
        set_target_properties(${program} PROPERTIES CXX_STANDARD 14)
        # target_compile_definitions(${program} PRIVATE
        #NO! -DBOOST_THREAD_VERSION=4
        #### -DBOOST_THREAD_PROVIDES_EXECUTORS
        #)
        target_link_libraries(${program} ${Boost_LIBRARIES} ${EXTRA_LIBS})
        add_test(NAME ${program} COMMAND ${program})
    endforeach()

    if(asio_FOUND)
      add_executable(test_priority_scheduler priority_scheduler.cpp)
      target_include_directories(test_priority_scheduler SYSTEM BEFORE PRIVATE ${asio_INCLUDE_DIR})
      #XXX target_compile_definitions(test_priority_scheduler PRIVATE -std=c++03)
      set_target_properties(test_priority_scheduler PROPERTIES CXX_STANDARD 98)
      target_link_libraries(test_priority_scheduler ${Boost_LIBRARIES} ${EXTRA_LIBS})
      add_test(NAME test_priority_scheduler COMMAND test_priority_scheduler)
    endif()

  endif()

  add_executable(test_threads threads_test.cpp
    threadpool.cpp threadpool.hpp
  )
  set_target_properties(test_threads PROPERTIES CXX_STANDARD 98)
  target_link_libraries(test_threads ${Boost_LIBRARIES} ${EXTRA_LIBS})
  add_test(NAME test_threads COMMAND test_threads)

endif()

