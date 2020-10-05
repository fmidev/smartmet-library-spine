PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

REQUIRES = jsoncpp configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

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
	$(JSONCPP_LIBS) \
	$(LIBCONFIGPP_LIBS) \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_regex \
	-lboost_date_time \
	-lboost_thread \
	-lboost_program_options \
	-lboost_system \
	-lbz2 -lz -lpthread -ldl

all: $(PROG)
clean:
	rm -f $(PROG) *~

test: $(PROG)
	@rm -rf /tmp/$$UID/bscachetest #Cache test uses this
	@rm -rf /tmp/$$UID/bscachetest2 #Cache test uses this
	@rm -rf /tmp/$$UID/bscachetest3 #Cache test uses this
	@mkdir -p /tmp/$$UID
	@echo Running tests:
	@ok=true; \
	for prog in $(PROG); do \
	./$$prog || ok=false; \
	done; \
	$$ok

$(PROG) : % : %.cpp Makefile ../libsmartmet-spine.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
