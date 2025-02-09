CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc

PKGCONFIG ?= pkg-config

CFLAGS := -g -Wall -I.
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
	server/tcp_server.o \
	server/ipc.o \
	server/log.o \
	server/playback.o \
	server/media_frame.o \
	server/mediainput_avcodec.o \
	server/mediaresampler.o \
	server/mediaoutput_alsa.o \
	server/mediainput_alsa.o \

CLIENT_OBJS := \
	client/main.o \
	client/tcp_client.o \

#LIBBANJO_OBJS := \
#	libbanjo/libbanjo.o \
#	libbanjo/tcp.o

ALL_OBJS := $(SERVER_OBJS) $(CLIENT_OBJS) $(LIBBANJO_OBJS)

ALL_DEPS := $(patsubst %.o,%.d,$(ALL_OBJS))

all: banjo-server banjo-client

clean:
	rm -f banjo-server banjo-client $(ALL_OBJS) $(ALL_DEPS)

banjo-server: $(SERVER_OBJS) $(LIBBANJO_OBJS)
	@echo "LD $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

banjo-client: $(CLIENT_OBJS) $(LIBBANJO_OBJS)
	@echo "LD $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o:%.c
	@$(CC) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $<
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

ifneq ("$(MAKECMDGOALS)","clean")
-include $(ALL_DEPS)
endif
