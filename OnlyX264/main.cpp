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


//��汾142 2014-4-1
//���ɵ�h264�ļ�����VLC����������
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

//ΪNALU_t�ṹ������ڴ�ռ�
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

//�ͷ�
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
	Error=WSAStartup(VersionRequested,&WsaData); //����WinSock2
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
	x264_param_t param, *pX264Param;//��������
	//int m_width=Width;//������ʵ�������
	//int m_height=Height;//��s


	//x264_param_default(&param);//����Ĭ�ϲ��������common/common.c
	//* ʹ��Ĭ�ϲ�������������Ϊ�ҵ���ʵʱ���紫�䣬������ʹ����zerolatency��ѡ�ʹ�����ѡ��֮��Ͳ�����delayed_frames�������ʹ�õĲ��������Ļ�������Ҫ�ڱ������֮��õ�����ı���֡   
	//x264_param_default_preset(&param, "ultrafast"/*"veryfast"*/, "zerolatency"); 
	x264_param_default_preset(&param, "veryfast", "zerolatency"); 

	//* cpuFlags   
	param.i_threads  = X264_THREADS_AUTO/*X264_SYNC_LOOKAHEAD_AUTO*/;//* ȡ�ջ���������ʹ�ò������ı�֤.  
	//param.i_threads  = 1;
	param.i_log_level = X264_LOG_NONE; 
	//* ��Ƶѡ��  
	param.i_width = width;
	param.i_height = height;

	//param.i_fps_den  = 1; //* ֡�ʷ�ĸ   
	//param.i_fps_num  = 25;//* ֡�ʷ���  
	//param.i_keyint_min=5;//�ؼ�֡��С���
	param.i_keyint_max = 25;//�ؼ�֡�����
	param.b_annexb=1;//1ǰ��Ϊ0x00000001,0Ϊnal����
	param.b_repeat_headers=1;//�ؼ�֡ǰ���Ƿ��sps��pps֡��0 �� 1����	
	//	param.analyse.i_subpel_refine=7;//ѹ������1~7
	//	param.i_log_level=X264_LOG_NONE;//������ʾ��Ϣ����
	x264_param_apply_profile(&param, "baseline");

	*ppEncoder=x264_encoder_open(&param);//���ݲ�����ʼ��X264����
	x264_picture_init(pPicin);//��ʼ��ͼƬ��Ϣ
	x264_picture_alloc(pPicin, X264_CSP_I420, width, height);//ͼƬ��I420��ʽ����ռ䣬���Ҫx264_picture_clean
}

void rtp(x264_nal_t *nal_t)
{
	//dump(n);//���NALU���Ⱥ�TYPE
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

	memset(sendbuf,0,1500);//���sendbuf����ʱ�Ὣ�ϴε�ʱ�����գ������Ҫts_current�������ϴε�ʱ���ֵ

	//rtp�̶���ͷ��Ϊ12�ֽ�,�þ佫sendbuf[0]�ĵ�ַ����rtp_hdr���Ժ��rtp_hdr��д�������ֱ��д��sendbuf��
	rtp_hdr =(RTP_FIXED_HEADER*)&sendbuf[0]; 

	//����RTP HEADER��
	rtp_hdr->version = 2;   //�汾�ţ��˰汾�̶�Ϊ2
	rtp_hdr->marker  = 0;   //��־λ���ɾ���Э��涨��ֵ��
	rtp_hdr->payload = H264;//�������ͺţ�
	rtp_hdr->ssrc    = htonl(10);//���ָ��Ϊ10�������ڱ�RTP�Ự��ȫ��Ψһ

	//��һ��NALUС��1400�ֽڵ�ʱ�򣬲���һ����RTP������
	if(nal_t->i_payload<=UDP_MAX_SIZE){
		//����rtp M λ��
		rtp_hdr->marker=1;
		rtp_hdr->seq_no = htons(seq_num ++); //���кţ�ÿ����һ��RTP����1

		//����NALU HEADER,�������HEADER����sendbuf[12]
		nalu_hdr = (NALU_HEADER*)&sendbuf[12]; //��sendbuf[12]�ĵ�ַ����nalu_hdr��֮���nalu_hdr��д��ͽ�д��sendbuf�У�
		nalu_hdr->F = forbidden_bit;
		nalu_hdr->NRI = nal_reference_idc;//��Ч������n->nal_reference_idc�ĵ�6��7λ����Ҫ����5λ���ܽ���ֵ����nalu_hdr->NRI��
		nalu_hdr->TYPE = nal_unit_type;

		nalu_payload=&sendbuf[13];//ͬ��sendbuf[13]����nalu_payload
		memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1,len-1);//ȥ��naluͷ��naluʣ������д��sendbuf[13]��ʼ���ַ�����

		ts_current=ts_current+timestamp_increse;
		rtp_hdr->timestamp=htonl(ts_current);
		bytes=len + 12 ;	//���sendbuf�ĳ���,Ϊnalu�ĳ��ȣ�����NALUͷ����ȥ��ʼǰ׺������rtp_header�Ĺ̶�����12�ֽ�
		send(socket1,sendbuf,bytes,0);//����RTP��
		//Sleep(100);
	}else{
		int packetNum = len/UDP_MAX_SIZE;
		if (len%UDP_MAX_SIZE != 0)
			packetNum ++;

		int lastPackSize = len - (packetNum-1)*UDP_MAX_SIZE;
		int packetIndex = 1 ;

		ts_current=ts_current+timestamp_increse;

		rtp_hdr->timestamp=htonl(ts_current);

		//���͵�һ����FU��S=1��E=0��R=0

		rtp_hdr->seq_no = htons(seq_num ++); //���кţ�ÿ����һ��RTP����1
		//����rtp M λ��
		rtp_hdr->marker=0;
		//����FU INDICATOR,�������HEADER����sendbuf[12]
		fu_ind = (FU_INDICATOR*)&sendbuf[12]; //��sendbuf[12]�ĵ�ַ����fu_ind��֮���fu_ind��д��ͽ�д��sendbuf�У�
		fu_ind->F = forbidden_bit;
		fu_ind->NRI = nal_reference_idc;
		fu_ind->TYPE = 28;

		//����FU HEADER,�������HEADER����sendbuf[13]
		fu_hdr =(FU_HEADER*)&sendbuf[13];
		fu_hdr->S=1;
		fu_hdr->E=0;
		fu_hdr->R=0;
		fu_hdr->TYPE=nal_unit_type;

		nalu_payload=&sendbuf[14];//ͬ��sendbuf[14]����nalu_payload
		memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1,UDP_MAX_SIZE);//ȥ��NALUͷ
		bytes=UDP_MAX_SIZE+14;//���sendbuf�ĳ���,Ϊnalu�ĳ��ȣ���ȥ��ʼǰ׺��NALUͷ������rtp_header��fu_ind��fu_hdr�Ĺ̶�����14�ֽ�
		send( socket1, sendbuf, bytes, 0 );//����RTP��

		//�����м��FU��S=0��E=0��R=0
		for(packetIndex=2;packetIndex<packetNum;packetIndex++)
		{
			rtp_hdr->seq_no = htons(seq_num ++); //���кţ�ÿ����һ��RTP����1

			//����rtp M λ��
			rtp_hdr->marker=0;
			//����FU INDICATOR,�������HEADER����sendbuf[12]
			fu_ind =(FU_INDICATOR*)&sendbuf[12]; //��sendbuf[12]�ĵ�ַ����fu_ind��֮���fu_ind��д��ͽ�д��sendbuf�У�
			fu_ind->F=forbidden_bit;
			fu_ind->NRI=nal_reference_idc;
			fu_ind->TYPE=28;

			//����FU HEADER,�������HEADER����sendbuf[13]
			fu_hdr =(FU_HEADER*)&sendbuf[13];
			fu_hdr->S=0;
			fu_hdr->E=0;
			fu_hdr->R=0;
			fu_hdr->TYPE=nal_unit_type;

			nalu_payload=&sendbuf[14];//ͬ��sendbuf[14]�ĵ�ַ����nalu_payload
			memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1+(packetIndex-1)*UDP_MAX_SIZE,UDP_MAX_SIZE);//ȥ����ʼǰ׺��naluʣ������д��sendbuf[14]��ʼ���ַ�����
			bytes=UDP_MAX_SIZE+14;//���sendbuf�ĳ���,Ϊnalu�ĳ��ȣ���ȥԭNALUͷ������rtp_header��fu_ind��fu_hdr�Ĺ̶�����14�ֽ�
			send( socket1, sendbuf, bytes, 0 );//����rtp��				
		}

		//�������һ����FU��S=0��E=1��R=0

		rtp_hdr->seq_no = htons(seq_num ++);
		//����rtp M λ����ǰ����������һ����Ƭʱ��λ��1		
		rtp_hdr->marker=1;
		//����FU INDICATOR,�������HEADER����sendbuf[12]
		fu_ind =(FU_INDICATOR*)&sendbuf[12]; //��sendbuf[12]�ĵ�ַ����fu_ind��֮���fu_ind��д��ͽ�д��sendbuf�У�
		fu_ind->F=forbidden_bit;
		fu_ind->NRI=nal_reference_idc;
		fu_ind->TYPE=28;

		//����FU HEADER,�������HEADER����sendbuf[13]
		fu_hdr =(FU_HEADER*)&sendbuf[13];
		fu_hdr->S=0;
		fu_hdr->E=1;
		fu_hdr->R=0;
		fu_hdr->TYPE=nal_unit_type;

		nalu_payload=&sendbuf[14];//ͬ��sendbuf[14]�ĵ�ַ����nalu_payload
		memcpy(nalu_payload,nal_t->p_payload+index_of_nalu+1+(packetIndex-1)*UDP_MAX_SIZE,lastPackSize-1);//��nalu���ʣ���-1(ȥ����һ���ֽڵ�NALUͷ)�ֽ�����д��sendbuf[14]��ʼ���ַ�����
		bytes=lastPackSize-1+14;//���sendbuf�ĳ���,Ϊʣ��nalu�ĳ���l-1����rtp_header��FU_INDICATOR,FU_HEADER������ͷ��14�ֽ�
		send( socket1, sendbuf, bytes, 0 );//����rtp��		
	}

	//Sleep(33);
}

int main()
{
	int yuvsize=Width*Height*3/2;
	//�ļ����ص�ַ��http://trace.eas.asu.edu/yuv/
	FILE*fyuv=fopen("20140615194958174_Sequence.yuv","rb");
	FILE*f264=fopen("20140615194958174_Sequence.h264","wb");
	if (fyuv==NULL||f264==NULL){
		printf("file err\n");
		return 0;
	}
	x264_t* encoder;//��������
	x264_picture_t Picin;//����ͼ��YUV
	x264_picture_t Picout;//���ͼ��RAW
	x264_nal_t* nal_t;
	int i_nal;
	int i=0;

	int	bytes=0;
	InitWinsock(); //��ʼ���׽��ֿ�
	struct sockaddr_in server;
	int len =sizeof(server);
	timestamp_increse=(unsigned int)(90000.0 / framerate); //+0.5);

	server.sin_family=AF_INET;
	server.sin_port=htons(DEST_PORT);          
	server.sin_addr.s_addr=inet_addr(DEST_IP); 
	socket1=socket(AF_INET,SOCK_DGRAM,0);
	connect(socket1, (const sockaddr *)&server, len) ;//����UDP�׽���

	init_h264_encoder(&encoder, &Picin, Width, Height);


	x264_encoder_headers(encoder,&nal_t,&i_nal);

	for(i=0;i<i_nal;i++){
		//��ȡSPS���ݣ�PPS����
		print_nal_t(nal_t, i);
		//if (nal_t[i].i_type==NAL_SPS||nal_t[i].i_type==NAL_PPS){	
			//fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);
			rtp(&nal_t[i]);
		//}
	}

	Picin.i_pts = 0;
	while(fread(Picin.img.plane[0],1,yuvsize,fyuv)==yuvsize){		
		x264_encoder_encode(encoder,&nal_t,&i_nal,&Picin,&Picout);
		//�����Ļ������ x264 [warning]: non-strictly-monotonic PTS
		Picin.i_pts++;
		for(i=0;i<i_nal;i++){
			//��ȡ֡����
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