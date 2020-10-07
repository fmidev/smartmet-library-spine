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
	$(CONFIGPP_LIBS) \
	$(MYSQL_LIBS) \
	$(JSONCPP_LIBS) \
	$(ICU_I18_LIBS) \
	-ldl \
	-lrt

# What to install

LIBFILE = lib$(LIB).so

# Compilation directories

vpath %.cpp $(SUBNAME)
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
debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)

clean:
	rm -f $(LIBFILE) $(OBJS) $(patsubst obj/%.o,obj/%.d,$(OBJS)) *~ $(SUBNAME)/*~

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp test/*.cpp

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

test:
	$(MAKE) -C test $@

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

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
