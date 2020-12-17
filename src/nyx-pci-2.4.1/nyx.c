//
// YxxxxP PCI servo interface card driver
//
// 2020, http://yurtaev.com
//

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
//#include <asm/cacheflush.h>

#include "nyx2.h"

#define DEVICE_NAME "nyx"
#define CLASS_NAME "servo"
#define VER "2.4.1"

#define YSSC_VENDOR_ID 0x1067
#define YMTL_VENDOR_ID 0x1313
#define YSSC2P_DEVICE_ID 0x55c2
#define YSSC3P_DEVICE_ID 0x55c3
#define YMTL2P_DEVICE_ID 0x0712
#define YSSC_MAX_BOARDS 8


static struct pci_device_id nyx_pci_ids[] = {
	{ PCI_DEVICE(YSSC_VENDOR_ID, YSSC2P_DEVICE_ID) },
	{ PCI_DEVICE(YSSC_VENDOR_ID, YSSC3P_DEVICE_ID) },
	{ PCI_DEVICE(YMTL_VENDOR_ID, YMTL2P_DEVICE_ID) },
	{0,}
};

MODULE_DEVICE_TABLE(pci, nyx_pci_ids);

static int major;

static int nyx_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void nyx_remove(struct pci_dev *pdev);

static struct pci_driver nyx_pci_driver = {
	.name = DEVICE_NAME,
	.id_table = nyx_pci_ids,
	.probe = nyx_probe,
	.remove = nyx_remove
};

struct nyx_priv {
	volatile nyx_iomem __iomem *iomem;	// card DPRAM over the PCI
	nyx_dpram *dpram;		// local copy in coherent memory
	size_t dpram_size;
	dma_addr_t dpram_bus_addr;	// bus address of that memory to tell the DMA engine
	int minor;
///	int irq;
///	wait_queue_head_t wq;
};

struct nyx_fpriv {
	struct nyx_priv *y;

	unsigned int nodma;
	char *vm_start;
	unsigned long vm_size;
};

#define NYX_MAX_CARDS 8
static int nyx_cards;
static struct nyx_priv *nyx_priv[NYX_MAX_CARDS];
// fops

static int nyx_open(struct inode *inode, struct file *file);
static int nyx_release(struct inode *inode, struct file *file);
static ssize_t nyx_read(struct file *filp, char *buf, size_t size, loff_t *offs);
static ssize_t nyx_write(struct file *filp, const char *buf, size_t size, loff_t *offs);
static loff_t nyx_lseek(struct file *filp, loff_t offs, int orig);
static long nyx_ioctl(struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param);
static int nyx_mmap(struct file *filp, struct vm_area_struct *vma);

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.open = nyx_open,
	.release = nyx_release,
	.read = nyx_read,
	.write = nyx_write,
	.llseek = nyx_lseek,
	.unlocked_ioctl = nyx_ioctl,
	.mmap = nyx_mmap,
};

/////////////////////////////////////////////////////////////////////////////

static struct class* nyx_class = NULL;
static struct device* nyx_device = NULL;

static char *nyx_devnode(struct device *dev, umode_t *mode)
{
	if (mode) *mode = 0666;
	return NULL;
}

static int __init nyx_init(void)
{
	int n;

	printk(KERN_INFO "%s: " VER " loading...\n", DEVICE_NAME);

	nyx_cards = 0;
	n = pci_register_driver(&nyx_pci_driver);
	if (n) {
		goto fail0;
	}

	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		printk(KERN_ALERT "%s: can't get major number\n", DEVICE_NAME);
		goto fail1;
	}

	/* We can either tie our device to a bus (existing, or one that we create)
	* or use a "virtual" device class. For this example, we choose the latter */
	nyx_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(nyx_class)) {
		printk(KERN_ALERT "%s: failed to register device class '%s'\n", DEVICE_NAME, CLASS_NAME);
		goto fail2;
	}
	nyx_class->devnode = nyx_devnode;

	for (n = 0; n < nyx_cards; n++) {
		/* With a class, the easiest way to instantiate a device is to call device_create() */
		nyx_device = device_create(nyx_class, NULL, MKDEV(major, n), NULL, DEVICE_NAME "%d", n);
		if (IS_ERR(nyx_device)) {
			printk(KERN_ALERT "%s.%d: failed to create device '%d:%d'\n", DEVICE_NAME, n, major, n);
			goto fail3;
		}
	}

	///printk(KERN_ALERT "nyx: ready major=%d\n", major);

	return 0;

fail3:	while (--n >= 0)
		device_destroy(nyx_class, MKDEV(major, n));
	class_unregister(nyx_class);
	class_destroy(nyx_class);
fail2:	unregister_chrdev(major, DEVICE_NAME);
fail1:	pci_unregister_driver(&nyx_pci_driver);
fail0:	return -1;
}

static void __exit nyx_exit(void)
{
	int n;

	for (n = 0; n < nyx_cards; n++)
		device_destroy(nyx_class, MKDEV(major, n));
	class_unregister(nyx_class);
	class_destroy(nyx_class);
	unregister_chrdev(major, DEVICE_NAME);
	pci_unregister_driver(&nyx_pci_driver);
}

/////////////////////////////////////////////////////////////////////////////

/*
static irqreturn_t nyx_irq_handler(int irq, void *dev_id)
{

	struct nyx_priv *y = dev_id;
	unsigned i;

	if (y && ((i = y->iomem->dma_isr) & 1)) {
		y->iomem->dma_isr = i & 2;
		wake_up_interruptible(&y->wq);
//		printk(KERN_INFO "%s.%d: irq\n", DEVICE_NAME, y->minor);
		return IRQ_HANDLED;
	}
//	printk(KERN_INFO "%s.%d: irq miss\n", DEVICE_NAME, y->minor);
	return IRQ_NONE;
}
*/

void release_device(struct pci_dev *pdev)
{
	struct nyx_priv *y;

	y = pci_get_drvdata(pdev);
	if (y) {
		/// if (y->irq) free_irq(y->irq, y);
		if (y->dpram) dma_free_coherent(&pdev->dev, y->dpram_size, y->dpram, y->dpram_bus_addr);
		if (y->iomem) iounmap(y->iomem);
		kfree(y);
	}
	pci_release_region(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	pci_disable_device(pdev);
}

//
//
//
static int nyx_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int bar, err;
	u16 vendor, device;
	unsigned long mmio_start, mmio_len;
	struct nyx_priv *y;
	int minor = nyx_cards;

	if (minor >= NYX_MAX_CARDS) {
		printk(KERN_INFO "%s.%d: max %d cards supported\n", DEVICE_NAME, minor, NYX_MAX_CARDS);
		return -EIO;
	}


	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

	printk(KERN_INFO "%s.%d: found %02x:%02x.%02x %04x:%04x\n", DEVICE_NAME, minor, 
		pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
		vendor, device);

	bar = pci_select_bars(pdev, IORESOURCE_MEM);
	err = pci_enable_device_mem(pdev);
	if (err) {
		return err;
	}

	err = pci_request_region(pdev, bar, DEVICE_NAME);
	if (err) {
		pci_disable_device(pdev);
		return err;
		}

	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	y = kzalloc(sizeof(struct nyx_priv), GFP_KERNEL);
	if (!y) {
		release_device(pdev);
		return -ENOMEM;
	}

	nyx_priv[minor] = y;
	y->minor = minor;

	/*
	if (!request_irq(pdev->irq, nyx_irq_handler, IRQF_SHARED, "nyx", y)) {
		y->irq = pdev->irq;
		init_waitqueue_head(&y->wq);
	} else
		printk(KERN_INFO "%s.%d: can't hook to irq %d\n", DEVICE_NAME, minor, pdev->irq);
	*/

	y->iomem = ioremap(mmio_start, mmio_len);
	if (!y->iomem) {
		release_device(pdev);
		return -EIO;
	}

	pci_set_drvdata(pdev, y);
	printk(KERN_INFO "%s.%d: magic=%08x config=%08x status=%08x\n", DEVICE_NAME, minor,
		y->iomem->dpram.magic,
		y->iomem->dpram.config,
		y->iomem->dpram.status
	);

	y->dpram_size = sizeof(nyx_dpram);
	y->dpram = (nyx_dpram *)dma_alloc_coherent(&pdev->dev, y->dpram_size, &y->dpram_bus_addr, 0);
	if (!y->dpram) {
		printk(KERN_ERR "%s.%d: can't allocate DMA memory\n", DEVICE_NAME, minor);
		iounmap(y->iomem);
		release_device(pdev);
		return -EIO;
	} else {
		size_t offs = offsetof(struct nyx_dpram, fb);	// should be 512
		//printk(KERN_INFO "%s.%d: DMA mem vm:%p bus:%llx offs:%zu\n", DEVICE_NAME, minor, y->dpram, y->dpram_bus_addr, offs);
	}

	//memset(y->dpram, 'X', y->dpram_size);
	nyx_cards++;

	return 0;
}

//
static void nyx_remove(struct pci_dev *pdev)
{
	release_device(pdev);
	printk(KERN_INFO "%s: unload\n", DEVICE_NAME);
}

/////////////////////////////////////////////////////////////////////////////
/// fops

#define RETRIES 50

static int nyx_open(struct inode *inode, struct file *filp)
{
	struct nyx_fpriv *f;
	int major, minor;

	major = imajor(inode);
	minor = iminor(inode);
	///printk("nyx: open maj=%d min=%d\n", major, minor);

	if (minor >= nyx_cards || nyx_priv[minor] == NULL)
		return -EINVAL;

	f = kzalloc(sizeof(struct nyx_fpriv), GFP_KERNEL);
	if (!f) 
		return -EFAULT;

	f->y = nyx_priv[minor];
	f->nodma = 1;
	filp->private_data = f;

	return 0;
}

static int nyx_release(struct inode *inode, struct file *file)
{
	/* client related data can be retrived from file->private_data
	   and released here */
	if (file->private_data)
		kfree(file->private_data);
	return 0;
}

static int dpram_read(struct nyx_priv *y, size_t offs, size_t size)  {
	int retries = 0;
	uint32_t j;

	if (y == NULL) return -1;
	if (y->iomem == NULL) return -2;
	if (offs + size > y->dpram_size) return -3;

	y->iomem->dma_dst = y->dpram_bus_addr + offs;
	y->iomem->dma_src = offs;
	y->iomem->dma_len = size;
	y->iomem->jtag = 0x40;    // start DMA transfer ram write <- pci

	do {
		ndelay(500);
		j = y->iomem->jtag;
	} while ((j & 0x40) && retries++ < RETRIES);

	//if (wait_event_interruptible_timeout(y->wq, !((j = y->iomem->jtag) & 0x40), 1) <= 0)
	//if (wait_event_timeout(y->wq, !((j = y->iomem->jtag) & 0x40), 1) <= 0)
	//	printk(KERN_ERR "nyx: DMA read event timeout\n");

	if (j & 0x40) {
		printk(KERN_ERR "%s.%d: DMA read timeout\n", DEVICE_NAME, y->minor);
		return -4;
	}

	if (j & 0x10) {	// DMA error
		printk(KERN_ERR "%s.%d: DMA read error\n", DEVICE_NAME, y->minor);
		return -5;
	}
	//printk(KERN_INFO "nyx: DMA read %d retries\n", retries);

	return 0;
}

static int dpram_write(struct nyx_priv *y, size_t offs, size_t size)  {
	int retries = 0;
	uint32_t j;

	if (y == NULL) return -1;
	if (y->iomem == NULL) return -1;
	if (offs + size > y->dpram_size) return -3;

	y->iomem->dma_dst = y->dpram_bus_addr + offs;
	y->iomem->dma_src = offs;
	y->iomem->dma_len = size;
	y->iomem->jtag = 0x60;	// start DMA transfer ram read -> pci

	do {
		ndelay(500);
		j = y->iomem->jtag;
	} while ((j & 0x40) && retries++ < RETRIES);

	if (retries >= RETRIES) {
		printk(KERN_ERR "%s.%d: DMA write timeout\n", DEVICE_NAME, y->minor);
		return -4;
	}

	if (j & 0x10) {	// DMA error
		printk(KERN_ERR "%s.%d: DMA write error\n", DEVICE_NAME, y->minor);
		return -3;
	}
	//printk(KERN_INFO "nyx: DMA write %d retries\n", retries);

	return 0;

}

static ssize_t nyx_read(struct file *filp, char *buf, size_t size, loff_t *offs)
{
	int rc;
/*	int major, minor;
	short count;

	major = MAJOR(filp->f_dentry->d_inode->i_rdev);
	minor = MINOR(filp->f_dentry->d_inode->i_rdev);
*/
	struct nyx_fpriv *f = (struct nyx_fpriv *)filp->private_data;
	struct nyx_priv *y = f->y;

	if (*offs + size > y->dpram_size)
		size = y->dpram_size - *offs;

	if (size <= 0)
		return 0;

	y->iomem->jtag = 0x80;	// sync pulse

	if (f->nodma & 2) {
		y->iomem->jtag = 0;
		if (copy_to_user(buf, (u8*)&y->iomem->dpram + *offs, size))
			return -EFAULT;
	} else {
		rc = dpram_read(y, *offs, size);
		if (rc < 0)
			return -EFAULT;

		// if the buf was obtained by the mmap - skip the copying
		if (!(buf >= f->vm_start && buf + size <= f->vm_start + f->vm_size))
			if (copy_to_user(buf, (u8*)y->dpram + *offs, size))
				return -EFAULT;
	}

	*offs += size;

	return size;
}

static ssize_t nyx_write(struct file *filp, const char *buf, size_t size, loff_t *offs)
{
/*	int major, minor;
	short count;

	memset(msg, 0, 32);
	major = MAJOR(filp->f_dentry->d_inode->i_rdev);
	minor = MINOR(filp->f_dentry->d_inode->i_rdev);
	// -- copy the string from the user space program which open and write this device
	count = copy_from_user( msg, buff, len );

	printk("FILE OPERATION WRITE:%d:%d\n",major,minor);
	printk("msg: %s", msg);

	return len;
*/
	int rc;
	struct nyx_fpriv *f = (struct nyx_fpriv *)filp->private_data;
	struct nyx_priv *y = f->y;

	if (*offs + size > y->dpram_size)
		size = y->dpram_size - *offs;

	if (size <= 0)
		return 0;

	if (f->nodma & 1) {
		if (copy_from_user((u8*)&y->iomem->dpram + *offs, buf, size))
			return -EFAULT;
	} else {
		if (!(buf >= f->vm_start && buf + size <= f->vm_start + f->vm_size))
			if (copy_from_user((u8*)y->dpram + *offs, buf, size))
				return -EFAULT;

		rc = dpram_write(y, *offs, size);
		if (rc < 0)
			return -EFAULT;
	}

	*offs += size;

	return size;
}

static loff_t nyx_lseek(struct file *filp, loff_t offs, int orig)
{
	loff_t npos = 0;
	struct nyx_fpriv *f = (struct nyx_fpriv *)filp->private_data;
	struct nyx_priv *y = f->y;

	switch (orig) {
	case SEEK_SET:
		npos = offs;
		break;
	case SEEK_CUR:
		npos = filp->f_pos + offs;
		break;
	case SEEK_END:
		npos = y->dpram_size - offs;
		break;
	}
	if (npos > y->dpram_size) npos = y->dpram_size;
	if (npos < 0) npos = 0;
	filp->f_pos = npos;
	return npos;
}

static long nyx_ioctl(struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param)
{
	struct nyx_fpriv *f = (struct nyx_fpriv *)filp->private_data;
//	struct nyx_priv *y = f->y;

	switch (ioctl_num) {
	case 1:
		f->nodma = ioctl_param & 3;
		f->y->iomem->dma_isr = (ioctl_param>>1) & 2;
		///printk(KERN_INFO "nyx: nodma=%x\n", y->nodma);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int nyx_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct nyx_fpriv *f = (struct nyx_fpriv *)filp->private_data;
	struct nyx_priv *y = f->y;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (size > y->dpram_size && size > PAGE_SIZE)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, y->dpram_bus_addr >> PAGE_SHIFT, size, vma->vm_page_prot)) {
		return -EFAULT;
	}

	f->vm_start = (char *)vma->vm_start;
	f->vm_size = size;

	return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dmitry Yurtaev <dmitry@yurtaev.com>");
MODULE_DESCRIPTION("NYX servo interface card");
MODULE_VERSION(VER);
MODULE_SUPPORTED_DEVICE(DEVICE_NAME);

module_init(nyx_init);
module_exit(nyx_exit);
