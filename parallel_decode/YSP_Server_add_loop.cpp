#define __USE_XOPEN

#define TOTAL_GROUP 10
#define GROUP_ITEM_COUNT 4
////////////////////////////////////////////////////////
//                                                    //
//                                                    //
//              一定要准备好足够的文件!!!!                //
//                                                    //
//                                                    //
//                                                    //
////////////////////////////////////////////////////////
#include<fcntl.h>// 提供open()函数
#include<dirent.h>// 提供目录流操作函数
#include<sys/stat.h>// 提供属性操作函数
#include<sys/types.h>// 提供mode_t 类型
#include <pthread.h>
#include <string>
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <map>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <opencv2/core/core.hpp> 
#include <opencv2/highgui/highgui.hpp> 
#include <opencv2/opencv.hpp>

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")

using namespace std;
using namespace cv;

//定义全局变量
typedef struct {
	string filename;
	AVFormatContext* pFormatCtx;
	AVBitStreamFilterContext* h264bsfc;
	AVBitStreamFilterContext* h265bsfc;
	AVBitStreamFilterContext* mpeg4bsfc;
	int decode_id;                                   //标识了一个解码线程
} group_item;

typedef struct {
	char OutUrl[100];
	int  decoding;
	int  decoded_frame;
    group_item items[GROUP_ITEM_COUNT];
	pthread_mutex_t send_frame_mutex;
} decode_group;

decode_group decode_group_list[TOTAL_GROUP];

inline uint64_t gettimemsec()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000UL + t.tv_usec / 1000;
}
/***********************************************************************
 * 
 * 
 * 
 *   得到含有 AVFormatContext的Vector数组(线程遍历每个Vector读取帧)
 * 
 * 
 * 
 * *********************************************************************/
///////Return:    文件夹中的文件总数

int decode_group_init(string videoPath)
{
    DIR* videoFolder = NULL;
	int videoCount = 0;                              //用来给每个Vector中的vid赋值
    struct dirent* videoFile = NULL;
    videoFolder = opendir(videoPath.c_str());
	string filename;
	int ret = 0;

	AVPacket packet;
	videoFolder = opendir(videoPath.c_str());
	printf("打开文件夹完成\n");
	if(decode_group_list == NULL)
	{
		printf("decode_group为空\n");
	}
	int group_iterator = 0;
	while((videoFile = readdir(videoFolder)) != NULL)
    {
        printf("读到文件开始循环\n");
        if(videoFile->d_type == DT_DIR)              //如果是目录就忽略
        {
            continue;
        }
        else                                        //如果是文件
        {
			printf("开始对Vector进行赋值\n");
			//sprintf(filename,"%s/%s",videoPath,videoFile->d_name);
			filename = videoPath + '/' + videoFile->d_name;
			printf("filename = %s\n",filename.c_str());
			sprintf(decode_group_list[group_iterator/GROUP_ITEM_COUNT].OutUrl, "rtsp://192.168.1.162/decode_push%d", group_iterator/GROUP_ITEM_COUNT);
			decode_group_list[group_iterator/GROUP_ITEM_COUNT].decoding = 0;
			decode_group_list[group_iterator/GROUP_ITEM_COUNT].decoded_frame = 0;
			decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].decode_id = group_iterator;            //指定decode id


			decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].filename = filename.c_str();//文件名

			decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
			decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].h265bsfc = av_bitstream_filter_init("hevc_mp4toannexb");
			decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].mpeg4bsfc = av_bitstream_filter_init("mpeg4_unpack_bframes");

			printf("开始为Context指针分配存储空间\n");
			decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].pFormatCtx     = avformat_alloc_context();
			//对decode_group_list中的每一个元素中的Contex成员打开文件并初始化
			printf("开始赋值Context\n");
			if ((ret = avformat_open_input(&(decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT]).pFormatCtx, filename.c_str(), NULL, NULL)) != 0) {
				printf("Couldn't open input stream:%s,%d\n", videoFile->d_name,ret);
				return -1;
			}
			printf("绑定文件到Context成功:%s\n",filename.c_str());
			if (avformat_find_stream_info(decode_group_list[group_iterator/GROUP_ITEM_COUNT].items[group_iterator%GROUP_ITEM_COUNT].pFormatCtx, NULL) < 0) {
				printf("Couldn't find stream information.\n");
				return -1;
			}

			if (pthread_mutex_init(&decode_group_list[group_iterator/GROUP_ITEM_COUNT].send_frame_mutex, NULL) != 0)
			{
				printf("互斥锁初始化失败\n");
			}
        }
		group_iterator ++;
    }
	return 0;
}

void* decode_thread(void* params) 
{
	int decode_id = *(int*)params;
	int group_id = decode_id/GROUP_ITEM_COUNT;                 //组号
	int item_id  = decode_id%GROUP_ITEM_COUNT;                 //组中的元素编号

	int width = 0;
	int height = 0;

	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	AVPacket packet;

	AVFormatContext *ic = NULL;                         //用来发送rtsp
	
	uint64_t waitst, waited;
	uint64_t totaltime = 0;
	int nframe = 0;
	int packetframes = 0;
	int videoindex = -1;
	int ret;

	int badcount = 0;
	int goodcount = 0;

	int bag_count = 0;

	waitst = gettimemsec();


	pFormatCtx = decode_group_list[group_id].items[item_id].pFormatCtx;
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type
			== AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	}
	if (videoindex == -1) {
		printf("Didn't find a video stream.\n");
		goto error0;
	}
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;  
	width  = pCodecCtx->width;
	height = pCodecCtx->height;

	switch (pCodecCtx->codec_id)
	{
		case AV_CODEC_ID_H264:
		case AV_CODEC_ID_MPEG4:
		case AV_CODEC_ID_H265:
			break;
		case AV_CODEC_ID_MJPEG:
		case AV_CODEC_ID_MSMPEG4V1:
		case AV_CODEC_ID_MSMPEG4V2:
		case AV_CODEC_ID_MSMPEG4V3:
		case AV_CODEC_ID_LJPEG:
		case AV_CODEC_ID_JPEG2000:
		default:
		{
			goto error0;
		}
	}

	ret = avformat_alloc_output_context2(&ic, 0, "rtsp", decode_group_list[group_id].OutUrl);
	if (ret != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		//throw exception(buf);
		cout<<" buf3"<<endl;
	}
	//b 添加视频流 
	AVStream *vs = avformat_new_stream(ic, NULL);
	if (!vs)
	{
		//throw exception("avformat_new_stream failed");
		cout<<" avformat_new_stream failed"<<endl;
	}
	vs->codecpar->codec_tag = 0;
	//从编码器复制参数
	avcodec_parameters_from_context(vs->codecpar, vc);
	av_dump_format(ic, 0, decode_group_list[group_id].OutUrl, 1);


	///打开rtmp 的网络输出IO
	ret = avio_open(&ic->pb, decode_group_list[group_id].OutUrl, AVIO_FLAG_WRITE);
	if (ret != 0)
	{
		cout<<ret<<endl;
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		//throw exception(buf);
			cout<< buf<<endl;
	}

	//写入封装头
	ret = avformat_write_header(ic, NULL);
	if (ret != 0)
	{
		cout<<ret<<endl;
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		//throw exception(buf);
			cout<<" buf5"<<endl;
	}


	av_init_packet(&packet);
	
		
	while (av_read_frame(pFormatCtx, &packet) == 0) 
	{
		
		if (packet.stream_index == videoindex && packet.size != 0) 
		{

			if (pCodecCtx->codec_id == AV_CODEC_ID_H264)      //加入filter对extradata进行规范，防止解码出现错误
			{
				av_apply_bitstream_filters(pFormatCtx->streams[videoindex]->codec,&packet,decode_group_list[group_id].items[item_id].h264bsfc);
			}
			else if (pCodecCtx->codec_id == AV_CODEC_ID_H265)
			{
				av_apply_bitstream_filters(pFormatCtx->streams[videoindex]->codec,&packet,decode_group_list[group_id].items[item_id].h265bsfc);
			}
			else if (pCodecCtx->codec_id == AV_CODEC_ID_MPEG4)
			{
				av_apply_bitstream_filters(pFormatCtx->streams[videoindex]->codec,&packet,decode_group_list[group_id].items[item_id].mpeg4bsfc);
			}
			if(decode_group_list[group_id].decoding == item_id)   //若正在解码的是当前码流则直接send frame
			{
				//send frame
				ret = av_interleaved_write_frame(ic, &packet);
				if (ret == 0)
				{
					cout << "#" << flush;
				}
			}
			else if(packet.flags==1)//判断是否为关键帧
			{	
				if(pthread_mutex_lock(&decode_group_list[group_id].send_frame_mutex) != 0)
				{
					printf("申请锁遇到错误\n");
					return -1;
				}
				if(decode_group_list[group_id].decoded_frame > 10)
				{
					decode_group_list[group_id].decoding = item_id;
					decode_group_list[group_id].decoded_frame = 0;
					//send          key            frame
					ret = av_interleaved_write_frame(ic, &packet);
					if (ret == 0)
					{
						cout << "#" << flush;
					}
				}
				pthread_mutex_unlock(&decode_group_list[group_id].send_frame_mutex);
			}
			//else
			//{
			//      当前码流不是正在解码的，且当前帧不是关键帧，直接略过当前帧
			//}
			av_packet_unref(&packet);      //清空packet数据块
			av_init_packet(&packet);       //重置packet结构体
		}
	
	}

	printf("over\n");
	usleep(500000);//0.5s


	error0:
		//close(sockfd);
		//avformat_close_input(&pFormatCtx);
		return 0;
}

int main(int argc, char* argv[])
{

	pthread_t decode_tid[GROUP_ITEM_COUNT*TOTAL_GROUP];
	avcodec_register_all();
	av_register_all();
	avformat_network_init();

	
	if(decode_group_init("./videos") != 0)                    //读videos文件夹下的所有视频文件
	{
		printf("decode_group_init failed!!!!!!!!!!!\n");
	}
	int decode_id[GROUP_ITEM_COUNT*TOTAL_GROUP];
	for(int i = 0;i < GROUP_ITEM_COUNT*TOTAL_GROUP;i++)       //创建所有的解码线程
	{
		decode_id[i] = i;
		pthread_create(&decode_tid[i], 0, decodeThread, (void*)&decode_id[i]));
	}
	

	while (1)
	{
		sleep(10);
		preflow = totalflow;
        printf("解码线程结束\n");
		preframes = totaldecodeframe;
	}
}
