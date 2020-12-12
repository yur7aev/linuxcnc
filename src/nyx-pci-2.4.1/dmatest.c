#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "nyx2.h"

#define NYXDF "/dev/nyx0"

void dump(unsigned char *s, int size)
{
	int i;

	for (i = 0; i < size; i++)
		printf("%02x ", s[i]);
	putchar('\n');
}

int main(int argc, char *argv[])
{
	static unsigned char sbuf[1024];
	unsigned char *buf;
	int fd, rc, i;
	int count = 32;

	if (argc > 1) count = atoi(argv[1]) * 4;
	if (count > 1024) count = 1024;
	printf("count=%d\n", count);

	fd = open(NYXDF, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "can't open " NYXDF "\n");
		return -1;
	}

/*
	buf = mmap(NULL, 512, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == (void *)-1) {
		fprintf(stderr, "mmap failed\n");
	} else {
		memset(buf, 0, 16);
		dump(buf, 16);
		read(fd, buf, 16);	// includes "magic" and "config" field
		dump(buf, 16);

		munmap(buf, 32);
	}
*/
	//ioctl(fd, 1, 4);	// use irq
	ioctl(fd, 1, 0);

//	memset(sbuf, 0, count);
	for (i = 0; i < count; i++)
		sbuf[i] = i;

	lseek(fd, offsetof(struct nyx_dpram, req.byte), SEEK_SET);
	//printf("writing...\n");
	write(fd, sbuf, count);

	lseek(fd, offsetof(struct nyx_dpram, req.byte), SEEK_SET);
	//printf("reading...\n");
	read(fd, sbuf, count);	// includes "magic" and "config" field

	printf("dumping...\n");
	dump(sbuf, count);

	ioctl(fd, 1, 3);
/*
	lseek(fd, offsetof(struct nyx_dpram, req.byte), SEEK_SET);
	read(fd, sbuf, count);	// includes "magic" and "config" field
	dump(sbuf, count);
*/
	close(fd);
	return 0;
}
