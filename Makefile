SUBNAME = spine
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

REQUIRES = gdal jsoncpp mariadb icu-i18n configpp

include $(shell smartbuildcfg --prefix)/share/smartmet/devel/makefile.inc

# ISO C++ forbids casting between pointer-to-function and pointer-to-object
CFLAGS += -Wno-pedantic -Wno-deprecated-declarations

DEFINES = -DUNIX -D_REENTRANT

# ── OpenTelemetry C++ (optional) ─────────────────────────────────────────────
# Detection order:
#   1. System pkg-config  (e.g. opentelemetry-cpp-devel RPM if ever packaged)
#   2. Local build in third_party/opentelemetry-cpp/install
#      (see docs/build-opentelemetry.md for instructions)
#
OTEL_PC := opentelemetry-cpp
OTEL_LOCAL := $(CURDIR)/third_party/opentelemetry-cpp/install

ifeq ($(shell pkg-config --exists $(OTEL_PC) 2>/dev/null && echo yes),yes)
  DEFINES  += -DSMARTMET_SPINE_OPENTELEMETRY
  CFLAGS   += $(shell pkg-config --cflags $(OTEL_PC))
  LIBS     += $(shell pkg-config --libs   $(OTEL_PC))
else ifneq ($(wildcard $(OTEL_LOCAL)/include/opentelemetry/trace/provider.h),)
  DEFINES  += -DSMARTMET_SPINE_OPENTELEMETRY
  CFLAGS   += -I$(OTEL_LOCAL)/include
  LIBS     += -L$(OTEL_LOCAL)/lib64 -L$(OTEL_LOCAL)/lib \
              -lopentelemetry_trace \
              -lopentelemetry_exporter_otlp_http \
              -lopentelemetry_otlp_recordable \
              -lopentelemetry_resources \
              -lopentelemetry_common \
              -lopentelemetry_proto \
              -lprotobuf \
              -lcurl \
              -Wl,-rpath,$(OTEL_LOCAL)/lib64 \
              -Wl,-rpath,$(OTEL_LOCAL)/lib
endif
# ─────────────────────────────────────────────────────────────────────────────

# Common library compiling template

LIBS +=	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-ldouble-conversion \
	-lboost_regex \
	-lboost_timer \
	-lboost_chrono \
	-lboost_thread \
	-lboost_program_options \
	-lboost_locale \
	-lctpp2 \
	$(REQUIRED_LIBS) \
	$(PREFIX_LDFLAGS) \
	-lbacktrace \
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
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol |\
			grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)'; \
	then \
		rm -v $(LIBFILE); \
		exit 1; \
	fi

smartmet_plugin_test: obj/SmartmetPluginTest.o $(LIBFILE)
	$(CXX) $(CFLAGS) -o $@ $* -L. -lsmartmet-spine -lboost_program_options $(CONFIGPP_LIBS) -lpthread

clean:
	rm -f $(LIBFILE) $(OBJS) $(patsubst obj/%.o,obj/%.d,$(OBJS)) *~ $(SUBNAME)/*~
	$(MAKE) -C app $@
	$(MAKE) -C test $@

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

obj/%.o : %.cpp
	@mkdir -p $(objdir)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
