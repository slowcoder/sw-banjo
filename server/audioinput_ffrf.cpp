#include <stdlib.h>
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>
}

//#define LOGLEVEL LOGLEVEL_DEBUG
#include "shared/log.h"
#include "shared/ffrf-protocol.h"

#include "audioinput_ffrf.h"

typedef struct ffrfctx {
	TCP *pTCP;

	AVFormatContext *fmt_ctx;
	AVPacket *pkt;
	AVCodecContext *audio_dec_ctx;

	int audio_stream_idx;
	AVStream *audio_stream;
	AVFrame *frame;

} ffrfctx_t;

static int readFunction(void* opaque, uint8_t* buf, int buf_size) {
	ffrpframe_t frame;
	ffrfctx_t *pCtx = (ffrfctx_t*)opaque;
	int r;

	LOGD("read:  %p,%p,%i",opaque,buf,buf_size);

	memset(&frame,0,sizeof(ffrpframe_t));

	frame.hdr.cmd = FFRPCMD_READ;
	frame.hdr.payloadlen = sizeof(frame.readcmd);
	frame.readcmd.len = buf_size;

	r = pCtx->pTCP->send(&frame,sizeof(frame.hdr) + frame.hdr.payloadlen);
	if( r < 0 ) return -1;

	int32_t rb;
	pCtx->pTCP->recv(&rb,sizeof(int32_t),1000);
	if( rb > 0 ) {
		r = pCtx->pTCP->recv(buf,rb,1000);
		LOGD("  returning %i",r);
		return r;
	}

//	return rb;

	return -2;
}

static int64_t seekFunction(void* opaque, int64_t offset, int whence) {
	uint8_t ffrfwhence;
	ffrpframe_t frame;
	ffrfctx_t *pCtx = (ffrfctx_t*)opaque;

	LOGD("seek:  %p,%i,%i",opaque,offset,whence);

	if( whence & AVSEEK_FORCE ) return -1;
	if( whence & AVSEEK_SIZE ) return -1;

	switch(whence) {
	case SEEK_SET: ffrfwhence = FFRPWHENCE_SET; break;
	case SEEK_CUR: ffrfwhence = FFRPWHENCE_CUR; break;
	case SEEK_END: ffrfwhence = FFRPWHENCE_END; break;
	default:
		ASSERT(0,"Unsupported FFmpeg whence %i",whence);
	}

	memset(&frame,0,sizeof(ffrpframe_t));

	frame.hdr.cmd = FFRPCMD_SEEK;
	frame.hdr.payloadlen = sizeof(frame.seekcmd);
	frame.seekcmd.offset = offset;
	frame.seekcmd.whence = ffrfwhence;

	pCtx->pTCP->send(&frame,sizeof(frame.hdr) + frame.hdr.payloadlen);

	int32_t rb;
	pCtx->pTCP->recv(&rb,sizeof(int32_t),1000);

	return rb;
}

static int media_open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type) {
	int ret, stream_index;
	AVStream *st;
	const AVCodec *dec = NULL;
 
	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) {
		LOGE("Could not find %s stream in input",av_get_media_type_string(type) );
		return ret;
	} else {
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];
 
		/* find decoder for the stream */
		dec = avcodec_find_decoder(st->codecpar->codec_id);
		if( !dec ) {
			LOGE("Failed to find %s codec",av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if( !*dec_ctx ) {
			LOGE("Failed to allocate the %s codec context",av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}
 
		/* Copy codec parameters from input stream to output codec context */
		if( (ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0 ) {
			LOGE("Failed to copy %s codec parameters to decoder context",av_get_media_type_string(type));
			return ret;
		}
 
        /* Init the decoders */
		if( (ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0 ) {
			LOGE("Failed to open %s codec",av_get_media_type_string(type));
			return ret;
		}
		LOG("Stream %i seems to be audio?",stream_index);
		*stream_idx = stream_index;
    }
 
	return 0;
}

int AudioinputFFRF::updateMeta(mediaframe_t *pFrame) {
	ffrpframe_t netframe;

	if( pFrame == NULL ) return -1;

	// No need to send the same info twice
	if( (pFrame->pts == metacache.pts) && (pFrame->duration == metacache.duration) ) {
		return 0;
	}
	metacache.pts = pFrame->pts;
	metacache.duration = pFrame->duration;

	memset(&netframe,0,sizeof(ffrpframe_t));

	netframe.hdr.cmd = FFRPSTATUS_PROGRESS;
	netframe.hdr.payloadlen = sizeof(netframe.progressstatus);
	netframe.progressstatus.pts = pFrame->pts;
	netframe.progressstatus.duration = pFrame->duration;

	pCtx->pTCP->send(&netframe,sizeof(netframe.hdr) + netframe.hdr.payloadlen);

	return 0;
}

AudioinputFFRF::AudioinputFFRF() {

//	pTCP = new TCP();

	//pCtx = (ffrfctx_t*)calloc(1,sizeof(ffrfctx_t));

	metacache.pts = metacache.duration = 0;
}

AudioinputFFRF::~AudioinputFFRF() {
	close();
//	delete pTCP;
}

int AudioinputFFRF::handleConnection(TCP *pTCP) {
	int r = 0;

	pCtx = (ffrfctx_t*)calloc(1,sizeof(ffrfctx_t));
	pCtx->pTCP = pTCP;

	const int ioBufferSize = 32768 * 2;
	unsigned char *ioBuffer = (unsigned char *)av_malloc(ioBufferSize + AV_INPUT_BUFFER_PADDING_SIZE); // can get av_free()ed by libav
	AVIOContext *avioContext = avio_alloc_context(	ioBuffer,
													ioBufferSize,
													0, /* Writeable? */
													pCtx /* Opaque */,
													&readFunction,
													NULL, /* writeFunction */
													&seekFunction);

#if 1
	AVFormatContext *pFmtCtx = avformat_alloc_context();
	pFmtCtx->pb = avioContext;

	avformat_open_input(&pFmtCtx, "dummyFileName", NULL, NULL);
	pCtx->fmt_ctx = pFmtCtx;
#endif

	/* retrieve stream information */
	if( avformat_find_stream_info(pCtx->fmt_ctx, NULL) < 0 ) {
		LOGE("Could not find stream information");
		goto err_exit;
	}

	LOGD(">> Duration: %lli seconds",pCtx->fmt_ctx->duration / AV_TIME_BASE);

	/* open codec */
	if( media_open_codec_context(&pCtx->audio_stream_idx, &pCtx->audio_dec_ctx, pCtx->fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0 ) {
		pCtx->audio_stream = pCtx->fmt_ctx->streams[pCtx->audio_stream_idx];
	}

	pCtx->frame = av_frame_alloc();
	if( pCtx->frame == NULL ) {
		LOGE("Could not allocate frame");
		goto err_exit;
	}

	pCtx->pkt = av_packet_alloc();
	if( pCtx->pkt == NULL ) {
		LOGE("Could not allocate packet");
		goto err_exit;
	}

	LOG("Opened..");

	return r;
err_exit:
	close();
	return -1;
}

int AudioinputFFRF::openPort(uint16_t port) {
#if 1
	ASSERT(0,"Broken function");
	return -1;
#else

	pTCP->listen(port);
	LOG("Waiting for connection on port %u",port);
	pTCP->accept();

	return handleConnection(pTCP);
#endif
}

void AudioinputFFRF::close(void) {
	if( pCtx == NULL ) {
		LOGE("Told to close when context is NULL");
		return;
	}

	if( pCtx->fmt_ctx != NULL ) avformat_free_context(pCtx->fmt_ctx);
	if( pCtx->pkt != NULL )     av_packet_free(&pCtx->pkt);
	if( pCtx->frame != NULL )   av_frame_free(&pCtx->frame);

	ffrpframe_t frame;
	frame.hdr.cmd = FFRPCMD_DONE;
	frame.hdr.payloadlen = 0;
	pCtx->pTCP->send(&frame,sizeof(frame.hdr));

	pCtx->pTCP->close();

	free(pCtx);

	pCtx = NULL;
}

static void upmix(mediaframe_t *pFrame) {
	int i;

	if( pFrame->fmt == eMediasamplefmt_S16 ) {
		int16_t *pSrc,*pDst;

		pSrc = (int16_t*)pFrame->data[0];
		pDst = (int16_t*)malloc(pFrame->numFrames * 2 * sizeof(int16_t));
		for(i=0;i<pFrame->numFrames;i++) {
			pDst[i*2+0] = pSrc[i];
			pDst[i*2+1] = pSrc[i];
		}
		free(pFrame->data[0]);
		pFrame->data[0] = pDst;
	} else {
		ASSERT(0,"Implement me. fmt=%i",pFrame->fmt);
	}
}

static void deplanar(mediaframe_t *pMediaOut,AVFrame *pAVIn,enum AVSampleFormat infmt) {
	int i;

	if( infmt == AV_SAMPLE_FMT_S16P ) { // Untested
		uint16_t *din[2],*dout;

		din[0] = (uint16_t*)pAVIn->extended_data[0];
		din[1] = (uint16_t*)pAVIn->extended_data[1];

		pMediaOut->data[0] = malloc(pAVIn->nb_samples*sizeof(uint16_t)*2);
		dout = (uint16_t*)pMediaOut->data[0];

		for(i=0;i<pAVIn->nb_samples;i++) {
			dout[i*2+0] = din[0][i];
			dout[i*2+1] = din[1][i];
		}
		pMediaOut->fmt = eMediasamplefmt_S16;
	} else if( infmt == AV_SAMPLE_FMT_FLTP ) {
		float *dout;
		float *din[2];

		din[0] = (float*)pAVIn->extended_data[0];
		din[1] = (float*)pAVIn->extended_data[1];

		pMediaOut->data[0] = malloc(pAVIn->nb_samples*sizeof(float)*2);
		dout = (float*)pMediaOut->data[0];

		for(i=0;i<pAVIn->nb_samples;i++) {
			dout[i*2+0] = din[0][i];
			dout[i*2+1] = din[1][i];
		}
		pMediaOut->fmt = eMediasamplefmt_F32;
	} else {
		ASSERT(0,"Unhandled deplaning format");
	}
}

void AudioinputFFRF::sendMetadataUpdate(void) {
	ffrpframe_t netframe;
	const AVDictionaryEntry *e = NULL;

	memset(&netframe,0,sizeof(ffrpframe_t));

	netframe.hdr.cmd = FFRPSTATUS_METADATA;
	netframe.hdr.payloadlen = sizeof(netframe.metadata);

	while ((e = av_dict_iterate(pCtx->fmt_ctx->metadata, e))) {
		if( strcasecmp(e->key,"TITLE") == 0 ) {
			strncpy(netframe.metadata.title,e->value,FFRPMETA_MAXSTR);
			strncpy(metacache.title,e->value,FFRPMETA_MAXSTR);
		} else if( strcasecmp(e->key,"ALBUM") == 0 ) {
			strncpy(netframe.metadata.album,e->value,FFRPMETA_MAXSTR);
			strncpy(metacache.album,e->value,FFRPMETA_MAXSTR);
		} else if( strcasecmp(e->key,"ARTIST") == 0 ) {
			strncpy(netframe.metadata.artist,e->value,FFRPMETA_MAXSTR);
			strncpy(metacache.artist,e->value,FFRPMETA_MAXSTR);
		}
	}

	LOG("Sending metadata update");
	pCtx->pTCP->send(&netframe,sizeof(netframe.hdr) + netframe.hdr.payloadlen);
}

int AudioinputFFRF::handleUpdatedMetadata(void) {
	LOGD("AVFormat METADATA UPDATED!");
	int dirty = 0;

	const AVDictionaryEntry *e = NULL;
	while ((e = av_dict_iterate(pCtx->fmt_ctx->metadata, e))) {
		LOGD("Metadata: \"%s\" = \"%s\"",e->key,e->value);

		if( strcasecmp(e->key,"TITLE") == 0 ) {
			if( strncmp(metacache.title,e->key,FFRPMETA_MAXSTR) != 0 ) dirty = 1;
		} else if( strcasecmp(e->key,"ARTIST") == 0 ) {
			if( strncmp(metacache.artist,e->key,FFRPMETA_MAXSTR) != 0 ) dirty = 1;
		}
		else if( strcasecmp(e->key,"ALBUM") == 0 ) {
			if( strncmp(metacache.album,e->key,FFRPMETA_MAXSTR) != 0 ) dirty = 1;			
		}
	}

	if( dirty ) sendMetadataUpdate();

	return 0;

	AVDictionaryEntry *pEntry;
	pEntry = av_dict_get(pCtx->fmt_ctx->metadata,"TITLE",NULL,0);
	if( pEntry == NULL ) {
		LOGE("No title found");
		return 0;
	}

	LOG("Title: \"%s\"",pEntry->value);

//	pCtx->fmt_ctx->metadata;
	return 0;
}

int AudioinputFFRF::tryGetNextFrame(mediaframe_t **ppRetFrame) {
	int r;

	r = av_read_frame(pCtx->fmt_ctx, pCtx->pkt);
	if( r < 0 ) {
		LOGE("Error reading frame");
		return -1;
	}
	if( pCtx->pkt->stream_index != pCtx->audio_stream_idx ) {
		LOG("%i != %i",pCtx->pkt->stream_index,pCtx->audio_stream_idx);
		av_packet_unref(pCtx->pkt);
		return 1;
	}

	// Got some metadata?
	if( pCtx->fmt_ctx->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED ) {
		handleUpdatedMetadata();
		pCtx->fmt_ctx->event_flags ^= AVFMT_EVENT_FLAG_METADATA_UPDATED;
	}

	AVCodecContext *dec = pCtx->audio_dec_ctx;

	r = avcodec_send_packet(dec,pCtx->pkt);
	if( r < 0 ) {
		LOGE("Error sending frame");
		return -1;
	}

	av_packet_unref(pCtx->pkt);

	r = avcodec_receive_frame(dec,pCtx->frame);
	// AVERROR(EAGAIN) = Need more input (avcodec_send_packet)
	// AVERROR_EOF     = Decoder is flushed. Nothing to output
	if( r == AVERROR_EOF || r == AVERROR(EAGAIN) ) {
		char tmpbuf[256];
		av_strerror(r,tmpbuf,255);
		LOGE("RecFrame: %s",tmpbuf);
		return 1;
	}

	if( dec->codec->type != AVMEDIA_TYPE_AUDIO ) {
		LOGW("Got a non-audio frame from an audio stream?");
		return 1;
	}

	return 0;
}

mediaframe_t *AudioinputFFRF::getNextFrame(void) {
	int r;

	if( pCtx == NULL ) {
		LOGE("Told to process when context is NULL");
		return NULL;
	}

	do {
		r = tryGetNextFrame(NULL);
		if( r < 0 ) return NULL;
	} while(r != 0); // tryGetNextFrame returns positive numbers for "retry"

	AVCodecContext *dec = pCtx->audio_dec_ctx;

	eMediasamplefmt mediasamplefmt = eMediasamplefmt_Invalid;

	switch(dec->sample_fmt) {
	case AV_SAMPLE_FMT_S16: mediasamplefmt = eMediasamplefmt_S16; break;
	case AV_SAMPLE_FMT_S32: mediasamplefmt = eMediasamplefmt_S32; break;
	case AV_SAMPLE_FMT_FLTP: mediasamplefmt = eMediasamplefmt_F32; break;
	default:
		LOGE("Unknown sampleformat %i",dec->sample_fmt);
		return NULL;
	}

	mediaframe_t *pRetFrame;

	pRetFrame = mediaframe_alloc();
	pRetFrame->fmt = mediasamplefmt;
	pRetFrame->numFrames   = pCtx->frame->nb_samples;
	pRetFrame->numChannels = dec->ch_layout.nb_channels;
	pRetFrame->rate        = pCtx->frame->sample_rate;

	if( av_sample_fmt_is_planar(dec->sample_fmt) ) {
		deplanar(pRetFrame,pCtx->frame,dec->sample_fmt);
		pRetFrame->bIsPlanar = 0;
	} else {
		pRetFrame->bIsPlanar = 0;
		pRetFrame->data[0]   = malloc(pCtx->frame->linesize[0]);
		memcpy(pRetFrame->data[0],pCtx->frame->extended_data[0],pCtx->frame->linesize[0]);		
	}

	int64_t pts;
	AVRational time_base = pCtx->fmt_ctx->streams[pCtx->audio_stream_idx]->time_base;

	pts = (pCtx->frame->pts * time_base.num) / time_base.den;

	pRetFrame->pts = pts;
	LOGD("pts=%lli  TB=%d/%d",pts,time_base.num,time_base.den);
	pRetFrame->duration = pCtx->fmt_ctx->duration / AV_TIME_BASE;

	av_frame_unref(pCtx->frame);

	if( pRetFrame->numChannels == 1 ) {
		upmix(pRetFrame);
	}

	return pRetFrame;
}
