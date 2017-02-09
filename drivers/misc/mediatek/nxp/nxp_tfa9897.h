
#ifndef NXP_TFA9897_H__
#define NXP_TFA9897_H__

#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
//#include <linux/io.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

#include <cust_eint.h>
#include <linux/jiffies.h>

/* Pre-defined definition */
#define NXP_DEBUG_ON
#define NXP_DEBUG_ARRAY_ON
#define NXP_DEBUG_FUNC_ON


//Log define
#define NXP_INFO(fmt,arg...)           printk("<<-NXP-INFO->> "fmt"\n",##arg)
#define NXP_ERROR(fmt,arg...)          printk("<<-NXP-ERROR->> "fmt"\n",##arg)
#define NXP_DEBUG(fmt,arg...)          do{\
                                         if(NXP_DEBUG_ON)\
                                         printk("<<-NXP-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
                                       }while(0)
#define NXP_DEBUG_ARRAY(array, num)    do{\
                                         s32 i;\
                                         u8* a = array;\
                                         if(NXP_DEBUG_ARRAY_ON)\
                                         {\
                                            printk("<<-NXP-DEBUG-ARRAY->>\n");\
                                            for (i = 0; i < (num); i++)\
                                            {\
                                                printk("%02x   ", (a)[i]);\
                                                if ((i + 1 ) %10 == 0)\
                                                {\
                                                    printk("\n");\
                                                }\
                                            }\
                                            printk("\n");\
                                        }\
                                       }while(0)
#define NXP_DEBUG_FUNC()               do{\
                                         if(NXP_DEBUG_FUNC_ON)\
                                         printk("<<-NXP-FUNC->> Func:%s@Line:%d\n",__func__,__LINE__);\
                                       }while(0)
#define NXP_SWAP(x, y)                 do{\
                                         typeof(x) z = x;\
                                         x = y;\
                                         y = z;\
                                       }while (0)

#endif /* TOUCHPANEL_H__ */
