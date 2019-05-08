g++ -o parallel_decode YSP_Server_add_loop.cpp -I/usr/local/ffmpeg-4.1.3/include -L/usr/local/ffmpeg-4.1.3/lib -lavcodec -lrt -lavformat -lavfilter -lavutil -lswscale -lpthread -fpermissive -std=c++11

