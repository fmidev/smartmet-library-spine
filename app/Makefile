
REQUIRES = configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# ISO C++ forbids casting between pointer-to-function and pointer-to-object
CFLAGS += -Wno-pedantic

DEFINES = -DUNIX -D_REENTRANT

# Common library compiling template

LIBS +=	-L$(libdir) \
	-lsmartmet-macgyver \
	-lboost_filesystem \
	-lboost_thread \
	-lboost_program_options \
	-lboost_system \
	-lboost_locale \
	-lctpp2 \
	$(CONFIGPP_LIBS) \
	-ldl \
	-lrt \
	-lpthread

# What to install

PROGS = smartmet-plugin-test

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

smartmet-plugin-test: obj/SmartmetPluginTest.o ../libsmartmet-spine.so
	$(CXX) $(CFLAGS) -o $@ obj/SmartmetPluginTest.o -L .. -lsmartmet-spine $(LIBS)

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
