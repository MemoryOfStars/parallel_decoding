typedef struct {
	string filename;
	AVFormatContext* pFormatCtx;
	AVBitStreamFilterContext* h264bsfc;
	AVBitStreamFilterContext* h265bsfc;
	AVBitStreamFilterContext* mpeg4bsfc;
} group_item;

typedef struct {
	char OutUrl[100];
	int  decoding;
	int  decoded_frame;
    group_item items[GROUP_ITEM_COUNT];
} decode_group;

decode_group decode_group_list[TOTAL_GROUP];