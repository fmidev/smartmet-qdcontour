MODULE = qdcontour
SPEC = smartmet-qdcontour

REQUIRES = geos gdal

# Due to GEOS make_unique problems:
# CXX_STD=c++11

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# Compiler options

DEFINES = -DUNIX -DUSE_UNSTABLE_GEOS_CPP_API

LIBS += $(REQUIRED_LIBS) \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lsmartmet-imagine \
	-lsmartmet-gis \
	-lsmartmet-tron \
	-lboost_iostreams \
	-lboost_system \
	-lboost_filesystem \
	-lboost_thread \
	-lstdc++ -lm

LDFLAGS = 

# Compilation directories

vpath %.cpp source main
vpath %.h include
vpath %.o $(objdir)
vpath %.d $(objdir)

# The files to be compiled

HDRS = $(patsubst include/%,%,$(wildcard *.h include/*.h))

MAINSRCS     = $(patsubst main/%,%,$(wildcard main/*.cpp))
MAINPROGS    = $(MAINSRCS:%.cpp=%)
MAINOBJS     = $(MAINSRCS:%.cpp=%.o)
MAINOBJFILES = $(MAINOBJS:%.o=obj/%.o)

SRCS     = $(patsubst source/%,%,$(wildcard source/*.cpp))
OBJS     = $(SRCS:%.cpp=%.o)
OBJFILES = $(OBJS:%.o=obj/%.o)

INCLUDES := -Iinclude $(INCLUDES)

# For make depend:

ALLSRCS = $(wildcard main/*.cpp source/*.cpp)

.PHONY: test rpm

# The rules

all: objdir $(MAINPROGS)
debug: objdir $(MAINPROGS)
release: objdir $(MAINPROGS)
profile: objdir $(MAINPROGS)

.SECONDEXPANSION:
$(MAINPROGS): % : $(OBJFILES) $(MAINOBJFILES)
	$(CXX) $(LDFLAGS) $(filter -fsanitize=%,$(CFLAGS)) -o $@ obj/$@.o $(OBJFILES) $(LIBS)

clean:
	rm -f $(MAINPROGS) source/*~ include/*~
	rm -rf obj

format:
	clang-format -i -style=file include/*.h source/*.cpp main/*.cpp

install:
	mkdir -p $(bindir)
	@list='$(MAINPROGS)'; \
	for prog in $$list; do \
	  echo $(INSTALL_PROG) $$prog $(bindir)/$$prog; \
	  $(INSTALL_PROG) $$prog $(bindir)/$$prog; \
	done

test:
	make --quiet -C test test

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

-include obj/*.d
