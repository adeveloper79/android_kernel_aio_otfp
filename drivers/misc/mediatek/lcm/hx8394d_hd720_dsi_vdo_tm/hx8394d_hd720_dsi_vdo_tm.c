

#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_pm_ldo.h>
    #include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif
//Lenovo-sw wuwl10 add 20150113 for esd recover backlight
#ifndef BUILD_LK
static unsigned int esd_last_backlight_level = 255;
#endif

static const unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)
#define PHYSICAL_WIDTH (68)
#define PHYSICAL_HEIGHT (121)
#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#ifndef GPIO_LCD_BIAS_ENP_PIN
#define GPIO_LCD_BIAS_ENP_PIN GPIO122
#endif
#ifndef GPIO_LCD_BIAS_ENN_PIN
#define GPIO_LCD_BIAS_ENN_PIN GPIO95
#endif
#ifndef GPIO_LCM_BL_EN
#define GPIO_LCM_BL_EN GPIO113
#endif
#ifndef GPIO_LCM_LED_EN
#define GPIO_LCM_LED_EN GPIO94
#endif
#ifndef GPIO_DISP_ID0_PIN
#define GPIO_DISP_ID0_PIN GPIO114
#endif

#define LCM_DSI_CMD_MODE									0
#ifndef FPGA_EARLY_PORTING
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define LCM_ID_HX8394 0x94

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
       

static struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},

    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_backlight_level_setting[] = {
{0x51, 1, {0xFF}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_cabc_level_setting[] = {
{0x55, 1, {0x00}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_inverse_off_setting[] = {
{0x20, 1, {0x00}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_inverse_on_setting[] = {
{0x21, 1, {0x00}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
       	}
    }
	
}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		params->physical_width = PHYSICAL_WIDTH;
		params->physical_height = PHYSICAL_HEIGHT;
#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = BURST_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
        //The following defined the fomat for data coming from LCD engine.
        params->dsi.data_format.color_order     = LCM_COLOR_ORDER_RGB;
        params->dsi.data_format.trans_seq       = LCM_DSI_TRANS_SEQ_MSB_FIRST;
        params->dsi.data_format.padding         = LCM_DSI_PADDING_ON_LSB;
        params->dsi.data_format.format              = LCM_DSI_FORMAT_RGB888;

        // Highly depends on LCD driver capability.
        params->dsi.packet_size=256;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 12;
		params->dsi.vertical_frontporch					= 15;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 16;
		params->dsi.horizontal_backporch				= 55;
		params->dsi.horizontal_frontporch				= 55;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

#ifndef FPGA_EARLY_PORTING
        params->dsi.PLL_CLOCK = 220; //this value must be in MTK suggested table
#else
        params->dsi.pll_div1 = 0;
        params->dsi.pll_div2 = 0;
        params->dsi.fbk_div = 0x1;
#endif
//lenovo_sw wuwl10 20150113 modify for enable esd check
#if 0
	    params->dsi.esd_check_enable =1;
	    params->dsi.customization_esd_check_enable =1;

	    params->dsi.lcm_esd_check_table[2].cmd =0xD9;
	    params->dsi.lcm_esd_check_table[2].count =1;
	    params->dsi.lcm_esd_check_table[2].para_list[0] =0x80;


	    params->dsi.lcm_esd_check_table[1].cmd =0x0A;
	    params->dsi.lcm_esd_check_table[1].count =1;
	    params->dsi.lcm_esd_check_table[1].para_list[0] =0x1C;

	    params->dsi.lcm_esd_check_table[0].cmd =0x09;
	    params->dsi.lcm_esd_check_table[0].count =4;
	    params->dsi.lcm_esd_check_table[0].para_list[0] =0x80;
	    params->dsi.lcm_esd_check_table[0].para_list[1] =0x73;
	    params->dsi.lcm_esd_check_table[0].para_list[2] =0x06;
	    params->dsi.lcm_esd_check_table[0].para_list[3] =0x00;
#endif

}
const static unsigned char LCD_MODULE_ID = 0x01; 

static unsigned int lcm_compare_id(void)
{

unsigned char  id_pin_read = 0;


#ifdef GPIO_DISP_ID0_PIN
	id_pin_read = mt_get_gpio_in(GPIO_DISP_ID0_PIN);
	#endif
	#ifdef BUILD_LK
		dprintf(0, "%s,LCD_ID_value=%d \n", __func__, id_pin_read);
	#endif
	if(LCD_MODULE_ID == id_pin_read)
		return 1;
	else
		return 0;
}


static void lcm_init(void)
{
	unsigned int data_array[16];
	int ret=0;

	SET_RESET_PIN(1);
	MDELAY(1);
   	SET_RESET_PIN(0);
	MDELAY(1);
    SET_RESET_PIN(1);
	MDELAY(5);

	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
	MDELAY(10);

	mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
	MDELAY(50);


#if 1

        data_array[0]= 0x00043902; 
        data_array[1]= 0x9483FFB9; 
        dsi_set_cmdq(&data_array, 2, 1); 


        data_array[0]= 0x00033902; 
        data_array[1]= 0x008373BA; 
        dsi_set_cmdq(&data_array, 2, 1); 

        data_array[0]= 0x00103902; 
        data_array[1]= 0x110B6CB1; 
        data_array[2]= 0xF1110437; 
        data_array[3]= 0x2395E380; 
        data_array[4]= 0x18D2C080; 
        dsi_set_cmdq(&data_array, 5, 1); 



        data_array[0]= 0x000C3902; 
        data_array[1]= 0x0E6400B2; 
        data_array[2]= 0x0823320D; 
        data_array[3]= 0x004D1C08; 
        dsi_set_cmdq(&data_array, 4, 1); 


        data_array[0]= 0x000D3902; 
        data_array[1]= 0x03FF00B4; 
        data_array[2]= 0x03500350; 
        data_array[3]= 0x016A0150; 
        data_array[4]= 0x0000006A; 
        dsi_set_cmdq(&data_array, 5, 1); 

        data_array[0]= 0x00043902; 
        data_array[1]= 0x010E41BF; 
        dsi_set_cmdq(&data_array, 2, 1); 

        data_array[0]= 0x00263902; 
        data_array[1]= 0x000700D3; //GIP 
        data_array[2]= 0x00100000; 
        data_array[3]= 0x00051032; 
        data_array[4]= 0x00103200; 
        data_array[5]= 0x10320000; 
        data_array[6]= 0x36000000; 
        data_array[7]= 0x37090903; 
        data_array[8]= 0x00370000; 
        data_array[9]= 0x0A000000; 
        data_array[10]= 0x00000100; 
        dsi_set_cmdq(&data_array, 11, 1); 

        data_array[0]= 0x002D3902; 
        data_array[1]= 0x000302D5; 
        data_array[2]= 0x04070601; 
        data_array[3]= 0x22212005; 
        data_array[4]= 0x18181823; 
        data_array[5]= 0x18181818; 
        data_array[6]= 0x18181818; 
        data_array[7]= 0x18181818; 
        data_array[8]= 0x18181818; 
        data_array[9]= 0x18181818; 
        data_array[10]= 0x24181818; 
        data_array[11]= 0x19181825; 
        data_array[12]= 0x00000019; 
        dsi_set_cmdq(&data_array, 13, 1); 


        data_array[0]= 0x002D3902; 
        data_array[1]= 0x070405D6; 
        data_array[2]= 0x03000106; 
        data_array[3]= 0x21222302; 
        data_array[4]= 0x18181820; 
        data_array[5]= 0x58181818; 
        data_array[6]= 0x18181858; 
        data_array[7]= 0x18181818; 
        data_array[8]= 0x18181818; 
        data_array[9]= 0x18181818; 
        data_array[10]= 0x25181818; 
        data_array[11]= 0x18191924; 
        data_array[12]= 0x00000018; 
        dsi_set_cmdq(&data_array, 13, 1); 
        

        data_array[0]= 0x002B3902; 
        data_array[1]= 0x211C07E0; 
        data_array[2]= 0x2C3F3B35; 
        data_array[3]= 0x0C0B0648; 
        data_array[4]= 0x15120E17; 
        data_array[5]= 0x12061412; 
        data_array[6]= 0x1C071814; 
        data_array[7]= 0x3F3B3622; 
        data_array[8]= 0x0A06482D; 
        data_array[9]= 0x120F170C; 
        data_array[10]= 0x07141215; 
        data_array[11]= 0x00181412; 
        dsi_set_cmdq(&data_array, 12, 1); 

        data_array[0]= 0x00023902; 
        data_array[1]= 0x000009CC; 
        dsi_set_cmdq(&data_array, 2, 1); 


        data_array[0]= 0x00053902; 
        data_array[1]= 0x40C000C7; 
        data_array[2]= 0x000000C0; 
        dsi_set_cmdq(&data_array, 3, 1); 
        MDELAY(10); 

        data_array[0]= 0x00033902; 
        data_array[1]= 0x007575B6; 
        dsi_set_cmdq(&data_array, 2, 1); 

        data_array[0]= 0x00033902; 
        data_array[1]= 0x001430C0; 
        dsi_set_cmdq(&data_array, 2, 1); 

        data_array[0]= 0x00023902; 
        data_array[1]= 0x000007BC; 
        dsi_set_cmdq(&data_array, 2, 1); 
  
         data_array[0]= 0x00043902; 
        data_array[1]= 0x14001fC9;// PWM 20K 
        dsi_set_cmdq(&data_array, 2, 1); 

        data_array[0] = 0x00352300;//te on 
        dsi_set_cmdq(&data_array, 1, 1); 

        data_array[0] = 0x00512300;//bl mode 
        dsi_set_cmdq(&data_array, 1, 1); 

        data_array[0] = 0x24532300;//bl mode 
        dsi_set_cmdq(&data_array, 1, 1); 

	data_array[0] = 0x02552300;//cabc 03
	dsi_set_cmdq(&data_array, 1, 1);
MDELAY(5);

       data_array[0]= 0x00033902; 
        data_array[1]= 0x000101E4;// 
        dsi_set_cmdq(&data_array, 2, 1); 


        data_array[0] = 0x00110500;         
        dsi_set_cmdq(&data_array, 1, 1); 
        MDELAY(120); 

        data_array[0] = 0x00290500;         
        dsi_set_cmdq(&data_array, 1, 1); 
        //MDELAY(10); 
#else
        data_array[0]= 0x00043902;
	data_array[1]= 0x9583FFB9;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00053902;
	data_array[1]= 0x7D0000B0;
	data_array[2]= 0x0000000C;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0]= 0x000E3902;
	data_array[1]= 0xA08333BA;
	data_array[2]= 0x0080B265;
	data_array[3]= 0x0FFF1000;
	data_array[4]= 0x00000800;
	dsi_set_cmdq(&data_array, 5, 1);

	data_array[0]= 0x000C3902;
	data_array[1]= 0x17176CB1;
	data_array[2]= 0xF1110423;
	data_array[3]= 0x23959980;
	dsi_set_cmdq(&data_array, 4, 1);

        data_array[0]= 0x00023902;
	data_array[1]= 0x000077D2;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00063902;
	data_array[1]= 0x0CB400B2;
	data_array[2]= 0x00002A10;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0]= 0x00383902;
	data_array[1]= 0x000900D3; //GIP
	data_array[2]= 0x00100000;
	data_array[3]= 0x00011032;
	data_array[4]= 0xC0133201;
	data_array[5]= 0x10320000;
	data_array[6]= 0x37000008;
	data_array[7]= 0x37030304;
	data_array[8]= 0x00470004;
	data_array[9]= 0x0A000000;
	data_array[10]= 0x15010100;
	data_array[11]= 0x00000000;
	data_array[12]= 0xC0030000;
	data_array[13]= 0x04020800;
	data_array[14]= 0x15010000;
	dsi_set_cmdq(&data_array, 15, 1);

	data_array[0]= 0x00173902;
	data_array[1]= 0x30FF00B4;
	data_array[2]= 0x30433043;
	data_array[3]= 0x01500143;
	data_array[4]= 0x013A0150;
	data_array[5]= 0x013A033A;
	data_array[6]= 0x004A014A;
	dsi_set_cmdq(&data_array, 7, 1);

	data_array[0]= 0x002D3902;
	data_array[1]= 0x183939D5;
	data_array[2]= 0x03000118;
	data_array[3]= 0x07040502;
	data_array[4]= 0x18181806;
	data_array[5]= 0x38181818;
	data_array[6]= 0x21191938;
	data_array[7]= 0x18222320;
	data_array[8]= 0x18181818;
	data_array[9]= 0x18181818;
	data_array[10]= 0x18181818;
	data_array[11]= 0x18181818;
	data_array[12]= 0x00000018;
	dsi_set_cmdq(&data_array, 13, 1);


	data_array[0]= 0x002D3902;
	data_array[1]= 0x193939D6;
	data_array[2]= 0x04070619;
	data_array[3]= 0x00030205;
	data_array[4]= 0x18181801;
	data_array[5]= 0x38181818;
	data_array[6]= 0x22181838;
	data_array[7]= 0x58212023;
	data_array[8]= 0x58585858;
	data_array[9]= 0x58585858;
	data_array[10]= 0x58585858;
	data_array[11]= 0x58585858;
	data_array[12]= 0x00000058;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00023902;
	data_array[1]= 0x00000ACB;
	dsi_set_cmdq(&data_array, 2, 1);
	
        data_array[0]= 0x00023902;
	data_array[1]= 0x000008CC;
	dsi_set_cmdq(&data_array, 2, 1);


        data_array[0]= 0x00033902;
	data_array[1]= 0x001530C0;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00053902;
	data_array[1]= 0x0C0800C7;
	data_array[2]= 0x0000D000;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0]= 0x002B3902;
	data_array[1]= 0x090300E0;
	data_array[2]= 0x183F382F;
	data_array[3]= 0x0D0A0739;
	data_array[4]= 0x15131018;
	data_array[5]= 0x11071514;
	data_array[6]= 0x03001713;
	data_array[7]= 0x3F382F08;
	data_array[8]= 0x0A073918;
	data_array[9]= 0x1310180D;
	data_array[10]= 0x06141316;
	data_array[11]= 0x00171310;
	dsi_set_cmdq(&data_array, 12, 1);

//********************YYG*******************//
        data_array[0]= 0x00023902;
	data_array[1]= 0x000001BD;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00043902;
	data_array[1]= 0x010002EF;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00023902;
	data_array[1]= 0x000000BD;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00073902;
	data_array[1]= 0xF300B1EF;
	data_array[2]= 0x00022F13;
	dsi_set_cmdq(&data_array, 3, 1);

        data_array[0]= 0x00033902;
	data_array[1]= 0x0060B2EF;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x000E3902;
	data_array[1]= 0x4B26B5EF;
        data_array[2]= 0x0004000F;
	data_array[3]= 0x00808001;
	data_array[4]= 0x00000044;
	dsi_set_cmdq(&data_array, 5, 1);

        data_array[0]= 0x00163902;
	data_array[1]= 0x0040B6EF;
        data_array[2]= 0x01000080;
	data_array[3]= 0x2E090000;
	data_array[4]= 0x20200009;
	data_array[5]= 0x00092E09;
	data_array[6]= 0x00002020;
	dsi_set_cmdq(&data_array, 7, 1);

        data_array[0]= 0x000A3902;
	data_array[1]= 0x7000B7EF;
        data_array[2]= 0x03800400;
	data_array[3]= 0x00000480;
	dsi_set_cmdq(&data_array, 4, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0000C0EF;
        data_array[2]= 0x62D363D2;
	data_array[3]= 0xC260C3D5;
	data_array[4]= 0x81CE8355;
	data_array[5]= 0x02050349;
	data_array[6]= 0x01AC0037;
        data_array[7]= 0x01ED03D8;
	data_array[8]= 0x0182004C;
        data_array[9]= 0x02CC0327;
	data_array[10]= 0x02D1001B;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0000C1EF;
        data_array[2]= 0x60216138;
	data_array[3]= 0xC316C2D9;
	data_array[4]= 0x82898035;
	data_array[5]= 0x01560100;
	data_array[6]= 0x018F0237;
        data_array[7]= 0x021603D2;
	data_array[8]= 0x012002C6;
        data_array[9]= 0x032F03C4;
	data_array[10]= 0x02D1035E;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0001C2EF;
        data_array[2]= 0x1C70E098;
	data_array[3]= 0xC134A031;
	data_array[4]= 0x8562E062;
	data_array[5]= 0x7DC567C5;
	data_array[6]= 0x08976D8B;
        data_array[7]= 0x6C08BA16;
	data_array[8]= 0x2780282C;
        data_array[9]= 0x323E5F58;
	data_array[10]= 0x991580B1;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00243902;
	data_array[1]= 0x0002C4EF;
        data_array[2]= 0x8EE98E89;
	data_array[3]= 0x1F641C29;
	data_array[4]= 0x3F9F3E25;
	data_array[5]= 0x731D72EF;
	data_array[6]= 0xE186E097;
        data_array[7]= 0xDA0DDB2E;
	data_array[8]= 0x88888888;
        data_array[9]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 10, 1);

        data_array[0]= 0x00283902;
	data_array[1]= 0x0002C5EF;
        data_array[2]= 0x83888289;
	data_array[3]= 0x12101312;
	data_array[4]= 0x2B212A25;
	data_array[5]= 0x6743664B;
	data_array[6]= 0xFD86FC97;
        data_array[7]= 0xC60DC72E;
	data_array[8]= 0xA21BBA5C;
        data_array[9]= 0x88888888;
	data_array[10]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 11, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0003C7EF;
        data_array[2]= 0x344124B1;
	data_array[3]= 0xF033E002;
	data_array[4]= 0x70C67085;
	data_array[5]= 0xAED6CCBB;
	data_array[6]= 0xB105A576;
        data_array[7]= 0x8633865C;
	data_array[8]= 0x4C464C5F;
        data_array[9]= 0x59CD58FE;
	data_array[10]= 0x7266737C;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0000C8EF;
        data_array[2]= 0x004D0000;
	data_array[3]= 0x00E6009A;
	data_array[4]= 0x01800133;
	data_array[5]= 0x021A01CD;
	data_array[6]= 0x02B30266;
        data_array[7]= 0x034D0300;
	data_array[8]= 0x03E6039A;
        data_array[9]= 0x04000400;
	data_array[10]= 0x00000400;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0000C9EF;
        data_array[2]= 0x00100000;
	data_array[3]= 0x0053002D;
	data_array[4]= 0x00B30080;
	data_array[5]= 0x012800EB;
	data_array[6]= 0x01B0016A;
        data_array[7]= 0x024801FA;
	data_array[8]= 0x02EE0299;
        data_array[9]= 0x03A20346;
	data_array[10]= 0x00000400;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0000CAEF;
        data_array[2]= 0x00400000;
	data_array[3]= 0x00C00080;
	data_array[4]= 0x01400100;
	data_array[5]= 0x01C00180;
	data_array[6]= 0x02400200;
        data_array[7]= 0x02C00280;
	data_array[8]= 0x03400300;
        data_array[9]= 0x03C00380;
	data_array[10]= 0x00000400;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00303902;
	data_array[1]= 0x0000CBEF;
        data_array[2]= 0x006F0000;
	data_array[3]= 0x010C00C2;
	data_array[4]= 0x01940152;
	data_array[5]= 0x021101D3;
	data_array[6]= 0x0286024C;
        data_array[7]= 0x02F702BF;
	data_array[8]= 0x0363032D;
        data_array[9]= 0x03CC0398;
	data_array[10]= 0x00000400;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

        data_array[0]= 0x00183902;
	data_array[1]= 0x0004CCEF;
        data_array[2]= 0x300D300C;
	data_array[3]= 0x601A6058;
	data_array[4]= 0xC035C031;
	data_array[5]= 0x88888888;
	data_array[6]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 7, 1);

        data_array[0]= 0x00073902;
	data_array[1]= 0x3210D3EF;
        data_array[2]= 0x00107654;
	dsi_set_cmdq(&data_array, 3, 1);

        data_array[0]= 0x00063902;
	data_array[1]= 0x3210D4EF;
        data_array[2]= 0x00007654;
	dsi_set_cmdq(&data_array, 3, 1);

        data_array[0]= 0x00053902;
	data_array[1]= 0x0101D5EF;
        data_array[2]= 0x00000001;
	dsi_set_cmdq(&data_array, 3, 1);

        data_array[0]= 0x00033902;
	data_array[1]= 0x0012D6EF;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00063902;
	data_array[1]= 0xC14CDEEF;
        data_array[2]= 0x00008D52;
	dsi_set_cmdq(&data_array, 3, 1);

        data_array[0]= 0x00143902;
	data_array[1]= 0xAB00DFEF;
        data_array[2]= 0x0A0B0C0D;
	data_array[3]= 0x0A0B0C0D;
	data_array[4]= 0x00FF0000;
	data_array[5]= 0x0A0B0C0D;
	dsi_set_cmdq(&data_array, 6, 1);

//********************YYG*******************//

        data_array[0]= 0x00033902;
	data_array[1]= 0x006E6EB6;
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(5);
	data_array[0] = 0x00110500;	
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;	
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(20);
#endif
#ifdef GPIO_LCM_BL_EN
mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ONE);
#endif
#ifdef GPIO_LCM_LED_EN
mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ONE);
#endif
} 


       
 

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               



static void lcm_suspend(void)
{
#ifdef GPIO_LCM_BL_EN
	mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ZERO);
#endif
#ifdef GPIO_LCM_LED_EN
	mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ZERO);
#endif
	SET_RESET_PIN(1);	
	MDELAY(10);	
	SET_RESET_PIN(0);
MDELAY(10);	
//	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
SET_RESET_PIN(1);

MDELAY(120);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
MDELAY(5);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
MDELAY(50);
}



static void lcm_resume(void)
{

	lcm_init();
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);	
}

static void lcm_setbacklight(unsigned int level)
{
#ifdef BUILD_LK
	dprintf(0,"%s,lk hx8394d tm backlight: level = %d\n", __func__, level);
#else
	printk("%s, kernel hx8394d tm backlight: level = %d\n", __func__, level);
//Lenovo-sw wuwl10 add 20150113 for esd recover backlight
	esd_last_backlight_level = level;
#endif
//Lenovo-sw wuwl10 add 20150109 for min backlight control
	if((0 < level) && (level < 5))
	{
		level = 5;
	}
	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = level;
	
	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);

}


//Lenovo-sw wuwl10 add 20150113 for esd recover backlight begin
#ifndef BUILD_LK
static void lcm_esd_recover_backlight(void)
{
	printk("%s, kernel hx8394d tm recover backlight: level = %d\n", __func__, esd_last_backlight_level);

	// recovder last backlight level.
	lcm_backlight_level_setting[0].para_list[0] = esd_last_backlight_level;
	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}
#endif
//Lenovo-sw wuwl10 add 20150113 for esd recover backlight end

#ifdef CONFIG_LENOVO_CUSTOM_LCM_FEATURE
static void lcm_set_cabcmode(unsigned int mode)
{
#ifdef BUILD_LK
	dprintf(0,"%s mode = %d\n", __func__, mode);
#else
	printk("%s mode = %d\n", __func__, mode);
#endif
	// Refresh value of backlight level.
	lcm_cabc_level_setting[0].para_list[0] = mode;
	
	push_table(lcm_cabc_level_setting, sizeof(lcm_cabc_level_setting) / sizeof(struct LCM_setting_table), 1);

}
static void lcm_set_inversemode(unsigned int mode)
{
#ifdef BUILD_LK
	dprintf(0,"%s mode = %d\n", __func__, mode);
#else
	printk("%s mode = %d\n", __func__, mode);
#endif
	// Refresh value of backlight level.
if(mode)	
	push_table(lcm_inverse_on_setting, sizeof(lcm_inverse_on_setting) / sizeof(struct LCM_setting_table), 1);
else
	push_table(lcm_inverse_off_setting, sizeof(lcm_inverse_off_setting) / sizeof(struct LCM_setting_table), 1);
}
#endif
LCM_DRIVER hx8394d_hd720_dsi_vdo_tm_lcm_drv = 
{
    .name			= "hx8394d_dsi_vdo_tm",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.set_backlight	= lcm_setbacklight,
//Lenovo-sw wuwl10 add 20150113 for esd recover backlight begin
#ifndef BUILD_LK
	//.esd_recover_backlight = lcm_esd_recover_backlight,^
#endif
//Lenovo-sw wuwl10 add 20150113 for esd recover backlight end
#ifdef CONFIG_LENOVO_CUSTOM_LCM_FEATURE
	.set_cabcmode = lcm_set_cabcmode,
	.set_inversemode = lcm_set_inversemode,
#endif
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};
