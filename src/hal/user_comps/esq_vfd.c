/*
    gs2_vfd.c
    Copyright (C) 2013 Sebastian Kuzminsky
    Copyright (C) 2009 John Thornton
    Copyright (C) 2007, 2008 Stephen Wille Padnos, Thoth Systems, Inc.

    Based on a work (test-modbus program, part of libmodbus) which is
    Copyright (C) 2001-2005 Stéphane Raimbault <stephane.raimbault@free.fr>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA.


    This is a userspace program that interfaces the ESQ VFD. Based on GS2 VFD
    component.

*/
#define ULAPI

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include "rtapi.h"
#include "hal.h"
#include <modbus.h>

/* Read Registers:
	0x6000 = status register 1
	0x6001 = status register 2
	0x6002 = fault code
#define START_REGISTER_R	0x6000
#define NUM_REGISTERS_R		3
 write registers:
	0x2000 = control register
	0x3000 = freq set
	0x3001 = PID ref
	0x3002 = PID f/b
	0x3003 = torque setting
	0x3004 = upper limit freq fwd
	0x3005 = upper limit freq rev
	0x3006 = upper limit torque fwd
	0x3007 = upper limit braking torque
	0x3008 = spec control
	0x3009 = virtual terminal 1
	0x300A = virtual terminal 2
//#define START_REGISTER_W	0x091A
//#define NUM_REGISTERS_W		5
*/

/* HAL data struct */
typedef struct {
  hal_bit_t	*ready;
  hal_bit_t	*running_fwd;
  hal_bit_t	*running_rev;
  hal_bit_t	*running;
  hal_bit_t	*stopped;
  hal_bit_t	*faulted;
  hal_u32_t	*fault_code;

  hal_bit_t	*fwd;
  hal_bit_t	*rev;
  hal_float_t	*freq_cmd;

  uint16_t old_ctl;
  uint16_t old_freq;
  int error_cnt;
  hal_float_t	looptime;
} haldata_t;

static int done;
char *modname = "esq_vfd";

static struct option long_options[] = {
    {"bits", 1, 0, 'b'},
    {"device", 1, 0, 'd'},
    {"debug", 0, 0, 'g'},
    {"help", 0, 0, 'h'},
    {"name", 1, 0, 'n'},
    {"parity", 1, 0, 'p'},
    {"rate", 1, 0, 'r'},
    {"stopbits", 1, 0, 's'},
    {"target", 1, 0, 't'},
    {"verbose", 0, 0, 'v'},
    {"disable", no_argument, NULL, 'X'},
    {0,0,0,0}
};

static char *option_string = "gb:d:hn:p:r:s:t:vX";

static char *bitstrings[] = {"5", "6", "7", "8", NULL};

// The old libmodbus (v2?) used strings to indicate parity, the new one
// (v3.0.1) uses chars.  The gs2_vfd driver gets the string indicating the
// parity to use from the command line, and I don't want to change the
// command-line usage.  The command-line argument string must match an
// entry in paritystrings, and the index of the matching string is used as
// the index to the parity character for the new libmodbus.
static char *paritystrings[] = {"even", "odd", "none", NULL};
static char paritychars[] = {'E', 'O', 'N'};

static char *ratestrings[] = {"110", "300", "600", "1200", "2400", "4800", "9600",
    "19200", "38400", "57600", "115200", NULL};
static char *stopstrings[] = {"1", "2", NULL};

static void quit(int sig) {
    done = 1;
}

int match_string(char *string, char **matches) {
    int len, which, match;
    which=0;
    match=-1;
    if ((matches==NULL) || (string==NULL)) return -1;
    len = strlen(string);
    while (matches[which] != NULL) {
        if ((!strncmp(string, matches[which], len)) && (len <= strlen(matches[which]))) {
            if (match>=0) return -1;        // multiple matches
            match=which;
        }
        ++which;
    }
    return match;
}

#define ESQ_REG_FREQCMD 0x3000
#define ESQ_REG_CONTROL 0x2000


void write_data(modbus_t *mb_ctx, haldata_t *data) {
    uint16_t ctl = 0;
    uint16_t freq = abs((int)(*(data->freq_cmd) * 10));

    if (data == NULL || mb_ctx == NULL) return;

    if (*data->fwd) {
	ctl = 1;
    } else if (*data->rev) {
	ctl = 2;
    } else {
	ctl = 5;
    }

    if (freq != data->old_freq)
	modbus_write_register(mb_ctx, ESQ_REG_FREQCMD, freq);

    if (ctl != data->old_ctl)
	modbus_write_register(mb_ctx, ESQ_REG_CONTROL, ctl);
}

void usage(int argc, char **argv) {
    printf("Usage:  %s [options]\n", argv[0]);
    printf(
    "This is a userspace HAL program, typically loaded using the halcmd \"loadusr\" command:\n"
    "    loadusr gs2_vfd\n"
    "There are several command-line options.  Options that have a set list of possible values may\n"
    "    be set by using any number of characters that are unique.  For example, --rate 5 will use\n"
    "    a baud rate of 57600, since no other available baud rates start with \"5\"\n"
    "-b or --bits <n> (default 8)\n"
    "    Set number of data bits to <n>, where n must be from 5 to 8 inclusive\n"
    "-d or --device <path> (default /dev/ttyS0)\n"
    "    Set the name of the serial device node to use\n"
    "-v or --verbose\n"
    "    Turn on verbose mode.\n"
    "-g or --debug\n"
    "    Turn on debug mode.  This will cause all modbus messages to be\n"
    "    printed in hex on the terminal.\n"
    "-n or --name <string> (default gs2_vfd)\n"
    "    Set the name of the HAL module.  The HAL comp name will be set to <string>, and all pin\n"
    "    and parameter names will begin with <string>.\n"
    "-p or --parity {even,odd,none} (defalt odd)\n"
    "    Set serial parity to even, odd, or none.\n"
    "-r or --rate <n> (default 38400)\n"
    "    Set baud rate to <n>.  It is an error if the rate is not one of the following:\n"
    "    110, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200\n"
    "-s or --stopbits {1,2} (default 1)\n"
    "    Set serial stop bits to 1 or 2\n"
    "-t or --target <n> (default 1)\n"
    "    Set MODBUS target (slave) number.  This must match the device number you set on the GS2.\n"
    );
}

void read_data(modbus_t *mb_ctx, haldata_t *data) {
    uint16_t buf[3];	/* a little padding in there */
    int retval;

    if (data == NULL || mb_ctx == NULL) return;

    retval = modbus_read_registers(mb_ctx, 0x6000, 3, buf);

    if (retval == 3) {
	*data->running_fwd = *data->running = *data->running_rev = *data->faulted = 0;
	*data->ready = 1;

	switch (buf[0]) {
	case 1: *data->running_fwd = *data->running = 1; break;
	case 2: *data->running_rev = *data->running = 1; break;
	case 3: break; // stop
	case 4: // fault
	case 5: // powoff?
	default: *data->faulted = 1; break;
	}

	*data->ready = buf[1] & 1;
	*data->fault_code = buf[2];
	data->error_cnt = 0;
    } else {
	if(data->error_cnt++ > 10) {
	    *data->ready = 0;
	}
    }
}

int main(int argc, char **argv)
{
    int retval = 0;
    modbus_t *mb_ctx;
    haldata_t *haldata;
    int slave;
    int hal_comp_id;
    struct timespec loop_timespec, remaining;
    int baud, bits, stopbits, verbose, debug;
    char *device, *endarg;
    char parity;
    int opt;
    int argindex, argvalue;
    int enabled;

    float accel_time = 10.0;
    float decel_time = 0.0;  // this means: coast to a stop, don't try to control deceleration time
    int braking_resistor = 0;


    done = 0;

    // assume that nothing is specified on the command line
    baud = 19200;
    bits = 8;
    stopbits = 1;
    debug = 0;
    verbose = 0;
    device = "/dev/ttyS1";
    parity = 'E';
    enabled = 1;

    /* slave / register info */
    slave = 1;

    // process command line options
    while ((opt=getopt_long(argc, argv, option_string, long_options, NULL)) != -1) {
        switch(opt) {
            case 'X':  // disable by default on startup
                enabled = 0;
                break;
            case 'b':   // serial data bits, probably should be 8 (and defaults to 8)
                argindex=match_string(optarg, bitstrings);
                if (argindex<0) {
                    printf("gs2_vfd: ERROR: invalid number of bits: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                bits = atoi(bitstrings[argindex]);
                break;
            case 'd':   // device name, default /dev/ttyS0
                // could check the device name here, but we'll leave it to the library open
                if (strlen(optarg) > FILENAME_MAX) {
                    printf("gs2_vfd: ERROR: device node name is too long: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                device = strdup(optarg);
                break;
            case 'g':
                debug = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'n':   // module base name
                if (strlen(optarg) > HAL_NAME_LEN-20) {
                    printf("gs2_vfd: ERROR: HAL module name too long: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                modname = strdup(optarg);
                break;
            case 'p':   // parity, should be a string like "even", "odd", or "none"
                argindex=match_string(optarg, paritystrings);
                if (argindex<0) {
                    printf("gs2_vfd: ERROR: invalid parity: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                parity = paritychars[argindex];
                break;
            case 'r':   // Baud rate, 19200 default
                argindex=match_string(optarg, ratestrings);
                if (argindex<0) {
                    printf("gs2_vfd: ERROR: invalid baud rate: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                baud = atoi(ratestrings[argindex]);
                break;
            case 's':   // stop bits, defaults to 1
                argindex=match_string(optarg, stopstrings);
                if (argindex<0) {
                    printf("gs2_vfd: ERROR: invalid number of stop bits: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                stopbits = atoi(stopstrings[argindex]);
                break;
            case 't':   // target number (MODBUS ID), default 1
                argvalue = strtol(optarg, &endarg, 10);
                if ((*endarg != '\0') || (argvalue < 1) || (argvalue > 254)) {
                    printf("gs2_vfd: ERROR: invalid slave number: %s\n", optarg);
                    retval = -1;
                    goto out_noclose;
                }
                slave = argvalue;
                break;
            case 'h':
            default:
                usage(argc, argv);
                exit(0);
                break;
        }
    }

    printf("%s: device='%s', baud=%d, parity='%c', bits=%d, stopbits=%d, address=%d, enabled=%d\n",
           modname, device, baud, parity, bits, stopbits, slave, enabled);
    /* point TERM and INT signals at our quit function */
    /* if a signal is received between here and the main loop, it should prevent
            some initialization from happening */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    /* Assume 19.2k E-8-1 serial settings, device 1 */
    mb_ctx = modbus_new_rtu(device, baud, parity, bits, stopbits);
    if (mb_ctx == NULL) {
        printf("%s: ERROR: couldn't open modbus serial device: %s\n", modname, modbus_strerror(errno));
        goto out_noclose;
    }

    /* the open has got to work, or we're out of business */
    if (((retval = modbus_connect(mb_ctx))!=0) || done) {
        printf("%s: ERROR: couldn't open serial device: %s\n", modname, modbus_strerror(errno));
        goto out_noclose;
    }

    modbus_set_debug(mb_ctx, debug);

    modbus_set_slave(mb_ctx, slave);

    /* create HAL component */
    hal_comp_id = hal_init(modname);
    if ((hal_comp_id < 0) || done) {
        printf("%s: ERROR: hal_init failed\n", modname);
        retval = hal_comp_id;
        goto out_close;
    }

    /* grab some shmem to store the HAL data in */
    haldata = (haldata_t *)hal_malloc(sizeof(haldata_t));
    if ((haldata == 0) || done) {
        printf("%s: ERROR: unable to allocate shared memory\n", modname);
        retval = -1;
        goto out_close;
    }

    retval = hal_pin_bit_newf(HAL_OUT, &(haldata->ready),       hal_comp_id, "%s.ready", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_bit_newf(HAL_OUT, &(haldata->running_fwd), hal_comp_id, "%s.running-fwd", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_bit_newf(HAL_OUT, &(haldata->running_rev), hal_comp_id, "%s.running-rev", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_bit_newf(HAL_OUT, &(haldata->running),     hal_comp_id, "%s.running", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_bit_newf(HAL_OUT, &(haldata->stopped),     hal_comp_id, "%s.stopped", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_bit_newf(HAL_OUT, &(haldata->faulted),     hal_comp_id, "%s.faulted", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_u32_newf(HAL_OUT, &(haldata->fault_code),  hal_comp_id, "%s.fault-code", modname); if (retval!=0) goto out_closeHAL;

    retval = hal_pin_bit_newf(HAL_IN, &(haldata->fwd), hal_comp_id, "%s.fwd", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_bit_newf(HAL_IN, &(haldata->rev), hal_comp_id, "%s.rev", modname); if (retval!=0) goto out_closeHAL;
    retval = hal_pin_float_newf(HAL_RW, &(haldata->freq_cmd), hal_comp_id, "%s.freq-cmd", modname); if (retval!=0) goto out_closeHAL;

    /* make default data match what we expect to use */
    *(haldata->ready) = 0;
    *(haldata->running_fwd) = 0;
    *(haldata->running_rev) = 0;
    *(haldata->running) = 0;
    *(haldata->stopped) = 1;
    *(haldata->faulted) = 0;
    *(haldata->fault_code) = 0;

    *(haldata->fwd) = 0;
    *(haldata->rev) = 0;
    *(haldata->freq_cmd) = 0;

    haldata->old_freq = -1;		// make sure the initial value gets output
    haldata->old_ctl = -1;
    haldata->error_cnt = 0;

    // Activate HAL component
    hal_ready(hal_comp_id);

    /* here's the meat of the program.  loop until done (which may be never) */
    while (done==0) {

        /* don't want to scan too fast, and shouldn't delay more than a few seconds */
        if (haldata->looptime < 0.001) haldata->looptime = 0.001;
        if (haldata->looptime > 2.0) haldata->looptime = 2.0;
        loop_timespec.tv_sec = (time_t)(haldata->looptime);
        loop_timespec.tv_nsec = (long)((haldata->looptime - loop_timespec.tv_sec) * 1000000000l);
        nanosleep(&loop_timespec, &remaining);

            read_data(mb_ctx, haldata);
            write_data(mb_ctx, haldata);
    }

    retval = 0;	/* if we get here, then everything is fine, so just clean up and exit */
out_closeHAL:
    hal_exit(hal_comp_id);
out_close:
    modbus_close(mb_ctx);
    modbus_free(mb_ctx);
out_noclose:
    return retval;
}
