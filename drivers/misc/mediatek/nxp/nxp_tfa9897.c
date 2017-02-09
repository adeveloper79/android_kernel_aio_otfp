/*
*
* NXP tfa9897
*
*/

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/module.h>
#include <linux/types.h>
//for platform device
#include <linux/platform_device.h>
//for misc device
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>   //proc file use

//#include <linux/pm.h>
#include "nxp_tfa9897.h"
//#include "../sound/AudDrv_Clk.h"

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

/*******************/

#define TFA9897_CONFIG_PROC_FILE "tfa9897"
#define I2C_DEVICE_ADDRESS_LEN 2
#define I2C_MASTER_CLOCK       400
#define MAX_TRANSACTION_LENGTH 8
#define MAX_I2C_TRANSFER_SIZE  (MAX_TRANSACTION_LENGTH - I2C_DEVICE_ADDRESS_LEN)
#define NXP_TFA9897_ADDR       0x68 

#define TFA9897_DEVICEID   0x0097
#define TFA9890_DEVICEID   0x0080

/*******************/
static int tfa9897_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tfa9897_i2c_remove(struct i2c_client *client);
static int tfa9897_platform_probe(struct platform_device *pdev);
static int tfa9897_platform_remove(struct platform_device *pdev);
static int tfa9897_misc_open(struct inode *inode, struct file *file);
static int tfa9897_misc_release(struct inode *inode, struct file *file);
static long tfa9897_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t tfa9897_write(struct file *fp, const char __user *data, size_t count, loff_t *offset);
static ssize_t tfa9897_read(struct file *fp,  char __user *data, size_t count,loff_t *offset);
static int tfa9897_config_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
static int tfa9897_config_write_proc(struct file *file, const char *buffer, unsigned long count, void *data);

static struct i2c_client *i2c_client_point = NULL;
static struct proc_dir_entry *tfa9897_config_proc = NULL;

static const struct i2c_device_id tfa9897_i2c_id[] = {{"tfa9897", 0}, {}};
static struct i2c_board_info __initdata tfa9897_i2c_board_info = { I2C_BOARD_INFO("tfa9897", (NXP_TFA9897_ADDR >> 1))};

static u8 *Tfa9897I2CDMABuf_va = NULL;
static dma_addr_t Tfa9897I2CDMABuf_pa = 0;

static struct i2c_driver tfa9897_i2c_driver =
{
    .driver = {
        .name = "tfa9897",
    },
    .probe = tfa9897_i2c_probe,
    .remove = tfa9897_i2c_remove,
    .id_table = tfa9897_i2c_id,
};

#ifdef CONFIG_OF
static const struct of_device_id tfa9897_of_match[] = {
	{ .compatible = "mediatek,tfa9897", },
	{},
};
#endif

static struct platform_driver tfa9897_platform_driver = {
	.probe      = tfa9897_platform_probe,
	.remove     = tfa9897_platform_remove,    
	.driver     = {
		.name  = "tfa9897",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tfa9897_of_match,
#endif
	}
};

static struct platform_device tfa9897_dev = {
	.name = "tfa9897",
	.id   = -1,
};

static struct file_operations tfa9897_fops = {
	.owner = THIS_MODULE,
	.open = tfa9897_misc_open,
	.release = tfa9897_misc_release,
	.unlocked_ioctl = tfa9897_unlocked_ioctl,
	.write	= tfa9897_write,
	.read	= tfa9897_read,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tfa9897_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tfa9897",
	.fops = &tfa9897_fops,
};
static char cmd_buf[256];

static const struct  file_operations tfa9897_proc_fops = {
     .write = tfa9897_config_write_proc,
     .read = tfa9897_config_read_proc,
};

static int nxp_i2c_master_send(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	
	msg.timing = I2C_MASTER_CLOCK;

	if(count <= 8)
	{	
		msg.addr = client->addr & I2C_MASK_FLAG;
	}
	else
	{
		msg.addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
	}	
		
	msg.flags = client->flags & I2C_M_TEN;

	msg.len = count;
	msg.buf = (char *)buf;
	msg.ext_flag = client->ext_flag;
	ret = i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? count : ret;
}

static int nxp_i2c_master_recv(const struct i2c_client *client, char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;

	msg.timing = I2C_MASTER_CLOCK;
	msg.flags = client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.ext_flag = client->ext_flag;
	msg.buf = (char *)buf;

	if(count <= 8)
	{
		msg.addr = client->addr & I2C_MASK_FLAG;
	}
	else
	{
		msg.addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
	}

	ret = i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg received), return #bytes received,
	 * else error code.
	 */
	return (ret == 1) ? count : ret;
}

static int i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
    u8 buf;
    int ret = 0;
    
    buf = addr;
    ret = nxp_i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        NXP_INFO("send command error!!\n");
        return -EFAULT;
    }
    ret = nxp_i2c_master_recv(client, (char*)&buf, 1);
    if (ret < 0) {
        NXP_INFO("reads data error!!\n");
        return -EFAULT;
    } else {
        //NXP_INFO("%s(0x%02X) = %02X\n", __func__, addr, buf);    
    }
    *data = buf;
    return 0;
}
/*----------------------------------------------------------------------------*/
static int i2c_write_byte(struct i2c_client *client, u8 addr, u8 data)
{
    u8 buf[] = {addr, data};
    int ret = 0;

    ret = nxp_i2c_master_send(client, (const char*)buf, sizeof(buf));
    if (ret < 0) {
        NXP_INFO("send command error!!\n");
        return -EFAULT;
    } else {
        //NXP_INFO("%s(0x%02X)= %02X\n", __func__, addr, data);
    }
    return 0;
}

static int i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    if (len == 1) {
        return i2c_read_byte(client, addr, data);
    } else {
        u8 beg = addr; 
        struct i2c_msg msgs[2] = {
            {
                .addr = client->addr,    .flags = 0,
                .len = 1,                .buf= &beg
            },
            {
                .addr = client->addr,    .flags = I2C_M_RD,
                .len = len,             .buf = data,
            }
        };
        int err;

        if (!client)
            return -EINVAL;
        else if (len > MAX_TRANSACTION_LENGTH) {        
            NXP_INFO(" length %d exceeds %d\n", len, MAX_TRANSACTION_LENGTH);
            return -EINVAL;
        }

        err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
        if (err != 2) {
            NXP_INFO("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
            err = -EIO;
        } else {
            static char buf[128];
            int idx, buflen = 0;
            for (idx = 0; idx < len; idx++)
                buflen += snprintf(buf+buflen, sizeof(buf)-buflen, "%02X ", data[idx]);
            //NXP_INFO("%s(0x%02X,%2d) = %s\n", __func__, addr, len, buf);
            err = 0;    /*no error*/
        }
        return err;
    }

}

static int i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{   /*because address also occupies one byte, the maximum length for write is 7 bytes*/
    int err, idx, num;
    char buf[MAX_TRANSACTION_LENGTH];

    if (!client)
        return -EINVAL;
    else if (len >= MAX_TRANSACTION_LENGTH) {        
        NXP_INFO(" length %d exceeds %d\n", len, MAX_TRANSACTION_LENGTH);
        return -EINVAL;
    }    

    num = 0;
    buf[num++] = addr;
    for (idx = 0; idx < len; idx++)
        buf[num++] = data[idx];

    err = nxp_i2c_master_send(client, buf, num);
    if (err < 0) {
        NXP_INFO("send command error!!\n");
        return -EFAULT;
    } else {
        static char buf[128];
        int idx, buflen = 0;
        for (idx = 0; idx < len; idx++)
            buflen += snprintf(buf+buflen, sizeof(buf)-buflen, "%02X ", data[idx]);
        NXP_INFO("%s(0x%02X,%2d)= %s\n", __func__, addr, len, buf);    
        err = 0;    /*no error*/
    }
    return err;
}
static int i2c_write_dummy( struct i2c_client *client, u16 addr )
{
	int err, idx, num;

    char buf[MAX_TRANSACTION_LENGTH];
	if (!client)
		return -EINVAL;


    //NXP_INFO("i2c_write_dummy to device %02X address %04X\n", client->addr, addr );
	num = 0;
		buf[num++] = addr;

    err = nxp_i2c_master_send(client, buf, num);
    if (err < 0) {
        NXP_INFO("send command error!!\n");
        return -EFAULT;
    } else {
        //NXP_INFO("%s(0x%02X,%2d)= %s\n", __func__, addr, buf);    
        err = 0;    /*no error*/
    }

    return 0;
}

static int tfa9897_config_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int err =0,i=0;
	u8 raw_buffer_curr[10] = {0};

	char *ptr = page;

	NXP_INFO(" %s \n", __func__ );

	ptr += sprintf( ptr, "==== tfa9897_config_read_proc====\n" );
	err = i2c_read_block(i2c_client_point,0x00,raw_buffer_curr,2);
	if (err!=0) NXP_ERROR("read error\n");
	
	for ( i = 0 ; i < 10 ; i++ )
    {
        ptr += sprintf( ptr, "0x%02X ", raw_buffer_curr[i] );

    }    
  ptr += sprintf( ptr, "\n" );
  *eof = 1;
  return ( ptr - page );
}


static int tfa9897_config_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int ret;
	int cmd, p1, p2, p3, p4, p5, p6;   
	int clk_en;

    NXP_INFO("%s write count %ld\n", __func__,count );

	if (count == 0) return -1;
	if(count > 255) count = 255;

	ret = copy_from_user(cmd_buf, buffer, count);
	if (ret < 0) {
		NXP_ERROR("copy from user error\n");
		return -1;
	}
	
	cmd_buf[count] = '\0';
	NXP_INFO("[****Debug****]Write %s\n", cmd_buf);

	sscanf(cmd_buf, "%x %x %x %x %x %x %x", &cmd, &p1, &p2, &p3, &p4, &p5, &p6);

    return count;
}

static int tfa9897_checkID(struct i2c_client *client)
{
	u8 databuf[2];
	u8 ii = 0;
	int res = 0;
	int deviceid = 0;

	memset(databuf, 0, sizeof(u8)*2);
	databuf[0] = 0x03;    
	ii = 0;
	while(ii < 5)
	{
		res = nxp_i2c_master_send(client, databuf, 0x1);
		if(res <= 0)
		{
			ii++; 
		}
        else
		{
			break;
		}
	}
	if(ii == 5)
	{
		NXP_INFO("tfa9897_checkID error: i2c write to TFA9897!\n");
		goto exit_tfa9897_checkID;
	}
        
	databuf[0] = 0x0;
	ii = 0;
	while(ii < 5)
	{        
		res = nxp_i2c_master_recv(client, databuf, 0x02);
		if(res <= 0)
		{
			ii++;
		}
		else
		{
			break;
		}
	}
	if(ii == 5)
	{
		NXP_INFO("tfa9897_checkID error: i2c read from TFA9897!\n");
		goto exit_tfa9897_checkID;
	}

	deviceid = (databuf[0]<<8)|databuf[1];
	if ((deviceid!=TFA9897_DEVICEID) && (deviceid!=TFA9890_DEVICEID))
	{
		NXP_INFO("tfa9897 device id: 0x%x, fail\n", deviceid);
		goto exit_tfa9897_checkID;
	}
	else
	{
		NXP_INFO("tfa9897 device id: 0x%x, pass\n ", deviceid);
	}

exit_tfa9897_checkID:
	if (res <= 0)
	{
		//NXP_INFO("tfa9897 I2C error, can not communication\n ");
		return -1;
	}
	return 0;
}

static void tfa9897_dump(struct i2c_client *client)
{
	u8 reg[8]={0x01,0x04,0x05,0x06,0x07,0x08,0x09,0x80};
	u8 databuf[2];
	u8 i = 0;
	int res = 0;
	int regval[8];
	
	memset(databuf, 0, sizeof(u8)*2);
	for (i = 0; i < 8; i++)
	{
		databuf[0] = reg[i];
		res = nxp_i2c_master_send(client, databuf, 0x1);
        
		res = nxp_i2c_master_recv(client, databuf, 0x02);
		regval[i] = (databuf[0]<<8)|databuf[1];

		NXP_INFO("tfa9897 reg[%d]: 0x%x\n ", reg[i], regval[i]);
	}
}

static void tfa9897_bypass(struct i2c_client *client)
{
	u8 databuf[3];
	int res = 0;
	
	databuf[0] = 0x09;
        databuf[1] = 0x00;
	databuf[2] = 0x02;
	res = nxp_i2c_master_send(client, databuf, 0x3);
	msleep(10);

	databuf[0] = 0x04;
        databuf[1] = 0x78; //0x70;
	databuf[2] = 0x0b; //0x18;
	res = nxp_i2c_master_send(client, databuf, 0x3);

	databuf[0] = 0x09;
        databuf[1] = 0x02; //0x42;
	databuf[2] = 0x08; //0x6c;
	res = nxp_i2c_master_send(client, databuf, 0x3);
}

static int tfa9897_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	NXP_INFO("prob");

	i2c_client_point = client;

	hwPowerOn(MT6325_POWER_LDO_VRF18_1,VOL_1850,"AUDIOPA" ); //sisley2,LDO enable
#ifndef CONFIG_LENOVO_EARPHONE_GNDIC
	mt_set_gpio_mode(GPIO_SMARTPA_LDO_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_SMARTPA_LDO_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_SMARTPA_LDO_EN_PIN, GPIO_OUT_ONE);
	msleep(50);
#endif
	mt_set_gpio_mode(GPIO_SMARTPA_RST_PIN, GPIO_MODE_00); /*GPIO125*/
	mt_set_gpio_out(GPIO_SMARTPA_RST_PIN, GPIO_OUT_ZERO);
	msleep(2);
	mt_set_gpio_out(GPIO_SMARTPA_RST_PIN, GPIO_OUT_ONE);
	msleep(2);
	mt_set_gpio_out(GPIO_SMARTPA_RST_PIN, GPIO_OUT_ZERO);
	msleep(10);

	mt_set_gpio_mode(GPIO_SMARTPA_I2S_BCK_PIN, GPIO_MODE_02); /*GPIO127*/
	mt_set_gpio_mode(GPIO_SMARTPA_I2S_WS_PIN, GPIO_MODE_02);  /*GPIO128*/
	mt_set_gpio_mode(GPIO_SMARTPA_I2S_DOUT_PIN, GPIO_MODE_02); /*GPIO129*/
	mt_set_gpio_mode(GPIO_SMARTPA_I2S_DIN_PIN, GPIO_MODE_01); /*GPIO133*/

	err = tfa9897_checkID(client);
	if(err < 0)
	{
		goto exit_init_failed;
	}

	//tfa9897_dump(client);
        //tfa9897_bypass(client);
	//tfa9897_dump(client);

	tfa9897_config_proc = proc_create(TFA9897_CONFIG_PROC_FILE, 0660, NULL, &tfa9897_proc_fops);

	if (tfa9897_config_proc == NULL)
	{
		NXP_ERROR("create_proc_entry %s failed\n", TFA9897_CONFIG_PROC_FILE);
		goto exit_init_failed;
	}

	//creat misc device
	if (err = misc_register(&tfa9897_misc_device))
	{
		NXP_ERROR("fail to register tfa9897_misc_device\n");
		goto exit_init_failed;
	}

	if (Tfa9897I2CDMABuf_va == NULL)
	{
		Tfa9897I2CDMABuf_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, &Tfa9897I2CDMABuf_pa, GFP_KERNEL);
	}
	if(!Tfa9897I2CDMABuf_va)
	{
		NXP_ERROR("tfa9897 dma_alloc_coherent error\n");
		goto exit_init_failed;
	}
	NXP_INFO("tfa9897_i2c_probe success\n");
	return 0;
	
exit_init_failed:
	NXP_INFO("tfa9897_i2c_probe failed\n");
	return -1;
	
}

 static int tfa9897_i2c_remove(struct i2c_client *client)
 {
	if(Tfa9897I2CDMABuf_va)
	{
		dma_free_coherent(NULL, 4096, Tfa9897I2CDMABuf_va, Tfa9897I2CDMABuf_pa);
		Tfa9897I2CDMABuf_va = NULL;
		Tfa9897I2CDMABuf_pa = 0;
	}
	return 0;
 }

static int tfa9897_platform_probe(struct platform_device *pdev)
{
	NXP_INFO("init--platform probe---->");

	if(i2c_add_driver(&tfa9897_i2c_driver))
	{
		NXP_ERROR("add i2c driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int tfa9897_platform_remove(struct platform_device *pdev)
{
 
	i2c_del_driver(&tfa9897_i2c_driver);
  
	return 0;
}

static int tfa9897_misc_open(struct inode *inode, struct file *file)
{

	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int tfa9897_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}
/*----------------------------------------------------------------------------*/

static long tfa9897_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	NXP_INFO("cmd is 0x%x\n",cmd);
	switch(cmd)
	{
		case I2C_SLAVE:
			i2c_client_point->addr = arg;
			break;
		default:
			break;
			
	}
	return 0;
}

static ssize_t tfa9897_write(struct file *file, const char __user *data, size_t count, loff_t *offset)
{
	int i = 0;
	int ret;
	char *tmp;

	//if (count > 8192)
	//	count = 8192;

	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	if (copy_from_user(tmp,data,count)) {
		kfree(tmp);
		return -EFAULT;
	}

	//NXP_INFO("i2c-dev: i2c-%d writing %zu bytes.\n", iminor(file->f_path.dentry->d_inode), count);

	for(i = 0;  i < count; i++)
	{
		Tfa9897I2CDMABuf_va[i] = tmp[i];
	}

	if(count <= 8)
	{
		ret = nxp_i2c_master_send(i2c_client_point,tmp,count);
	}
	else
	{
		ret = nxp_i2c_master_send(i2c_client_point, Tfa9897I2CDMABuf_pa, count);
	}
	kfree(tmp);
	return ret;
}

static ssize_t tfa9897_read(struct file *file,  char __user *data, size_t count,loff_t *offset)
{
	int i = 0;
	char *tmp;
	int ret;

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;

	//NXP_INFO("i2c-dev: i2c-%d reading %zu bytes.\n", iminor(file->f_path.dentry->d_inode), count);

	if(count <= 8)
	{
		ret = nxp_i2c_master_recv(i2c_client_point,tmp,count);
	}
	else
	{
		ret = nxp_i2c_master_recv(i2c_client_point,Tfa9897I2CDMABuf_pa,count);
		for(i = 0; i < count; i++)
		{
			tmp[i] = Tfa9897I2CDMABuf_va[i];
		}
	}
	
	if (ret >= 0)
		ret = copy_to_user(data,tmp,count)?-EFAULT:ret;
	kfree(tmp);
	return ret;
}

/* called when loaded into kernel */
static int __init tfa9897_driver_init(void)
{
	NXP_INFO("init");

	i2c_register_board_info(1, &tfa9897_i2c_board_info, 1);

	if(platform_driver_register(&tfa9897_platform_driver))
	{
		NXP_ERROR("failed to register platform driver");
		return -1;
	}

	return 0;
}

/* should never be called */
static void __exit tfa9897_driver_exit(void)
{
	NXP_INFO("exit");

	i2c_del_driver(&tfa9897_i2c_driver);
}

module_init(tfa9897_driver_init);
module_exit(tfa9897_driver_exit);

