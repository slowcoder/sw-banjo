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
#define LOGLEVEL LOGLEVEL_DEBUG
#include "shared/log.h"
#include "shared/util.h"

#include "audioinput_ffmpeg.h"

typedef struct mediainputffmpegctx {
	AVFormatContext *fmt_ctx;
	AVPacket *pkt;
	AVCodecContext *audio_dec_ctx;

	int audio_stream_idx;
	AVStream *audio_stream;
	AVFrame *frame;
} mediainputffmpegctx_t;

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


AudioinputFFMPEG::~AudioinputFFMPEG() {
	close();
}

int  AudioinputFFMPEG::openstream(const char *pzURI) {
	int r;

	LOG("Opening..");

	if( !bHasNetworkInited ) {
		avformat_network_init();
		bHasNetworkInited = 1;
	}

	pCtx = (mediainputffmpegctx_t*)calloc(1,sizeof(mediainputffmpegctx_t));

	LOG("About to open \"%s\"",pzURI);
	r = avformat_open_input(&pCtx->fmt_ctx,
		pzURI,
		NULL,
		NULL);
	if( r != 0 ) {
		char errstr[128];

		av_make_error_string(errstr,127,r);

		LOGE("av_open_input() = %s (%i)",errstr,r);
		goto err_exit;
	}

	/* retrieve stream information */
	if( avformat_find_stream_info(pCtx->fmt_ctx, NULL) < 0 ) {
		LOGE("Could not find stream information");
		goto err_exit;
	}

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

	return 0;
err_exit:
	close();
	return -1;
}

void AudioinputFFMPEG::close(void) {
	if( pCtx == NULL ) {
		LOGE("Told to close when context is NULL");
		return;
	}

	if( pCtx->fmt_ctx != NULL ) avformat_close_input(&pCtx->fmt_ctx);
	if( pCtx->pkt != NULL )     av_packet_free(&pCtx->pkt);
	if( pCtx->frame != NULL )   av_frame_free(&pCtx->frame);

	free(pCtx);
}
#if 0
static void handle_updated_metadata(mediainputffmpegctx_t *pCtx) {
	LOG("AVFormat METADATA UPDATED!");

	const AVDictionaryEntry *e = NULL;
	while ((e = av_dict_iterate(pCtx->fmt_ctx->metadata, e))) {
		LOG("Metadata: \"%s\" = \"%s\"",e->key,e->value);
	}
	return;

	AVDictionaryEntry *pEntry;
	pEntry = av_dict_get(pCtx->fmt_ctx->metadata,"title",NULL,0);
	if( pEntry == NULL ) {
		LOGE("No title found");
		return;
	}

	LOG("Title: \"%s\"",pEntry->value);

//	pCtx->fmt_ctx->metadata;
}
#endif
mediaframe_t *AudioinputFFMPEG::getNextFrame(void) {
	int r;

	if( pCtx == NULL ) {
		LOGE("Told to process when context is NULL");
		return NULL;
	}

//needmoreframes:
	// Read frames until we get a frame from our stream, or the stream ends
	do {
		r = av_read_frame(pCtx->fmt_ctx, pCtx->pkt);
		if( r < 0 ) { // End of stream?
			LOGE("Reached end of stream?");
			return NULL;
		}

		if( pCtx->fmt_ctx->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED ) {

			//handle_updated_metadata(pCtx);

//			pCtx->fmt_ctx->event_flags &= ~(AVFMT_EVENT_FLAG_METADATA_UPDATED);
		} 

		if( pCtx->pkt->stream_index != pCtx->audio_stream_idx ) {
			LOGD("%i != %i",pCtx->pkt->stream_index,pCtx->audio_stream_idx);
			av_packet_unref(pCtx->pkt);
		}

	} while( pCtx->pkt->stream_index != pCtx->audio_stream_idx );

	// Submit the packet to the decoder;
	AVCodecContext *dec = pCtx->audio_dec_ctx;

	r = avcodec_send_packet(dec,pCtx->pkt);
	if( r < 0 ) {
		char tmpbuf[256];
		av_strerror(r,tmpbuf,255);

		LOGE("Error sumbitting a packet for decoding: %s",tmpbuf);
		return NULL;
	}

	av_packet_unref(pCtx->pkt);


#if 0
	r = avcodec_receive_frame(dec,pCtx->frame);
	// those two return values are special and mean there is no output
	// frame available, but there were no errors during decoding
	if( r == AVERROR_EOF || r == AVERROR(EAGAIN) ) {
		char tmpbuf[256];
		av_strerror(r,tmpbuf,255);
		LOGE("RecFrame: %s",tmpbuf);
		goto needmoreframes;
	}
#else

	// Get a frame from the decoder (if there is one)
	// NOTE: We make the assumption that an encoded frame
	//       will only generate <=0 decoded frames, which
	//       seems to hold true for all tested audio codec.
	//       This is _not_ true to video codecs

	do {
		r = avcodec_receive_frame(dec,pCtx->frame);
		// those two return values are special and mean there is no output
		// frame available, but there were no errors during decoding
		if( r == AVERROR_EOF || r == AVERROR(EAGAIN) ) {
			char tmpbuf[256];
			av_strerror(r,tmpbuf,255);
			LOGE("RecFrame: %s",tmpbuf);
		}
	} while( r == AVERROR_EOF || r == AVERROR(EAGAIN) );
#endif
	if( dec->codec->type != AVMEDIA_TYPE_AUDIO ) {
		LOGW("Got a non-audio frame from an audio stream?");
		return NULL;
	}

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

	av_frame_unref(pCtx->frame);

	if( pRetFrame->numChannels == 1 ) {
		upmix(pRetFrame);
	}

	return pRetFrame;
}
