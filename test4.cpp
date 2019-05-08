#include <opencv2/core/core.hpp> 
#include <opencv2/highgui/highgui.hpp> 
#include <opencv2/opencv.hpp>
#include <stdlib.h>
#include <iostream>
extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
//#pragma comment(lib,"opencv_world300.lib")
using namespace std;
using namespace cv;
int main(int argc, char *argv[])
{
    //海康相机的rtsp url
    //char *inUrl = "rtsp://test:test123456@192.168.1.64";
    //nginx-rtmp 直播服务器rtmp推流URL
    char *outUrl = "rtsp://192.168.1.162/test3";

    //注册所有的编解码器
    avcodec_register_all();

    //注册所有的封装器
    av_register_all();

    //注册所有网络协议
    avformat_network_init();


    //VideoCapture cam;
    Mat frame;
    frame.create(1080, 1920, CV_8UC3);
    //namedWindow("video");

    //像素格式转换上下文
    SwsContext *vsc = NULL;

    //输出的数据结构
    AVFrame *yuv = NULL;

    //编码器上下文
    AVCodecContext *vc = NULL;

    //rtmp flv 封装器                                                           
    AVFormatContext *ic = NULL;


    try
    {   ////////////////////////////////////////////////////////////////

        int fps = 25;
        int inWidth=1920;
        int inHeight=1080;
        ///2 初始化格式转换上下文
        vsc = sws_getCachedContext(vsc,
            inWidth, inHeight, AV_PIX_FMT_RGB24,     //源宽、高、像素格式
            inWidth, inHeight, AV_PIX_FMT_YUV420P,//目标宽、高、像素格式
            SWS_BICUBIC,  // 尺寸变化使用算法
            0, 0, 0
        );
        if (!vsc)
        {
            throw exception("sws_getCachedContext failed!");
            cout<<"sws_getCachedContext failed!"<<endl;
        }
        ///3 初始化输出的数据结构
        yuv = av_frame_alloc();
        yuv->format = AV_PIX_FMT_YUV420P;
        yuv->width = inWidth;
        yuv->height = inHeight;
        yuv->pts = 0;
        //分配yuv空间
        int ret = av_frame_get_buffer(yuv, 32);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            //throw exception(buf);
            cout<<"buf1"<<endl;
        }

        ///4 初始化编码上下文
        //a 找到编码器
        AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec)
        {
            //throw exception("Can`t find h264 encoder!");
            cout<<"Can`t find h264 encoder!"<<endl;
        }
        //b 创建编码器上下文
        vc = avcodec_alloc_context3(codec);
        if (!vc)
        {
            //throw exception("avcodec_alloc_context3 failed!");
            cout<<"avcodec_alloc_context3 failed!"<<endl;
        }
        //c 配置编码器参数
        vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //全局参数
        vc->codec_id = codec->id;
        vc->thread_count = 8;

        vc->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的bit位大小 50kB
        vc->width = inWidth;
        vc->height = inHeight;
        vc->time_base = { 1,fps };
        vc->framerate = { fps,1 };

        //画面组的大小，多少帧一个关键帧
        vc->gop_size = 50;
        vc->max_b_frames = 0;
        vc->pix_fmt = AV_PIX_FMT_YUV420P;
        //d 打开编码器上下文
        ret = avcodec_open2(vc, 0, 0);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            //throw exception(buf);
            cout<<" buf2"<<endl;
        }
        cout << "avcodec_open2 success!" << endl;

        ///5 输出封装器和视频流配置
        //a 创建输出封装器上下文
        ret = avformat_alloc_output_context2(&ic, 0, "rtsp", outUrl);
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
        av_dump_format(ic, 0, outUrl, 1);


        ///打开rtmp 的网络输出IO
        ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
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

        AVPacket pack;
        memset(&pack, 0, sizeof(pack));
        int vpts = 0;


        FILE *fp_rgb=fopen("out_rgb123.rgb","r");
        char *buf = NULL;
        buf = (char *)malloc(1920*1080*3);
        if (NULL == buf)
        {
            cout<<"error"<<endl;
        }
        //ret=fread(frame.data,1,1920*1080*3,fp_rgb);
        // FILE*fp_yuv=fopen("out_yuv123.yuv","w");
        for (;;)
        {
            if(fread(buf,1,1920*1080*3,fp_rgb) == 0)
            {
                fseek(fp_rgb, 0, 0);
            }
            memcpy(frame.data, buf, 1920*1080*3);

            
            ///rgb to yuv
            //输入的数据结构
            uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
            //indata[0] bgrbgrbgr
            //plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr 
            indata[0] = frame.data;
            int insize[AV_NUM_DATA_POINTERS] = { 0 };
            //一行（宽）数据的字节数
            insize[0] = frame.cols * frame.elemSize();
            int h = sws_scale(vsc, indata, insize, 0, frame.rows, //源数据
                yuv->data, yuv->linesize);
            //cout<<frame.rows<<frame.cols<<yuv->linesize[0]<<"       "<<frame.elemSize()<<endl;
            // fwrite(yuv->data[0],1,1920*1080,fp_yuv);    //Y 
            // fwrite(yuv->data[1],1,1920*1080/4,fp_yuv);  //U
            // fwrite(yuv->data[2],1,1920*1080/4,fp_yuv);  //V


            if (h <= 0)
            {
                continue;
            }
            //cout << h << " " << flush;
            ///h264编码
            yuv->pts = vpts;
            vpts++;
            ret = avcodec_send_frame(vc, yuv);
            if (ret != 0)
                continue;

            ret = avcodec_receive_packet(vc, &pack);
            if (ret != 0 || pack.size > 0)
            {
                //cout << "*" << pack.size << flush;
            }
            else
            {
                continue;
            }
            //推流
            pack.pts = av_rescale_q(pack.pts, vc->time_base, vs->time_base);
            pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
            pack.duration = av_rescale_q(pack.duration, vc->time_base, vs->time_base);

            ret = av_interleaved_write_frame(ic, &pack);
            if (ret == 0)
            {
                cout << "#" << flush;
            }
        }
        free(buf);
        fclose(fp_rgb);
    }
    catch (exception &ex)
    {
        // if (cam.isOpened())
        //     cam.release();
        if (vsc)
        {
            sws_freeContext(vsc);
            vsc = NULL;
        }

        if (vc)
        {
            avio_closep(&ic->pb);
            avcodec_free_context(&vc);
        }

        cerr << ex.what() << endl;
    }
    getchar();
    return 0;
}
