#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "stdint.h"
#include <winsock2.h>
#include "common.h"

extern "C"
{
#include "x264_config.h"
#include "x264.h"
}

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libx264.lib")

#define Width 720
#define Height 576
#define UDP_MAX_SIZE 1400


//库版本142 2014-4-1
//生成的h264文件可用VLC播放器播放
char* nal_unit_type[64] = {"NAL_UNKNOWN", 
	"NAL_SLICE", 
	"NAL_SLICE_DPA", 
	"NAL_SLICE_DPB", 
	"NAL_SLICE_DPC",
	"NAL_SLICE_IDR",
	"NAL_SEI",
	"NAL_SPS",
	"NAL_PPS",
	"NAL_AUD"};
//gobal var
RTP_FIXED_HEADER *rtp_hdr;
NALU_HEADER		*nalu_hdr;
FU_INDICATOR	*fu_ind;
FU_HEADER		*fu_hdr;
unsigned short seq_num =0;
float framerate=25;
unsigned int timestamp_increse=0,ts_current=0;
SOCKET    socket1;

void print_nal_t(x264_nal_t* nal_t, int index)
{
	printf("Type:[%s];Leght of Startcode:[%d]\n", nal_unit_type[nal_t[index].i_type], nal_t[index].b_long_startcode);

}

//为NALU_t结构体分配内存空间
NALU_t *AllocNALU(int buffersize)
{
	NALU_t *n;

	if ((n = (NALU_t*)calloc (1, sizeof (NALU_t))) == NULL)
	{
		printf("AllocNALU: n");
		exit(0);
	}

	n->max_size=buffersize;

	if ((n->buf = (char*)calloc (buffersize, sizeof (char))) == NULL)
	{
		free (n);
		printf ("AllocNALU: n->buf");
		exit(0);
	}

	return n;
}

//释放
void FreeNALU(NALU_t *n)
{
	if (n)
	{
		if (n->buf)
		{
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
}

BOOL InitWinsock()
{
	int Error;
	WORD VersionRequested;
	WSADATA WsaData;
	VersionRequested=MAKEWORD(2,2);
	Error=WSAStartup(VersionRequested,&WsaData); //启动WinSock2
	if(Error!=0)
	{
		return FALSE;
	}
	else
	{
		if(LOBYTE(WsaData.wVersion)!=2||HIBYTE(WsaData.wHighVersion)!=2)
		{
			WSACleanup();
			return FALSE;
		}

	}
	return TRUE;
}

void init_h264_encoder(x264_t** ppEncoder, x264_picture_t* pPicin, int width, int height)
{
	printf("libx264-version:%d\n",X264_BUILD);
	x264_param_t param, *pX264Param;//参数设置
	//int m_width=Width;//宽，根据实际情况改
	//int m_height=Height;//高s


	//x264_param_default(&param);//设置默认参数具体见common/common.c
	//* 使用默认参数，在这里因为我的是实时网络传输，所以我使用了zerolatency的选项，使用这个选项之后就不会有delayed_frames，如果你使用的不是这样的话，还需要在编码完成之后得到缓存的编码帧   
	//x264_param_default_preset(&param, "ultrafast"/*"veryfast"*/, "zerolatency"); 
	x264_param_default_preset(&param, "veryfast", "zerolatency"); 

	//* cpuFlags   
	param.i_threads  = X264_THREADS_AUTO/*X264_SYNC_LOOKAHEAD_AUTO*/;//* 取空缓冲区继续使用不死锁的保证.  
	//param.i_threads  = 1;
	param.i_log_level = X264_LOG_NONE; 
	//* 视频选项  
	param.i_width = width;
	param.i_height = height;

	//param.i_fps_den  = 1; //* 帧率分母   
	//param.i_fps_num  = 25;//* 帧率分子  
	//param.i_keyint_min=5;//关键帧最小间隔
	param.i_keyint_max = 25;//关键帧最大间隔
	param.b_annexb=1;//1前面为0x00000001,0为nal长度
	param.b_repeat_headers=1;//关键帧前面是否放sps跟pps帧，0 否 1，放	
	//	param.analyse.i_subpel_refine=7;//压缩级别1~7
	//	param.i_log_level=X264_LOG_NONE;//设置显示信息级别
	x264_param_apply_profile(&param, "baseline");

	*ppEncoder=x264_encoder_open(&param);//根据参数初始化X264级别
	x264_picture_init(pPicin);//初始化图片信息
	x264_picture_alloc(pPicin, X264_CSP_I420, width, height);//图片按I420格式分配空间，最后要x264_picture_clean
}

void rtp(x264_nal_t *nal_t)
{
	//dump(n);//输出NALU长度和TYPE
	char sendbuf[1500];
	char* nalu_payload = NULL;
	int index_of_nalu;
	unsigned int len;
	int bytes;
	if (nal_t->b_long_startcode)
	{
		index_of_nalu = 4;
	} 
	else
	{
		index_of_nalu = 3;
	}
	len = nal_t->i_payload - index_of_nalu;
	//get nalu bit info
	int forbidden_bit = nal_t->p_payload[index_of_nalu] & 0x80;            //! should be always FALSE
	int nal_reference_idc = (nal_t->p_payload[index_of_nalu] & 0x60) >> 5;        //! NALU_PRIORITY_xxxx
	int nal_unit_type = nal_t->p_payload[index_of_nalu] & 0x1f;            //! NALU_TYPE_xxxx    

	memset(sendbuf,0,1500);//清空sendbuf；此时会将上次的时间戳清空，因此需要ts_current来保存上次的时间戳值

	//rtp固定包头，为12字节,该句将sendbuf[0]的地址赋给rtp_hdr，以后对rtp_hdr的写入操作将直接写入sendbuf。
	rtp_hdr =(RTP_FIXED_HEADER*)&sendbuf[0]; 

	//设置RTP HEADER，
	rtp_hdr->version = 2;   //版本号，此版本固定为2
	rtp_hdr->marker  = 0;   //标志位，由具体协议规定其值。
	rtp_hdr->payload = H264;//负载类型号，
	rtp_hdr->ssrc    = htonl(10);//随机指定为10，并且在本RTP会话中全局唯一

	//当一个NALU小于1400字节的时候，采用一个单RTP包发送
	if(nal_t->i_payload<=UDP_MAX_SIZE){
		//设置rtp M 位；
		rtp_hdr->marker=1;
		rtp_hdr->seq_no = htons(seq_num ++); //序列号，每发送一个RTP包增1

		//设置NALU HEADER,并将这个HEADER填入sendbuf[12]
		nalu_hdr = (NALU_HEADER*)&sendbuf[12]; //将sendbuf[12]的地址赋给nalu_hdr，之后对nalu_hdr的写入就将写入sendbuf中；
		nalu_hdr->F = forbidden_bit;
		nalu_hdr->NRI = nal_reference_idc;//有效数据在n->nal_reference_idc的第6，7位，需要右移5位才能将其值赋给nalu_hdr->NRI。
		nalu_hdr->TYPE = nal_unit_type;

		nalu_payload=&sendbuf[13];//同理将sendbuf[13]赋给nalu_payload
		memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1,len-1);//去掉nalu头的nalu剩余内容写入sendbuf[13]开始的字符串。

		ts_current=ts_current+timestamp_increse;
		rtp_hdr->timestamp=htonl(ts_current);
		bytes=len + 12 ;	//获得sendbuf的长度,为nalu的长度（包含NALU头但除去起始前缀）加上rtp_header的固定长度12字节
		send(socket1,sendbuf,bytes,0);//发送RTP包
		//Sleep(100);
	}else{
		int packetNum = len/UDP_MAX_SIZE;
		if (len%UDP_MAX_SIZE != 0)
			packetNum ++;

		int lastPackSize = len - (packetNum-1)*UDP_MAX_SIZE;
		int packetIndex = 1 ;

		ts_current=ts_current+timestamp_increse;

		rtp_hdr->timestamp=htonl(ts_current);

		//发送第一个的FU，S=1，E=0，R=0

		rtp_hdr->seq_no = htons(seq_num ++); //序列号，每发送一个RTP包增1
		//设置rtp M 位；
		rtp_hdr->marker=0;
		//设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
		fu_ind = (FU_INDICATOR*)&sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
		fu_ind->F = forbidden_bit;
		fu_ind->NRI = nal_reference_idc;
		fu_ind->TYPE = 28;

		//设置FU HEADER,并将这个HEADER填入sendbuf[13]
		fu_hdr =(FU_HEADER*)&sendbuf[13];
		fu_hdr->S=1;
		fu_hdr->E=0;
		fu_hdr->R=0;
		fu_hdr->TYPE=nal_unit_type;

		nalu_payload=&sendbuf[14];//同理将sendbuf[14]赋给nalu_payload
		memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1,UDP_MAX_SIZE);//去掉NALU头
		bytes=UDP_MAX_SIZE+14;//获得sendbuf的长度,为nalu的长度（除去起始前缀和NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度14字节
		send( socket1, sendbuf, bytes, 0 );//发送RTP包

		//发送中间的FU，S=0，E=0，R=0
		for(packetIndex=2;packetIndex<packetNum;packetIndex++)
		{
			rtp_hdr->seq_no = htons(seq_num ++); //序列号，每发送一个RTP包增1

			//设置rtp M 位；
			rtp_hdr->marker=0;
			//设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
			fu_ind =(FU_INDICATOR*)&sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
			fu_ind->F=forbidden_bit;
			fu_ind->NRI=nal_reference_idc;
			fu_ind->TYPE=28;

			//设置FU HEADER,并将这个HEADER填入sendbuf[13]
			fu_hdr =(FU_HEADER*)&sendbuf[13];
			fu_hdr->S=0;
			fu_hdr->E=0;
			fu_hdr->R=0;
			fu_hdr->TYPE=nal_unit_type;

			nalu_payload=&sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
			memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1+(packetIndex-1)*UDP_MAX_SIZE,UDP_MAX_SIZE);//去掉起始前缀的nalu剩余内容写入sendbuf[14]开始的字符串。
			bytes=UDP_MAX_SIZE+14;//获得sendbuf的长度,为nalu的长度（除去原NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度14字节
			send( socket1, sendbuf, bytes, 0 );//发送rtp包				
		}

		//发送最后一个的FU，S=0，E=1，R=0

		rtp_hdr->seq_no = htons(seq_num ++);
		//设置rtp M 位；当前传输的是最后一个分片时该位置1		
		rtp_hdr->marker=1;
		//设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
		fu_ind =(FU_INDICATOR*)&sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
		fu_ind->F=forbidden_bit;
		fu_ind->NRI=nal_reference_idc;
		fu_ind->TYPE=28;

		//设置FU HEADER,并将这个HEADER填入sendbuf[13]
		fu_hdr =(FU_HEADER*)&sendbuf[13];
		fu_hdr->S=0;
		fu_hdr->E=1;
		fu_hdr->R=0;
		fu_hdr->TYPE=nal_unit_type;

		nalu_payload=&sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
		memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1+(packetIndex-1)*UDP_MAX_SIZE,lastPackSize-1);//将nalu最后剩余的-1(去掉了一个字节的NALU头)字节内容写入sendbuf[14]开始的字符串。
		bytes=lastPackSize-1+14;//获得sendbuf的长度,为剩余nalu的长度l-1加上rtp_header，FU_INDICATOR,FU_HEADER三个包头共14字节
		send( socket1, sendbuf, bytes, 0 );//发送rtp包		
	}

	//Sleep(33);
}

int main()
{
	int yuvsize=Width*Height*3/2;
	//文件下载地址：http://trace.eas.asu.edu/yuv/
	FILE*fyuv=fopen("20140615194958174_Sequence.yuv","rb");
	FILE*f264=fopen("20140615194958174_Sequence.h264","wb");
	if (fyuv==NULL||f264==NULL){
		printf("file err\n");
		return 0;
	}
	x264_t* encoder;//对象句柄，
	x264_picture_t Picin;//传入图像YUV
	x264_picture_t Picout;//输出图像RAW
	x264_nal_t* nal_t;
	int i_nal;
	int i=0;

	int	bytes=0;
	InitWinsock(); //初始化套接字库
	struct sockaddr_in server;
	int len =sizeof(server);
	timestamp_increse=(unsigned int)(90000.0 / framerate); //+0.5);

	server.sin_family=AF_INET;
	server.sin_port=htons(DEST_PORT);          
	server.sin_addr.s_addr=inet_addr(DEST_IP); 
	socket1=socket(AF_INET,SOCK_DGRAM,0);
	connect(socket1, (const sockaddr *)&server, len) ;//申请UDP套接字

	init_h264_encoder(&encoder, &Picin, Width, Height);


	x264_encoder_headers(encoder,&nal_t,&i_nal);

	for(i=0;i<i_nal;i++){
		//获取SPS数据，PPS数据
		print_nal_t(nal_t, i);
		//if (nal_t[i].i_type==NAL_SPS||nal_t[i].i_type==NAL_PPS){	
			//fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);
			rtp(&nal_t[i]);
		//}
	}

	Picin.i_pts = 0;
	while(fread(Picin.img.plane[0],1,yuvsize,fyuv)==yuvsize){		
		x264_encoder_encode(encoder,&nal_t,&i_nal,&Picin,&Picout);
		//少这句的话会出现 x264 [warning]: non-strictly-monotonic PTS
		Picin.i_pts++;
		for(i=0;i<i_nal;i++){
			//获取帧数据
			print_nal_t(nal_t, i);
			//if (nal_t[i].i_type==NAL_SLICE||nal_t[i].i_type==NAL_SLICE_IDR){	
				//fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);
				rtp(&nal_t[i]);
			//}					
		}
	}

	x264_encoder_encode(encoder,&nal_t,&i_nal,NULL,&Picout);
	for(i=0;i<i_nal;i++){
		print_nal_t(nal_t, i);
		if (nal_t[i].i_type==NAL_SLICE||nal_t[i].i_type==NAL_SLICE_IDR){	
			fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);
		}					
	}

	x264_picture_clean(&Picin);	
	x264_encoder_close(encoder);
	fclose(fyuv);
	fclose(f264);
	return 0;
}