
REQUIRES = configpp libpqxx

include $(shell smartbuildcfg --prefix)/share/smartmet/devel/makefile.inc

# ISO C++ forbids casting between pointer-to-function and pointer-to-object
CFLAGS += -Wno-pedantic

DEFINES = -DUNIX -D_REENTRANT

# Common library compiling template

LIBS +=	\
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lboost_thread \
	-lboost_program_options \
	-lboost_locale \
	-lctpp2 \
	$(REQUIRED_LIBS) \
	$(PREFIX_LDFLAGS) \
	-ldl \
	-lrt \
	-lpthread

# What to install

PROGS = smartmet-plugin-test cfgvalidate

PLUGIN_TEST_SRCS = SmartmetPluginTest.cpp PluginTest.cpp
PLUGIN_TEST_OBJS = $(patsubst %.cpp, obj/%.o, $(PLUGIN_TEST_SRCS))

CFGVALIDATE_SRCS = cfgvalidate.cpp
CFGVALIDATE_OBJS = $(patsubst %.cpp, obj/%.o, $(CFGVALIDATE_SRCS))

# Compilation directories

vpath %.h $(SUBNAME)
vpath %.o $(objdir)

INCLUDES := -I../spine $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(PROGS)
debug: all
release: all
profile: all

smartmet-plugin-test: $(PLUGIN_TEST_OBJS) ../libsmartmet-spine.so
	$(CXX) $(CFLAGS) -o $@ $(PLUGIN_TEST_OBJS) -L .. -lsmartmet-spine -lfmt -lfontconfig $(LIBS)

cfgvalidate: $(CFGVALIDATE_OBJS) ../libsmartmet-spine.so
	$(CXX) $(CFLAGS) -o $@ $(CFGVALIDATE_OBJS) -L .. -lsmartmet-spine $(LIBS)

clean:
	rm -f obj/*.o obj/*.d $(PROGS)

format:
	clang-format -i -style=file *.h *.cpp

install:
	mkdir -p $(bindir)
	$(INSTALL_PROG) $(PROGS) $(bindir)/

objdir:
	@mkdir -p $(objdir)

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
