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
//库版本142 2014-4-1
//生成的h264文件可用VLC播放器播放
int main()
{
	printf("libx264-version:%d\n",X264_BUILD);
	x264_t*h;//对象句柄，
	x264_picture_t m_picin;//传入图像YUV
	x264_picture_t m_picout;//输出图像RAW
	x264_param_t param, *pX264Param;//参数设置
	int m_width=Width;//宽，根据实际情况改
	int m_height=Height;//高s
	//文件下载地址：http://trace.eas.asu.edu/yuv/
	FILE*fyuv=fopen("20140611203748920_Sequence.yuv","rb");
	FILE*f264=fopen("20140611203748920_Sequence.h264","wb");
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

	//==============================================================
	////pX264Param = &param;
	////int yuvsize=m_height*m_width*3/2;
	//* 使用默认参数，在这里因为我的是实时网络传输，所以我使用了zerolatency的选项，使用这个选项之后就不会有delayed_frames，如果你使用的不是这样的话，还需要在编码完成之后得到缓存的编码帧   
	////x264_param_default_preset(pX264Param, "ultrafast"/*"veryfast"*/, "zerolatency");  

	//* cpuFlags   
	////pX264Param->i_threads  = X264_THREADS_AUTO/*X264_SYNC_LOOKAHEAD_AUTO*/;//* 取空缓冲区继续使用不死锁的保证.  
	////pX264Param->i_log_level = X264_LOG_NONE; 
	//* 视频选项      
	////pX264Param->i_width   = Width; //* 要编码的图像宽度.   
	////pX264Param->i_height  = Height; //* 要编码的图像高度   
	//pX264Param->i_frame_total = 0; //* 编码总帧数.不知道用0.   
	//pX264Param->i_keyint_max = 10;   

	//* 流参数   
	////pX264Param->i_bframe  = 0;  
// 	pX264Param->b_open_gop  = 0;  
// 	pX264Param->i_bframe_pyramid = 0;  
// 	pX264Param->i_bframe_adaptive = 0; 

	//* Log参数，不需要打印编码信息时直接注释掉就行   
	//pX264Param->i_log_level  = X264_LOG_DEBUG;  

	//* 速率控制参数   
	//pX264Param->rc.i_bitrate = 1024 * 10;//* 码率(比特率,单位Kbps)  

	//* muxing parameters   
	////pX264Param->i_fps_den  = 1; //* 帧率分母   
	////pX264Param->i_fps_num  = 25;//* 帧率分子   
// 	pX264Param->i_timebase_den = pX264Param->i_fps_num;  
// 	pX264Param->i_timebase_num = pX264Param->i_fps_den;  
	/*
	//设置x264输出中最大、最小的IDR帧（亦称关键帧）间距。
	//最大间距默认值（fps的10倍）对大多数视频都很好；最小间距与fps相等
	pX264Param->i_keyint_max = 150;
	pX264Param->i_keyint_min = 50;

	//* 设置Profile.使用Baseline profile   
	x264_param_apply_profile(pX264Param, x264_profile_names[0]);  
	
	//完全关闭自适应I帧决策。
	pX264Param->i_scenecut_threshold = 0;

	//设置亚像素估计的复杂度。值越高越好。级别1-5简单控制亚像素的细化力度。级别6给模式决策开启RDO（码率失真优化模式），
	//级别8给运动矢量和帧内预测模式开启RDO。开启RDO会显著增加耗时。
	//使用小于2的值会开启一个快速的、低质量的预测模式，效果如同设置了一个很小的 –scenecut值。不推荐这样设置。
	//pX264Param->analyse.i_subpel_refine = 1;

	//为mb-tree ratecontrol（Macroblock Tree Ratecontrol）和vbv-lookahead设置可用的帧的数量。最大可设置为250。
	//对于mb-tree而言，调大这个值会得到更准确地结果，但也会更慢。
	//mb-tree能使用的最大值是–rc-lookahead和–keyint中较小的那一个。
	pX264Param->rc.i_lookahead = 0; 

	//i_luma_deadzone[0]和i_luma_deadzone[1]分别对应inter和intra，取值范围1~32
	//测试可以得知，这连个参数的调整可以对数据量有很大影响，值越大数据量相应越少，占用带宽越低.
	pX264Param->analyse.i_luma_deadzone[0] = 32;
	pX264Param->analyse.i_luma_deadzone[1] = 32;

	//码率控制模式有ABR（平均码率）、CQP（恒定质量）、CRF（恒定码率）.
	//ABR模式下调整i_bitrate，CQP下调整i_qp_constant调整QP值，范围0~51，值越大图像越模糊，默认23.
	//太细致了人眼也分辨不出来，为了增加编码速度降低数据量还是设大些好,CRF下调整f_rf_constant和f_rf_constant_max影响编码速度和图像质量（数据量）；
	pX264Param->rc.i_rc_method = X264_RC_CQP;
	pX264Param->rc.i_qp_constant = 30;

	//自适应量化器模式。不使用自适应量化的话，x264趋向于使用较少的bit在缺乏细节的场景里。自适应量化可以在整个视频的宏块里更好地分配比特。它有以下选项：
	//0-完全关闭自适应量化器;1-允许自适应量化器在所有视频帧内部分配比特;2-根据前一帧强度决策的自变量化器（实验性的）。默认值=1
	pX264Param->rc.i_aq_mode = 0;

	//为’direct’类型的运动矢量设定预测模式。有两种可选的模式：spatial（空间预测）和temporal（时间预测）。默认：’spatial’
	//可以设置为’none’关闭预测，也可以设置为’auto’让x264去选择它认为更好的模式，x264会在编码结束时告诉你它的选择。
	pX264Param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_NONE;

	//开启明确的权重预测以增进P帧压缩。越高级的模式越耗时，有以下模式：
	//0 : 关闭; 1 : 静态补偿（永远为-1）; 2 : 智能统计静态帧，特别为增进淡入淡出效果的压缩率而设计
	//pX264Param->analyse.i_weighted_pred = X264_WEIGHTP_NONE;

	//设置全局的运动预测方法，有以下5种选择：dia（四边形搜索）, hex（六边形搜索）, umh（不均匀的多六边形搜索）
	//esa（全局搜索），tesa（变换全局搜索），默认：’hex’
	pX264Param->analyse.i_me_method = X264_ME_DIA;

	//merange控制运动搜索的最大像素范围。对于hex和dia，范围被控制在4-16像素，默认就是16。
	//对于umh和esa，可以超过默认的 16像素进行大范围的运行搜索，这对高分辨率视频和快速运动视频而言很有用。
	//注意，对于umh、esa、tesa，增大merange会显著地增加编码耗时。默认：16
	pX264Param->analyse.i_me_range = 4;

	//关闭或启动为了心理视觉而降低psnr或ssim的优化。此选项同时也会关闭一些不能通过x264命令行设置的内部的心理视觉优化方法。
	//pX264Param->analyse.b_psy = 0;

	//Mixed refs（混合参照）会以8×8的切块为参照取代以整个宏块为参照。会增进多帧参照的帧的质量，会有一些时间耗用.
	//pX264Param->analyse.b_mixed_references = 0;

	//通常运动估计都会同时考虑亮度和色度因素。开启此选项将会忽略色度因素换取一些速度的提升。
	pX264Param->analyse.b_chroma_me = 1;

	//使用网格编码量化以增进编码效率：0-关闭, 1-仅在宏块最终编码时启用, 2-所有模式下均启用.
	//选项1提供了速度和效率间较好的均衡，选项2大幅降低速度.
	//注意：需要开启 –cabac选项生效.
	//pX264Param->b_cabac = 1;
	//pX264Param->analyse.i_trellis = 1;

	//停用弹性内容的二进制算数编码（CABAC：Context Adaptive Binary Arithmetic Coder）资料流压缩，
	//切换回效率较低的弹性内容的可变长度编码（CAVLC：Context Adaptive Variable Length Coder）系统。
	//大幅降低压缩效率（通常10~20%）和解码的硬件需求。
	//pX264Param->b_cabac = 1;
	//pX264Param->i_cabac_init_idc = 0;

	//关闭P帧的早期跳过决策。大量的时耗换回非常小的质量提升。
	pX264Param->analyse.b_fast_pskip = 0;

	//DCT抽样会丢弃看上去“多余”的DCT块。会增加编码效率，通常质量损失可以忽略。
	pX264Param->analyse.b_dct_decimate = 1;

	//open-gop是一个提高效率的编码技术。有三种模式:none-停用open-gop;normal-启用open-gop;
	//bluray-启用open-gop。一个效率较低的open-gop版本，因为normal模式无法用于蓝光编码.
	//某些解码器不完全支援open-gop资料流，这就是为什么此选项并未默认为启用。如果想启用open-gop，应该先测试所有可能用来拨放的解码器。
	pX264Param->b_open_gop = 1;

	//完全关闭内置去块滤镜，不推荐使用。
	//调节H.264标准中的内置去块滤镜。这是个性价比很高的选则, 关闭.
	//pX264Param->b_deblocking_filter = 1;
	//pX264Param->i_deblocking_filter_alphac0 = 0;
	//pX264Param->i_deblocking_filter_beta = 0;

	//去掉信噪比的计算，因为在解码端也可用到.
	pX264Param->analyse.b_psnr = 0; //是否使用信噪比.
	*/
	//限制输出文件的profile。这个参数将覆盖其它所有值，此选项能保证输出profile兼容的视频流。
	//如果使用了这个选项，将不能进行无损压缩。可选：baseline，main，high
	////x264_param_apply_profile(pX264Param, "baseline");
	//==============================================================
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