MAKEFLAGS 	+= -rR --no-print-directory

# commands
CC			:= $(shell which ccache) ${TARGET}gcc
CTAGS		:= ctags
CSCOPE		:= cscope
GDB			:= gdb


# variable
NAME		:= STone
DIR_NAME	:= $(lastword $(subst /, , $(realpath .)))
#VERSION		:= $(shell git describe)
VERSION		:= 0.0.1


BINS		:=
BIN_SYMS	:=

BUILD_DIR	:= build
DEPEND_DIR	:= depend
INCLUDE_DIR := . include

SRC_FILES	:=
HEAD_FILES	:= $(sort $(shell test -d include && find include -name '*.h'))
DEP_FILES	:=
OBJ_FILES	:=

ifndef (${DESTDIR})
DESTDIR   := output
endif


# compilation flags
CFLAGS		:= -std=gnu99 -pipe -O0 -ggdb3 -Wall -Wextra -Wabi -Werror-implicit-function-declaration -Wmissing-prototypes $(addprefix -I,${INCLUDE_DIR}) -DSTONE_VERSION=\"${VERSION}\"
LDFLAGS		:=

CSCOPE_OPT	:= -b -R -s src -U -I include
CTAGS_OPT	:= -R src


# sub makefiles
SUB_MAKES	:= $(sort $(shell test -d src && find src -name Makefile.sub))
ifeq (${SUB_MAKES},)
$(error "No sub makefiles")
endif
include ${SUB_MAKES}

define BIN_template
$$($(1)_BIN): $$($(1)_DEPEND_LIB) $$($(1)_OBJ_FILES)
	@echo " LD       $$@"
	@${CC} -o $$@ $$($(1)_OBJ_FILES) ${LDFLAGS} $$($(1)_LD)
	@objcopy --only-keep-debug $$@ $$@.debug
#	@strip $$@
	@objcopy --add-gnu-debuglink=$$@.debug $$@
	@chmod -x $$@.debug

$$($(1)_LIB): $$($(1)_DEPEND_LIB) $$($(1)_OBJ_FILES)
	@echo " LD       $$@"
	@${CC} -o $$@.$$($(1)_LIB_VERSION) $$($(1)_OBJ_FILES) -shared -Wl,-soname,$$($(1)_SONAME) ${LDFLAGS} $$($(1)_LD)
	@objcopy --only-keep-debug $$@.$$($(1)_LIB_VERSION) $$@.$$($(1)_LIB_VERSION).debug
#	@strip $$@.$$($(1)_LIB_VERSION)
	@objcopy --add-gnu-debuglink=$$@.$$($(1)_LIB_VERSION).debug $$@.$$($(1)_LIB_VERSION)
	@chmod -x $$@.$$($(1)_LIB_VERSION).debug
	@ln -sf $$(notdir $$@.$$($(1)_LIB_VERSION)) $$@.$$(basename $$($(1)_LIB_VERSION))
	@ln -sf $$($(1)_SONAME) $$@

$$($(1)_BUILD_DIR)/%.o: $$($(1)_SRC_DIR)/%.c
	@echo " CC       $$@"
	@${CC} -c $${CFLAGS} $$($(1)_CFLAG) -Wp,-MD,$$($(1)_DEPEND_DIR)/$$*.d,-MT,$$@ -o $$@ $$<

BINS		+= $$($(1)_BIN) $$($(1)_LIB)
SRC_FILES	+= $$($(1)_SRC_FILES)
HEAD_FILES	+= $$($(1)_HEAD_FILES)
DEP_FILES	+= $$($(1)_DEP_FILES)
OBJ_FILES	+= $$($(1)_OBJ_FILES)
endef

$(foreach prog,${BIN_SYMS},$(eval $(call BIN_template,${prog})))


BIN_DIRS	:= $(sort $(dir ${BINS}))
OBJ_DIRS	:= $(sort $(dir ${OBJ_FILES}))
DEP_DIRS	:= $(patsubst ${BUILD_DIR}/%,${DEPEND_DIR}/%,${OBJ_DIRS})


# phony target
.DEFAULT_GOAL	:= all
.PHONY: all binaries clean cscope ctags debug distclean lib prepare realclean stat stat-extra TAGS tar

all: binaries cscope tags

binaries: prepare $(sort ${BINS})

check:
	@echo 'Checking source files...'
	-@${CC} -fsyntax-only ${CFLAGS} ${SRC_FILES}

clean:
	@echo ' RM       -Rf $(foreach dir,${BIN_DIRS},$(word 1,$(subst /, ,$(dir)))) ${BUILD_DIR}'
	@rm -Rf $(foreach dir,${BIN_DIRS},$(word 1,$(subst /, ,$(dir)))) ${BUILD_DIR}

cscope: cscope.out

ctags TAGS: tags

debug: binaries
	@echo ' GDB'
	${GDB} bin/stone

distclean realclean: clean
	@echo ' RM       -Rf cscope.out doc ${DEPEND_DIR} tags'
	@rm -Rf cscope.out doc ${DEPEND_DIR} tags

doc: Doxyfile ${LIBOBJECT_SRC_FILES} ${HEAD_FILES}
	@echo ' DOXYGEN'
	@doxygen

install:
	@echo ' MKDIR     ${DESTDIR}'
	@mkdir -p ${DESTDIR}/usr/bin ${DESTDIR}/usr/sbin ${DESTDIR}/usr/lib/stone
	@echo ' CP'
	@cp bin/stone ${DESTDIR}/usr/sbin
	@cp bin/stone-admin ${DESTDIR}/usr/bin
	@cp lib/lib*.so ${DESTDIR}/usr/lib/stone
	@mv ${DESTDIR}/usr/lib/stone/libstone.so ${DESTDIR}/usr/lib

prepare: ${BIN_DIRS} ${DEP_DIRS} ${OBJ_DIRS}

rebuild: clean all

stat:
	@wc $(sort ${SRC_FILES} ${HEAD_FILES})
	@git diff -M --cached --stat=${COLUMNS}

stat-extra:
	@c_count -w 48 $(sort ${HEAD_FILES} ${SRC_FILES})

tar: ${NAME}.tar.bz2


# real target
${BIN_DIRS} ${DEP_DIRS} ${OBJ_DIRS}:
	@echo " MKDIR    $@"
	@mkdir -p $@

${NAME}.tar.bz2:
	git archive --format=tar --prefix=${DIR_NAME}/ master | bzip2 -9c > $@

cscope.out: ${SRC_FILES} ${HEAD_FILES}
	@echo " CSCOPE"
	@${CSCOPE} ${CSCOPE_OPT}

tags: ${SRC_FILES} ${HEAD_FILES}
	@echo " CTAGS"
	@${CTAGS} ${CTAGS_OPT}

ifneq (${DEP_FILES},)
include ${DEP_FILES}
endif
