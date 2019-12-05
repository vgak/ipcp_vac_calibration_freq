#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "lomo.h"
#include "lomo_mdr_cmd.h"

// typedef struct
// {
// 	uint8_t start;
// 	uint8_t length; //(w/o crc)
// 	uint8_t dev_addr;
// 	uint8_t dev_cmd;
// 	uint8_t dev_cmd_attr[?];
// 	uint8_t crc;
// } packet_t;

static int tty_config(int fd, speed_t speed);
static int lomo_write(int lomo_fd, const uint8_t *cmd, int cmd_length);
static int lomo_read(int lomo_fd, uint8_t *buffer, int buffer_size);

static uint8_t lomo_get_packet_crc(const uint8_t *buffer, size_t buffer_length);

int lomo_open(const char *tty_filename, int *lomo_fd)
{
	int ret = 0;
	int fd;
	int r;

	fd = open(tty_filename, O_RDWR | O_NOCTTY);
	if (fd == -1)
	{
		fprintf(stderr, "# E: unable to open LOMO [%s] (%s)\n", tty_filename, strerror(errno));
		ret = -1;
		goto lomo_open_exit;
	}

	r = tty_config(fd, B115200);
	if (r == -1)
	{
		ret = -2;
		close(fd);
		goto lomo_open_exit;
	}

	*lomo_fd = fd;

	lomo_open_exit:
	return ret;
}

int lomo_close(int lomo_fd)
{
	int ret = 0;
	int r;

	r = close(lomo_fd);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to close LOMO (%s)\n", strerror(errno));
		ret = -1;
		goto lomo_close_exit;
	}

	lomo_close_exit:
	return ret;
}

int lomo_init(int lomo_fd)
{
	int ret = 0;
	int r;
	int n;

// SEND:    0x1B 0x04 0x8A 0x00 0xFA

	uint8_t cmd[] = {PACKET_START, 0x00, HUB_ADDR, CMD_HUB_RESET_SYSTEM, 0x00};
	cmd[1] = sizeof(cmd)-1;
	cmd[sizeof(cmd)-1] = lomo_get_packet_crc(cmd, sizeof(cmd)-1);

	r = lomo_write(lomo_fd, cmd, sizeof(cmd));
	if (r < 0)
	{
		ret = -1;
		goto lomo_init_exit;
	}

// RECEIVE: 0x1B 0x04 0x8A 0x00 0xFA

	uint8_t buffer[32] = {0};
	r = lomo_read(lomo_fd, buffer, sizeof(buffer));
	if (r < 0)
	{
		ret = -2;
		goto lomo_init_exit;
	}

	n = r;

	int check_err =
		(buffer[0] != PACKET_START) ||
		(buffer[1] != sizeof(cmd)-1) ||
		(buffer[2] != HUB_ADDR) ||
		(buffer[3] != RET_HUB_HOME_FIND) ||
		(buffer[n-1] != lomo_get_packet_crc(buffer, n-1));

	if (check_err)
	{
		ret = -3;
		goto lomo_init_exit;
	}

// RECEIVE: 0x1B 0x0F 0x10 0x01 0x30 0x47 0x01 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x40
// ID,VERSION,SUB_VERSION,+8byte серийный номер

	r = lomo_read(lomo_fd, buffer, sizeof(buffer));
	if (r < 0)
	{
		ret = -4;
		goto lomo_init_exit;
	}

	n = r;

	check_err =
		(buffer[0] != PACKET_START) ||
		(buffer[1] != 0x0F) ||
		(buffer[2] != 0x10) ||
		(buffer[3] != RET_DEVICE_VERSION) ||
		(buffer[n-1] != lomo_get_packet_crc(buffer, n-1));

	if (check_err)
	{
		ret = -5;
		goto lomo_init_exit;
	}
/*
	printf("id = 0x%02x\n", buffer[4]);
	printf("version = %d\n", buffer[5]);
	printf("sub_version = %d\n", buffer[6]);
	printf("serial = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		buffer[7], buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14]);
//*/

// RECEIVE: 0x1B 0x04 0x8A 0x01 0xA4

	r = lomo_read(lomo_fd, buffer, sizeof(buffer));
	if (r < 0)
	{
		ret = -6;
		goto lomo_init_exit;
	}

	n = r;

	check_err =
		(buffer[0] != PACKET_START) ||
		(buffer[1] != sizeof(cmd)-1) ||
		(buffer[2] != HUB_ADDR) ||
		(buffer[3] != RET_HUB_END_FIND) ||
		(buffer[n-1] != lomo_get_packet_crc(buffer, n-1));

	if (check_err)
	{
		ret = -7;
		goto lomo_init_exit;
	}

	lomo_init_exit:
	return ret;
}

int lomo_read_value(int lomo_fd, double *value)
{
	int ret = 0;
	int r;
	int n;

	// uint8_t cmd[] = {PACKET_START, 0x04, HUB_ADDR, CMD_HUB_RESET_SYSTEM, 0x00};
	// uint32_t num = 19200;
	uint32_t num = 40000;
	// uint32_t num = 5000;
	uint8_t cmd[] = {PACKET_START, 0x00, 0x10, CMD_ADS1252_N_MEASURE, 0x00, 0x00, 0x00, 0x00, 0x00};
	cmd[1] = sizeof(cmd)-1;
	cmd[4] = num & 0xff;
	cmd[5] = (num >> 8) & 0xff;
	cmd[6] = (num >> 16) & 0xff;
	cmd[7] = (num >> 24) & 0xff;
	cmd[sizeof(cmd)-1] = lomo_get_packet_crc(cmd, sizeof(cmd)-1);

	r = lomo_write(lomo_fd, cmd, sizeof(cmd));
	if (r < 0)
	{
		ret = -1;
		goto lomo_read_value_exit;
	}

	uint8_t buffer[32] = {0};
	r = lomo_read(lomo_fd, buffer, sizeof(buffer));
	if (r < 0)
	{
		ret = -2;
		goto lomo_read_value_exit;
	}

	n = r;

	uint64_t val = 0;
	val = buffer[10];
	val <<= 8;
	val |= buffer[9];
	val <<= 8;
	val |= buffer[8];
	val <<= 8;
	val |= buffer[7];
	val <<= 8;
	val |= buffer[6];
	val <<= 8;
	val |= buffer[5];
	val <<= 8;
	val |= buffer[4];

	uint32_t num_ret = 0;
	num_ret = buffer[14];
	num_ret <<= 8;
	num_ret |= buffer[13];
	num_ret <<= 8;
	num_ret |= buffer[12];
	num_ret <<= 8;
	num_ret |= buffer[11];
/*
	printf("val = %lu, num_ret = %u, double = %le, voltage = %le\n",
		val,
		num_ret,
		(((double)val)/((double)num_ret))/(1L << 24),
		2.5 * (((double)val)/((double)num_ret))/(1L << 24)
		);
//*/
	int check_err =
		(buffer[0] != PACKET_START) ||
		(buffer[1] != n - 1) ||
		(buffer[2] != 0x10) ||
		(buffer[3] != RET_ADS1252_N_MEASURE) ||
		(buffer[n-1] != lomo_get_packet_crc(buffer, n-1));

	if (check_err)
	{
		ret = -3;
		goto lomo_read_value_exit;
	}

	*value = (((double)val)/((double)num_ret))/(1L << 24);

	lomo_read_value_exit:
	return ret;
}

static uint8_t lomo_get_packet_crc(const uint8_t *buffer, size_t buffer_length)
{
	uint8_t crc = 0;

	for (size_t i = 0; i < buffer_length; ++i)
	{
		uint8_t byte = buffer[i];

		for (int j = 0; j < 8; ++j)
		{
			if ((byte ^ crc) & 0x01)
			{
				crc = 0x80 | ((crc ^ 0x18) >> 1);
			}
			else
			{
				crc >>= 1;
			}

			byte >>= 1;
		}
	}

	return crc;
}

static int tty_config(int fd, speed_t speed)
{
	struct termios serial_port_settings;
	int r;

	// Get the current attributes of the Serial port
	r = tcgetattr(fd, &serial_port_settings);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to get terminal settings (%s)\n", strerror(errno));
		return -1;
	}

	r = cfsetispeed(&serial_port_settings, speed);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to set terminal input speed (%s)\n", strerror(errno));
		return -1;
	}

	r = cfsetospeed(&serial_port_settings, speed);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to set terminal output speed (%s)\n", strerror(errno));
		return -1;
	}

	serial_port_settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
	serial_port_settings.c_lflag &= ~(ECHOCTL | ECHOKE);

	serial_port_settings.c_oflag &= ~(OPOST | ONLCR | OCRNL);

	serial_port_settings.c_iflag &= ~(INLCR | IGNCR | ICRNL | IGNBRK);
	serial_port_settings.c_iflag &= ~(IUCLC | PARMRK);
	serial_port_settings.c_iflag &= ~(INPCK | ISTRIP);
	serial_port_settings.c_iflag &= ~(IXON | IXOFF | IXANY);

	serial_port_settings.c_cflag |= (CREAD | CLOCAL);
	serial_port_settings.c_cflag |= (CS8);
	serial_port_settings.c_cflag &= ~(CSTOPB);
	serial_port_settings.c_cflag &= ~(PARENB | PARODD | CMSPAR);
	serial_port_settings.c_cflag &= ~(CRTSCTS);
	// serial_port_settings.c_cflag &= ~(CNEW_RTSCTS);

	serial_port_settings.c_cc[VMIN] = 1; // Read at least 255 characters
	serial_port_settings.c_cc[VTIME] = 0; // Wait indefinetly

	r = tcsetattr(fd, TCSANOW, &serial_port_settings);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to setup terminal settings (%s)\n", strerror(errno));
		return -1;
	}

	// Discards old data in rx and tx buffer
	r = tcflush(fd, TCIOFLUSH);
	if (r == -1)
	{
		fprintf(stderr, "# E: unable to flush input of terminal (%s)\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int lomo_write(int lomo_fd, const uint8_t *cmd, int cmd_length)
{
	int ret = 0;
	int n;

/*
printf("write = ");
for (size_t j = 0; j < cmd_length; ++j)
{
	printf("0x%02x ", cmd[j]);
}
printf("\n");
//*/

	n = write(lomo_fd, cmd, cmd_length);
	if (n == -1)
	{
		fprintf(stderr, "# E: unable to write to LOMO (%s)\n", strerror(errno));
		ret = -1;
		goto lomo_write_exit;
	}
	else if (n != cmd_length)
	{
		fprintf(stderr, "# E: unable to write full command to LOMO (%d bytes written)\n", n);
		ret = -2;
		goto lomo_write_exit;
	}

	ret = n;

	lomo_write_exit:
	return ret;
}

static int lomo_read(int lomo_fd, uint8_t *buffer, int buffer_size)
{
	int ret = 0;
	int n;
	int i;

	int exit_count = 2;

	i = 0;
	while (i < buffer_size)
	{
		n = read(lomo_fd, &buffer[i], 1);
		if (n == -1)
		{
			fprintf(stderr, "# E: unable to read from LOMO (%s)\n", strerror(errno));
			ret = -1;
			goto lomo_read_exit;
		}
		else if (n != 1)
		{
			fprintf(stderr, "# E: unable to read a symbol from LOMO (%d)\n", n);
			ret = -2;
			goto lomo_read_exit;
		}

		i++;

		if(i == 2)
		{
			exit_count = buffer[1] + 1;
			if (exit_count > buffer_size)
			{
				ret = -3;
				goto lomo_read_exit;
			}
		}

		if (i == exit_count)
		{
			break;
		}
	}

	lomo_read_exit:
	if ((i > 0) && (ret == 0))
	{
		ret = i;
	}

/*
printf("read = ");
for (size_t j = 0; j < i; ++j)
{
	printf("0x%02x ", buffer[j]);
}
printf("\n");
//*/

	return ret;
}