// NALDecoder.cpp : Defines the entry point for the console application.   
//   

#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>   

typedef struct   
{   
	int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)   
	unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)   
	unsigned max_size;            //! Nal Unit Buffer size   
	int forbidden_bit;            //! should be always FALSE   
	int nal_reference_idc;        //! NALU_PRIORITY_xxxx   
	int nal_unit_type;            //! NALU_TYPE_xxxx       
	char *buf;                    //! contains the first byte followed by the EBSP   
	unsigned short lost_packets;  //! true, if packet loss is detected   
} NALU_t;   

FILE *bits = NULL;                //!< the bit stream file   
static int FindStartCode2 (unsigned char *Buf);//���ҿ�ʼ�ַ�0x000001   
static int FindStartCode3 (unsigned char *Buf);//���ҿ�ʼ�ַ�0x00000001   
//static bool flag = true;   
static int info2=0, info3=0;   


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

void OpenBitstreamFile (char *fn)   
{   
	if (NULL == (bits=fopen(fn, "rb")))   
	{   
		printf("open file error\n");   
		exit(0);   
	}   
}   

int GetAnnexbNALU (NALU_t *nalu)   
{   
	int pos = 0;   
	int StartCodeFound, rewind;   
	unsigned char *Buf;   

	if ((Buf = (unsigned char*)calloc (nalu->max_size , sizeof(char))) == NULL)    
		printf ("GetAnnexbNALU: Could not allocate Buf memory\n");   

	nalu->startcodeprefix_len=3;//��ʼ���������еĿ�ʼ�ַ�Ϊ3���ֽ�   

	if (3 != fread (Buf, 1, 3, bits))//�������ж�3���ֽ�   
	{   
		free(Buf);   
		return 0;   
	}   
	info2 = FindStartCode2 (Buf);//�ж��Ƿ�Ϊ0x000001    
	if(info2 != 1) {//������ǣ��ٶ�һ���ֽ�   
		if(1 != fread(Buf+3, 1, 1, bits))//��һ���ֽ�   
		{   
			free(Buf);   
			return 0;   
		}   
		info3 = FindStartCode3 (Buf);//�ж��Ƿ�Ϊ0x00000001   
		if (info3 != 1)//������ǣ�����-1   
		{    
			free(Buf);   
			return -1;   
		}   
		else {//�����0x00000001,�õ���ʼ�ַ�Ϊ4���ֽ�   
			pos = 4;   
			nalu->startcodeprefix_len = 4;   
		}   
	}

	else{//�����0x000001,�õ���ʼ�ַ�Ϊ3���ֽ�   
		nalu->startcodeprefix_len = 3;   
		pos = 3;   
	}   

	StartCodeFound = 0;//������һ����ʼ�ַ��ı�־λ   
	info2 = 0;   
	info3 = 0;   

	while (!StartCodeFound)   
	{   
		if (feof (bits))//�ж��Ƿ����ļ�β   
		{   
			nalu->len = (pos-1)-nalu->startcodeprefix_len;   
			memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);        
			nalu->forbidden_bit = (nalu->buf[0]>>7) & 1;   
			nalu->nal_reference_idc = (nalu->buf[0]>>5) & 3;   
			nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;   
			free(Buf);   
			return pos-1;   
		}   
		Buf[pos++] = fgetc (bits);//��һ���ֽڵ�BUF��   
		info3 = FindStartCode3(&Buf[pos-4]);//�ж��Ƿ�Ϊ0x00000001   
		if(info3 != 1)   
			info2 = FindStartCode2(&Buf[pos-3]);//�ж��Ƿ�Ϊ0x000001   
		StartCodeFound = (info2 == 1 || info3 == 1);   
	}   
	//flag = false;   


	// Here, we have found another start code (and read length of startcode bytes more than we should   
	// have.  Hence, go back in the file   
	rewind = (info3 == 1)? -4 : -3;   

	if (0 != fseek (bits, rewind, SEEK_CUR))//���ļ�ָ������˿�ʼ�ֽڵ��ֽ���   
	{   
		free(Buf);   
		printf("GetAnnexbNALU: Cannot fseek in the bit stream file");   
	}   

	// Here the Start code, the complete NALU, and the next start code is in the Buf.     
	// The size of Buf is pos, pos+rewind are the number of bytes excluding the next   
	// start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU   

	nalu->len = (pos+rewind)-nalu->startcodeprefix_len;   
	memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);   
	nalu->forbidden_bit = nalu->buf[0] & 0x80; //(nalu->buf[0]>>7) & 1;   
	nalu->nal_reference_idc = nalu->buf[0] & 0x60; //(nalu->buf[0]>>5) & 3;   
	nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;   
	free(Buf);   

	return (pos+rewind);//����������ʼ�ַ�֮�������ֽ���   
}   
void dump(NALU_t *n)   
{   
	if (!n)return;   
	//printf("a new nal:");   
	printf(" len: %d  ", n->len);   
	printf("nal_unit_type: %x\n", n->nal_unit_type);   
}   
int main(int argc, char* argv[])   
{   
	OpenBitstreamFile("akiyo_cif.h264");   
	NALU_t *n;   
	n = AllocNALU(8000000);   
	while(!feof(bits)) {   
		GetAnnexbNALU(n);   
		dump(n);   
	}   
	FreeNALU(n);   
	return 0;   
}   
static int FindStartCode2 (unsigned char *Buf)   
{   
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=1) return 0; //�ж��Ƿ�Ϊ0x000001,����Ƿ���1   
	else return 1;   
}   

static int FindStartCode3 (unsigned char *Buf)   
{   
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=0 || Buf[3] !=1) return 0;//�ж��Ƿ�Ϊ0x00000001,����Ƿ���1   
	else return 1;   
}   