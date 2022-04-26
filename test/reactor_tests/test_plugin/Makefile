TOP := $(shell cd ../../.. && pwd)

REQUIRES = jsoncpp configpp

include $(shell smartbuildcfg --prefix)/share/smartmet/devel/makefile.inc

INCLUDES += \
	-I ../../../spine

LIBS += \
	$(TOP)/libsmartmet-spine.so

DEFINES = -DUNIX -D_REENTRANT

LIBFILE := test_plugin.so

# The files to be compiled

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.h)
OBJS = $(patsubst %.cpp, .obj/%.o, $(notdir $(SRCS)))

all:	$(LIBFILE)

clean:
	rm -rf .obj
	rm -rf $(LIBFILE)

$(LIBFILE) : $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol |\
			grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)' |\
			grep -v 'SmartMet::Engine::'; \
		then rm -v $(LIBFILE); \
		exit 1; \
	fi

.obj/%.o: %.cpp
	mkdir -p .obj
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst .obj/%.o, .obj/%.d, $@) -MT $@ -o $@ $<

ifneq ($(wildcard .obj/*.d),)
-include $(wildcard .obj/*.d)
endif
