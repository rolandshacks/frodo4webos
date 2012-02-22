###############################################################################
#
# Makefile
#
###############################################################################

fn_toslash = $(subst \,/,$(1))
fn_tobackslash = $(subst /,\,$(1))

ifeq ($(TARGET),)
  TARGET = $(notdir $(CURDIR))
endif

ifeq ($(OS),Windows_NT)
  BUILD_PLATFORM = WINDOWS
  NATIVE_SLASH = $(call fn_tobackslash,/)
else
  BUILD_PLATFORM = LINUX
  NATIVE_SLASH = /
endif
  
ifeq ($(PLATFORM),)
  PLATFORM = PALM
endif

ifeq ($(PLATFORM),PALM)
  EXE_EXTENSION =
  LIB_EXTENSION = .a
  LIB_PREFIX = lib
else
  ifeq ($(BUILD_PLATFORM),WINDOWS)
    EXE_EXTENSION = .exe
    LIB_EXTENSION = .lib
    LIB_PREFIX =
  else
    EXE_EXTENSION =
    LIB_EXTENSION = .a
    LIB_PREFIX = lib
  endif
endif

ifeq ($(PLATFORM),PALM)
  STDDEF += WEBOS=1
endif

ifeq ($(TARGETDIR),)
  TARGETDIR = target
endif

ifeq ($(RESOURCES),)
    RESOURCES = $(sort $(subst .svn,,$(wildcard ./resources/*)))
endif

ifeq ($(UPLOADDIR),)
    UPLOADDIR = /media/internal/$(TARGET)
endif

STAGINGDIR = $(TARGETDIR)$(NATIVE_SLASH)staging
RESOURCEDIR = $(STAGINGDIR)$(NATIVE_SLASH)resources

ifeq ($(PLATFORM),PALM)
ifeq ($(PALMPDK),)
    ifeq ($(BUILD_PLATFORM),WINDOWS)
        PALMPDK = C:\Tools\webOS\PDK
    else
        PALMPDK = /opt/PalmPDK
    endif
endif
endif

ifeq ($(SRC),)
  SRC = $(wildcard *.cpp)
endif

ifeq ($(EMULATOR),)
EMULATOR = 0
endif

ifeq ($(DEBUG),)
DEBUG = 0
endif

ifeq ($(PLATFORM),PALM)
  ifeq ($(EMULATOR),1)
    CC          = i686-pc-linux-gnu-gcc
    AR          = i686-pc-linux-gnueabi-ar
    STDLIBDIRS  = $(PALMPDK)\emulator\lib
    STDINCDIRS  = $(PALMPDK)\include $(PALMPDK)\include\SDL
    STDLIBS     = m rt SDL SDL_ttf SDL_image GLES_CM pdl
  else
    CC          = arm-none-linux-gnueabi-gcc
    AR          = arm-none-linux-gnueabi-ar
    STDLIBDIRS  = $(PALMPDK)\device\lib
    STDINCDIRS  = $(PALMPDK)\include $(PALMPDK)\include\SDL
    STDLIBS     = m rt SDL SDL_ttf SDL_image GLES_CM pdl
  endif
else
    CC          = g++
    AR          = ar
    STDLIBDIRS  = lib

    ifeq ($(BUILD_PLATFORM),WINDOWS)
		STDINCDIRS  = C:\Tools\MINGW\include\SDL
		STDLIBS    += m mingw32 SDLmain SDL SDL_ttf SDL_image opengl32
		STDLIBS    += kernel32 user32 gdi32 winspool comdlg32 advapi32 \
		              shell32 ole32 oleaut32 uuid odbc32 odbccp32 ws2_32 \
		              secur32 winmm iphlpapi
	    CC_FLAGS   += -mwindows
	else
		STDINCDIRS  = /usr/include/SDL
	    STDLIBS     = m rt SDL SDL_ttf SDL_image GL
	endif
                  
endif

#DISABLE ALL WARNINGS
CC_FLAGS   += -w
#CC_FLAGS   += -Wl,--allow-shlib-undefined 

ifeq ($(DEBUG),1)
  CC_FLAGS += -g -O0
else
  # CC_FLAGS += -O2
  CC_FLAGS += -O3
endif

CC_INCDIRS  = $(addprefix -I,$(INCDIRS) $(PLATFORMINCDIRS) $(STDINCDIRS))
CC_LIBDIRS  = $(addprefix -L,$(LIBDIRS) $(PLATFORMLIBDIRS) $(STDLIBDIRS))
CC_LIBS     = $(addprefix -l,$(LIBS) $(PLATFORMLIBS) $(STDLIBS) $(DEFAULTLIBS))
CC_SRC      = $(sort $(SRC) )
CC_HDR      = $(sort $(HDR) $(foreach dir,$(HDRDIRS),$(wildcard $(dir)/*.h)) $(QT_HDR) $(QT_UIHDR))
CC_DEF      = $(addprefix -D,$(DEF) $(STDDEF))

ifeq ($(OBJS),)
CC_OBJ      = $(CC_SRC:.cpp=.opp)
CC_OBJC     = $(CC_SRC:.c=.o)
else
CC_OBJ      = $(sort $(OBJS) )
endif

ifeq ($(PROJECT_TYPE),static)
  ifeq ($(BUILD_CMD),)
    BUILD_CMD = build_static
  endif
  OUTFILE = $(TARGETDIR)$(NATIVE_SLASH)$(LIB_PREFIX)$(TARGET)$(LIB_EXTENSION)
else
  ifeq ($(BUILD_CMD),)
    BUILD_CMD = build_executable
  endif
  OUTFILE = $(TARGETDIR)$(NATIVE_SLASH)$(TARGET)$(EXE_EXTENSION)
endif

ifeq ($(COMPILE),)
COMPILE = compile
endif

ifeq ($(BUILD),)
BUILD = build
endif

ifeq ($(UPLOAD),)
UPLOAD = upload
endif

ifeq ($(DEFAULT),)
DEFAULT = all
endif

###############################################################################

.PHONY: default all rebuild $(COMPILE) $(BUILD_CMD) $(DEFAULT) compile build killall prepare build_executable build_static
.PHONY: clean $(UPLOAD) upload debug run shell package resources dump
.PHONY: $(RESOURCES) upload_target upload_resources
.PHONY: prepare_$(BUILD_PLATFORM) clean_$(BUILD_PLATFORM)

default: $(DEFAULT)
rebuild: clean build

all: $(BUILD_CMD)

$(CC_OBJ) $(CC_OBJC): $(HDR)

$(SRC): $(HDR) 

%.opp: %.cpp
	@echo compile $*.cpp
	$(CC) $(DEVICEOPTS) $(CC_FLAGS) $(CC_DEF) $(CC_INCDIRS) -c $*.cpp -o $*.opp

%.o: %.c
	@echo compile $*.c
	$(CC) $(DEVICEOPTS) $(CC_FLAGS) $(CC_DEF) $(CC_INCDIRS) -c $*.c -o $*.o

prepare_WINDOWS:
	@cmd /C if not exist "$(TARGETDIR)" mkdir "$(TARGETDIR)"
	@cmd /C if not exist "$(STAGINGDIR)" mkdir "$(STAGINGDIR)"
	@cmd /C if not exist "$(RESOURCEDIR)" mkdir "$(RESOURCEDIR)"

prepare_LINUX:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(STAGINGDIR)
	@mkdir -p $(RESOURCEDIR)

prepare: prepare_$(BUILD_PLATFORM)

compile: prepare $(CC_OBJ) $(CC_OBJC)

build_executable: $(COMPILE)
	@echo build executable
	$(CC) $(CC_OBJ) $(CC_LIBDIRS) $(CC_LIBS) $(LDFLAGS) -o $(OUTFILE)

build_static: $(COMPILE)
	@echo build static $(CC_OBJ) $(CC_OBJC)
	$(AR) rcs $(OUTFILE) $(CC_OBJ) $(CC_OBJC)

clean_WINDOWS:
	@echo clean
	@cmd /C if exist "$(RESOURCEDIR)" rmdir /s /q "$(RESOURCEDIR)"
	@cmd /C if exist "$(STAGINGDIR)" rmdir /s /q "$(STAGINGDIR)"
	@cmd /C if exist "$(TARGETDIR)" rmdir /s /q "$(TARGETDIR)"
	@cmd /C if exist "$(OUTFILE)" del "$(OUTFILE)"
	@cmd /C del *.o
	@cmd /C del *.opp
	@cmd /C del *.ipk

clean_LINUX:
	@echo clean
	@rm -rf $(TARGETDIR)
	@rm -f $(OUTFILE) *.o *.opp *.ipk

clean: clean_$(BUILD_PLATFORM)

package: $(BUILD_CMD) resources
	@echo package
	cmd /C copy $(OUTFILE) $(STAGINGDIR)
	cmd /C if exist $(RESOURCEDIR)$(NATIVE_SLASH)appinfo.json copy "$(RESOURCEDIR)$(NATIVE_SLASH)appinfo.json" "$(STAGINGDIR)"
	cmd /C if exist $(RESOURCEDIR)$(NATIVE_SLASH)logo.png copy "$(RESOURCEDIR)$(NATIVE_SLASH)logo.png" "$(STAGINGDIR)"
	cmd /C echo filemode.755=$(TARGET) > $(STAGINGDIR)$(NATIVE_SLASH)package.properties
	cmd /C palm-package -o $(TARGETDIR) $(STAGINGDIR)

killall:
	@echo upload
	echo cmd /C plink -P 10022 root@localhost -pw "" "killall -9 gdbserver $(TARGET)"

$(RESOURCES): 
	cmd /C copy "$(call fn_tobackslash,$@)" "$(call fn_tobackslash,$(RESOURCEDIR)$(NATIVE_SLASH)$(notdir $@))"

resources: $(RESOURCES)

upload_resources: resources
	@echo upload resources
	plink -P 10022 root@localhost -pw "" "mkdir -p $(UPLOADDIR)/resources"
	pscp -scp -P 10022 -pw "" $(RESOURCEDIR)$(NATIVE_SLASH)*.* root@localhost:$(UPLOADDIR)/resources

upload_target: $(BUILD_CMD) killall
	@echo upload target
	plink -P 10022 root@localhost -pw "" "mkdir -p $(UPLOADDIR)"
	pscp -scp -P 10022 -pw "" $(OUTFILE) root@localhost:$(UPLOADDIR)

upload: $(BUILD_CMD) upload_target upload_resources 
	
debug: upload
	@echo debug
	plink -P 10022 root@localhost -pw "" "/usr/bin/gdbserver host:2345 $(UPLOADDIR)/$(TARGET) &"

run: upload
	@echo run
	plink -P 10022 root@localhost -pw "" "$(UPLOADDIR)/$(TARGET)"
    
shell:
	@echo shell
	@novacom open tty:// = novaterm

dump:
	@echo PLATFORM=$(PLATFORM)
	@echo TARGETDIR=$(TARGETDIR)
	@echo STAGINGDIR=$(STAGINGDIR)
	@echo RESOURCEDIR=$(RESOURCEDIR)
	@echo OUTFILE=$(OUTFILE)
	@echo TARGET=$(TARGET)
	@echo RESOURCES=$(RESOURCES)
	@echo BUILD_PLATFORM=$(BUILD_PLATFORM)
	@echo PALMPDK=$(PALMPDK)
	@echo CC_SRC=$(CC_SRC)
	@echo CC_HDR=$(CC_HDR)
	@echo CC_DEF=$(CC_DEF)
	@echo CC_OBJ=$(CC_OBJ)
	@echo CC_OBJC=$(CC_OBJC)
	@echo CC_LIBS=$(CC_LIBS)
	@echo CC_INCDIRS=$(CC_INCDIRS)
	@echo CC_LIBDIRS=$(CC_LIBDIRS)
	@echo DEVICEOPTS=$(DEVICEOPTS)
	@echo CC_FLAGS=$(CC_FLAGS)
	@echo LDFLAGS=$(LDFLAGS)
	@echo UPLOADDIR=$(UPLOADDIR)
