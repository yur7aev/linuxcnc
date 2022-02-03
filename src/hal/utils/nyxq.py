#! /usr/bin/python2
#
# nyxq - YxxxxP control utility
#
# License: GPL Version 2
#
# 2018-2022, dmitry@yurtaev.com
#

from glob import glob
from mmap import mmap
from ctypes import *
import struct
import time
import sys
import re
import string


VER = "nyxq v3.0.9"

class nyx_dpram_hdr(Structure):
	_fields_ = [
		( "magic",    c_uint ),
		( "config",   c_uint ),
		( "status",   c_uint ),
		( "config2",  c_uint ),
		]

# ------------------------------

instance = -1
boards = []
first_arg = 0
vendor_ids = [ 0x1067,  0x1313 ]
device_ids = [ 0x55c2,  0x55c3,  0x0712 ]

if len(sys.argv) > 1:
	if sys.argv[1] == '-0': instance = 0
	if sys.argv[1] == '-1': instance = 1
	if sys.argv[1] == '-2': instance = 2
	if sys.argv[1] == '-3': instance = 3
	if instance >= 0: first_arg = 1

try:
	i = 0
	for dir in sorted(glob("/sys/bus/pci/devices/*")):
		vendor = int(open(dir + "/vendor", "r").read(), 16)
		device = int(open(dir + "/device", "r").read(), 16)
		if (vendor in vendor_ids) and (device in device_ids):
			boards.append(dir)
			if i >= instance and not 'mem' in globals():
				with open(dir + "/resource0", "r+b" ) as f:
					mem = mmap(f.fileno(), 4096)
					pcidev = re.sub(r'.+/', '', dir)
			i += 1
except:
	print("need root access to PCI device")
	exit(1)

if len(boards) == 0 or not 'mem' in globals():
	print "can't find any YxxxnP card"
	exit(1)

if instance < 0 and i > 1:
	print "multiple boards found:"
	print '\n'.join(boards)
	print "specify instance with -0, -1..."
	exit(1)

dph = nyx_dpram_hdr.from_buffer(mem, 0x800)
magic = dph.magic & 0xffffff00;

num_axes = dph.config & 0xff
num_yio = (dph.config>>8) & 0xff

#print "firmware %x %x %x %x" % (magic, dph.config, dph.status, dph.config2)

if magic == 0x55c20200:		# fw version 2.x.x
	num_gpi = 12+17
	num_enc = 2
	size_yio = 1
	num_gpo = 8
	num_dac = 2
	num_dbg = 0
elif magic == 0x4e590300:	# fw version 3.x.x
	num_gpi =  dph.config2 & 0xff
	num_enc = (dph.config>>16) & 0xff
	size_yio = 2
	num_gpo = (dph.config2>>8) & 0xff
	num_dac = (dph.config>>24) & 0xff
	num_dbg = (dph.config2>>16) & 0xff
else:
	print "unsupported firmware %x %x %x %x" % (magic, dph.config, dph.status, dph.config2)
	exit(1)

# ------------------------------


# TODO: generate from nyx2.h

class servo_fb(Structure):
	_fields_ = [
		( "state", c_uint ),
		( "pos", c_int ),
		( "vel", c_int ),
		( "alarm", c_ushort ),
		( "trq", c_short ),
		( "mon", c_uint * 4 ),		# 8 longs
		]
class nyx_rly(Structure):
	_fields_ = [
		( "seq", c_uint ),
		( "irq", c_uint ),
		( "valid", c_uint ),
		( "gpi", c_uint * int(num_gpi/32+1) ),
		( "enc", c_int * num_enc ),
		( "yi", c_uint * (num_yio * size_yio) ),
		( "dbg", c_uint * num_dbg ),
		( "fb",  servo_fb * num_axes ),
		( "unused", c_uint * (192 - 3 - int(num_gpi/32+1) - num_enc - num_yio*size_yio - num_dbg - 8*num_axes) ),
		]

class servo_cmd(Structure):
	_fields_ = [
		( "flags", c_uint ),
		( "cmd", c_int ),
		( "flim", c_ushort ),
		( "rlim", c_ushort ),
		( "mon", c_uint * 5 ),
		]

class nyx_cmd(Structure):
	_fields_ = [
		( "seq", c_uint ),
		( "gpo", c_uint * int(num_gpo/32+1) ),
		( "dac", c_ushort * num_dac ),
		( "yo", c_uint * (num_yio * size_yio) ),
		( "axis",  servo_cmd * num_axes ),
		( "unused", c_uint * (192 - 1 - int(num_gpo/32+1) - int(num_dac/2) - num_yio*size_yio - 8*num_axes) ),
		]

class servo_info(Structure):
	_fields_ = [
		( "text", c_ubyte * 64 ),
		( "online", c_uint ),
		( "answ", c_uint ),
		( "buf", c_ubyte * (32*8) ),
		]

class nyx_data(Union):
	_fields_ = [
		( "byte",  c_ubyte * (120*4) ),
		( "word",  c_ushort * (120*2) ),
		( "dword", c_uint * (120) ),
		( "servo_info", servo_info ),
		]

class nyx_dpram(Structure):
	_fields_ = [
		( "magic",    c_uint ),
		( "config",   c_uint ),
		( "status",   c_uint ),
		( "config2",  c_uint ),
		# nyx_req
		( "code",     c_uint ),
		( "arg1",     c_uint ),
		( "arg2",     c_uint ),
		( "arg3",     c_uint ),
		( "buf",      nyx_data ),
		( "rly",      nyx_rly ),
		( "cmd",      nyx_cmd ),
		]

dp = nyx_dpram.from_buffer(mem, 0x800)

# ------------------------------

def flip32(data):
	sl = struct.Struct('<I')
	sb = struct.Struct('>I')
	b = ''.join(map(chr, data))
	d = bytearray(len(data))
	for offset in range(0, len(data), 4):
		 sb.pack_into(d, offset, sl.unpack_from(b, offset)[0])
	return d

def read_bitfile(filename):
	short = struct.Struct('>H')
	ulong = struct.Struct('>I')

	bitfile = open(filename, 'rb')

	l = short.unpack(bitfile.read(2))[0]
	if l != 9: raise Exception("missing <0009> header (0x%x), not a bit file" % l)
	bitfile.read(l)
	l = short.unpack(bitfile.read(2))[0]
	d = bitfile.read(l)
	if d != b'a': raise Exception("missing <a> header, not a bit file")

	l = short.unpack(bitfile.read(2))[0]
	d = bitfile.read(l)
	print("design: %s" % d)

	if b"PARTIAL=TRUE" in d: raise Exception("partial bitstream unsupported")

	node_nr = 0
	KEYNAMES = {b'b': "device:", b'c': "date:", b'd': "time:"}

	while 1:
		k = bitfile.read(1)
		if not k:
			bitfile.close()
			raise Exception("unexpected EOF")
		elif k == b'e':
			l = ulong.unpack(bitfile.read(4))[0]
			d = bitfile.read(l)
			bitfile.close()
			print "size: %d/%x" % (len(d), len(d))
			return d
		elif k in KEYNAMES:
			l = short.unpack(bitfile.read(2))[0]
			d = bitfile.read(l)
			print KEYNAMES[k], d
		else:
			print("unexpected key: %s" % k)
			l = short.unpack(bitfile.read(2))[0]
			d = bitfile.read(l)

def fatal(msg):
	sys.stderr.write("error: ")
	sys.exit(msg)

# ------------------------------
class ReqError(Exception):
	def __init__(self, msg):
		self.msg = msg

def req(code, a1=0, a2=0, a3=0):
	dp.code = 0
	while (dp.status & 2) == 0: time.sleep(0.01)
	dp.arg1 = a1
	dp.arg2 = a2
	dp.arg3 = a3
	dp.code = code
	while (dp.status & 2) != 0: time.sleep(0.01)
	while (dp.status & 0x0c) == 0: time.sleep(0.01)
	s = dp.status
	if s & 0x8:
		errtxt = [ "NO_ERROR", "BAD_CODE", "BAD_FUNC", "BAD_AXIS", "BAD_ARG", "NO_AXIS", "FAILED", "UNAVAIL", "TIMEOUT" ];

		msg = "%d (%04x %04x)" % (dp.arg1, dp.arg2, dp.arg3)
		if dp.arg1 < len(errtxt):
			msg += " " + errtxt[dp.arg1]
		raise ReqError(msg)
	dp.code = 0
	return True

def cstr(arr):
	return cast(arr, c_char_p).value.split(b'\0',1)[0]

def info():
	req(0x00010000)
	print cstr(dp.buf.byte)
	if dp.status & 1: print "realtime"

def dna():
	req(0x00080000)
	print cstr(dp.buf.byte)

def bin(a, l):
	s = ' ' * (32 - l)
	for i in reversed(range(l)):
		if a & (1<<i):
			s += "1"
		else:
			s += "-"
	return s

def io_info():
        for i in range(num_enc):
            print "ENC%d: %d" % (i, dp.rly.enc[i])
        for i in range(num_dac):
            print "DAC%d: %d" % (i, dp.cmd.dac[i])
	print "------- fedcba9876543210fedcba9876543210"
	print "GPI:    " + bin(dp.rly.gpi[0], 29)
	print "GPO:    " + bin(dp.cmd.gpo[0], 8)

	for i in range(num_yio):
		yi = dp.rly.yi[i*2]
		yi2 = dp.rly.yi[i*2+1]
                yo = dp.cmd.yo[i*2]
                yo2 = dp.cmd.yo[i*2+1]

		typ = yi >> 24
		ok = (yi >> 16) & 0xff

		if typ != 0:
			print "%x:" % (i),
			if typ == 1:
				print "YI16 " + bin(yi, 16),
			elif typ == 2:
				print "YO16 " + bin(yo, 16),
			elif typ == 3:
				print "YENC " + "%d" % (yi & 0xffff),
			elif typ == 5:
                            i1 = "1" if yi & 0x1 else "-"
                            o1 = "1" if yo & 0x10000 else "-"
                            i2 = "1" if yi & 0x4 else "-"
                            o2 = "1" if yo & 0x40000 else "-"
                            print "YAO2 " + "%+.3fV %05d %c%c %+.3fV %05d %c%c" % (\
                                    ((yo2 & 0xffff) - 32767) * 10.0 / 32768 , yi2 & 0xffff, i1, o1,
                                    ((yo2 >> 16) - 32767) * 10.0 / 32768 , yi2 >> 16, i2, o2),
			else:
				print typ, "?", "%x,%x %x,%x" % (yi, yi2, yo,yo2),
			if ok:
				print "err:",ok
			else:
				print
	print "------- fedcba9876543210fedcba9876543210"

def servo_info():
	req(0x00030001)
	print cstr(dp.buf.byte)
	# dump(dp.buf.byte)
	for a in range(num_axes):
		s = dp.rly.fb[a].state
		if s & 0x10:	# YC_ONLINE
			print "%d: [%02x] pos=%11d vel=%.1f trq=%.1f" % (a, dp.rly.fb[a].alarm, dp.rly.fb[a].pos, dp.rly.fb[a].vel/10, dp.rly.fb[a].trq/10),
			if s & 0x00010: print "on",
			if s & 0x00080: print "rdy",
			if s & 0x00100: print "svon",
			if s & 0x00200: print "inp",
			if s & 0x00400: print "atsp",
			if s & 0x00800: print "zsp",
			if s & 0x01000: print "Ilim",
			if s & 0x02000: print "alm",
			if s & 0x04000: print "wrn",
			if s & 0x08000: print "abs",
			if s & 0x10000: print "abslost",

			if s & 0x000040: print "zpass",
			if s & 0x400000: print "ori",
			if s & 0x800000: print "orc",
			if s & 0x1000000: print "fwd",
			if s & 0x2000000: print "rev",
			# print "[%x]"%(s),
			print

def servo_cmd():
	for a in range(num_axes):
		s = dp.cmd.axis[a].flags
		print "%d: pos=%11d flim=%.1f rlim=%.1f" % (a, dp.cmd.axis[a].cmd, dp.cmd.axis[a].flim/10, dp.cmd.axis[a].rlim/10),
		if s & 0x00040: print "rdy",
		if s & 0x00080: print "svon",
		if s & 0x00100: print "fwd",
		if s & 0x00200: print "rev",
		if s & 0x00400: print "Ilim",
		if s & 0x80000: print "vel",
		print

def servo_fw():
	for a in range(num_axes):
		try:
			req(0x00030007, a)
                        s = bytearray(dp.buf.byte)
                        print "%d:" % a, s[0:63].replace('\0', ' ')
		except:
			pass

def servo_alarm():
	for a in range(num_axes):
		try:
			req(0x00030008, a)
                        s = bytearray(dp.buf.byte)
                        print "%d:" % a, s[0:63].replace('\0', ' ')
		except:
			pass

def servo_mon():
	print "seq=%d %x config2=%x" % (dp.rly.seq, dp.cmd.seq, dp.config2)
	for a in range(num_axes):
		r = dp.rly.fb[a]
		c = dp.cmd.axis[a]
		if r.state & 0x80000000: # YR_VALID
			print "%d: [%02x] state=%x pos=%d vel=%d trq=%d t=%s -> flags=%x cmd=%d" % (
					a, r.alarm, r.state, r.pos, r.vel, r.trq, r.mon[3],
					c.flags, c.cmd
					)

def param2no(s):
	s = s.upper()
	j4g = "ABCDEFGHJOSLTN"
	m = re.match('^P(['+j4g+'])(\d{1,2})$', s)	# J3/J4
	if m:
		g = j4g.find(m.group(1))
		if g < 0: exit(1)
		return (g << 8) + int(m.group(2))
	m = re.match('(P|SV|SP)(\d+)$', s)		# J2/J2S/MDS
	if m: return int(m.group(2))
	m = re.match('PN([0-9A-F]+)(L?)$', s)		# SGDS/SGDV
	if m:
		no = int(m.group(1), 16)
		if m.group(2): no = no | 0x10000
		return no
	print "invalid parameter number"
	exit(1)

def axrange(s):
	l = []
	for i in s.split(','):
		m = re.match("(\d+)-(\d+)", i)
		if m:
			l += range(int(m.group(1)), int(m.group(2))+1)
		else:
			l.append(int(i))
	return list(set(l))

# param read
def servo_pr(l):
	first = 0
	second = 0
	for s in l:
		r = re.match('([0-9,-]+):(\S+)', s)
		if r:
			ax = axrange(r.group(1))
			p = param2no(r.group(2))
			for a in ax:
				if a >= 0 and a < 16 and a < num_axes:
					m = 1<<a
					if second & m:
						sys.exit('more than 2 params for axis %d in %s' % (a, s))
					elif first & m:
						dp.buf.dword[a+32] = p
						second |= m
					else:
						dp.buf.dword[a] = p
						dp.buf.dword[a+32] = 0	# unused param
						first |= m
				else:
					sys.exit('bad axis %d' % a)
		else:
			sys.exit('bad parameter format <axis>:<param>, %s' % s)
	req(0x00030011, first)
	for a in range(0, 16):
		if first & (1<<a):
			p = dp.buf.dword[a+0]
			v = dp.buf.dword[a+16]
			print "%d:P%d=%d 0x%x" % (a, p, v, v)
		if second & (1<<a):
			p = dp.buf.dword[a+32]
			v = dp.buf.dword[a+48]
			print "%d:P%d=%d 0x%x" % (a, p, v, v)

# param write
def servo_pw(nv, l):
	first = 0
	second = 0
	for s in l:
		r = re.match('([0-9,-]+):(\S+)=([0-9a-fA-Fx]+)', s)		# J2/J2S/MDS
		if r:
			ax = axrange(r.group(1))
			v = int(r.group(3), 0)
			p = param2no(r.group(2))
			for a in ax:
				if a >= 0 and a < 16:
					m = 1<<a
					if second & m:
						sys.exit('more than 2 params for axis %d in %s' % (a, s))
					elif first & m:
						dp.buf.dword[a+32] = p
						dp.buf.dword[a+48] = v
						second |= m
					else:
						dp.buf.dword[a] = p
						dp.buf.dword[a+16] = v
						dp.buf.dword[a+32] = 0	# unused param
						first |= m
				else:
					sys.exit('bad axis %d' % a)
		else:
			sys.exit('bad parameter format <axis>:<param>=<value>, %s' % s)
	if nv:
		req(0x0003001a, first)
	else:
		req(0x00030012, first)

# mechatrolink abs encoder init
def servo_abs(l):
	first = 0
	for s in l:
		r = re.match('([0-9,-]+)', s)
		if r:
			ax = axrange(r.group(1))
			for a in ax:
				if a >= 0 and a < num_axes:
					m = 1<<a
					first |= m
				else:
					sys.exit('bad axis %d' % a)
		else:
			sys.exit('bad axis format, %s' % s)
	req(0x00030019, first)

def pll(y, p, i, s, h):
	dp.buf.dword[0] = int(y)
	dp.buf.dword[1] = int(p)
	dp.buf.dword[2] = int(i)
	dp.buf.dword[3] = int(s)
	dp.buf.dword[4] = int(h)
	req(0x00090000, 0, 0, 5)

# ------------------------------

def dump(b, a=0):
	i = 0
	for x in b:
		if i and not (i % 16): print
		if not (i % 16): print "%05x:" % (a + i),
		print "%02x" % x,
		i += 1
	print

import subprocess

def reboot():
	conf = subprocess.check_output(['/usr/bin/setpci', '-s', pcidev, '10.l', '4.w', 'latency_timer']).split()
	dp.code = 0
	while (dp.status & 2) == 0: time.sleep(0.01)
	dp.arg1 = 1
	dp.code = 0x00060000
	print "rebooting..."
	time.sleep(2)
	res = subprocess.check_output(['/usr/bin/setpci', '-s', pcidev, '10.l='+conf[0], '4.w='+conf[1], 'latency_timer='+conf[2]])
	print res

def flash_read(a):
	while True:
		req(0x00050021, 0xdeadbeef, a, 256)
		d = flip32(dp.buf.byte[0:256])
		dump(d, a)
		if raw_input("next? ") in ['q', "n", 'x']:
			break
		a += 256

def flash_bootloader():
	BOOT_OFFS = 0x80000
	SAFE_OFFS = 0x10000
	data = [
		0xFFFFFFFFL,    #  DUMMYWORD,  DUMMYWORD
		0xAA995566L,    #  SYNCWORD
		0x31E1FFFFL,
		0x32610000L + (BOOT_OFFS&0xffff),     #  GENERAL1 multiboot[15:0] = 0000
		0x32810300L + ((BOOT_OFFS>>16)&0xff), #  GENERAL2 SPIx1 read cmd = 03, multiboot[23:16] = 08
		0x32A10000L + (SAFE_OFFS&0xffff),     #  GENERAL3 fallback[15:0] =  0000
		0x32C10300L + ((SAFE_OFFS>>16)&0xff), #  GENERAL4 read cmd, fallbach[23:16] = 01
		0x32E10000L,
		0x30A10000L,
		0x33012100L,
		0x3201001FL,
		0x30A1000EL,
		0x20002000L,    #  NOOP, NOOP
		0x20002000L,
		0x20002000L,
		0x20002000L,
	]
	l = len(data)

	print "erasing 0"
	req(0x00050022, 0xdeadbeef, 0x00000)	# erase first sector
	dp.buf.dword[0:l] = data
	print "writing 0"
	req(0x00050023, 0xdeadbeef, 0x00000, l<<2)
	print "veryfying 0"
	req(0x00050021, 0xdeadbeef, 0, l*4)
	if dp.buf.dword[0:l] != data:
		print "FAILED! DO NOT REBOOT - TRY AGAIN"
	else:
		print "success"

def flash_program(f, a=0x80000, verify_only=0):
	d = read_bitfile(f)
	l = len(d)

	if not verify_only:
		print "erasing ",
		for o in range(a, a + l, 0x10000):
			print o/0x10000,
			sys.stdout.flush()
			req(0x00050022, 0xdeadbeef, o)
		print

		print "writing", l, "bytes",
		for o in range(0, l, 0x100):
			s = min(256, l - o)
			sys.stdout.write(".")
			sys.stdout.flush()
			fd = flip32((c_ubyte * s).from_buffer_copy(d[o:o+s]))
			dp.buf.byte[0:s] = fd
			req(0x00050023, 0xdeadbeef, a+o, s)
		print

	print "verifying", l, "bytes ",
	errcnt = 0
	for o in range(0, l, 0x100):
		s = min(256, l - o)
		req(0x00050021, 0xdeadbeef, a+o, s)
		if flip32(dp.buf.byte[0:s]) != d[o:o+s]:
			sys.stdout.write("E")
			errcnt += 1
		else:
			sys.stdout.write(".")
		sys.stdout.flush()
	print
	if errcnt:
		print "FAILED! DO NOT REBOOT - TRY AGAIN"
	else:
		print "success"

def config(cfg):
	if cfg == 'load':
		req(0x00040015)    # load
	elif cfg == 'save':
		req(0x00040016)    # save
	elif cfg == None:
		req(0x00040021) # read
		print "excfg: %x" % dp.buf.dword[1]
                print "pll: %d %d %d %d %d" % (dp.buf.dword[2], dp.buf.dword[3], dp.buf.dword[4], dp.buf.dword[5], dp.buf.dword[6])
                print "cyc: %d" % (dp.buf.dword[7])
        else:
		req(0x00040023, int(cfg, 16))    # write

def servo_config(cycle):
	req(0x000a0000, int(cycle))	# SVCFG

# ==============================

def arg(n, m=None, d=None):
	n += first_arg
	if len(sys.argv) <= n:
		if d != None: return d
		if m == None: return None
		print VER
		print "usage: nyxq " + m
		exit(1)
	return sys.argv[n]

def args(n, m):
	n += first_arg
	if len(sys.argv) <= n:
		print VER
		print "usage: nyxq " + m
		exit(1)
	return sys.argv[n:]


cmd = arg(1, "[info|servo|io|flash|reboot|config|pll] ...")
try:
	if cmd == 'info':
		info()
	elif cmd == 'servo':
		subcmd = arg(2, "servo [info|mon|fw|alarm|cmd|pr|pw|pwnv|config] ...")
		if   subcmd == 'info':		servo_info()
		elif subcmd == 'mon':		servo_mon()
		elif subcmd == 'fw':		servo_fw()
		elif subcmd == 'alarm':		servo_alarm()
		elif subcmd == 'cmd':		servo_cmd()
		elif subcmd == 'pr':
			p = args(3, "servo pr <axis>:<param> ...")
			servo_pr(p)
		elif subcmd == 'pw':
			p = args(3, "servo pw <axis>:<param>=<value> ...")
			servo_pw(False, p)
		elif subcmd == 'pwnv':
			p = args(3, "servo pwnv <axis>:<param>=<value> ...")
			servo_pw(True, p)
		elif subcmd == 'abs':
			p = args(3, "servo abs <axis> ...")
			servo_abs(p)
		elif subcmd == 'config':
			p = arg(3, "servo config <cycle>")
			servo_config(p)
		else:
			print "error: nyxq servo ?"
	elif cmd == 'reboot':
		reboot()
	elif cmd == 'pll':
		msg = "pll <insync> <kp> <ki> <step> <hunt>"
		y = arg(2, msg)
		p = arg(3, msg)
		i = arg(4, msg)
		s = arg(5, msg)
		h = arg(6, msg)
		pll(y, p, i, s, h)
	elif cmd == 'io':
		io_info()
	elif cmd == 'config':
		config(arg(2))
	elif cmd == 'dna':
		dna()
	elif cmd == 'flash':
		subcmd = arg(2, "flash [dump|erase|program|bootloader] ...")
		if subcmd == 'dump':
			a = int(arg(3, "flash dump [<addr>]"), 16)
			flash_read(a)
		elif subcmd == 'erase':
			a = int(arg(3, "flash erase <addr>"), 16)
			flash_erase(a)
		elif subcmd == 'program':
			f = arg(3, "flash program <file.bit>")
			flash_program(f, 0x80000)
		elif subcmd == 'programsafe':
			f = arg(3, "flash programsafe <file.bit>")
			flash_program(f, 0x10000)
		elif subcmd == 'verify':
			f = arg(3, "flash verify <file.bit> <addr>")
			a = int(arg(4, "flash verify <file.bit> <addr>"), 16)
			flash_program(f, a, 1)
		elif subcmd == 'bootloader':
			flash_bootloader()
		else:
			print "error: nyxq flash ?"
	else:
		print "error: nyx ?"
except ReqError as err:
	print "Error:", err.msg
