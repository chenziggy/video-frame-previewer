#include <stdio.h>

#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include "preview.h"

int scale_frame(struct SwsContext *img_convert_ctx, AVFrame *frame, AVFrame *frameRGB)
{
	// 1 先进行转换,  YUV420=>RGB24:
	int w = frame->width;
	int h = frame->height;

	// 计算给定图像参数和像素格式的图片数据所需的内存大小
	int numBytes = avpicture_get_size(AV_PIX_FMT_BGR24, w, h);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)frameRGB, buffer, AV_PIX_FMT_BGR24, w, h);

	// av_image_fill_arrays()

	sws_scale(img_convert_ctx, frame->data, frame->linesize,
						0, h, frameRGB->data, frameRGB->linesize);

	return numBytes;
}

int gen_frame_from_pkt(AVCodecContext *avctx, AVFrame *frame, AVPacket *pkt)
{
	// 使用 avcodec_send_packet() 将 AVPacket 发送到解码器
	if (avcodec_send_packet(avctx, pkt))
	{
		    printf("%s %d avcodec_send_packet fail\n", __func__, __LINE__);
		return -1;
	}
	// 使用 avcodec_receive_frame() 接收解码器解码后的 AVFrame
	int ret = avcodec_receive_frame(avctx, frame);
	if (ret < 0)
	{
		if (ret == AVERROR(EAGAIN))
			printf("111");
		if (ret == AVERROR_EOF)
			printf("222");
		        printf("%s %d avcodec_receive_frame fail, ret: %d\n", __func__, __LINE__, ret);
		return -1;
	}

	return 0;
}

PreviewResult *decode_sample(SampleData sampleDataArr[], int count, int width, int height)
{
	// 解码前的数据一般都是由 AVPacket 存储
	AVPacket av_pkt;
	// 解码后的数据则由 AVFrame 存储
	AVFrame *av_frame;
	// 用于存储图像缩放、色彩空间转换等视频图像处理相关的上下文信息
	struct SwsContext *sws_ctx;
	// 编解码器
	AVCodec *av_codec;
	// 描述一个编解码器的上下文结构体
	AVCodecContext *avcodec_ctx;
	struct AVCodecParameters av_codec_params;
	int ret;
	int frame_count;
	PreviewResult result;
	AVFrame *resultFrame;

	av_init_packet(&av_pkt);
	/*av_pkt.data = sampleData;
	av_pkt.size = sampleDataSize;
	av_pkt.duration = 1000;
	av_pkt.pos = 0;*/

	av_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!av_codec)
	{
		printf("initialize codec failed!");
	}

	avcodec_ctx = avcodec_alloc_context3(NULL);
	if (!avcodec_ctx)
	{
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	av_codec_params.codec_type = AVMEDIA_TYPE_UNKNOWN;
	av_codec_params.codec_id = AV_CODEC_ID_H264;
	// av_codec_params.codec_tag = 828601953;
	// av_codec_params.format = AV_PIX_FMT_YUV420P;
	av_codec_params.width = width;
	av_codec_params.height = height;
	// av_codec_params.bit_rate = 29347;
	// av_codec_params.bits_per_coded_sample = 24;
	// av_codec_params.bits_per_raw_sample = 8;
	// 最大分辨率和帧率等参数符合H.264标准中level 3.0的要求
	av_codec_params.level = 30;
	// av_codec_params.sample_aspect_ratio = (AVRational){1, 1};
	av_codec_params.field_order = AV_FIELD_UNKNOWN;
	av_codec_params.color_range = AVCOL_RANGE_UNSPECIFIED;
	av_codec_params.color_primaries = AVCOL_PRI_UNSPECIFIED;
	av_codec_params.color_trc = AVCOL_TRC_UNSPECIFIED;
	av_codec_params.color_space = AVCOL_SPC_UNSPECIFIED;
	av_codec_params.chroma_location = AVCHROMA_LOC_LEFT;
	av_codec_params.video_delay = 0;
	if (avcodec_parameters_to_context(avcodec_ctx, &av_codec_params) < 0)
	{
		printf("copy params failed!");
	}

	ret = avcodec_open2(avcodec_ctx, av_codec, NULL);
	if (ret != 0)
	{
		printf("open codec failed!");
	}

	sws_ctx = sws_getContext(avcodec_ctx->width,
													 avcodec_ctx->height,
													 avcodec_ctx->pix_fmt,
													 avcodec_ctx->width,
													 avcodec_ctx->height,
													 AV_PIX_FMT_RGB24,
													 SWS_BICUBIC, NULL, NULL, NULL);

	av_frame = av_frame_alloc();

	for (int i = 0; i < count; i++)
	{
		av_pkt.data = sampleDataArr[i].data;
		av_pkt.size = sampleDataArr[i].size;
		av_pkt.dts = sampleDataArr[i].dts;
		printf("pkt info: %p %d %d\n", av_pkt.data, av_pkt.size, av_pkt.dts);
		if (gen_frame_from_pkt(avcodec_ctx, av_frame, &av_pkt) < 0)
		{
			printf("Failed to generate frame");
			goto unref;
		}
		printf("gen frame succes %d\n", i);
		av_packet_unref(&av_pkt);
	}

	resultFrame = av_frame_alloc();
	result.size = scale_frame(sws_ctx, av_frame, resultFrame);
	result.frameData = resultFrame->data[0];

unref:
	av_packet_unref(&av_pkt);
	sws_freeContext(sws_ctx);
	avcodec_free_context(&avcodec_ctx);
	av_frame_free(&av_frame);

	return &result;
}
