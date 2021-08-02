SUBNAME = spine
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

REQUIRES = gdal jsoncpp mysql icu-i18n configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# ISO C++ forbids casting between pointer-to-function and pointer-to-object
CFLAGS += -Wno-pedantic

DEFINES = -DUNIX -D_REENTRANT

# Common library compiling template

LIBS +=	-L$(libdir) \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
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
	$(REQUIRED_LIBS) \
	-ldl \
	-lrt

# What to install

LIBFILE = lib$(LIB).so

PROGS = smartmet_plugin_test

# Compilation directories

vpath %.cpp $(SUBNAME) app
vpath %.h $(SUBNAME)
vpath %.o $(objdir)

# The files to be compiled

SRCS = $(wildcard $(SUBNAME)/*.cpp)
HDRS = $(wildcard $(SUBNAME)/*.h)
OBJS = $(patsubst $(SUBNAME)/%.cpp,obj/%.o,$(SRCS))

INCLUDES := -Iinclude $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)
	$(MAKE) -C app

debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol; \
		then rm -v $(LIBFILE); \
		exit 1; \
	fi

smartmet_plugin_test: obj/SmartmetPluginTest.o $(LIBFILE)
	$(CXX) $(CFLAGS) -o $@ $* -L. -lsmartmet-spine -lboost_program_options $(CONFIGPP_LIBS) -lpthread

clean:
	rm -f $(LIBFILE) $(OBJS) $(patsubst obj/%.o,obj/%.d,$(OBJS)) *~ $(SUBNAME)/*~
	$(MAKE) -C app $@

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp test/*.cpp
	$(MAKE) -C app $@

install:
	@mkdir -p $(includedir)/$(INCDIR)
	@list='$(HDRS)'; \
	for hdr in $$list; do \
	  HDR=$$(basename $$hdr); \
	  echo $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$HDR; \
	  $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$HDR; \
	done
	@mkdir -p $(libdir)
	echo $(INSTALL_PROG) $(LIBFILE) $(libdir)/$(LIBFILE)
	$(INSTALL_PROG) $(LIBFILE) $(libdir)/$(LIBFILE)
	$(MAKE) -C app $@

test:
	$(MAKE) -C test $@

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

objdir:
	mkdir -p $(objdir)

obj/%.o : %.cpp objdir
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
