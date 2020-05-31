obj-m += nyx.o
LM=/lib/modules/$(shell uname -r)

all:
	make -C $(LM)/build M=$(PWD) modules

clean:
	make -C $(LM)/build M=$(PWD) clean

test: install
	modprobe nyx

install: all
	-rmmod nyx
	install nyx.ko $(LM)/kernel/drivers/misc
	if ! grep nyx /etc/modules ; then echo nyx >>/etc/modules ; fi
	depmod -a

uninstall:
	-rmmod nyx
	rm -f $(LM)/kernel/drivers/misc/nyx.ko
	grep -v nyx /etc/modules >/etc/modules.temp
	mv /etc/modules.temp /etc/modules
	depmod -a
