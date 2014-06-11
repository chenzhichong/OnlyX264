#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdint.h"
extern "C"
{
#include "x264_config.h"
#include "x264.h"
}
#pragma comment(lib,"libx264.lib")
//��汾142 2014-4-1
//���ɵ�h264�ļ�����VLC����������
int main()
{
	printf("libx264-version:%d\n",X264_BUILD);
	x264_t*h;//��������
	x264_picture_t m_picin;//����ͼ��YUV
	x264_picture_t m_picout;//���ͼ��RAW
	x264_param_t param;//��������
	int m_width=352;//������ʵ�������
	int m_height=288;//��
	//�ļ����ص�ַ��http://trace.eas.asu.edu/yuv/
	FILE*fyuv=fopen("akiyo_cif.yuv","rb");
	FILE*f264=fopen("akiyo_cif.h264","wb");
	if (fyuv==NULL||f264==NULL){
		printf("file err\n");
		return 0;
	}
	x264_param_default(&param);//����Ĭ�ϲ��������common/common.c
	int yuvsize=m_height*m_width*3/2;
	param.i_width=m_width;
	param.i_height=m_height;
	param.i_keyint_min=5;//�ؼ�֡��С���
	param.i_keyint_max=250;//�ؼ�֡�����
	param.b_annexb=1;//1ǰ��Ϊ0x00000001,0Ϊnal����
	param.b_repeat_headers=0;//�ؼ�֡ǰ���Ƿ��sps��pps֡��0 �� 1����	
//	param.analyse.i_subpel_refine=7;//ѹ������1~7
//	param.i_log_level=X264_LOG_NONE;//������ʾ��Ϣ����
	h=x264_encoder_open(&param);//���ݲ�����ʼ��X264����
	x264_picture_init(&m_picin);//��ʼ��ͼƬ��Ϣ
	x264_picture_alloc(&m_picin,X264_CSP_I420,m_width,m_height);//ͼƬ��I420��ʽ����ռ䣬���Ҫx264_picture_clean
	x264_nal_t*nal_t;
	int i_nal;
	int i=0;
	x264_encoder_headers(h,&nal_t,&i_nal);
	for(i=0;i<i_nal;i++){
		//��ȡSPS���ݣ�PPS����
		if (nal_t[i].i_type==NAL_SPS||nal_t[i].i_type==NAL_PPS){	
			fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);			
		}
	}
	
	while(fread(m_picin.img.plane[0],1,yuvsize,fyuv)==yuvsize){		
		x264_encoder_encode(h,&nal_t,&i_nal,&m_picin,&m_picout);
		//�����Ļ������ x264 [warning]: non-strictly-monotonic PTS
	    m_picin.i_pts++;
		for(i=0;i<i_nal;i++){
			//��ȡ֡����
			if (nal_t[i].i_type==NAL_SLICE||nal_t[i].i_type==NAL_SLICE_IDR){	
				fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);
			}					
		}
	}
	x264_encoder_encode(h,&nal_t,&i_nal,NULL,&m_picout);
	for(i=0;i<i_nal;i++){
		if (nal_t[i].i_type==NAL_SLICE||nal_t[i].i_type==NAL_SLICE_IDR){	
			fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);
		}					
	}
	x264_picture_clean(&m_picin);	
	x264_encoder_close(h);
	fclose(fyuv);
	fclose(f264);
	return 0;
}