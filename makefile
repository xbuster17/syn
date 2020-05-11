.SILENT:
JOBS=4
EXEC = syn
MAIN = src/main.c

CCFLAG = -std=gnu99 -g -O3 -Wall -Wextra -Wno-psabi -ffast-math -funroll-loops -D _DEFAULT_SOURCE
CPPCFLAG = -std=gnu++11 -Wall -Wextra -g -O3

LFLAG = $(CCFLAG)
LCC=$(CC)

CC = $(CROSS)gcc
CPPC = $(CROSS)g++

PKG_CONFIG = $(CROSS)pkg-config
SDL2_CONFIG = $(CROSS)sdl2-config

LIBS = -lm -lz $(WIN_LIBS) \
       $(shell $(PKG_CONFIG) --cflags --libs SDL2_ttf SDL2_mixer)\
       $(shell $(SDL2_CONFIG) --cflags --libs)\

SRC = $(wildcard src/*.c)
OBJ = $(subst .c,.$(CROSSOBJ)o,$(subst $(MAIN),,$(SRC)))

all: @notify $(EXEC)

$(EXEC): $(MAIN) $(OBJ)
	$(call color_green,"$(LCC) $(EXEC)")
	$(LCC) $(LFLAG) $(OBJ) $(MAIN) $(LIBS)  -o $(EXEC)

%.$(CROSSOBJ)o: %.c %.h
	$(call color_green,"$(CC) $@")
	$(CC) -c $(CCFLAG) $< $(LIBS) -o $@

%.$(CROSSOBJ)opp: %.cpp %.hpp
	$(call color_green,"$(CPPC) $@")
	$(CPPC) -c $(CPPCFLAG) $< $(LIBS) -o $@



rm: rm-all
rm-all: clean
clean:
	$(call color_red,"rm:") $(OBJ)
	rm $(OBJ)





run:
	@make --silent --no-print-directory -j$(JOBS)
	$(call color_green,"running $(EXEC):")
	./$(EXEC)
	$(call color_green,"done $(EXEC)")
	$(call color_green,"________________________")


rund:
	@make --silent --no-print-directory -j$(JOBS)
	$(call color_green,"gdb -ex run $(EXEC)")
	gdb -ex run ./$(EXEC)
	$(call color_green,"done $(EXEC)")
	$(call color_green,"________________________")










#______________________________________________________________________________
# WINDOWS - mxe
WIN_EXEC = $(EXEC).exe
WIN_FLAG = CROSSOBJ=win. CROSS=i686-w64-mingw32.static- EXEC=$(WIN_EXEC) WIN_LIBS=""
# WIN_FLAG = CROSSOBJ=win CROSS=x86_64-w64-mingw32.static-

win:
	make -j$(JOBS) $(WIN_FLAG) && strip -s $(WIN_EXEC)

win-run:
	make -j$(JOBS) $(WIN_FLAG) && strip -s $(WIN_EXEC) && ./$(WIN_EXEC)

win-rm:
	make rm $(WIN_FLAG)

win-clean:
	make clean $(WIN_FLAG)




#______________________________________________________________________________
# ps vita
# edit project name at ./vita/CMakeLists.txt
VITA_ADDR=ftp://192.168.0.106:1337/ux0:vpks/
vita: $(SRC) $(MAIN)
	ln -f -s -r ./src ./psvita/
	mkdir -p ./psvita/build
	cd ./psvita/build && cmake .. && make

vita-push:
	cd ./psvita/build && curl -T test2.vpk $(VITA_ADDR)



#______________________________________________________________________________
# 3ds
# edit project name at ./3ds/Makefile
3DS_ADDR=192.168.0.143
3ds: @notify
	ln -f -s -r ./src ./n3ds/
	cd ./n3ds && make

3ds-push:
	cd n3ds && python servefiles.py $(3DS_ADDR) syn.3dsx

3ds-clean:
	cd n3ds && make clean



#______________________________________________________________________________
#emcc
web:
	$(W)
# -g --profiling
	# -s ERROR_ON_UNDEFINED_SYMBOLS=1

W=emcc -s ASSERTIONS=0  -s WASM=1 \
	-s TOTAL_MEMORY=256MB\
	-s TOTAL_STACK=64MB\
	-s AGGRESSIVE_VARIABLE_ELIMINATION=1 \
	-s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=1 \
	-s SINGLE_FILE=1 \
	-s USE_ZLIB=1 \
	-s USE_SDL=2 \
	-s USE_SDL_MIXER=2 \
	-s USE_SDL_TTF=2 \
	-s USE_SDL_IMAGE=2 \
	--shell-file www/base.html \
	-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'\
	-o www/$(EXEC).html -O3 $(CPPFLAG) $(SRC)\



#______________________________________________________________________________
# android - ndk
ANDROID_MK_FOLDER = android
ANDROID_MK_PATH = $(ANDROID_MK_FOLDER)/jni/src/Android.mk
ANDROID_MK_SOURCES = $(addprefix ../../../,$(SRC))

droid: |
	$(call color_green,"ln -f -s -r ./assets $(ANDROID_MK_FOLDER)/")
	@ln -f -s -r ./assets $(ANDROID_MK_FOLDER)/
	@mkdir -p $(ANDROID_MK_FOLDER)/jni/src

	$(call color_green,"generating android makefile $(ANDROID_MK_PATH)")
	@echo "#auto generated from root makefile ( $(shell pwd) )" > $(ANDROID_MK_PATH)
	@printf $(ANDROID_MK_PREFIX) >> $(ANDROID_MK_PATH)
	@echo LOCAL_SRC_FILES := $(ANDROID_MK_SOURCES) >> $(ANDROID_MK_PATH)
	@printf $(ANDROID_MK_SUFFIX) >> $(ANDROID_MK_PATH)
	# clear makefile's timestamp so it doesnt recompile everything
	@touch -d "1970-01-01 00:00:00.000000000 +0000" $(ANDROID_MK_PATH)

	cd $(ANDROID_MK_FOLDER) && make
	$(call color_green,"done")

# add libraries to compile on android here
define ANDROID_MK_PREFIX
'\n\
LOCAL_PATH := $$(call my-dir)\n\
include $$(CLEAR_VARS)\n\
LOCAL_MODULE := main\n\
SDL_PATH       := ../SDL2\n\
SDL_TTF_PATH   := ../SDL2_ttf\n\
LOCAL_C_INCLUDES := \
  $$(LOCAL_PATH)/$$(SDL_PATH)/include             \
  $$(LOCAL_PATH)/$$(SDL_TTF_PATH)                 \
\n'
endef

define ANDROID_MK_SUFFIX
'\n\
LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf \n\
LOCAL_LDLIBS := -lm -ldl -llog -lz\n\
LOCAL_CFLAGS += $(CCFLAG) -DGL_GLEXT_PROTOTYPES \n\
LOCAL_NEON_CFLAGS += $(CCFLAG) -mfloat-abi=softfp -mfpu=neon -march=armv7 \n\
APP_CFLAGS += $(LFLAG) \n\
include $$(BUILD_SHARED_LIBRARY)\n\
'
endef





















#______________________________________________________________________________
@notify:
	$(call color_green,"________________________")

color_red    =@echo -e "\e[91m"$(1)"\e[0m"$(2)
color_green  =@echo -e "\e[92m"$(1)"\e[0m"$(2)
color_gold   =@echo -e "\e[93m"$(1)"\e[0m"$(2)
color_blue   =@echo -e "\e[94m"$(1)"\e[0m"$(2)
color_purple =@echo -e "\e[95m"$(1)"\e[0m"$(2)
color_cyan   =@echo -e "\e[96m"$(1)"\e[0m"$(2)
color_yellow =@echo -e "\e[33m"$(1)"\e[0m"$(2)
