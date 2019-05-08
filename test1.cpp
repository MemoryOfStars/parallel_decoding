/** 
*  使用FFmpeg解析出H264、YUV数据 
*/  
  
#include <stdio.h>  
  
extern "C"  
{  
#include "libavcodec/avcodec.h"  
#include "libavformat/avformat.h"  
#include "libswscale/swscale.h"  
#include "libavutil/imgutils.h"  
};  
  
#pragma comment(lib, "avcodec.lib")  
#pragma comment(lib, "avformat.lib")  
#pragma comment(lib, "swscale.lib")  
#pragma comment(lib, "avutil.lib") 
#pragma pack (1)


typedef struct hiFrameInfo
{
    char type;
    int  len;
    int vid;                       //Video ID
    int fid;                       //Frame ID
}FrameInfo;



int main(int argc, char* argv[])  
{  
    AVFormatContext     *pFormatCtx = NULL;  
    AVCodecContext      *pCodecCtx = NULL;  
    AVCodec             *pCodec = NULL;  
    AVFrame             *pFrame = NULL, *pFrameYUV = NULL;  
    unsigned char       *out_buffer = NULL;  
    AVPacket            packet;  
    struct SwsContext   *img_convert_ctx = NULL;  
    int                 got_picture;  
    int                 videoIndex;  
    int                 frame_cnt = 1;  
  
    //char filepath[] = "Titanic.ts";  
    char filepath[] = "0.mkv";  
  
    FILE *fp_yuv = fopen("film.yuv", "wb+");  
    FILE *fp_h264 = fopen("film.h264", "wb+");
    FILE *fp_y = NULL;  
    if (fp_yuv == NULL || fp_h264 == NULL)  
    {  
        printf("FILE open error");  
        return -1;
    }  
  
    av_register_all();  
  
    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0){  
        printf("Couldn't open an input stream.\n");  
        return -1;  
    }  
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0){  
        printf("Couldn't find stream information.\n");  
        return -1;  
    }  
    videoIndex = -1;  
    for (int i = 0; i < pFormatCtx->nb_streams; i++)  
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){  
            videoIndex = i;  
            break;  
        }
  
    if (videoIndex == -1){  
        printf("Couldn't find a video stream.\n");  
        return -1;  
    }
  
    pCodecCtx = pFormatCtx->streams[videoIndex]->codec;  
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);  
    if (pCodec == NULL){  
        printf("Codec not found.\n");  
        return -1;  
    }  
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0){  
        printf("Could not open codec.\n");  
        return -1;  
    }  
 

    pFrame = av_frame_alloc();  
    pFrameYUV = av_frame_alloc();
    if (pFrame == NULL || pFrameYUV == NULL)  
    {  
        printf("memory allocation error\n");  
        return -1;  
    }  
  
    /** 
    *  RGB--------->AV_PIX_FMT_RGB24 
    *  YUV420P----->AV_PIX_FMT_YUV420P 
    *  UYVY422----->AV_PIX_FMT_UYVY422 
    *  YUV422P----->AV_PIX_FMT_YUV422P 
    */  
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));  
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,  
        AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);  
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,  
        pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);  
  
  
  
  
    /* 
    //针对H.264码流 
    unsigned char *dummy = NULL;   //输入的指针   
    int dummy_len; 
    const char nal_start[] = { 0, 0, 0, 1 }; 
    AVBitStreamFilterContext* bsfc = av_bitstream_filter_init("h264_mp4toannexb"); 
    av_bitstream_filter_filter(bsfc, pCodecCtx, NULL, &dummy, &dummy_len, NULL, 0, 0); 
    fwrite(pCodecCtx->extradata, pCodecCtx->extradata_size, 1, fp_h264); 
    av_bitstream_filter_close(bsfc); 
    free(dummy); 
    */  

   int Icounter = 0;
   char devidedByI[30];
   FrameInfo * frame_info = (FrameInfo *)malloc(sizeof(FrameInfo));
   int curFid = 0;
   int curVid = 0;

    while (av_read_frame(pFormatCtx, &packet) >= 0)  
    {  
        if (packet.stream_index == videoIndex)                                     
        {  
            //输出出h.264数据
            if(packet.flags & AV_PKT_FLAG_KEY)
            {//当前帧是关键帧
                if(fp_y != NULL)
                {
                    fclose(fp_y);                                                     //关闭上一个文件
                }
                sprintf(devidedByI,"h264/IFrame%d.h264",++Icounter);                  //按帧分割的H264文件
                fp_y = fopen(devidedByI,"wb+");

                curFid = 0;
                frame_info->fid  = curFid;
                frame_info->vid  = curVid ++;                                         //每遇到关键帧，换到下一个Vid
                frame_info->type = 'I';
                frame_info->len  = packet.size;

                // fwrite(&(frame_info->fid),  1, sizeof(int), fp_y);
                // fwrite(&(frame_info->vid),  1, sizeof(int), fp_y);
                // fwrite(&(frame_info->type), 1, sizeof(char), fp_y);
                fwrite(&(frame_info->len),  1, sizeof(int), fp_y);

                fwrite(packet.data, 1, packet.size, fp_y);

                printf("成功提取%d个关键帧\n",Icounter);
            }
            else
            {//当前帧不是关键帧

                frame_info->fid  = ++curFid;
                //frame_info->vid  = curVid;
                frame_info->type = 'N';
                frame_info->len  = packet.size;

                // fwrite(&(frame_info->fid),  1, sizeof(int), fp_y);
                // fwrite(&(frame_info->vid),  1, sizeof(int), fp_y);
                // fwrite(&(frame_info->type), 1, sizeof(char), fp_y);
                fwrite(&(frame_info->len),  1, sizeof(int), fp_y);
                fwrite(packet.data, 1, packet.size, fp_y);
            }
            if(fp_h264 != NULL)
            {
                printf("打开文件成功\n");
                fwrite(packet.data,1,packet.size,fp_h264);
            }

            //fwrite(packet.data,1,packet->size,fp_y);
            //针对H.264码流  
            //fwrite(nal_start, 4, 1, fp_h264);  
            //fwrite(packet.data + 4, packet.size - 4, 1, fp_h264);  
  
            if (avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, &packet) < 0)  
            {  
                printf("Decode Error.\n");  
                return -1;  
            }  
            if (got_picture)  
            {  

                sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,  
                    pFrameYUV->data, pFrameYUV->linesize);  
  
                //输出出YUV数据  
                int y_size = pCodecCtx->width * pCodecCtx->height;  
                //fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);       //Y   
                //fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);   //U  
                //fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);   //V  
                  
                printf("Succeed to decode %d frame!\n", frame_cnt);  
                frame_cnt++;  
            }  
        }  
        av_free_packet(&packet);  
    }  
  
    //flush decoder  
    //FIX: Flush Frames remained in Codec  
    
  
  
    fclose(fp_yuv);  
    fclose(fp_h264);  
    sws_freeContext(img_convert_ctx);  
    av_free(out_buffer);  
    av_frame_free(&pFrameYUV);  
    av_frame_free(&pFrame);  
    avcodec_close(pCodecCtx);  
    avformat_close_input(&pFormatCtx);  
  
    return 0;  
}  