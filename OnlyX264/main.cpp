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
//库版本142 2014-4-1
//生成的h264文件可用VLC播放器播放
int main()
{
	printf("libx264-version:%d\n",X264_BUILD);
	x264_t*h;//对象句柄，
	x264_picture_t m_picin;//传入图像YUV
	x264_picture_t m_picout;//输出图像RAW
	x264_param_t param;//参数设置
	int m_width=352;//宽，根据实际情况改
	int m_height=288;//高
	//文件下载地址：http://trace.eas.asu.edu/yuv/
	FILE*fyuv=fopen("akiyo_cif.yuv","rb");
	FILE*f264=fopen("akiyo_cif.h264","wb");
	if (fyuv==NULL||f264==NULL){
		printf("file err\n");
		return 0;
	}
	x264_param_default(&param);//设置默认参数具体见common/common.c
	int yuvsize=m_height*m_width*3/2;
	param.i_width=m_width;
	param.i_height=m_height;
	param.i_keyint_min=5;//关键帧最小间隔
	param.i_keyint_max=250;//关键帧最大间隔
	param.b_annexb=1;//1前面为0x00000001,0为nal长度
	param.b_repeat_headers=0;//关键帧前面是否放sps跟pps帧，0 否 1，放	
//	param.analyse.i_subpel_refine=7;//压缩级别1~7
//	param.i_log_level=X264_LOG_NONE;//设置显示信息级别
	h=x264_encoder_open(&param);//根据参数初始化X264级别
	x264_picture_init(&m_picin);//初始化图片信息
	x264_picture_alloc(&m_picin,X264_CSP_I420,m_width,m_height);//图片按I420格式分配空间，最后要x264_picture_clean
	x264_nal_t*nal_t;
	int i_nal;
	int i=0;
	x264_encoder_headers(h,&nal_t,&i_nal);
	for(i=0;i<i_nal;i++){
		//获取SPS数据，PPS数据
		if (nal_t[i].i_type==NAL_SPS||nal_t[i].i_type==NAL_PPS){	
			fwrite(nal_t[i].p_payload,1,nal_t[i].i_payload,f264);			
		}
	}
	
	while(fread(m_picin.img.plane[0],1,yuvsize,fyuv)==yuvsize){		
		x264_encoder_encode(h,&nal_t,&i_nal,&m_picin,&m_picout);
		//少这句的话会出现 x264 [warning]: non-strictly-monotonic PTS
	    m_picin.i_pts++;
		for(i=0;i<i_nal;i++){
			//获取帧数据
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