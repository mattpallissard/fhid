// bastardized from linux/samples/uhid/uhid-example.c

#include <errno.h>
#include <fcntl.h>
#include <linux/uhid.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * HID Report Desciptor
 * We emulate a basic 3 button mouse with wheel and 3 keyboard LEDs. This is
 * the report-descriptor as the kernel will parse it:
 *
 * INPUT(1)[INPUT]
 *   Field(0)
 *     Physical(GenericDesktop.Pointer)
 *     Application(GenericDesktop.Mouse)
 *     Usage(3)
 *       Button.0001
 *       Button.0002
 *       Button.0003
 *     Logical Minimum(0)
 *     Logical Maximum(1)
 *     Report Size(1)
 *     Report Count(3)
 *     Report Offset(0)
 *     Flags( Variable Absolute )
 *   Field(1)
 *     Physical(GenericDesktop.Pointer)
 *     Application(GenericDesktop.Mouse)
 *     Usage(3)
 *       GenericDesktop.X
 *       GenericDesktop.Y
 *       GenericDesktop.Wheel
 *     Logical Minimum(-128)
 *     Logical Maximum(127)
 *     Report Size(8)
 *     Report Count(3)
 *     Report Offset(8)
 *     Flags( Variable Relative )
 * OUTPUT(2)[OUTPUT]
 *   Field(0)
 *     Application(GenericDesktop.Keyboard)
 *     Usage(3)
 *       LED.NumLock
 *       LED.CapsLock
 *       LED.ScrollLock
 *     Logical Minimum(0)
 *     Logical Maximum(1)
 *     Report Size(1)
 *     Report Count(3)
 *     Report Offset(0)
 *     Flags( Variable Absolute )
 *
 * This is the mapping that we expect:
 *   Button.0001 ---> Key.LeftBtn
 *   Button.0002 ---> Key.RightBtn
 *   Button.0003 ---> Key.MiddleBtn
 *   GenericDesktop.X ---> Relative.X
 *   GenericDesktop.Y ---> Relative.Y
 *   GenericDesktop.Wheel ---> Relative.Wheel
 *   LED.NumLock ---> LED.NumLock
 *   LED.CapsLock ---> LED.CapsLock
 *   LED.ScrollLock ---> LED.ScrollLock
 *
 * This information can be verified by reading /sys/kernel/debug/hid/<dev>/rdesc
 * This file should print the same information as showed above.
 */

static unsigned char rdesc[] = {
	0x05, 0x01, /* USAGE_PAGE (Generic Desktop) */
	0x09, 0x02, /* USAGE (Mouse) */
	0xa1, 0x01, /* COLLECTION (Application) */
	0x09, 0x01, /* USAGE (Pointer) */
	0xa1, 0x00, /* COLLECTION (Physical) */
	0x85, 0x01, /* REPORT_ID (1) */
	0x05, 0x09, /* USAGE_PAGE (Button) */
	0x19, 0x01, /* USAGE_MINIMUM (Button 1) */
	0x29, 0x03, /* USAGE_MAXIMUM (Button 3) */
	0x15, 0x00, /* LOGICAL_MINIMUM (0) */
	0x25, 0x01, /* LOGICAL_MAXIMUM (1) */
	0x95, 0x03, /* REPORT_COUNT (3) */
	0x75, 0x01, /* REPORT_SIZE (1) */
	0x81, 0x02, /* INPUT (Data,Var,Abs) */
	0x95, 0x01, /* REPORT_COUNT (1) */
	0x75, 0x05, /* REPORT_SIZE (5) */
	0x81, 0x01, /* INPUT (Cnst,Var,Abs) */
	0x05, 0x01, /* USAGE_PAGE (Generic Desktop) */
	0x09, 0x30, /* USAGE (X) */
	0x09, 0x31, /* USAGE (Y) */
	0x09, 0x38, /* USAGE (WHEEL) */
	0x15, 0x81, /* LOGICAL_MINIMUM (-127) */
	0x25, 0x7f, /* LOGICAL_MAXIMUM (127) */
	0x75, 0x08, /* REPORT_SIZE (8) */
	0x95, 0x03, /* REPORT_COUNT (3) */
	0x81, 0x06, /* INPUT (Data,Var,Rel) */
	0xc0, /* END_COLLECTION */
	0xc0, /* END_COLLECTION */
	0x05, 0x01, /* USAGE_PAGE (Generic Desktop) */
	0x09, 0x06, /* USAGE (Keyboard) */
	0xa1, 0x01, /* COLLECTION (Application) */
	0x85, 0x02, /* REPORT_ID (2) */
	0x05, 0x08, /* USAGE_PAGE (Led) */
	0x19, 0x01, /* USAGE_MINIMUM (1) */
	0x29, 0x03, /* USAGE_MAXIMUM (3) */
	0x15, 0x00, /* LOGICAL_MINIMUM (0) */
	0x25, 0x01, /* LOGICAL_MAXIMUM (1) */
	0x95, 0x03, /* REPORT_COUNT (3) */
	0x75, 0x01, /* REPORT_SIZE (1) */
	0x91, 0x02, /* Output (Data,Var,Abs) */
	0x95, 0x01, /* REPORT_COUNT (1) */
	0x75, 0x05, /* REPORT_SIZE (5) */
	0x91, 0x01, /* Output (Cnst,Var,Abs) */
	0xc0, /* END_COLLECTION */
};

char INPUT_SUFFIX[] = "fhid";
char INPUT_PREFIX_ENVVAR[] = "XDG_RUNTIME_DIR";
char DEVICE_PATH[] = "/dev/uhid";

enum click { PRESS, RELEASE };
static int btn1_down = RELEASE;
static int btn2_down = RELEASE;

static int uhid_write(int fd, const struct uhid_event *ev)
{
	ssize_t ret;

	if ((ret = write(fd, ev, sizeof(*ev))) < 0)
		ret = -errno;
	else if (ret != sizeof(*ev))
		ret = -EFAULT;
	else
		ret = 0;

out:
	if (errno)
		fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
	errno = 0;
	return ret;
}

static int create(int fd)
{
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char *)ev.u.create.name, "test-uhid-device");
	ev.u.create.rd_data = rdesc;
	ev.u.create.rd_size = sizeof(rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a37;
	ev.u.create.version = 0;
	ev.u.create.country = 0;

	return uhid_write(fd, &ev);
}

static void destroy(int fd)
{
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;

	uhid_write(fd, &ev);
}

static int send_event(int fd)
{
	struct uhid_event ev;
	int ret;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
	ev.u.input.size = 5;

	ev.u.input.data[0] = 0x1;
	if (btn1_down == PRESS)
		ev.u.input.data[1] |= 0x1;
	if (btn2_down == PRESS)
		ev.u.input.data[1] |= 0x2;

	ret = uhid_write(fd, &ev);
	return ret;
}

static int keyboard(FILE *f, int fd)
{
	char buf[128];
	char *b = buf;
	size_t bsize = sizeof(buf);
	int ret, i, j, k;

	int toggle[] = { RELEASE, PRESS};
	int *button[] = { &btn1_down, &btn2_down };

	if ((ret = getline(&b, &bsize, f)) == -1)
		return 1;

	if ((i = buf[0] - '0') > 4 || i < 1)
		return 1;

	j = i % 2;
	k = (i - 2) > 0;

	fprintf(stdout,
		"recieved: %s_%s\n",
		j ? "press" : "release",
		k ? "right" : "left");

	*(button[k]) = toggle[j];
	if ((ret = send_event(fd)))
		*(button[k]) = toggle[!j];

	return ret;
}

int main(int argc, char **argv)
{
	int dfd, ifd;
	int ret = EXIT_SUCCESS;
	struct timespec ts = { 0, 500000 };
	const char *path = DEVICE_PATH;
	char fs[1024] = "";
	sprintf(fs, "%s/%s", getenv(INPUT_PREFIX_ENVVAR), INPUT_SUFFIX);

	struct pollfd pfds[2];

	if (!access(fs, F_OK))
		unlink(fs);

	umask(0000);
	mkfifo(fs, 0666);

	fprintf(stdout, "open: %s\n", fs);
	if ((ifd = open(fs, O_RDWR | O_CREAT | O_CLOEXEC)) < 0)
		goto out;

	fprintf(stdout, "open: %s\n", path);
	if ((dfd = open(path, O_RDWR | O_CLOEXEC)) < 0)
		goto out;

	FILE *rx = fdopen(ifd, "r");
	setvbuf(rx, NULL, _IONBF, 1024);
	setvbuf(stdout, NULL, _IONBF, 1024);

	if ((ret = create(dfd)))
		goto cleanup;

	pfds[0].fd = ifd;
	pfds[0].events = POLLIN;
	pfds[1].fd = dfd;
	pfds[1].events = POLLIN;

	for (;;) {
		if ((ret = poll(pfds, 2, -1)) < 0)
			break;

		if (pfds[1].revents & POLLHUP)
			break;

		if (pfds[0].revents & POLLIN)
			if ((ret = keyboard(rx, dfd)))
				break;

		nanosleep(&ts, NULL);
	}

cleanup:
	destroy(dfd);
	destroy(ifd);
out:
	if (errno)
		fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
	return ret;
}
