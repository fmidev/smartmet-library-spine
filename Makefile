SUBNAME = spine
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

bindir = $(PREFIX)/bin
includedir = $(PREFIX)/include
objdir = obj

# Compiler options

DEFINES = -DUNIX -D_REENTRANT

ifeq ($(CXX), clang++)
 FLAGS = -std=c++11 -fPIC \
	-Weverything \
	-Wno-shadow \
	-Wno-c++98-compat-pedantic \
	-Wno-float-equal \
	-Wno-padded \
	-Wno-missing-prototypes \
	-Wno-global-constructors \
	-Wno-exit-time-destructors \
	-Wno-documentation-unknown-command \
	-Wno-sign-conversion

 INCLUDES = -isystem $(includedir)/smartmet \
	-isystem $(includedir)/mysql
	-isystem $(includedir)/jsoncpp
else

 FLAGS = -std=c++11 -Wall -W -fPIC -Wno-unused-parameter -fno-omit-frame-pointer -fdiagnostics-color=always

 FLAGS_DEBUG = \
	-Wcast-align \
	-Wcast-qual \
	-Wconversion \
	-Winline \
	-Wno-multichar \
	-Wno-pmf-conversions \
	-Wold-style-cast \
	-Woverloaded-virtual  \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wsign-promo \
	-Wwrite-strings \
	-Wctor-dtor-privacy \
	-Wnon-virtual-dtor

 INCLUDES = -I$(includedir)/smartmet \
	-I$(includedir)/mysql \
	`pkg-config --cflags jsoncpp`

endif

# Compile options in release and debug modes

CFLAGS_RELEASE = $(DEFINES) $(FLAGS) $(FLAGS_RELEASE) -DNDEBUG -O2 -g
CFLAGS_DEBUG   = $(DEFINES) $(FLAGS) $(FLAGS_DEBUG)   -Werror  -O0 -g

# Compile option overrides

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  override CFLAGS += $(CFLAGS_DEBUG)
else
  override CFLAGS +=  $(CFLAGS_RELEASE)
endif

# Common library compiling template

LIBS =	-L$(libdir) \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-L$(libdir)/mysql -lmysqlclient_r \
	-lboost_filesystem \
	-lboost_regex \
	-lboost_timer \
	-lboost_chrono \
	-lboost_thread \
	-lboost_date_time \
	-lboost_program_options \
	-lboost_system \
	-lboost_locale \
	-lctpp2 \
        -lgdal \
	-lconfig++ \
	`pkg-config --libs jsoncpp` \
	`pkg-config --libs icu-i18n` \
	-ldl \
	-lrt

# What to install

LIBFILE = lib$(LIB).so

# How to install

INSTALL_PROG = install -p -m 755
INSTALL_DATA = install -p -m 664

# Compilation directories

vpath %.cpp source
vpath %.h include
vpath %.o $(objdir)

# The files to be compiled

SRCS = $(wildcard source/*.cpp)
HDRS = $(wildcard include/*.h)
OBJS = $(patsubst source/%.cpp,obj/%.o,$(SRCS))

INCLUDES := -Iinclude $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)

clean:
	rm -f $(LIBFILE) $(OBJS) $(patsubst obj/%.o,obj/%.d,$(OBJS)) *~ source/*~ include/*~

format:
	clang-format -i -style=file include/*.h source/*.cpp test/*.cpp

install:
	@mkdir -p $(includedir)/$(INCDIR)
	for hdr in $(notdir $(HDRS)); do \
	  echo $(INSTALL_DATA) include/$$hdr $(includedir)/$(INCDIR)/$$hdr; \
	  $(INSTALL_DATA) include/$$hdr $(includedir)/$(INCDIR)/$$hdr; \
	done
	@mkdir -p $(libdir)
	$(INSTALL_PROG) $(LIBFILE) $(libdir)/$(LIBFILE)

test:
	$(MAKE) -C test $@

objdir:
	@mkdir -p $(objdir)

rpm: clean
	@if [ -e $(SPEC).spec ]; \
	then \
	  tar -czvf $(SPEC).tar.gz --transform "s,^,$(SPEC)/," * ; \
	  rpmbuild -ta $(SPEC).tar.gz ; \
	  rm -f $(SPEC).tar.gz ; \
	else \
	  echo $(SPEC).spec file missing; \
	fi;

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	@mkdir -p obj
	@#echo Compiling $<
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d.new, $@) -o $@ $<
	@sed -e "s|^$(notdir $@):|$@:|" $(patsubst obj/%.o, obj/%.d.new, $@) >$(patsubst obj/%.o, obj/%.d, $@)
	@rm -f $(patsubst obj/%.o, obj/%.d.new, $@)

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
