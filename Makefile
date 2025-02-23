CROSS_COMPILE ?= 
CC  := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++

PKGCONFIG ?= pkg-config

CFLAGS := -g -Wall -Ishared/include/
CLIENT_CFLAGS := -Iclient/include/
SERVER_CFLAGS := -Iserver/include/
LDFLAGS := -g `$(PKGCONFIG) --libs alsa` -lsoxr

CFLAGS  += `$(PKGCONFIG) --cflags libavformat`
LDFLAGS += `$(PKGCONFIG) --libs   libavformat`
CFLAGS  += `$(PKGCONFIG) --cflags libavutil`
LDFLAGS += `$(PKGCONFIG) --libs   libavutil`
CFLAGS  += `$(PKGCONFIG) --cflags libavcodec`
LDFLAGS += `$(PKGCONFIG) --libs   libavcodec`
CFLAGS  += `$(PKGCONFIG) --cflags libswresample`
LDFLAGS += `$(PKGCONFIG) --libs   libswresample`

SERVER_OBJS := \
	server/main.o \
	server/config.o \
	server/playback.o \
	server/mediaframe.o \
	server/mediaresampler.o \
	server/audioinput_alsa.o \
	server/audioinput_ffmpeg.o \
	server/audioinput_sp.o \
	server/audioinput_ffrf.o \
	server/audiooutput_alsa.o \

CLIENT_OBJS := \
	client/main.o \
	client/ffrf.o \
	client/uriplayer.o \

SHARED_OBJS := \
	shared/log.o \
	shared/util.o \
	shared/bdp/bdp.o \
	shared/tcp.o \

ALL_OBJS := $(SERVER_OBJS) $(CLIENT_OBJS) $(SHARED_OBJS)

ALL_DEPS := $(patsubst %.o,%.d,$(ALL_OBJS))

all: banjo3-server banjo3-client

clean:
	@echo "Cleaning"
	@rm -f banjo3-server banjo3-client $(ALL_OBJS) $(ALL_DEPS)

banjo3-server: $(SERVER_OBJS) $(SHARED_OBJS)
	@echo "LD $@"
	@$(CXX) $(CFLAGS) -o $@ $^ $(LDFLAGS)

banjo3-client: $(CLIENT_OBJS) $(SHARED_OBJS)
	@echo "LD $@"
	@$(CXX) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server/%.o:server/%.c
	@$(CC) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $(SERVER_CFLAGS) $<
	@echo "CC  $<"
	@$(CC) $(CFLAGS) $(SERVER_CFLAGS) -c -o $@ $<

%.o:%.c
	@$(CC) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $<
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

server/%.o:server/%.cpp
	@$(CXX) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $(SERVER_CFLAGS) $<
	@echo "CXX $<"
	@$(CXX) $(CFLAGS) $(SERVER_CFLAGS) -c -o $@ $<

client/%.o:client/%.cpp
	@$(CXX) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $(CLIENT_CFLAGS) $<
	@echo "CXX $<"
	@$(CXX) $(CFLAGS) $(CLIENT_CFLAGS) -c -o $@ $<

%.o:%.cpp
	@$(CXX) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $<
	@echo "CXX $<"
	@$(CXX) $(CFLAGS) -c -o $@ $<

ifneq ("$(MAKECMDGOALS)","clean")
-include $(ALL_DEPS)
endif
