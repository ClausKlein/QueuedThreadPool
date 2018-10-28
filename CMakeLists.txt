cmake_minimum_required(VERSION 3.9.1)

## use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
    message(STATUS "use ccache")
    find_program(GXX_EXECUTABLE "clang++" HINTS /opt/local/bin)
    set(CMAKE_CXX_COMPILER "${CCACHE_EXECUTABLE}" CACHE PATH "ccache" FORCE)
    set(CMAKE_CXX_COMPILER_ARG1 ${GXX_EXECUTABLE} CACHE PATH "the real clang++" FORCE)
endif()

# The name of our project is "AgentPro". CMakeLists files in this project can
# refer to the root source directory of the project as ${AgentPro_SOURCE_DIR}
# and to the root binary directory of the project as ${AgentPro_BINARY_DIR}.
#====================
project(AgentppTest CXX)
#====================

# The version number.
set(AgentPro_VERSION_MAJOR 4)
set(AgentPro_VERSION_MINOR 1)

# ----------------------------------------------------------------------
# default cmake setup the build configuration for this project
# ----------------------------------------------------------------------
if ( NOT CMAKE_BUILD_TYPE )
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE )
  add_definitions(-D_DEBUG)
endif ( NOT CMAKE_BUILD_TYPE )

mark_as_advanced(CLEAR CMAKE_MACOSX_RPATH)
mark_as_advanced(CLEAR CMAKE_SKIP_RPATH)
mark_as_advanced(CLEAR CMAKE_SKIP_INSTALL_RPATH)

option(CMAKE_MACOSX_RPATH "Whether to use rpaths on Mac OS X." ON)
option(CMAKE_SKIP_RPATH "If true, do not add run time path information." OFF)
option(CMAKE_SKIP_INSTALL_RPATH "If true, do not include RPATHs in the install tree." OFF)

option(WARN_PEDANTIC "Compile with security and Weffc++ warnings" ON)

# ----------------------------------------------------------------------
# fix options, should not changed! ck see XXX
# ----------------------------------------------------------------------
set(WITH_THREADS ON CACHE BOOL "Use threads" FORCE)
# ----------------------------------------------------------------------

option(DISABLE_LOGGING "if you do not want any logging output " OFF)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(LINK_STATIC_RUNTIME "Link the runtime libs static" OFF)

option(USE_BOOST "build with BOOST support" ON)
option(USE_CTEST "build with unittests added by CK" ON)

# ----------------------------------------------------------------------
# Note: do not use WIN32, it is not unique! ck
# from http://www.cmake.org/Wiki/CMake_Useful_Variables#System_.26_Compiler_Information
# UNIX : is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
# WIN32: is TRUE on Windows, including CygWin
if ( NOT UNIX )
  # This variable is a special case of the more-general
  # CMAKE_<CONFIG>_POSTFIX variable for the DEBUG configuration.
  # See variable CMAKE_<CONFIG>_POSTFIX.
  # Default filename postfix for libraries only under configuration <CONFIG>.
  set( CMAKE_DEBUG_POSTFIX "D" CACHE STRING "Postfix added to the debug version of libraries and executables" )
  # Install path prefix, prepended onto install directories.
  # NOTE: can't be an OPTION; options are true/flase values only! ck
  set(CMAKE_INSTALL_PREFIX "C:/usr" CACHE STRING "Install path prefix" FORCE)
  message( "NOTE: CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

  if ( NOT BUILD_SHARED_LIBS AND LINK_STATIC_RUNTIME)
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
      endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
  endif ( NOT BUILD_SHARED_LIBS AND LINK_STATIC_RUNTIME)

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
    mark_as_advanced (${flag_var})
    message( "${flag_var}:'${${flag_var}}'" )
  endforeach ()

  set(WITH_PTHREAD OFF CACHE BOOL "Do not use pthread libs" FORCE)
else ( NOT UNIX )
  set(WITH_PTHREAD ON CACHE BOOL "Use pthread libs" FORCE)
endif ( NOT UNIX )
# ----------------------------------------------------------------------


# set this variable to specify a common place where CMake should put all
# libraries and executables (instead of CMAKE_CURRENT_BINARY_DIR)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Make sure the linker can find the library once it is built.
link_directories(${LIBRARY_OUTPUT_PATH})

find_library(PTHREAD_LIBRARY pthread)
if (PTHREAD_LIBRARY)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${PTHREAD_LIBRARY})
  add_definitions(-DHAVE_PTHREAD)
  # add_definitions(-DAGENTPP_OPAQUE_PTHREAD_T=long)
endif (PTHREAD_LIBRARY)

if (MSVC)
  ###NO! add_definitions(-DWIN32)
  if (NOT WARN_PEDANTIC)
    message("MSVC: no secure warnings!")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  endif (NOT WARN_PEDANTIC)
  if (MSVC_VERSION GREATER 1399)
    # If using Visual C++ 2005 (MSVC80) and greater (MSVC_VERSION=1400)
    # Disable Visual C++ Security Enhancements in the CRT
    # See http://msdn.microsoft.com/en-us/library/8ef0s5kh%28VS.100%29.aspx
    # This is non standard and Microsoft specific
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_OBSOLETE_NO_WARNINGS)
    message("MSVC: no obsolete warnings!")
  endif (MSVC_VERSION GREATER 1399)
  set(EXTRA_LIBS ${EXTRA_LIBS} ws2_32 CACHE PATH "extra libraries used")
  mark_as_advanced(FORCE EXTRA_LIBS)
else (MSVC)
  ### if(CMAKE_COMPILER_IS_GNUCXX)
    # each error message will appear on a single line.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0")

    ###FIXME Run the Clang Static Analyzer only.
    ###FIXME add_definitions(--analyze)

    add_definitions(-Wall -std=c++14)
    add_definitions(-Wextra)
    if (WARN_PEDANTIC)
      ## add_definitions(-Weffc++ -Wno-unused-parameter -Wno-tautological-compare )
      add_definitions(-Wunused-private-field -Wunknown-pragmas -Woverloaded-virtual -Wtautological-compare -Wmismatched-tags )
    else (WARN_PEDANTIC)
      add_definitions(-Wno-unused-parameter -Wno-unused-private-field -Wno-unknown-pragmas -Wno-tautological-compare)
    endif (WARN_PEDANTIC)
  ### endif(CMAKE_COMPILER_IS_GNUCXX)
endif (MSVC)

if (MSYS OR MINGW)
  set(EXTRA_LIBS ${EXTRA_LIBS} ws2_32 CACHE PATH "extra libraries used")
  mark_as_advanced(FORCE EXTRA_LIBS)
  message("XXX crosscompile for windows")
  add_definitions(-DWIN32)
endif (MSYS OR MINGW)

if (CYGWIN)
  add_definitions(-DNO_NANOSLEEP)
endif (CYGWIN)


if (DISABLE_LOGGING)
  set(WITH_LOGGING OFF CACHE BOOL "NO logging" FORCE)
  set(WITH_LOG_PROFILES OFF CACHE BOOL "NO profiles" FORCE)
  add_definitions(-D_NO_LOGGING)
else (DISABLE_LOGGING)
  set(WITH_LOGGING ON CACHE BOOL "Use logging" FORCE)
  set(WITH_LOG_PROFILES ON CACHE BOOL "Use log profiles" FORCE)
  add_definitions(-DDEFAULT_LOG_PROFILE)
endif (DISABLE_LOGGING)


if(USE_BOOST)
  option(Boost_DEBUG  "find boost at verbose mod"     ON)
  set(Boost_USE_MULTITHREADED     ON)
  set(Boost_NO_BOOST_CMAKE        ON)
  ## set(BOOST_ROOT /Users/clausklein/Workspace/cpp/boost_1_65_1/stage)
  set(BOOST_ROOT /opt/local CACHE PATH "path to boost install dir" FORCE)

  find_package(Boost 1.65 COMPONENTS unit_test_framework chrono atomic thread log system)
  if(Boost_FOUND)
    include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIR})
  else()
    #NOTE! only header needed
    include_directories(BEFORE SYSTEM ${BOOST_ROOT}/include)
  endif()
endif()


if (USE_BOOST OR USE_CTEST)

  # enable ctest
  enable_testing()

  if(Boost_FOUND)
    # additional examples from boost
    set(noinst_PROGRAMS
        ba_externallly_locked
        default_executor
        executor
        parallel_quick_sort
        serial_executor
        shared_monitor
        shared_mutex
        strict_lock
        synchronized_person
        thread_pool
        thread_tss_test
    )

    foreach(program ${noinst_PROGRAMS})
        add_executable(${program} ${program}.cpp)
        set_target_properties(${program} PROPERTIES CXX_STANDARD 14)
        target_link_libraries(${program} ${Boost_LIBRARIES})
    endforeach()

    add_test(NAME test_shared_mutex COMMAND shared_mutex)
    add_test(NAME test_executor COMMAND executor)
  endif()

  add_executable(test_threads threads_test.cpp
    threadpool.cpp threadpool.hpp
  )
  set_target_properties(test_threads PROPERTIES CXX_STANDARD 14)
  #NO! target_compile_definitions(test_threads PRIVATE -DUSE_AGENTPP)
  target_link_libraries(test_threads ${EXTRA_LIBS})
  add_test(NAME test_threads COMMAND test_threads)

endif ()

