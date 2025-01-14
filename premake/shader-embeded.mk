#
# Copyright 2011-2014 Branimir Karadzic. All rights reserved.
# License: http://www.opensource.org/licenses/BSD-2-Clause
#

ifndef VERBOSE
SILENT = @
endif

THISDIR := $(dir $(lastword $(MAKEFILE_LIST)))

UNAME := $(shell uname)
ifeq ($(UNAME),$(filter $(UNAME),Linux))
OS=linux
else
ifeq ($(UNAME),$(filter $(UNAME),Darwin))
OS=darwin
else
OS=windows
endif
endif

SHADERC="$(THISDIR)../tools/bin/$(OS)/shaderc"

VS_FLAGS+=-i $(THISDIR)../src/ --type vertex
FS_FLAGS+=-i $(THISDIR)../src/ --type fragment

VS_SOURCES=$(wildcard vs_*.sc)
FS_SOURCES=$(wildcard fs_*.sc)

VS_BIN = $(addsuffix .bin.h, $(basename $(VS_SOURCES)))
FS_BIN = $(addsuffix .bin.h, $(basename $(FS_SOURCES)))

BIN = $(VS_BIN) $(FS_BIN)

SHADER_TMP = $(TEMP)/tmp

vs_%.bin.h : vs_%.sc
	@echo [$(<)]
	$(SILENT) $(SHADERC) $(VS_FLAGS) --platform linux                  -f $(<) -o $(SHADER_TMP) --bin2c $(basename $(<))_glsl
	@cat $(SHADER_TMP) > $(@)
	-$(SILENT) $(SHADERC) $(VS_FLAGS) --platform windows -p vs_3_0 -O 3 -f $(<) -o $(SHADER_TMP) --bin2c $(basename $(<))_dx9
	-@cat $(SHADER_TMP) >> $(@)
	-$(SILENT) $(SHADERC) $(VS_FLAGS) --platform windows -p vs_4_0 -O 3 -f $(<) -o $(SHADER_TMP) --bin2c $(basename $(<))_dx11
	-@cat $(SHADER_TMP) >> $(@)

fs_%.bin.h : fs_%.sc
	@echo [$(<)]
	$(SILENT) $(SHADERC) $(FS_FLAGS) --platform linux                  -f $(<) -o $(SHADER_TMP) --bin2c $(basename $(<))_glsl
	@cat $(SHADER_TMP) > $(@)
	-$(SILENT) $(SHADERC) $(FS_FLAGS) --platform windows -p ps_3_0 -O 3 -f $(<) -o $(SHADER_TMP) --bin2c $(basename $(<))_dx9
	-@cat $(SHADER_TMP) >> $(@)
	-$(SILENT) $(SHADERC) $(FS_FLAGS) --platform windows -p ps_4_0 -O 3 -f $(<) -o $(SHADER_TMP) --bin2c $(basename $(<))_dx11
	-@cat $(SHADER_TMP) >> $(@)

.PHONY: all
all: $(BIN)

.PHONY: clean
clean:
	@echo Cleaning...
	@-rm -vf $(BIN)

.PHONY: rebuild
rebuild: clean all
