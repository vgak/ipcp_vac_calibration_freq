#include <stdint.h>
#include <inttypes.h>

int lomo_open(const char *tty_filename, int *lomo_fd);
int lomo_close(int lomo_fd);
int lomo_init(int lomo_fd);
int lomo_read_value(int lomo_fd, double *value);
