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

#define Width 720
#define Height 576
//��汾142 2014-4-1
//���ɵ�h264�ļ�����VLC����������
int main()
{
	printf("libx264-version:%d\n",X264_BUILD);
	x264_t*h;//��������
	x264_picture_t m_picin;//����ͼ��YUV
	x264_picture_t m_picout;//���ͼ��RAW
	x264_param_t param, *pX264Param;//��������
	int m_width=Width;//������ʵ�������
	int m_height=Height;//��s
	//�ļ����ص�ַ��http://trace.eas.asu.edu/yuv/
	FILE*fyuv=fopen("20140611203748920_Sequence.yuv","rb");
	FILE*f264=fopen("20140611203748920_Sequence.h264","wb");
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

	//==============================================================
	////pX264Param = &param;
	////int yuvsize=m_height*m_width*3/2;
	//* ʹ��Ĭ�ϲ�������������Ϊ�ҵ���ʵʱ���紫�䣬������ʹ����zerolatency��ѡ�ʹ�����ѡ��֮��Ͳ�����delayed_frames�������ʹ�õĲ��������Ļ�������Ҫ�ڱ������֮��õ�����ı���֡   
	////x264_param_default_preset(pX264Param, "ultrafast"/*"veryfast"*/, "zerolatency");  

	//* cpuFlags   
	////pX264Param->i_threads  = X264_THREADS_AUTO/*X264_SYNC_LOOKAHEAD_AUTO*/;//* ȡ�ջ���������ʹ�ò������ı�֤.  
	////pX264Param->i_log_level = X264_LOG_NONE; 
	//* ��Ƶѡ��      
	////pX264Param->i_width   = Width; //* Ҫ�����ͼ����.   
	////pX264Param->i_height  = Height; //* Ҫ�����ͼ��߶�   
	//pX264Param->i_frame_total = 0; //* ������֡��.��֪����0.   
	//pX264Param->i_keyint_max = 10;   

	//* ������   
	////pX264Param->i_bframe  = 0;  
// 	pX264Param->b_open_gop  = 0;  
// 	pX264Param->i_bframe_pyramid = 0;  
// 	pX264Param->i_bframe_adaptive = 0; 

	//* Log����������Ҫ��ӡ������Ϣʱֱ��ע�͵�����   
	//pX264Param->i_log_level  = X264_LOG_DEBUG;  

	//* ���ʿ��Ʋ���   
	//pX264Param->rc.i_bitrate = 1024 * 10;//* ����(������,��λKbps)  

	//* muxing parameters   
	////pX264Param->i_fps_den  = 1; //* ֡�ʷ�ĸ   
	////pX264Param->i_fps_num  = 25;//* ֡�ʷ���   
// 	pX264Param->i_timebase_den = pX264Param->i_fps_num;  
// 	pX264Param->i_timebase_num = pX264Param->i_fps_den;  
	/*
	//����x264����������С��IDR֡����ƹؼ�֡����ࡣ
	//�����Ĭ��ֵ��fps��10�����Դ������Ƶ���ܺã���С�����fps���
	pX264Param->i_keyint_max = 150;
	pX264Param->i_keyint_min = 50;

	//* ����Profile.ʹ��Baseline profile   
	x264_param_apply_profile(pX264Param, x264_profile_names[0]);  
	
	//��ȫ�ر�����ӦI֡���ߡ�
	pX264Param->i_scenecut_threshold = 0;

	//���������ع��Ƶĸ��Ӷȡ�ֵԽ��Խ�á�����1-5�򵥿��������ص�ϸ�����ȡ�����6��ģʽ���߿���RDO������ʧ���Ż�ģʽ����
	//����8���˶�ʸ����֡��Ԥ��ģʽ����RDO������RDO���������Ӻ�ʱ��
	//ʹ��С��2��ֵ�Ὺ��һ�����ٵġ���������Ԥ��ģʽ��Ч����ͬ������һ����С�� �Cscenecutֵ�����Ƽ��������á�
	//pX264Param->analyse.i_subpel_refine = 1;

	//Ϊmb-tree ratecontrol��Macroblock Tree Ratecontrol����vbv-lookahead���ÿ��õ�֡����������������Ϊ250��
	//����mb-tree���ԣ��������ֵ��õ���׼ȷ�ؽ������Ҳ�������
	//mb-tree��ʹ�õ����ֵ�ǨCrc-lookahead�ͨCkeyint�н�С����һ����
	pX264Param->rc.i_lookahead = 0; 

	//i_luma_deadzone[0]��i_luma_deadzone[1]�ֱ��Ӧinter��intra��ȡֵ��Χ1~32
	//���Կ��Ե�֪�������������ĵ������Զ��������кܴ�Ӱ�죬ֵԽ����������ӦԽ�٣�ռ�ô���Խ��.
	pX264Param->analyse.i_luma_deadzone[0] = 32;
	pX264Param->analyse.i_luma_deadzone[1] = 32;

	//���ʿ���ģʽ��ABR��ƽ�����ʣ���CQP���㶨��������CRF���㶨���ʣ�.
	//ABRģʽ�µ���i_bitrate��CQP�µ���i_qp_constant����QPֵ����Χ0~51��ֵԽ��ͼ��Խģ����Ĭ��23.
	//̫ϸ��������Ҳ�ֱ治������Ϊ�����ӱ����ٶȽ����������������Щ��,CRF�µ���f_rf_constant��f_rf_constant_maxӰ������ٶȺ�ͼ������������������
	pX264Param->rc.i_rc_method = X264_RC_CQP;
	pX264Param->rc.i_qp_constant = 30;

	//����Ӧ������ģʽ����ʹ������Ӧ�����Ļ���x264������ʹ�ý��ٵ�bit��ȱ��ϸ�ڵĳ��������Ӧ����������������Ƶ�ĺ������õط�����ء���������ѡ�
	//0-��ȫ�ر�����Ӧ������;1-��������Ӧ��������������Ƶ֡�ڲ��������;2-����ǰһ֡ǿ�Ⱦ��ߵ��Ա���������ʵ���Եģ���Ĭ��ֵ=1
	pX264Param->rc.i_aq_mode = 0;

	//Ϊ��direct�����͵��˶�ʸ���趨Ԥ��ģʽ�������ֿ�ѡ��ģʽ��spatial���ռ�Ԥ�⣩��temporal��ʱ��Ԥ�⣩��Ĭ�ϣ���spatial��
	//��������Ϊ��none���ر�Ԥ�⣬Ҳ��������Ϊ��auto����x264ȥѡ������Ϊ���õ�ģʽ��x264���ڱ������ʱ����������ѡ��
	pX264Param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_NONE;

	//������ȷ��Ȩ��Ԥ��������P֡ѹ����Խ�߼���ģʽԽ��ʱ��������ģʽ��
	//0 : �ر�; 1 : ��̬��������ԶΪ-1��; 2 : ����ͳ�ƾ�̬֡���ر�Ϊ�������뵭��Ч����ѹ���ʶ����
	//pX264Param->analyse.i_weighted_pred = X264_WEIGHTP_NONE;

	//����ȫ�ֵ��˶�Ԥ�ⷽ����������5��ѡ��dia���ı���������, hex��������������, umh�������ȵĶ�������������
	//esa��ȫ����������tesa���任ȫ����������Ĭ�ϣ���hex��
	pX264Param->analyse.i_me_method = X264_ME_DIA;

	//merange�����˶�������������ط�Χ������hex��dia����Χ��������4-16���أ�Ĭ�Ͼ���16��
	//����umh��esa�����Գ���Ĭ�ϵ� 16���ؽ��д�Χ��������������Ը߷ֱ�����Ƶ�Ϳ����˶���Ƶ���Ժ����á�
	//ע�⣬����umh��esa��tesa������merange�����������ӱ����ʱ��Ĭ�ϣ�16
	pX264Param->analyse.i_me_range = 4;

	//�رջ�����Ϊ�������Ӿ�������psnr��ssim���Ż�����ѡ��ͬʱҲ��ر�һЩ����ͨ��x264���������õ��ڲ��������Ӿ��Ż�������
	//pX264Param->analyse.b_psy = 0;

	//Mixed refs����ϲ��գ�����8��8���п�Ϊ����ȡ�����������Ϊ���ա���������֡���յ�֡������������һЩʱ�����.
	//pX264Param->analyse.b_mixed_references = 0;

	//ͨ���˶����ƶ���ͬʱ�������Ⱥ�ɫ�����ء�������ѡ������ɫ�����ػ�ȡһЩ�ٶȵ�������
	pX264Param->analyse.b_chroma_me = 1;

	//ʹ�����������������������Ч�ʣ�0-�ر�, 1-���ں�����ձ���ʱ����, 2-����ģʽ�¾�����.
	//ѡ��1�ṩ���ٶȺ�Ч�ʼ�Ϻõľ��⣬ѡ��2��������ٶ�.
	//ע�⣺��Ҫ���� �Ccabacѡ����Ч.
	//pX264Param->b_cabac = 1;
	//pX264Param->analyse.i_trellis = 1;

	//ͣ�õ������ݵĶ������������루CABAC��Context Adaptive Binary Arithmetic Coder��������ѹ����
	//�л���Ч�ʽϵ͵ĵ������ݵĿɱ䳤�ȱ��루CAVLC��Context Adaptive Variable Length Coder��ϵͳ��
	//�������ѹ��Ч�ʣ�ͨ��10~20%���ͽ����Ӳ������
	//pX264Param->b_cabac = 1;
	//pX264Param->i_cabac_init_idc = 0;

	//�ر�P֡�������������ߡ�������ʱ�Ļ��طǳ�С������������
	pX264Param->analyse.b_fast_pskip = 0;

	//DCT�����ᶪ������ȥ�����ࡱ��DCT�顣�����ӱ���Ч�ʣ�ͨ��������ʧ���Ժ��ԡ�
	pX264Param->analyse.b_dct_decimate = 1;

	//open-gop��һ�����Ч�ʵı��뼼����������ģʽ:none-ͣ��open-gop;normal-����open-gop;
	//bluray-����open-gop��һ��Ч�ʽϵ͵�open-gop�汾����Ϊnormalģʽ�޷������������.
	//ĳЩ����������ȫ֧Ԯopen-gop�������������Ϊʲô��ѡ�δĬ��Ϊ���á����������open-gop��Ӧ���Ȳ������п����������ŵĽ�������
	pX264Param->b_open_gop = 1;

	//��ȫ�ر�����ȥ���˾������Ƽ�ʹ�á�
	//����H.264��׼�е�����ȥ���˾������Ǹ��Լ۱Ⱥܸߵ�ѡ��, �ر�.
	//pX264Param->b_deblocking_filter = 1;
	//pX264Param->i_deblocking_filter_alphac0 = 0;
	//pX264Param->i_deblocking_filter_beta = 0;

	//ȥ������ȵļ��㣬��Ϊ�ڽ����Ҳ���õ�.
	pX264Param->analyse.b_psnr = 0; //�Ƿ�ʹ�������.
	*/
	//��������ļ���profile�����������������������ֵ����ѡ���ܱ�֤���profile���ݵ���Ƶ����
	//���ʹ�������ѡ������ܽ�������ѹ������ѡ��baseline��main��high
	////x264_param_apply_profile(pX264Param, "baseline");
	//==============================================================
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