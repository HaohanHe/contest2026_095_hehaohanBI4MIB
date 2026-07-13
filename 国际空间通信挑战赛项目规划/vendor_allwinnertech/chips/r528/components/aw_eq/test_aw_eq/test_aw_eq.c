#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eq.h"
#include "aweq_interface.h"

void usage(void)
{
	printf("*****************************\n");
	printf("*USAGE:\n");
	printf("* test_aw_eq -i input -o output -r samplerate -n bin_num -c chan\n");
	printf("* test_aw_eq -i input -o output -f config_file\n");
	printf("*The input audio data must be 16bit.\n");
	printf("*For example:\n");
	printf("* test_aw_eq -i /data/s16le_48000_stereo.pcm -o /data/eq_out.pcm -r 48000 -c 2\n");
	printf("* test_aw_eq -i /data/s16le_48000_stereo.pcm -o /data/eq_out.pcm -f /data/EQ.conf\n");
	printf("*****************************\n");
	return;
}

eq_core_prms_t core_prms[] =
{
	//{12, 100, 0.0, LOWPASS_SHELVING},
	//{-12, 200, 2.0, BANDPASS_PEAK},
	{-15, 1000, 1.9, BANDPASS_PEAK},
	//{-12, 4000, 0.3, BANDPASS_PEAK},
	//{12, 16000, 0.0, HIHPASS_SHELVING},
};

int main(int argvs, char** argv)
{
	short buffer[64];
	void *equalizer;
	FILE *in = NULL;
	FILE *out = NULL;
	char *config_file = NULL;
	int bin_num = 10, samplerate =48000, chan = 2;
	int items, idx=0;
	int enabled = 0;

#if 0
	int freq, gain;
	int temp = 0;
	float q;
#endif

	eq_prms_t prms;

	//in = fopen("/data/test.pcm", "rb");
	//out = fopen("/data/eq_out.pcm","wb");

	printf("How many argvs : %d\n", argvs);
	if(argvs <= 1)
	{
		usage();
		return -1;
	}

	while(idx < argvs)
	{
		if(!strcmp(argv[idx], "-h"))
		{
			idx++;
			usage();
			return 0;
		}
		if(!strcmp(argv[idx], "-i"))
		{
			idx++;
			printf("in file name:%s\n", argv[idx]);
			in = fopen(argv[idx], "rb");
			if(!in)
			{
				printf("Invaild input file!\n");
				return -1;
			}
			idx++;
		}
		if(!strcmp(argv[idx], "-o"))
		{
			idx++;
			printf("out file name:%s\n", argv[idx]);
			out = fopen(argv[idx], "wb");
			if(!out)
			{
				printf("Invaild Output file!\n");
				return -1;
			}
			idx++;
		}
		if(!strcmp(argv[idx], "-f"))
		{
			idx++;
			printf("config file name:%s\n", argv[idx]);
			config_file = argv[idx];
			idx++;
		}
		if(!strcmp(argv[idx], "-n"))
		{
			idx++;
			bin_num=atoi(argv[idx]);
		}
		if(!strcmp(argv[idx], "-r"))
		{
			idx++;
			samplerate=atoi(argv[idx]);
		}
		if(!strcmp(argv[idx], "-c"))
		{
			idx++;
			chan=atoi(argv[idx]);
		}
		idx++;
	}

	memset(&prms, 0x00, sizeof(eq_prms_t));
	prms.core_prms = calloc(sizeof(eq_core_prms_t), bin_num);

	if (config_file) {
		parse_config_to_eq_prms(config_file, &prms, &enabled);
		printf("parse config file result:\n");
		print_eq_prms(&prms);
	}
	
	if (!enabled) {

		printf("use default parameter:\n");
		printf("bin_num : %d\n", bin_num);
		printf("samplerate : %d\n", samplerate);
		printf("chan : %d\n", chan);

		prms.biq_num = bin_num;
		prms.sampling_rate = samplerate;
		prms.chan = chan;
		
	#if 0
		idx = 0;
		while(idx < bin_num)
		{
			printf("****************** %d *************", idx + 1);
			printf("\n****************** filter_types *************\n");
			printf("%d:Low_freq_shelving\n", LOWPASS_SHELVING);
			printf("%d:Bandpass_peak\n", BANDPASS_PEAK);
			printf("%d:High_freq_shelving\n", HIHPASS_SHELVING);
			printf("%d:Low_pass\n", LOWPASS);
			printf("%d:High_pass\n", HIGHPASS);
			printf("please input the filter type you want:\n");
			scanf("%d", &temp);
			switch (temp)
			{
			case LOWPASS_SHELVING:
				prms.core_prms[idx].type = LOWPASS_SHELVING;
				printf("Low_freq_shelving\n");

				printf("* freq point(hz) : ");
				scanf("%d", &freq);
				prms.core_prms[idx].fc = freq;

				printf("* gain( -20 - 20 db) : ");
				scanf("%d", &gain);
				prms.core_prms[idx].G = gain;

				prms.core_prms[idx].Q = 1;

				break;
			case BANDPASS_PEAK:
				prms.core_prms[idx].type = BANDPASS_PEAK;
				printf("Bandpass_peak\n");

				printf("* freq point(hz) : ");
				scanf("%d", &freq);
				prms.core_prms[idx].fc = freq;

				printf("* gain( -20 - 20 db) : ");
				scanf("%d", &gain);
				prms.core_prms[idx].G = gain;

				printf("* q( the bigger, the narrower: the smaller, the wider ) : ");
				scanf("%f", &q);
				prms.core_prms[idx].Q = q;
				break;
			case HIHPASS_SHELVING:
				prms.core_prms[idx].type = HIHPASS_SHELVING;
				printf("High_freq_shelving\n");

				printf("* freq point(hz) : ");
				scanf("%d", &freq);
				prms.core_prms[idx].fc = freq;

				printf("* gain( -20 - 20 db) : ");
				scanf("%d", &gain);
				prms.core_prms[idx].G = gain;

				prms.core_prms[idx].Q = 1;
				break;
			case LOWPASS:
				prms.core_prms[idx].type = LOWPASS;
				printf("Low_pass\n");

				printf("* freq point(hz) : ");
				scanf("%d", &freq);
				prms.core_prms[idx].fc = freq;
				prms.core_prms[idx].G = 0;
				prms.core_prms[idx].Q = 1;
				break;
			case HIGHPASS:
				prms.core_prms[idx].type = HIGHPASS;
				printf("High_pass\n");

				printf("* freq point(hz) : ");
				scanf("%d", &freq);
				prms.core_prms[idx].fc = freq;
				prms.core_prms[idx].G = 0;
				prms.core_prms[idx].Q = 1;
				break;
			default:
				printf("error input");
				return -1;
				break;
			}
			idx++;
		}

		chan = 1;
		prms.biq_num = 4;
		prms.sampling_rate = 16000;
		prms.chan = chan;
		prms.core_prms = calloc(sizeof(eq_core_prms_t), 4);
	#endif

		prms.core_prms[0].type = HIGHPASS;
		prms.core_prms[0].fc = 16;
		prms.core_prms[0].G = -11;
		prms.core_prms[0].Q = 1;


		prms.core_prms[1].type = BANDPASS_PEAK;
		prms.core_prms[1].fc = 21;
		prms.core_prms[1].G = 5;
		prms.core_prms[1].Q = 1;


		prms.core_prms[2].type = BANDPASS_PEAK;
		prms.core_prms[2].fc = 41;
		prms.core_prms[2].G = 5;
		prms.core_prms[2].Q = 1;

		prms.core_prms[3].type = BANDPASS_PEAK;
		prms.core_prms[3].fc = 79;
		prms.core_prms[3].G = -4;
		prms.core_prms[3].Q = 1;

		prms.core_prms[4].type = BANDPASS_PEAK;
		prms.core_prms[4].fc = 263;
		prms.core_prms[4].G = -4;
		prms.core_prms[4].Q = 1;

		prms.core_prms[5].type = HIGHPASS;
		prms.core_prms[5].fc = 15;
		prms.core_prms[5].G = 1;
		prms.core_prms[5].Q = 1;

		prms.core_prms[6].type = BANDPASS_PEAK;
		prms.core_prms[6].fc = 72;
		prms.core_prms[6].G = -2;
		prms.core_prms[6].Q = 1;

		prms.core_prms[7].type = BANDPASS_PEAK;
		prms.core_prms[7].fc = 3021;
		prms.core_prms[7].G = -8;
		prms.core_prms[7].Q = 1;

		prms.core_prms[8].type = BANDPASS_PEAK;
		prms.core_prms[8].fc = 11614;
		prms.core_prms[8].G = -1;
		prms.core_prms[8].Q = 1;

		prms.core_prms[9].type = BANDPASS_PEAK;
		prms.core_prms[9].fc = 23584;
		prms.core_prms[9].G = 0;
		prms.core_prms[9].Q = 1;
	}

	equalizer = eq_create(&prms);
	if(equalizer == NULL)
	{
		printf("create equalizer handle error!\n");
		return -1;
	}

	while(!feof(in))
	{
		items = fread(buffer, chan*sizeof(short), 32, in);
		eq_process(equalizer, buffer, items);
		items = fwrite(buffer, chan*sizeof(short), items, out);
	}

	if (equalizer) {
		eq_destroy(equalizer);
		equalizer = NULL;
	}

	if (prms.core_prms) {
		free(prms.core_prms);
		prms.core_prms = NULL;
	}

	if (in) {
		fclose(in);
		in = NULL;
	}

	if (out) {
		fclose(out);
		out = NULL;
	}

	return 0;
}

