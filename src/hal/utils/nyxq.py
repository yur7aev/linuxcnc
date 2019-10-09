#!/usr/bin/python
#
# nyxq - YSSC2P/YSSC3P/YMDS2P control utility
#
# 2018, dmitry@yurtaev.com
#

from glob import glob
from mmap import mmap
from ctypes import *
import struct
import time
import sys
import re
import string

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
		( "gpi", c_uint ),
		( "enc", c_int * 2 ),
		( "yi", c_uint * 16 ),
		( "fb",  servo_fb * 16 ),
		( "unused", c_uint * (192-6-16-8*16) ),
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
		( "gpo", c_uint ),
		( "dac", c_ushort * 2 ),
		( "yo", c_uint * 16 ),
		( "axis",  servo_cmd * 16 ),
		( "unused", c_uint * (192-3-16-8*16) ),
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
		( "reserved", c_uint ),
		# nyx_req
		( "code",     c_uint ),
		( "arg1",     c_uint ),
		( "arg2",     c_uint ),
		( "len",      c_uint ),
		( "buf",      nyx_data ),
		( "rly",      nyx_rly ),
		( "cmd",      nyx_cmd ),
		]

#------------------------------

def flip32(data):
	sl = struct.Struct('<I')
	sb = struct.Struct('>I')
#	try:
#		b = buffer(data)
#	except NameError:
#		# Python 3 does not have 'buffer'
#		b = data
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
			#print("Found binary data: %s" % l)
			d = bitfile.read(l)
			# d = flip32(d)
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

#==============================================================================
# /sys/bus/pci/devices/0000:05:09.0

instance = -1
boards = []
first_arg = 0

if len(sys.argv) > 1:
	if sys.argv[1] == '-0': instance = 0
	if sys.argv[1] == '-1': instance = 1
	if sys.argv[1] == '-2': instance = 2
	if sys.argv[1] == '-3': instance = 3
	if instance >= 0: first_arg = 1

try:
	i = 0
	for dir in glob("/sys/bus/pci/devices/*"):
		vendor = int(open(dir + "/vendor", "r").read(), 16)
		device = int(open(dir + "/device", "r").read(), 16)
		if vendor == 0x1067 and (device == 0x55c3 or device == 0x55c2):
			boards.append(dir)
			if i >= instance and not 'mem' in globals():
				with open(dir + "/resource0", "r+b" ) as f:
					mem = mmap(f.fileno(), 4096)
					dp = nyx_dpram.from_buffer(mem, 0x800)
					pcidev = re.sub(r'.+/', '', dir)
			i += 1
except:
	print("need root access to PCI device")
	exit(1)

if len(boards):
	if instance < 0 and i > 1:
		print "multiple boards found:"
		print '\n'.join(boards)
		print "specify instance with -0, -1..."
		exit(1)
#	print "-------", boards[instance], "-------"

try:
	if dp.magic != 0x55c20201 and dp.magic != 0x55c20202:
		print "unsupported firmware %x" % dp.magic
		exit(1)
except:
	print "can't find any YSSC2P/YSSC3P/YMDS2P card"
	exit(1)

#------------------------------

def req(code, a1=0, a2=0, a3=0):
	dp.code = 0
	while (dp.status & 2) == 0: time.sleep(0.01)
	dp.arg1 = a1
	dp.arg2 = a2
	dp.len = a3
	dp.code = code
	while (dp.status & 2) != 0: time.sleep(0.01)
	while (dp.status & 0x0c) == 0: time.sleep(0.01)
	s = dp.status
	dp.code = 0
	if s & 0x8:
		print "req error", "%d." % dp.arg1
		return False
	return True

#------------------------------

def cstr(arr):
	return cast(arr, c_char_p).value.split(b'\0',1)[0]

def info():
#	print "magic: %x" % dp.magic,
	if dp.status&1: print "realtime"
	if req(0x00010000):
#		q =  dp.buf.byte[:]
#		for i in range(32):
#			print chr(q[i]),
#		print
#		print ''.join(chr(i) for i in q)
		print cstr(dp.buf.byte)

def dna():
	if req(0x00080000):
		print cstr(dp.buf.byte)

def bin(a, l):
	s = ' ' * (16 - l)
	for i in reversed(range(l)):
		if a & (1<<i):
			s += "1"
		else:
			s += "-"
	return s

def io_info():
	print "ENC0: %d" % dp.rly.enc[0]
	print "ENC1: %d" % dp.rly.enc[1]
	print "DAC0: %d" % dp.cmd.dac[0]
	print "DAC1: %d" % dp.cmd.dac[1]
	print "------- fedcba9876543210"
	print "GPI:    " + bin(dp.rly.gpi, 12)
	print "GPO:    " + bin(dp.cmd.gpo, 8)

	for i in range(0, 16):
#		print "%x: %08x %08x" % (i, dp.rly.yi[i], dp.cmd.yo[i])
		yi = dp.rly.yi[i]
		typ = yi >> 24
		ok = (yi >> 16) & 0xff

		if typ != 0:
			print "%x:" % i,
			if typ == 1:
				print "YI16 " + bin(yi, 16),
			elif typ == 2:
				print "YO16 " + bin(dp.cmd.yo[i], 16),
			else:
				print typ, "?",
			if ok:
				print "err:",ok
			else:
				print
	print "------- fedcba9876543210"

#------------------------------

def servo_enable(p):
	proto = {
		"sscnet": 1,
		"sscnet2": 2,
		"sscnet3": 3,
		"mds": 11,
		"m-ii": 22
	};
	if p in proto.keys():
		req(0x00030002, proto[p])
	else:
		print "usage: nyxq servo enable [" + string.joinfields(sorted(proto.keys()), '|') + ']'

def servo_disable():
	req(0x00030003)

def servo_start():
	req(0x00030004)

def servo_stop():
	req(0x00030005)

def servo_run(en):
	req(0x00030006, en)

def servo_info():
	conn = 0
	if req(0x00030001):
		print cstr(dp.buf.byte)
		# dump(dp.buf.byte)
		for a in range(8):
			s = dp.rly.fb[a].state
			if s & 0x10:	# YC_ONLINE
				print "%d: [%02x] pos=%11d vel=%.1f trq=%.1f" % (a, dp.rly.fb[a].alarm, dp.rly.fb[a].pos, dp.rly.fb[a].vel/10, dp.rly.fb[a].trq/10),
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
				print
	return conn


def servo_cmd():
	for a in range(8):
		s = dp.cmd.axis[a].flags
		print "%d: pos=%11d flim=%.1f rlim=%.1f" % (a, dp.cmd.axis[a].cmd, dp.cmd.axis[a].flim/10, dp.cmd.axis[a].rlim/10),
		if s & 0x00040: print "rdy",
		if s & 0x00080: print "svon",
		if s & 0x00100: print "fwd",
		if s & 0x00200: print "rev",
		if s & 0x00400: print "Ilim",
		if s & 0x80000: print "vel",
		print
	return 0

def servo_fw():
		for a in range(8):
			if req(0x00030007, a):
				print "%d:" % a, cstr(dp.buf.byte)

def servo_mon():
#	seq = dp.reserved
#	while (seq == dp.reserved): pass
	print "seq=%d %x res=%x" % (dp.rly.seq, dp.cmd.seq, dp.reserved)
	for a in range(7):
		r = dp.rly.fb[a]
		c = dp.cmd.axis[a]
		if r.state & 0x80000000: # YR_VALID
			print "%d: [%02x] state=%x pos=%d vel=%d trq=%d t=%s -> flags=%x cmd=%d" % (
					a, r.alarm, r.state, r.pos, r.vel, r.trq, r.mon[3],
					c.flags, c.cmd
					)
#	print "sy:%d..%d pr:%d..%d se:%d..%d" % (
#		dp.dbg.sync_start, dp.dbg.sync_end,
#		dp.dbg.proc_start, dp.dbg.proc_end,
#		dp.dbg.send_start, dp.dbg.send_end,
#		)
#	print "seq_miss:%d dpll_shift:%d" % (
#		dp.dbg.cmd_seq_miss, dp.dbg.dpll_shift
#		)


def pll(y, p, i, s):
	dp.buf.dword[0] = int(y)
	dp.buf.dword[1] = int(p)
	dp.buf.dword[2] = int(i)
	dp.buf.dword[4] = int(s)
	req(0x00090000, 0, 0, 4)

#------------------------------
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

	#req(0x00060000, 1);
	dp.code = 0
	while (dp.status & 2) == 0: time.sleep(0.01)
	dp.arg1 = 1
	dp.code = 0x00060000

	print "rebooting..."
	time.sleep(2)

	# print '/usr/bin/setpci', '-s', pcidev, '10.l='+conf[0], '4.w='+conf[1], 'latency_timer='+conf[2]
	res = subprocess.check_output(['/usr/bin/setpci', '-s', pcidev, '10.l='+conf[0], '4.w='+conf[1], 'latency_timer='+conf[2]])
	print res



def flash_read(a):
	#print [hex(x) for x in list(dp.buf.byte)[0:256]]
	while True:
		req(0x00050021, 0xdeadbeef, a, 256)
		#dump(list(dp.buf.byte)[0:256], a)
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

	return

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
			#dp.buf.byte[0:s] = (c_ubyte * s).from_buffer_copy(d[o:o+s])
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

def dbg():
	print "seq:%x gpi:%x enc0:%x enc1:%x" % (
			dp.rly.seq,
			dp.rly.gpi,
			dp.rly.enc[0], dp.rly.enc[1],
			)
	for a in range(8):
		r = dp.rly.fb[a]
		print "%x: %x pos=%d" % (a, r.state, r.pos)

def arg(n, m, d=None):
	n += first_arg
	if len(sys.argv) <= n:
		if d != None: return d
		print "nyxq v2.2.0"
		print "usage: nyxq " + m
		exit(1)
	return sys.argv[n]

cmd = arg(1, "[info|servo|flash|reboot] ...")

if cmd == 'info':
	info()
elif cmd == 'servo':
	subcmd = arg(2, "servo [info|enable|disable|start|run|stop|mon] ...")
	if subcmd == 'info':		servo_info()
	elif subcmd == 'enable':
		proto = arg(3, "servo enable [mds|sscnet|sscnet2]")
		servo_enable(proto)
	elif subcmd == 'disable':	servo_disable()
	elif subcmd == 'start':		servo_start()
	elif subcmd == 'run':
		mask = int(arg(3, "servo run <mask>", d=0))
		servo_run(mask)
	elif subcmd == 'stop':		servo_stop()
	elif subcmd == 'mon':		servo_mon()
	elif subcmd == 'fw':		servo_fw()
	elif subcmd == 'cmd':		servo_cmd()
	else:
		print "error: nyxq servo ?"
elif cmd == 'reboot':
	reboot()
elif cmd == 'pll':
	y = arg(2, "pll insync P I step")
	p = arg(3, "pll insync P I step")
	i = arg(4, "pll insync P I step")
	s = arg(5, "pll insync P I step")
	pll(y, p, i, s)
elif cmd == 'io':
	io_info()
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
		print "invalid command"
else:
	print "invalid command"
