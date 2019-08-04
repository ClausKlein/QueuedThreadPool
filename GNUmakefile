#
# Standard stuff
#
.SUFFIXES:

# Disable the built-in implicit rules.
MAKEFLAGS+= --no-builtin-rules

.PHONY: setup all test check format clean distclean


# see https://www.kdab.com/clang-tidy-part-1-modernize-source-code-using-c11c14/
# and https://github.com/llvm-mirror/clang-tools-extra/blob/master/clang-tidy/tool/run-clang-tidy.py
#
### checkAllHeader:='include/spdlog/[acdlstv].*'
## checkAllHeader?='include/spdlog/[^f].*'
checkAllHeader?='$(CURDIR)/.*'

# NOTE: to many errors with boost::test
# CHECKS:='-*,cppcoreguidelines-*'
## CHECKS?='-*,portability-*,readability-*'
CHECKS?='-*,misc-*,boost-*,cert-*'

CXX:=$(shell which clang++)
BUILD_DIR:=build
## GENERATOR:=-G Xcode
#NO! GENERATOR:=-G Ninja

all: setup .configure
	cmake --build $(BUILD_DIR)
	# $(MAKE) -C $(BUILD_DIR) $@ --no-print-directory
	## ninja -C $(BUILD_DIR) $@ -v

test: all
	cd $(BUILD_DIR) && ctest -C Debug --rerun-failed --output-on-failure .
	# $(MAKE) -C $(BUILD_DIR) $@ --no-print-directory
	## ninja -C $(BUILD_DIR) $@ -v

check: setup .configure compile_commands.json
	run-clang-tidy.py -header-filter=$(checkAllHeader) -checks=$(CHECKS) | tee run-clang-tidy.log 2>&1


.configure: CMakeLists.txt
	cd $(BUILD_DIR) && cmake $(GENERATOR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_COMPILER=${CXX} ..
	touch $@

setup: $(BUILD_DIR) .clang-tidy compile_commands.json


compile_commands.json: .configure
	ln -sf $(CURDIR)/$(BUILD_DIR)/compile_commands.json .

$(BUILD_DIR): GNUmakefile
	mkdir -p $@


format: .clang-format
	find . -type f -name '*.h' -o -name '*.cpp' | xargs clang-format -style=file -i

clean: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target $@
	# $(MAKE) -C $(BUILD_DIR) $@ --no-print-directory
	## ninja -C $(BUILD_DIR) $@

distclean:
	rm -rf $(BUILD_DIR) .configure compile_commands.json *~ .*~ tags
	find . -name '*~' -delete


# These rules keep make from trying to use the match-anything rule below
# to rebuild the makefiles--ouch!

## CMakeLists.txt :: ;
GNUmakefile :: ;
.clang-tidy :: ;
.clang-format :: ;

# Anything we don't know how to build will use this rule.  The command is
# a do-nothing command.
% :: ;


