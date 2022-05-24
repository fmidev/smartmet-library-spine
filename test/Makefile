PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

REQUIRES = jsoncpp configpp gdal

include $(shell smartbuildcfg --prefix)/share/smartmet/devel/makefile.inc

FLAGS = -std=$(CXX_STD) -Wall -W -fdiagnostics-color=$(GCC_DIAG_COLOR) -Wno-unused-parameter -DMYDEBUG

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize-address-use-after-scope
endif

CFLAGS = -DUNIX -O0 -g $(FLAGS)

INCLUDES += -I../spine

LIBS += ../libsmartmet-spine.so \
	-lsmartmet-macgyver \
	$(REQUIRED_LIBS) \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_regex \
	-lboost_date_time \
	-lboost_thread \
	-lboost_program_options \
	-lboost_chrono \
	-lboost_system \
	-lbz2 -lz -lpthread -ldl

all: all-reactor-tests $(PROG)

clean:
	rm -f $(PROG) *~
	$(MAKE) -C reactor_tests $@

test: all
	@rm -rf /tmp/$$UID/bscachetest #Cache test uses this
	@rm -rf /tmp/$$UID/bscachetest2 #Cache test uses this
	@rm -rf /tmp/$$UID/bscachetest3 #Cache test uses this
	@mkdir -p /tmp/$$UID
	@echo Running tests:
	@ok=true; \
	for prog in $(PROG); do \
	./$$prog || ok=false; \
	done; \
	$(MAKE) -C reactor_tests $@ || ok=false; \
	$$ok

all-reactor-tests:
	$(MAKE) -C reactor_tests

$(PROG) : % : %.cpp Makefile ../libsmartmet-spine.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)

JsonTest: CFLAGS += -Wno-deprecated-declarations
