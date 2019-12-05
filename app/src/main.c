#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

// === hardware includes

#include "lomo.h"

// === config
// TODO: make a config file

#define LOMO_TTY "/dev/ttyUSB2"
#define HANTEK_TMC "/dev/usbtmc1"
#define ADC_PASS 2
#define ADC_AVERAGE 2

#define AMP (2.0)
#define FREQ_STEP1 (10)
#define FREQ_STOP1 (1200)
#define FREQ_STEP2 (50)
#define FREQ_STOP2 (5000)

// === threads

void *commander(void *);
void *worker(void *);

// === utils

int get_run();
void set_run(int run_new);
double get_time();

int hantek_write(int dev, const char *cmd);
int hantek_read(int dev, char *buf, int buf_length);

// === global variables

char dir_str[100];

pthread_rwlock_t run_lock;
int run;
char filename_cal[100];
char filename_adc[100];
time_t start_time;
struct tm start_time_struct;

// === program entry point

int main(int argc, char const *argv[])
{
	int ret = 0;

	pthread_t t_commander;
	pthread_t t_worker;

	int status;

	// === check input parameters

	if (argc < 2)
	{
		fprintf(stderr, "# E: Usage: calibration <experiment_name>\n");
		ret = -1;
		goto main_exit;
	}

	// === get start time of experiment

	start_time = time(NULL);
	localtime_r(&start_time, &start_time_struct);

	// === we need actual information w/o buffering

	setlinebuf(stdout);
	setlinebuf(stderr);

	// === initialize run state variable

	pthread_rwlock_init(&run_lock, NULL);
	run = 1;

	// === create dirictory in "20191012_153504_<experiment_name>" format

	snprintf(dir_str, 100, "%04d%02d%02d_%02d%02d%02d_%s",
		start_time_struct.tm_year + 1900,
		start_time_struct.tm_mon + 1,
		start_time_struct.tm_mday,
		start_time_struct.tm_hour,
		start_time_struct.tm_min,
		start_time_struct.tm_sec,
		argv[1]
	);
	status = mkdir(dir_str, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (status == -1)
	{
		fprintf(stderr, "# E: unable to create experiment directory (%s)\n", strerror(errno));
		ret = -2;
		goto main_exit;
	}

	// === create file names

	snprintf(filename_cal, 100, "%s/cal.dat", dir_str);
	snprintf(filename_adc, 100, "%s/adc.dat", dir_str);

	// printf("filename_cal \"%s\"\n", filename_cal);
	// printf("filename_adc = \"%s\"\n", filename_adc);

	// === now start threads

	pthread_create(&t_commander, NULL, commander, NULL);
	pthread_create(&t_worker, NULL, worker, NULL);

	// === and wait ...

	pthread_join(t_worker, NULL);

	// === cancel commander thread becouse we don't need it anymore
	// === and wait for cancelation finish

	pthread_cancel(t_commander);
	pthread_join(t_commander, NULL);

	main_exit:
	return ret;
}

// === commander function

void *commander(void *arg)
{
	(void) arg;

	char str[100];
	char *s;
	int ccount;

	while(get_run())
	{
		printf("> ");

		s = fgets(str, 100, stdin);
		if (s == NULL)
		{
			fprintf(stderr, "# E: Exit\n");
			set_run(0);
			break;
		}

		switch(str[0])
		{
			case 'h':
				printf(
					"Help:\n"
					"\th -- this help;\n"
					"\tq -- exit the program;\n");
				break;
			case 'q':
				set_run(0);
				break;
			default:
				ccount = strlen(str)-1;
				fprintf(stderr, "# E: Unknown commang (%.*s)\n", ccount, str);
				break;
		}
	}

	return NULL;
}

// === worker function

void *worker(void *arg)
{
	(void) arg;

	int r;

	int    han_fd;
	int    han_index;
	double han_time;
	double han_amp = AMP;
	double han_freq = 500;

	int    adc_fd;
	int    adc_index;
	double adc_time;
	double adc_value;
	double adc_average;

	FILE  *cal_fp;
	FILE  *adc_fp;

	FILE  *gp;

	char   buf[100];

	// === first we connect to instruments

	r = open(HANTEK_TMC, O_RDWR);
	if(r == -1)
	{
		fprintf(stderr, "# E: Unable to open hantek (%s)\n", strerror(errno));
		goto worker_exit;
	}
	han_fd = r;

	r = lomo_open(LOMO_TTY, &adc_fd);
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to open adc (%d)\n", r);
		set_run(0);
		goto worker_pps_close;
	}

	// === init pps
// fprintf(stderr, "1\n");

	hantek_write(han_fd, "dds:switch 0");
	hantek_write(han_fd, "dds:wave:mode 0");
	hantek_write(han_fd, "dds:type sine");
	hantek_write(han_fd, "dds:offset 0");
	hantek_write(han_fd, "dds:freq 10");
	hantek_write(han_fd, "dds:amp 4.0");
	hantek_write(han_fd, "dds:switch 1");

// fprintf(stderr, "2\n");
	// === init adc

	r = lomo_init(adc_fd);
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to init adc (%d)\n", r);
		goto worker_pps_deinit;
	}

	// === create vac file

	cal_fp = fopen(filename_cal, "w+");
	if(cal_fp == NULL)
	{
		fprintf(stderr, "# E: Unable to open file \"%s\" (%s)\n", filename_cal, strerror(ferror(cal_fp)));
		goto worker_pps_deinit;
	}

	setlinebuf(cal_fp);

	// === create adc file

	adc_fp = fopen(filename_adc, "w+");
	if(adc_fp == NULL)
	{
		fprintf(stderr, "# E: Unable to open file \"%s\" (%s)\n", filename_adc, strerror(ferror(adc_fp)));
		goto worker_vac_fp_close;
	}

	setlinebuf(adc_fp);

	// === write vac header

	r = fprintf(cal_fp,
		"# 1: index\n"
		"# 2: time, s\n"
		"# 3: hantek amp, V\n"
		"# 4: hantek freq, Hz\n"
		"# 5: adc average value, a.u. [0-1]\n");
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to print to file \"%s\" (%s)\n", filename_cal, strerror(r));
		goto worker_adc_fp_close;
	}

	// === write adc header

	r = fprintf(adc_fp,
		"# 1: index\n"
		"# 2: time, s\n"
		"# 3: adc value, a.u. [0-1]\n");
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to print to file \"%s\" (%s)\n", filename_adc, strerror(r));
		goto worker_adc_fp_close;
	}

	// === open gnuplot

	snprintf(buf, 100, "gnuplot > %s/gnuplot.log 2>&1", dir_str);
	gp = popen(buf, "w");
	if (gp == NULL)
	{
		fprintf(stderr, "# E: unable to open gnuplot pipe (%s)\n", strerror(errno));
		goto worker_adc_fp_close;
	}

	setlinebuf(gp);

	// === prepare gnuplot

	r = fprintf(gp,
		"set xrange [0:]\n"
		"set yrange [0:]\n"
		"set size 1,1\n"
		"set origin 0,0\n"
		"set ylabel \"ADC average signal, a.u. [0-1]\"\n"
	);
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to print to gp (%s)\n", strerror(r));
		goto worker_gp_close;
	}

	// === let the action begins!

	adc_index = 0;
	han_index = 0;

	while(get_run())
	{
// fprintf(stderr, "3\n");
		han_time = get_time();
		if (han_time < 0)
		{
			fprintf(stderr, "# E: Unable to get time\n");
			set_run(0);
			continue;
		}

		han_freq = (han_index + 1) * FREQ_STEP1;

		if (han_freq > FREQ_STOP1)
		{
			han_freq = FREQ_STOP1 + ((han_index + 1) - (FREQ_STOP1 / FREQ_STEP1)) * FREQ_STEP2;
		}

		if (han_freq > FREQ_STOP2)
		{
			set_run(0);
			continue;
		}

// fprintf(stderr, "4\n");
		memset(buf, 0, 100);
		snprintf(buf, 100, "dds:freq %.2lf", han_freq);
		r = hantek_write(han_fd, buf);
		if(r < 0)
		{
			fprintf(stderr, "# E: Unable to set freq (%d)\n", r);
			set_run(0);
			continue;
		}

// fprintf(stderr, "5\n");

		adc_average = 0;

		for (int i = 0; i < ADC_PASS + ADC_AVERAGE; i++)
		{
			adc_time = get_time();
			if (adc_time < 0)
			{
				fprintf(stderr, "# E: Unable to get time\n");
				set_run(0);
				goto worker_while_continue;
			}

			r = lomo_read_value(adc_fd, &adc_value);
			if(r < 0)
			{
				fprintf(stderr, "# E: Unable to read adc (%d)\n", r);
				set_run(0);
				goto worker_while_continue;
			}
// fprintf(stderr, "6\n");

			r = fprintf(adc_fp, "%d\t%le\t%le\n", adc_index, adc_time, adc_value);
			if(r < 0)
			{
				fprintf(stderr, "# E: Unable to print to file \"%s\" (%s)\n", filename_adc, strerror(r));
				set_run(0);
				goto worker_while_continue;
			}

			if (han_index == 0)
			{
				r = fprintf(gp,
					"set title \"t = %lf s\"\n"
					"set xlabel \"ADC index\"\n"
					"plot \"%s\" u 1:3 w l lw 1 notitle\n"
					"unset title\n",
					adc_time,
					filename_adc
				);
			}
			else
			{
				r = fprintf(gp,
					"set multiplot title \"t = %lf s\" layout 2,1\n"
					"set xlabel \"ADC index\"\n"
					"plot \"%s\" u 1:3 w l lw 1 notitle\n"
					"set xlabel \"Hantek Freq, Hz\"\n"
					"plot \"%s\" u 4:5 w l lw 1 notitle\n"
					"unset multiplot\n",
					adc_time,
					filename_adc,
					filename_cal
				);
			}
			if(r < 0)
			{
				fprintf(stderr, "# E: Unable to print to gp (%s)\n", strerror(r));
				set_run(0);
				goto worker_while_continue;
			}

			// if (adc_value >= 0.99)
			// {
			// 	set_run(0);
			// 	break;
			// }

			if (i >= ADC_PASS)
			{
				adc_average += adc_value;
			}

			adc_index++;
		}

		adc_average /= ADC_AVERAGE;

		r = fprintf(cal_fp, "%d\t%le\t%le\t%le\t%le\n", han_index, han_time, han_amp, han_freq, adc_average);
		if(r < 0)
		{
			fprintf(stderr, "# E: Unable to print to file \"%s\" (%s)\n", filename_cal, strerror(r));
			set_run(0);
			continue;
		}

		r = fprintf(gp,
			"set multiplot title \"t = %lf s\" layout 2,1\n"
			"set xlabel \"ADC index\"\n"
			"plot \"%s\" u 1:3 w l lw 1 notitle\n"
			"set xlabel \"Hantek Freq, V\"\n"
			"plot \"%s\" u 4:5 w l lw 1 notitle\n"
			"unset multiplot\n",
			adc_time,
			filename_adc,
			filename_cal
		);
		if(r < 0)
		{
			fprintf(stderr, "# E: Unable to print to gp (%s)\n", strerror(r));
			set_run(0);
			continue;
		}

		han_index++;

		if (adc_average >= 0.99)
		{
			set_run(0);
		}

		worker_while_continue:
		continue;
	}

// fprintf(stderr, "7\n");

	hantek_write(han_fd, "dds:switch 0");
	hantek_write(han_fd, "dds:offset 0");
	hantek_write(han_fd, "dds:amp 0.01");

// fprintf(stderr, "8\n");

	worker_gp_close:

	r = pclose(gp);
	if (r == -1)
	{
		fprintf(stderr, "# E: Unable to close gnuplot pipe (%s)\n", strerror(errno));
	}

	worker_adc_fp_close:

	r = fclose(adc_fp);
	if (r == EOF)
	{
		fprintf(stderr, "# E: Unable to close file \"%s\" (%s)\n", filename_adc, strerror(errno));
	}

	worker_vac_fp_close:

	r = fclose(cal_fp);
	if (r == EOF)
	{
		fprintf(stderr, "# E: Unable to close file \"%s\" (%s)\n", filename_cal, strerror(errno));
	}

	worker_pps_deinit:

	worker_adc_close:

	r = lomo_close(adc_fd);
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to close lomo (%d)\n", r);
	}

	worker_pps_close:

	r = close(han_fd);
	if(r == -1)
	{
		fprintf(stderr, "# E: Unable to close hantek (%s)\n", strerror(errno));
	}

	worker_exit:

	return NULL;
}

// === utils

int get_run()
{
	int run_local;
	pthread_rwlock_rdlock(&run_lock);
		run_local = run;
	pthread_rwlock_unlock(&run_lock);
	return run_local;
}

void set_run(int run_new)
{
	pthread_rwlock_wrlock(&run_lock);
		run = run_new;
	pthread_rwlock_unlock(&run_lock);
}

double get_time()
{
	static int first = 1;
	static struct timeval t_first = {0};
	struct timeval t = {0};
	double ret;
	int r;

	if (first == 1)
	{
		r = gettimeofday(&t_first, NULL);
		if (r == -1)
		{
			fprintf(stderr, "# E: unable to get time (%s)\n", strerror(errno));
			ret = -1;
		}
		else
		{
			ret = 0.0;
			first = 0;
		}
	}
	else
	{
		r = gettimeofday(&t, NULL);
		if (r == -1)
		{
			fprintf(stderr, "# E: unable to get time (%s)\n", strerror(errno));
			ret = -2;
		}
		else
		{
			ret = (t.tv_sec - t_first.tv_sec) * 1e6 + (t.tv_usec - t_first.tv_usec);
			ret /= 1e6;
		}
	}

	return ret;
}

int hantek_write(int dev, const char *cmd)
{
	int r;

	r = write(dev, cmd, strlen(cmd));
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to write to hantek (%s)\n", strerror(errno));
	}

	return r;
}

int hantek_read(int dev, char *buf, int buf_length)
{
	int r;

	r = read(dev, buf, buf_length);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to read from hantek (%s)\n", strerror(errno));
	}

	return r;
}
