PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always
CXX_STD ?= c++11

FLAGS = -std=$(CXX_STD) -Wall -W -fdiagnostics-color=$(GCC_DIAG_COLOR) -Wno-unused-parameter -DMYDEBUG

EXTRAFLAGS = -Werror -pedantic -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Winline \
	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wno-pmf-conversions \
	-Wsign-promo -Wchar-subscripts -Wold-style-cast \
	-Wshadow -Woverloaded-virtual

DIFFICULTFLAGS = -Weffc++ -Wunreachable-code -Wredundant-decls

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize-address-use-after-scope
endif

CC = g++
CFLAGS = -DUNIX -O0 -g $(FLAGS)

# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -I/usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

ifneq "$(wildcard /usr/gdal30/include)" ""
  INCLUDES += -I/usr/gdal30/include
  LIBS += -L$(PREFIX)/gdal30/lib
else
  INCLUDES += -I/usr/include/gdal
endif

INCLUDES += -I../spine -I/usr/include/smartmet `pkg-config --cflags jsoncpp`
LIBS += ../libsmartmet-spine.so \
	-lsmartmet-macgyver \
	`pkg-config --libs jsoncpp` \
	`pkg-config --libs libconfig++` \
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
	@rm -rf /tmp/bscachetest #Cache test uses this
	@rm -rf /tmp/bscachetest2 #Cache test uses this
	@rm -rf /tmp/bscachetest3 #Cache test uses this
	@echo Running tests:
	@ok=true; \
	for prog in $(PROG); do \
	./$$prog || ok=false; \
	done; \
	$$ok

$(PROG) : % : %.cpp Makefile ../libsmartmet-spine.so
	$(CC) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
