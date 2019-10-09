/*
 * N Y X
 *
 * (c) 2016, dmitry@yurtaev.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <stdint.h>
//#include "../drivers/sscii/nyx.h"
#include "nyx.h"

#include <pci/pci.h>

#define MAP0_SIZE 65536
#define MAP0_MASK (MAP0_SIZE - 1)

#define MAP_SIZE 4096
#define MAP_MASK (MAP_SIZE - 1)

#define BOOT_OFFS 0x80000
#define SAFE_OFFS 0x10000

static uint32_t boot[16] = {
	0xFFFFFFFF,    //  DUMMYWORD,  DUMMYWORD
	0xAA995566,    //  SYNCWORD
	0x31E1FFFF,
	0x32610000 + (BOOT_OFFS&0xffff),     //  GENERAL1 multiboot[15:0] = 0000
	0x32810300 + ((BOOT_OFFS>>16)&0xff), //  GENERAL2 SPIx1 read cmd = 03, multiboot[23:16] = 08
	0x32A10000 + (SAFE_OFFS&0xffff),    //  GENERAL3 fallback[15:0] =  0000
	0x32C10300 + ((SAFE_OFFS>>16)&0xff), //  GENERAL4 read cmd, fallbach[23:16] = 01
	0x32E10000,
	0x30A10000,
	0x33012100,
	0x3201001F,
	0x30A1000E,
	0x20002000,    //  NOOP, NOOP
	0x20002000,
	0x20002000,
	0x20002000,
};

void msg(const char *msg, ...) {
	va_list ptr;

	va_start(ptr, msg);
	vfprintf(stderr, msg, ptr);
	fputc('\n', stderr);
	va_end(ptr);
}

void fatal(const char *msg, ...) {
	va_list ptr;

	va_start(ptr, msg);
	vfprintf(stderr, msg, ptr);
	fputc('\n', stderr);
	va_end(ptr);
	exit(EXIT_FAILURE);
}

//
void dump(uint8_t *p, size_t len, uint32_t start) {
	int i = 0;

	while(len-- > 0) {
		if((i & 0x1f) == 0) printf("%s%06x:", i ? "\n" : "", start + i);
		++i;
		printf(" %02x", *(uint8_t*)p++) ;
	}
	printf("\n");
}

nyx_dpram_mon *dpm;

// read flash from addr, len bytes, result in dpm->data2
void read_bytes(int addr, int len) {
	dpm->addr = addr;
	dpm->len = len;
//	printf("reading %d bytes at 0x%08x...\n", dpm->len, dpm->addr);
	dpm->cmd = 0x44444444; while(dpm->reply != 0x44444444) ;
	dpm->cmd = 0x11111111; while(dpm->reply != 0x11111111) ;
}

void program(int addr, int len) {
	dpm->addr = addr;
	dpm->len = len;
	dpm->cmd = 0x33333333; while(dpm->reply != 0x33333333) ;
	dpm->cmd = 0x11111111; while(dpm->reply != 0x11111111) ;
}

void erase_sector(int addr) {
	dpm->addr = addr;
//	printf("erasing sector at 0x%08x...\n", dpm->addr);
	dpm->cmd = 0x22222222; while(dpm->reply != 0x22222222) ;
	dpm->cmd = 0x11111111; while(dpm->reply != 0x11111111) ;
}

char *concat(char *dir, char *name)
{
	static char s[NAME_MAX+32];
	snprintf(s, NAME_MAX+32, "%s/%s", dir, name);
	return s;
}

int fread_hex(char *dir, char *name)
{
	FILE *f;
	int i = -1;
	char *s = concat(dir, name);

	if ((f = fopen(s, "r"))) {
		fscanf(f, "%x", &i);
		fclose(f);
	}

//	printf("%s -> %x\n", s, i);
	return i;
}

char *find_device()
{
	char *sys = "/sys/bus/pci/devices";
	DIR *dir = opendir(sys); // 0000:05:04.0/resource0";
	char *name = NULL;

	if (dir) {
		struct dirent *de;
		while ((de = readdir(dir))) {
			if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
				static char d[NAME_MAX+32];
				snprintf(d, NAME_MAX+32, "%s/%s", sys, de->d_name);

				if (fread_hex(d, "vendor") == 0x1067
				 && fread_hex(d, "device") == 0x55c2) {
					name = concat(d, "resource0");
					break;
				}
			}
		}
		closedir(dir);
	} else {
		msg("can't open dir %s", sys);
	}

	return name;
}

int load_bit_file(FILE *f, uint8_t **buf)
{
	uint16_t a;
	uint8_t b;
	uint32_t l;

	if (1 != fread(&a, 2, 1, f)) return -1;
	if (a != ntohs(9)) fatal("missing 9 header: %x", a);
	if (fseek(f, 9, SEEK_CUR)) return -3;

	if (1 != fread(&a, 2, 1, f)) return -4;
	if (a != ntohs(1)) return -5;
	if (1 != fread(&b, 1, 1, f)) return -6;
	if (b != 'a') return -7;

	if (1 != fread(&a, 2, 1, f)) return -8;
	if (fseek(f, ntohs(a), SEEK_CUR)) return -9;

	do {
		if (1 != fread(&b, 1, 1, f)) return -10;
		switch (b) {
		case 'b':
		case 'c':
		case 'd':
			if (1 != fread(&a, 2, 1, f)) return -11;
			if (fseek(f, ntohs(a), SEEK_CUR)) return -12;
			break;
		case 'e':
			if (1 != fread(&l, 4, 1, f)) return -13;
			l = ntohl(l);
			//msg("bitstream size: %d bytes", l);
			*buf = calloc(l, 255);
			if (!*buf) fatal("calloc(%d) failed\n", l);
			if (l != fread(*buf, 1, l, f)) return -14;
			return l;
		}
	} while (!feof(f));
	return -15;
}

#define OP_WRITE 0
#define OP_BOOT  1
#define OP_DUMP  2
#define OP_ERASE 3
#define OP_REBOOT  4
#define EEPROM_SIZE (16 * 131072)

int main(int argc, char* argv[])
{
	int fd;
	nyx_iomem *iomem;
	int i, j, a;
	char *resource;

	if (argc < 2) fatal("nyxflash v1.5 (c) 2018, dmitry@yurtaev.com\nusage: nyxflash <bitstream.bit> [ <offs> ]");

	int offs = argc > 2 ? strtol(argv[2], NULL, 16) : BOOT_OFFS;
	int op = OP_WRITE;
	int size = 0;
	uint8_t *buf = 0;

	if(!strcmp(argv[1], "BOOT")) {
		op = OP_WRITE;
		size = sizeof(boot);
		buf = (uint8_t*)boot;
		offs = 0;
	} else if (!strcmp(argv[1], "DUMP")) {
		// leave offs untouched
		op = OP_DUMP;
		size = 256;
		buf = calloc(size, 1);
		if (!buf) fatal("dump buffer calloc(%d) failed\n", size);
	} else if (!strcmp(argv[1], "ERASE")) {
		// leave offs untouched
		op = OP_ERASE;
		size = 1;
	} else if (!strcmp(argv[1], "REBOOT")) {
		op = OP_REBOOT;
	} else {
		op = OP_WRITE;
		FILE *f = fopen(argv[1], "r");
		if (!f) fatal("can't open input bitstream file %s", argv[1]);

		size = strlen(argv[1]);
		if (size > 3 && !strcmp(".bit", argv[1] + size - 4)) {
			size = load_bit_file(f, &buf);

			if (size <= 0) fatal("load_bit_file() size <= 0: %d", size);
			if (size != 340604) msg("suspicious bitstream size: %d", size);
			if (!buf) fatal("load_bit_file() returned NULL");
		} else {
			fseek(f, 0, SEEK_END);
			size = ftell(f);
			fseek(f, 0, SEEK_SET);

			if (size != 340604) msg("suspicious bitstream size: %d", size);

			buf = calloc(size, 255);
			if (!buf) fatal("bitsream buffer calloc(%d) failed\n", size);

			int rlen = fread(buf, 1, size, f);
			if (rlen != size) fatal("bitsream file read() returned %d, expected %d\n", rlen, size);
		}

		if (offs + size > EEPROM_SIZE) fatal("file doesn't fit into 16Mbit EEPROM");

		fclose(f);

		// swap bytes
		uint32_t *p = (uint32_t*)buf;
		for(i = (size) / 4 - 1; i >= 0; --i)
			p[i] = htonl(p[i]);
	}

	// -------------------- pci stuff -----------------------

	resource = find_device();
	if((fd = open(resource, O_RDWR | O_SYNC)) == -1) {
		fatal("can't open pci recource %s not - root?", resource);
	}

	iomem = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(iomem == (void *) -1) {
                perror("can't mmap");
		close(fd);
		exit(-1);
	}

	dpm = &iomem->dpram_mon;	// pointer to the DP RAM

	if ((dpm->magic & 0xffff0000) != 0x55c20000) fatal("can't find a YSSC2P card - invalid signature: 0x%x", dpm->magic);
	msg("found YSSC2P 0x%x", dpm->magic);

	uint8_t v_maj = (dpm->magic >> 8) & 0xff;
	uint8_t v_min = dpm->magic & 0xff;

	if(dpm->magic != 0x55c2ffff) {
		dpm->magic = 0x55c2ffff;
		msg("found YSSC2P v%d.%d - entering monitor mode", v_maj, v_min);
		while(dpm->reply != 0x11111111) ;
	}

	// ------------------------------------------------------

	int verify_errors = 0;

	if (op == OP_WRITE) {
		msg("writing %s %d bytes to 0x%x", argv[1], size, offs);

		fprintf(stderr, "erasing sector");
		for(a = offs; a < offs + size; a += 0x10000) {
			fprintf(stderr, " #%d", a >> 16);
			erase_sector(a);
		}
		msg("");

#define CHUNK 256

		fprintf(stderr, "writing sector");
		for(a = 0; a < size; a += 0x10000) {
			fprintf(stderr, " #%d", (offs + a) >> 16);
			int sector_errors = 0;
			for(i = 0; i < 0x10000; i += CHUNK) {
				int l = size - (a + i);
				if(l <= 0) break;
				if(l > CHUNK) l = CHUNK;

				for(j = 0; j < CHUNK; j++)
					dpm->data[j] = buf[a + i + j];

				//memcpy((void*)dpm->data, buf + a + i, CHUNK);
				uint32_t addr = offs + a + i;

				program(addr, l);
				read_bytes(addr, l);

				for(j = 0; j < l; j++) if(dpm->data2[j] != buf[a + i + j]) { ++sector_errors; ++verify_errors; }
			}
			if(sector_errors) fprintf(stderr, ":ERR");
		}
		msg("");

		if(verify_errors ) {
			msg("programming FAILED! %d verify errors", verify_errors);
		} else {
			msg("programming successful", verify_errors);
		}
	} else if(op == OP_DUMP) {
		msg("dumping contents at 0x%06x", offs);
		read_bytes(offs, 256);
		dump(dpm->data2, 256, offs);
	} else if(op == OP_ERASE) {
		for(a = offs; a < offs + size; a += 0x10000) {
			msg("erasing sector #%d", a >> 16);
			erase_sector(a);
		}
	} else if(op == OP_REBOOT) {
		struct pci_access *pacc;
		struct pci_dev    *dev;
		unsigned int conf4, conf10;
		unsigned int domain, bus, device, func;

		sscanf(resource, "/sys/bus/pci/devices/%x:%x:%x.%x", &domain, &bus, &device, &func);

		pacc = pci_alloc();
		pci_init(pacc);
		dev = pci_get_dev(pacc, domain, bus, device, func);
		conf4 = pci_read_long(dev, 0x04);
		conf10 = pci_read_long(dev, 0x10);

		//msg("dev: %s, 4:%x 10:%x\n", resource, conf4, conf10);
		msg("requesting FPGA reconfiguration");
		dpm->cmd = 0xeeeeeeee;
		sleep(1);
		//msg("4:%x 10:%x\n", pci_read_long(dev, 0x04), pci_read_long(dev, 0x10));
		pci_write_long (dev, 0x04, conf4);	// restore config after reboot
		pci_write_long (dev, 0x10, conf10);

		pci_cleanup(pacc);
	}

	msg("exiting monitor");
	dpm->cmd = 0x99999999;

	munmap(iomem, MAP_SIZE);
	close(fd);

	exit(verify_errors ? EXIT_FAILURE : EXIT_SUCCESS);
}
