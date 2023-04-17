#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

int main(int argc, char *argv[])
{
  AVFormatContext *pFormatCtx = NULL;         // 输入视频文件格式的上下文结构体
  int i, videoStream;                         // 视频流的索引
  AVCodecParameters *pCodecParameters = NULL; // 视频流的编解码参数
  AVCodec *pCodec = NULL;                     // 视频解码器
  AVFrame *pFrame = NULL;                     // 一帧视频数据
  AVPacket packet;                            // 视频数据包
  int frameFinished;                          // 一帧视频数据是否读取完毕
  int numBytes;                               // 存储输出图片数据的字节数
  uint8_t *buffer = NULL;                     // 存储输出图片数据的缓冲区
  int width, height;                          // 视频宽高
  struct SwsContext *sws_ctx = NULL;          // 颜色空间转换上下文
  AVPixelFormat pix_fmt = AV_PIX_FMT_RGB24;   // 输出图片的像素格式

  // 检查命令行参数是否正确
  if (argc < 3)
  {
    printf("Usage: %s <input_file> <output_prefix>\n", argv[0]);
    return -1;
  }

  // 打开视频文件
  if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
  {
    printf("Couldn't open input file.\n");
    return -1;
  }

  // 查找视频流信息
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
  {
    printf("Couldn't find stream information.\n");
    return -1;
  }

  // 输出输入文件信息
  av_dump_format(pFormatCtx, 0, argv[1], 0);

  // 查找第一个视频流
  videoStream = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++)
  {
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      videoStream = i;
      break;
    }
  }

  // 检查视频流是否存在
  if (videoStream == -1)
  {
    printf("Couldn't find a video stream.\n");
    return -1;
  }

  // 获取视频流的编解码参数
  pCodecParameters = pFormatCtx->streams[videoStream]->codecpar;

  // 查找视频解码器
  pCodec = avcodec_find_decoder(pCodecParameters->codec_id);
  if (pCodec == NULL)
  {
    printf("Codec not found.\n");
    return -1;
  }

  // 分配视频解码器上下文
  AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);

  // 将视频流的编解码参数赋值给解码器上
  if ((ret = avcodec_parameters_to_context(pCodecCtx, pStream->codecpar)) < 0)
  {
    printf("Failed to copy codec parameters to decoder context!\n");
    goto end;
  }

  // 打开解码器
  if ((ret = avcodec_open2(pCodecCtx, pCodec, NULL)) < 0)
  {
    printf("Failed to open codec!\n");
    goto end;
  }

  // 为解码器分配帧缓存
  pFrame = av_frame_alloc();
  if (!pFrame)
  {
    printf("Failed to allocate frame!\n");
    goto end;
  }

  // 为RGB图像分配帧缓存
  pFrameRGB = av_frame_alloc();
  if (!pFrameRGB)
  {
    printf("Failed to allocate RGB frame!\n");
    goto end;
  }

  // 确定所需的缓存大小并分配内存
  numBytes = av_image_get_buffer_size(
      AV_PIX_FMT_RGB24, pCodecCtx->width,
      pCodecCtx->height, 1);
  buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

  // 使用分配的缓存设置pFrameRGB，用于保存转换后的RGB图像
  av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize,
                       buffer, AV_PIX_FMT_RGB24,
                       pCodecCtx->width, pCodecCtx->height, 1);

  // 初始化SwsContext，用于颜色转换
  sws_ctx = sws_getContext(
      pCodecCtx->width, pCodecCtx->height,
      pCodecCtx->pix_fmt, pCodecCtx->width,
      pCodecCtx->height, AV_PIX_FMT_RGB24,
      SWS_BILINEAR, NULL, NULL, NULL);

  // 从输入文件读取数据，并循环解码每一帧
  while (av_read_frame(pFormatCtx, &packet) >= 0)
  {
    // 如果数据包的流不是视频流，则跳过
    if (packet.stream_index != video_stream_index)
    {
      av_packet_unref(&packet);
      continue;
    }

    // 解码数据包
    ret = avcodec_send_packet(pCodecCtx, &packet);
    if (ret < 0)
    {
      printf("Error sending packet to decoder!\n");
      av_packet_unref(&packet);
      break;
    }

    // 循环从解码器中取出解码后的数据
    while (ret >= 0)
    {
      ret = avcodec_receive_frame(pCodecCtx, pFrame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if (ret < 0)
      {
        printf("Error during decoding!\n");
        goto end;
      }

      // 转换颜色空间
      sws_scale(sws_ctx, (const uint8_t *const *)pFrame->data,
                pFrame->linesize, 0, pCodecCtx->height,
                pFrameRGB->data, pFrameRGB->linesize);

      // 将转换后的RGB图像保存为文件
      save_frame_as_ppm(pFrameRGB, pCodecCtx->width, pCodecCtx->height, frame_count++);

      // 清空帧缓存
      av_frame_unref(pFrame);
    }

    // 清空数据包
    av_packet_unref(&packet);
  }