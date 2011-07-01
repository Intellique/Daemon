MAKEFLAGS 	+= -rR --no-print-directory

# commands
CC			:= ccache ${TARGET}gcc
CTAGS		:= ctags
CSCOPE		:= cscope
GDB			:= gdb


# variable
NAME		:= StoriqArchiver
DIR_NAME	:= $(lastword $(subst /, , $(realpath .)))
#VERSION		:= $(shell git describe)
VERSION		:= 0.0.1


BINS		:=
BIN_SYMS	:=

BUILD_DIR	:= build
DEPEND_DIR	:= depend

SRC_FILES	:=
HEAD_FILES	:= $(sort $(shell test -d include && find include -name '*.h'))
DEP_FILES	:=
OBJ_FILES	:=


# compilation flags
CFLAGS		:= -std=gnu99 -pipe -O0 -ggdb3 -Wall -Wextra -pedantic -pthread -Wabi -Werror-implicit-function-declaration -Wmissing-prototypes -Iinclude -DSTORIQARCHIVER_VERSION=\"${VERSION}\"
LDFLAGS		:= -pthread

CSCOPE_OPT	:= -b -R -s src -U -I include
CTAGS_OPT	:= -R src


# sub makefiles
SUB_MAKES	:= $(sort $(shell test -d src && find src -name Makefile.sub))
ifeq (${SUB_MAKES},)
$(error "No sub makefiles")
endif
include ${SUB_MAKES}

define BIN_template
$$($(1)_BIN): $$($(1)_LIB) $$($(1)_OBJ_FILES)
	@echo " LD       $$@"
	@${CC} -o $$@ $$($(1)_OBJ_FILES) ${LDFLAGS} $$($(1)_LD)
	@echo " OBJCOPY  --only-keep-debug $$@ $$@.debug"
	@objcopy --only-keep-debug $$@ $$@.debug
	@echo " STRIP    $$@"
	@strip $$@
	@echo " OBJCOPY  --add-gnu-debuglink=$$@.debug $$@"
	@objcopy --add-gnu-debuglink=$$@.debug $$@
	@echo " CHMOD    -x $$@.debug"
	@chmod -x $$@.debug

$$($(1)_BUILD_DIR)/%.o: $$($(1)_SRC_DIR)/%.c
	@echo " CC       $$@"
	@${CC} -c $${CFLAGS} $$($(1)_CFLAG) -Wp,-MD,$$($(1)_DEPEND_DIR)/$$*.d,-MT,$$@ -o $$@ $$<

BINS		+= $$($(1)_BIN)
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

all: binaries tags

binaries: prepare $(sort ${BINS})

check:
	@echo 'Checking source files...'
	@${CC} -fsyntax-only ${CFLAGS} ${SRC_FILES}

clean:
	@echo ' RM       -Rf $(foreach dir,${BIN_DIRS},$(word 1,$(subst /, ,$(dir)))) ${BUILD_DIR}'
	@rm -Rf $(foreach dir,${BIN_DIRS},$(word 1,$(subst /, ,$(dir)))) ${BUILD_DIR}

cscope: cscope.out

ctags TAGS: tags

debug: binaries
	@echo ' GDB'
	${GDB} bin/storiqArchiver

distclean realclean: clean
	@echo ' RM       -Rf cscope.out doc ${DEPEND_DIR} tags'
	@rm -Rf cscope.out doc ${DEPEND_DIR} tags

doc: Doxyfile ${LIBOBJECT_SRC_FILES} ${HEAD_FILES}
	@echo ' DOXYGEN'
	@doxygen

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

titi:
	@echo ${BINS}

