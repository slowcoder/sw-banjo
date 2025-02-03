#include <stdlib.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>

#include "log.h"

#include "mediainput_avcodec.h"

typedef struct mediainput {
	AVFormatContext *fmt_ctx;
	AVPacket *pkt;
	AVCodecContext *audio_dec_ctx;

	int audio_stream_idx;
	AVStream *audio_stream;
	AVFrame *frame;
} mediainput_t;

static int bHasNetworkInitied = 0;

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
		*stream_idx = stream_index;
    }
 
	return 0;
}


struct mediainput *mediainput_avcodec_openstream(const char *pzURI) {
	mediainput_t *pCtx;
	int r;

	if( !bHasNetworkInitied ) {
		avformat_network_init();
		bHasNetworkInitied = 1;
	}

	pCtx = (mediainput_t*)calloc(1,sizeof(mediainput_t));

	r = avformat_open_input(&pCtx->fmt_ctx,
		pzURI,
		NULL,
		NULL);
	if( r != 0 ) {
		LOGE("av_open_input() = %s (%i)",av_err2str(r),r);
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

	return pCtx;
err_exit:
	mediainput_avcodec_close(pCtx);
	return NULL;
}

void               mediainput_avcodec_close(struct mediainput *pCtx) {
	if( pCtx == NULL ) {
		LOGE("Told to close when context is NULL");
		return;
	}

	if( pCtx->fmt_ctx != NULL ) avformat_close_input(&pCtx->fmt_ctx);
	if( pCtx->pkt != NULL )     av_packet_free(&pCtx->pkt);
	if( pCtx->frame != NULL )   av_frame_free(&pCtx->frame);
}

mediaframe_t      *mediainput_avcodec_getnextframe(struct mediainput *pCtx) {
	int r;

	if( pCtx == NULL ) {
		LOGE("Told to process when context is NULL");
		return NULL;
	}

	// Read frames until we get a frame from our stream, or the stream ends
	do {
		r = av_read_frame(pCtx->fmt_ctx, pCtx->pkt);
		if( r < 0 ) { // End of stream?
			LOGE("Reached end of stream?");
			return NULL;
		}

		if( pCtx->pkt->stream_index != pCtx->audio_stream_idx ) {
			av_packet_unref(pCtx->pkt);
		}

	} while( pCtx->pkt->stream_index != pCtx->audio_stream_idx );

	// Submit the packet to the decoder;
	AVCodecContext *dec = pCtx->audio_dec_ctx;

	if( avcodec_send_packet(dec,pCtx->pkt) < 0 ) {
		LOGE("Error sumbitting a packet for decoding");
		return NULL;
	}

//	av_packet_free(&pCtx->pkt);
	av_packet_unref(pCtx->pkt);


	// Get a frame from the decoder (if there is one)
	// NOTE: We make the assumption that an encoded frame
	//       will only generate <=0 decoded frames, which
	//       seems to hold true for all tested audio codec.
	//       This is _not_ true to video codecs

	do {
		r = avcodec_receive_frame(dec,pCtx->frame);
		// those two return values are special and mean there is no output
		// frame available, but there were no errors during decoding
	} while( r == AVERROR_EOF || r == AVERROR(EAGAIN) );

	if( dec->codec->type != AVMEDIA_TYPE_AUDIO ) {
		LOGW("Got a non-audio frame from an audio stream?");
		return NULL;
	}

	eMediasamplefmt mediasamplefmt = 0;

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
	pRetFrame->nb_samples = pCtx->frame->nb_samples;
	pRetFrame->channels   = dec->ch_layout.nb_channels;
	pRetFrame->rate       = pCtx->frame->sample_rate;

	if( av_sample_fmt_is_planar(dec->sample_fmt) ) {
		deplanar(pRetFrame,pCtx->frame,dec->sample_fmt);
		pRetFrame->is_planar = 0;
	} else {
		pRetFrame->is_planar = 0;
		pRetFrame->data[0]   = malloc(pCtx->frame->linesize[0]);
		memcpy(pRetFrame->data[0],pCtx->frame->extended_data[0],pCtx->frame->linesize[0]);		
	}

	av_frame_unref(pCtx->frame);

	return pRetFrame;
}
