MAKEFLAGS 	+= -rR --no-print-directory
SHELL=/bin/bash

# commands
# static code analysis tool: cppcheck, sparse (cgcc), splint
CC			:= $(shell which ccache) ${TARGET}gcc
CSCOPE		:= cscope
CTAGS		:= ctags
DOXYGEN		:= doxygen
GDB			:= gdb
GIT			:= git
OBJCOPY		:= ${TARGET}objcopy
STRIP		:= ${TARGET}strip


# variable
NAME		:= StoriqOne
DIR_NAME	:= $(lastword $(subst /, , $(realpath .)))
VERSION		:= v1.3.2


GIT_ARCHIVE := $(shell ./script/git-archive.pl ${DIR_NAME}).orig.tar.gz
GIT_HEAD	:= $(shell ./script/git-head.pl)

BINS		:=
BIN_SYMS	:=

TEST_BINS		:=
TEST_BIN_SYMS	:=

BUILD_DIR	:= build
CHCKSUM_DIR	:= checksum
DEPEND_DIR	:= depend
INCLUDE_DIR := . include ${CHCKSUM_DIR}

SRC_FILES	:=
HEAD_FILES	:= $(sort $(shell test -d include && find include -name '*.h'))
DEP_FILES	:=
OBJ_FILES	:=

LOCALE_PO	:=
LOCALE_MO	:=

TEST_SRC_FILES	:=
TEST_HEAD_FILES	:=

ifndef (${DESTDIR})
DESTDIR   := output
endif


# compilation flags
CFLAGS		:= -std=gnu99 -pipe -O2 -ggdb3 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Wabi -Werror-implicit-function-declaration -Wmissing-prototypes -Wformat -Wformat-security -Werror=format-security -fstack-protector-strong -fstack-protector --param ssp-buffer-size=4 $(addprefix -I,${INCLUDE_DIR})
LDFLAGS		:= -Wl,-z,relro,-z,now

CSCOPE_OPT	:= -b -R -s src -U -I include
CTAGS_OPT	:= -R src

VERSION_FILE	:= storiqone.version
VERSION_OPT		:= STORIQONE ${VERSION_FILE}


# sub makefiles
SUB_MAKES   := $(sort $(shell test -d src -a -d test && find src test -name Makefile.sub))
ifeq (${SUB_MAKES},)
$(error "No sub makefiles")
endif
include ${SUB_MAKES}

define BIN_template
$(1)_BUILD_DIR	:= $$(patsubst src/%,${BUILD_DIR}/%,$${$(1)_SRC_DIR})
$(1)_DEPEND_DIR	:= $$(patsubst src/%,${DEPEND_DIR}/%,$${$(1)_SRC_DIR})

$(1)_SRC_FILES	:= $$(sort $$(shell test -d $${$(1)_SRC_DIR} && find $${$(1)_SRC_DIR} -name '*.c'))
$(1)_HEAD_FILES	:= $$(sort $$(shell test -d $${$(1)_SRC_DIR} && find $${$(1)_SRC_DIR} -name '*.h'))
$(1)_OBJ_FILES	:= $$(sort $$(patsubst src/%.c,${BUILD_DIR}/%.o,$${$(1)_SRC_FILES}))
$(1)_DEP_FILES  := $$(sort $$(patsubst src/%.c,${DEPEND_DIR}/%.d,$${$(1)_SRC_FILES}))

$(1)_OBJ_DIRS := $$(sort $$(dir $${$(1)_OBJ_FILES}))
$(1)_DEP_DIRS := $$(patsubst ${BUILD_DIR}/%,${DEPEND_DIR}/%,$${$(1)_OBJ_DIRS})


$(1)_BIN_DIR := $$(dir $$($(1)_BIN))
$${$(1)_BIN}: ${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE} $${$(1)_DEPEND_LIB} $${$(1)_OBJ_FILES} | $${$(1)_BIN_DIR}
	@echo " LD         $$@"
	@${CC} -o $$@ $$($(1)_OBJ_FILES) ${LDFLAGS} $$($(1)_LD)
#	@${OBJCOPY} --only-keep-debug $$@ $$@.debug
#	@${STRIP} $$@
#	@${OBJCOPY} --add-gnu-debuglink=$$@.debug $$@
#	@chmod -x $$@.debug

$(1)_LIB_DIR := $$(dir $$($(1)_LIB))
$$($(1)_LIB): ${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE} $${$(1)_DEPEND_LIB} $${$(1)_OBJ_FILES} | $${$(1)_LIB_DIR}
	@echo " LD         $$@"
	@${CC} -o $$@.$$($(1)_LIB_VERSION) $$($(1)_OBJ_FILES) -shared -Wl,-soname,$$($(1)_SONAME) ${LDFLAGS} $$($(1)_LD)
#	@objcopy --only-keep-debug $$@.$$($(1)_LIB_VERSION) $$@.$$($(1)_LIB_VERSION).debug
#	@strip $$@.$$($(1)_LIB_VERSION)
#	@objcopy --add-gnu-debuglink=$$@.$$($(1)_LIB_VERSION).debug $$@.$$($(1)_LIB_VERSION)
#	@chmod -x $$@.$$($(1)_LIB_VERSION).debug
	@ln -sf $$(notdir $$@.$$($(1)_LIB_VERSION)) $$@.$$(basename $$($(1)_LIB_VERSION))
	@ln -sf $$($(1)_SONAME) $$@

$${$(1)_BUILD_DIR}/%.o: $${$(1)_SRC_DIR}/%.c | $${$(1)_OBJ_DIRS} $${$(1)_DEP_DIRS}
	@echo " CC         $$@"
	@${CC} -c $${CFLAGS} $$($(1)_CFLAG) -Wp,-MD,$$($(1)_DEPEND_DIR)/$$*.d,-MT,$$@ -o $$@ $$(firstword $$<)

${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE}: $${$(1)_SRC_FILES} $${$(1)_HEAD_FILES} | ${CHCKSUM_DIR}
	@echo " CHCKSUM    $$@"
	@./script/checksum.pl $(1) ${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE} $$(sort $${$(1)_SRC_FILES} $${$(1)_HEAD_FILES})

$${$(1)_DEP_DIRS} $${$(1)_OBJ_DIRS}:
	@echo " MKDIR      $$@"
	@mkdir -p $$@


BINS  += $${$(1)_BIN} $${$(1)_LIB} $${$(1)_LOCALE_MO}

SRC_FILES  += $${$(1)_SRC_FILES}
HEAD_FILES += $${$(1)_HEAD_FILES}
DEP_FILES  += $${$(1)_DEP_FILES}


ifneq (,$$($(1)_LOCALE))
$(1)_LOCALE_POT	:= locale/$$($(1)_LOCALE).pot
$(1)_LOCALE_PO	:= $$(wildcard locale/*/*/$$($(1)_LOCALE).po)
$(1)_LOCALE_MO	:= $$(patsubst %.po,%.mo,$$($(1)_LOCALE_PO))

LOCALE_POT		+= $$($(1)_LOCALE_POT)
LOCALE_PO		+= $$($(1)_LOCALE_PO)
LOCALE_MO		+= $$($(1)_LOCALE_MO)

$$($(1)_LOCALE_POT): $$($(1)_SRC_FILES)
	@echo " XGETTEXT   $${$(1)_LOCALE}.pot"
	@xgettext -d $$($(1)_LOCALE) -o $$@ --from-code=UTF-8 -i -w 128 -s $$($(1)_SRC_FILES)

$$($(1)_LOCALE_PO): $$($(1)_LOCALE_POT)
	@echo " MSGMERGE   $$@"
	@msgmerge -q --backup=off -F -N -U -i -w 128 $$@ $$<
	@touch $$@

%.mo: %.po
	@echo " MSGFMT     $$@"
	@msgfmt -f --check --output-file $$@ $$<

endif
endef

$(foreach prog,${BIN_SYMS},$(eval $(call BIN_template,${prog})))

define TEST_template
$(1)_BUILD_DIR	:= $$(patsubst test/%,${BUILD_DIR}/%,$${$(1)_SRC_DIR})
$(1)_DEPEND_DIR	:= $$(patsubst test/%,${DEPEND_DIR}/%,$${$(1)_SRC_DIR})

$(1)_SRC_FILES	:= $$(sort $$(shell test -d $${$(1)_SRC_DIR} && find $${$(1)_SRC_DIR} -name '*.c'))
$(1)_HEAD_FILES	:= $$(sort $$(shell test -d $${$(1)_SRC_DIR} && find $${$(1)_SRC_DIR} -name '*.h'))
$(1)_OBJ_FILES	:= $$(sort $$(patsubst test/%.c,${BUILD_DIR}/%.o,$${$(1)_SRC_FILES}))
$(1)_DEP_FILES	:= $$(sort $$(shell test -d $${$(1)_DEPEND_DIR} && find $${$(1)_DEPEND_DIR} -name '*.d'))

prepare_$(1): ${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE}

${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE}: $${$(1)_SRC_FILES} $${$(1)_HEAD_FILES}
	@echo " CHCKSUM    $$@"
	@./script/checksum.pl $(1) ${CHCKSUM_DIR}/$${$(1)_CHCKSUM_FILE} $$(sort $${$(1)_SRC_FILES} $${$(1)_HEAD_FILES})

$$($(1)_BIN): $$($(1)_LIB) $$($(1)_OBJ_FILES)
	@echo " LD         $$@"
	@${CC} -o $$@ $$($(1)_OBJ_FILES) ${LDFLAGS} $$($(1)_LD)
#	@${OBJCOPY} --only-keep-debug $$@ $$@.debug
#	@${STRIP} $$@
#	@${OBJCOPY} --add-gnu-debuglink=$$@.debug $$@
#	@chmod -x $$@.debug

$$($(1)_BUILD_DIR)/%.o: $$($(1)_SRC_DIR)/%.c
	@echo " CC         $$@"
	@${CC} -c $${CFLAGS} $$($(1)_CFLAG) -Wp,-MD,$$($(1)_DEPEND_DIR)/$$*.d,-MT,$$@ -o $$@ $$<

TEST_BINS		+= $$($(1)_BIN) $$($(1)_LIB)
TEST_SRC_FILES	+= $$($(1)_SRC_FILES)
TEST_HEAD_FILES	+= $$($(1)_HEAD_FILES)
DEP_FILES		+= $$($(1)_DEP_FILES)
OBJ_FILES		+= $$($(1)_OBJ_FILES)
endef

$(foreach prog,${TEST_BIN_SYMS},$(eval $(call TEST_template,${prog})))


BIN_DIRS	:= $(sort $(dir ${BINS}))


.DEFAULT_GOAL	:= all
.PHONY: all check clean cscope ctags debug distclean doc realclean stat stat-extra TAGS tar test

all: cscope tags ${VERSION_FILE} ${BINS} locales

check:
	@echo 'Checking source files...'
	@cppcheck -v --std=c99 $(addprefix -I,${INCLUDE_DIR}) ${SRC_FILES}
#-@${CC} -fsyntax-only ${CFLAGS} ${SRC_FILES}

clean:
	@echo ' RM         -Rf $(foreach dir,${BIN_DIRS},$(word 1,$(subst /, ,$(dir)))) ${BUILD_DIR} locales'
	@rm -Rf $(foreach dir,${BIN_DIRS},$(word 1,$(subst /, ,$(dir)))) ${BUILD_DIR} ${LOCALE_MO}

cscope: cscope.out

ctags TAGS: tags

debug: all
	@echo ' GDB'
	${GDB} bin/storiqone

distclean realclean: clean
	@echo ' RM         -Rf configure.vars cscope.out doc ${CHCKSUM_DIR} ${DEPEND_DIR} tags ${VERSION_FILE}'
	@rm -Rf configure.vars cscope.out doc ${CHCKSUM_DIR} ${DEPEND_DIR} tags ${VERSION_FILE}

doc: Doxyfile ${LIBOBJECT_SRC_FILES} ${HEAD_FILES}
	@echo ' DOXYGEN'
	@${DOXYGEN}

install:
	@echo ' MKDIR       ${DESTDIR}'
	@mkdir -p ${DESTDIR}/etc/{init.d,logrotate.d,storiq} ${DESTDIR}/usr/bin ${DESTDIR}/usr/sbin ${DESTDIR}/usr/lib/storiqone/{bin,lib,scripts}
	@echo ' CP'
	@./script/install.sh ${DESTDIR}

locales: $(sort ${LOCALE_MO})

package:
	@echo ' CLEAN'
	@dh_clean
	@echo ' UPDATE      src'
	@${GIT} archive --format=tar.gz -o ../${GIT_ARCHIVE} ${VERSION}
	@echo ' BUILD       package'
	@dpkg-buildpackage -us -uc -rfakeroot

rebuild: clean all

stat:
	@wc $(sort ${SRC_FILES} ${HEAD_FILES})
	@${GIT} diff -M --cached --stat=${COLUMNS}

stat-extra:
	@c_count2 -w 64 $(sort ${HEAD_FILES} ${SRC_FILES})

tar: ${NAME}.tar.bz2

test: prepare $(sort ${TEST_BINS})
	@./${CUNIT_BIN}


# real target
${BIN_DIRS} ${CHCKSUM_DIR} ${DEP_DIRS} ${OBJ_DIRS}:
	@echo " MKDIR      $@"
	@mkdir -p $@

${NAME}.tar.bz2:
	${GIT} archive --format=tar --prefix=${DIR_NAME}/ master | bzip2 -9c > $@

${VERSION_FILE}: ${GIT_HEAD}
	@echo ' GENERATE   storiq one version'
	@./script/version.pl ${VERSION_OPT}

configure.vars:
	@echo ' CONFIGURE'
	@./configure.pl

cscope.out: ${SRC_FILES} ${HEAD_FILES}
	@echo " CSCOPE"
	@${CSCOPE} ${CSCOPE_OPT}

tags: ${SRC_FILES} ${HEAD_FILES}
	@echo " CTAGS"
	@${CTAGS} ${CTAGS_OPT}

ifeq ($(findstring $(MAKECMDGOALS),check clean distclean doc package stat),)
-include configure.vars
-include ${DEP_FILES}
endif
