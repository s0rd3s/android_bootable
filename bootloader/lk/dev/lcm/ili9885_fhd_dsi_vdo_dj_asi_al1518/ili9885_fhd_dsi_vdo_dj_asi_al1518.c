/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#endif

#include <cust_gpio_usage.h>

#ifndef CONFIG_FPGA_EARLY_PORTING
#include <cust_i2c.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif


static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))



#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

/*****************************************************************************
 * Define
 *****************************************************************************/
#endif
#ifndef BUILD_LK
//extern int tps65132_write_bytes(unsigned char addr, unsigned char value);
#else
//extern int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value);
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(1080)
#define FRAME_HEIGHT									(1920)

#ifndef CONFIG_FPGA_EARLY_PORTING
#define GPIO_65132_ENP_EN GPIO_LCD_BIAS_ENN_PIN 
#define GPIO_65132_ENN_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[128];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	///{0x4F, 1, {0x01} },
	///{REGFLAG_DELAY, 120, {} }
};
static struct LCM_setting_table init_setting[] = {
          
		  
		  { 0xB0,3,{ 0x98,0x85,0x0A }}, //access command 2 register
		  
		  { 0xC4,7,{ 0x70,0x19,0x23,0x00,0x0F,0x0F,0x00 }}, 
		   
		  { 0xD0,15,{ 0x33,0x05,0x21,0xE7,0x62,0x00,0x88,0xA0,0xA0,0x03,0x02,0x80,0xD0,0x1B,0x10 }},   //VGH dual mode,0x VGL single mode,0x VGH=12V,0x VGL=-12V
		  
		  { 0xD2,4,{ 0x03,0x03,0x2A,0x22 }},
		  { 0xD3,11,{ 0x44,0x33,0x05,0x03,0x4A,0x4A,0x33,0x17,0x22,0x43,0x6E }}, //set GVDDP=4.1V,0x GVDDN=-4.1V,0x VGHO=12V,0x VGLO=-11V
		//  { 0xD5,14,{ 0x00,0x00,0x00,0x00,0x01,0x61,0x01,0x7D,0x01,0x7D,0x00,0x00,0x00,0x00 }}, //set VCOM
		   {0xD6, 12,{0x00,0x00,0x08,0x17,0x23,0x65,0x77,0x44,0x87,0x00,0x00,0x09}},  //P12_D[3] for sleep in current reduce setting
		  { 0xEC,7,{ 0x76,0x1E,0x32,0x00,0x46,0x00,0x02 }}, 
		  { 0xEF,1,{ 0x8F }}, 
		  
		  //gamma setting
		  
		  
		  
		  { 0xC7,122,{ 0x00,0x0F,0x00,0x1E,0x00,0x38,0x00,0x4E,0x00,0x61,0x00,0x73,0x00,0x82,0x00,0x91,0x00,0x9E,0x00,0xCC,0x00,0xF2,0x01,0x2F,0x01,0x5F,0x01,0xAB,0x01,0xE9,0x01,0xEB,0x02,0x23,0x02,0x60,0x02,0x88,0x02,0xC1,0x02,0xE8,0x03,0x19,0x03,0x27,0x03,0x37,0x03,0x48,0x03,0x59,0x03,0x6A,0x03,0x75,0x03,0x7C,0x03,0x87,0x00,0x0F,0x00,0x1E,0x00,0x38,0x00,0x4E,0x00,0x61,0x00,0x73,0x00,0x82,0x00,0x91,0x00,0x9E,0x00,0xCC,0x00,0xF2,0x01,0x2F,0x01,0x5F,0x01,0xAB,0x01,0xE9,0x01,0xEB,0x02,0x23,0x02,0x60,0x02,0x88,0x02,0xC1,0x02,0xE8,0x03,0x19,0x03,0x27,0x03,0x37,0x03,0x48,0x03,0x59,0x03,0x6A,0x03,0x75,0x03,0x7C,0x03,0x87,0x01,0x01 }},
		  
		  
		  //GIP setting
		  { 0xE5,73,{ 0x6D,0x92,0x80,0x34,0x76,0x12,0x12,0x00,0x00,0x36,0x36,0x24,0x24,0x26,0xF6,0xF6,0xF6,0xF6,0xF6,0xF6,0x6D,0x9B,0x89,0x34,0x76,0x1B,0x1B,0x09,0x09,0x3F,0x3F,0x2D,0x2D,0x26,0xF6,0xF6,0xF6,0xF6,0xF6,0xF6,0x04,0x40,0x90,0x00,0xD6,0x00,0x00,0x00,0x00,0x01,0x01,0x44,0x00,0xD6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x07 }},
		  { 0xEA,66,{ 0x70,0x00,0x03,0x03,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x89,0x8a,0x05,0x05,0x22,0x0a,0x0a,0x0b,0x00,0x08,0x00,0x30,0x01,0x09,0x00,0x40,0x80,0xc0,0x00,0x00,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xDD,0x22,0x22,0xCC,0xDD,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCC,0x33,0x33,0x33,0x33,0x33,0x33 }},
		  { 0xEE,2,{0x22,0x10 }},
		  
		  //set DCHG=OFF=1 & LVD GIP behavior
		  { 0xEB,32,{ 0xa3,0xcf,0x73,0x18,0x54,0x55,0x55,0x55,0x55,0x00,0x55,0x55,0x55,0x55,0x55,0x25,0x0D,0x0F,0x00,0x00,0x00,0x00,0x00,0x55,0x55,0x55,0x55,0x32,0x3E,0x55,0x43,0x55 }},  
		 // { 0xD7,5,{ 0x00,0x00,0x00,0x81,0x07}},
		 //{ 0xD7,1,{ 0x07}},  
		  {0xB0,  1,{0x11}},
	  {REGFLAG_DELAY, 10, {} },
		  { 0x11, 0, {} },

		  {REGFLAG_DELAY, 120, {} },
		  {0x29, 0, {} },
		  {REGFLAG_DELAY, 20, {} },

		  { 0xB0,3,{ 0x98,0x85,0x0A }}, //access command 2 register
		  //{ 0xD7,5,{ 0x00,0x00,0x00,0x81,0x07}}, 
		  { 0xD7,1,{ 0x07}}, 	//for A
		  {0xB0,  1,{0x11}},

		  
		  

		   
/*
	{0xB0,  3,{0x98,0x85,0x0A}}, //access command 2 register
	
	{0xC4,  7,{0x70,0x19,0x23,0x00,0x0F,0x0F,0x00 }},
	{0xD0, 15,{0x45,0x05,0x32,0xD7,0x62,0xC0,0x88,0xA0,0xA0,0x03,0x02,0x84,0xD0,0x1B,0x10}},  //VGH dual mode, VGL dual mode, VGH=14V, VGL=-12V
	{0xD2,  2,{0x03,0x03}}, 
	{0xD3, 11,{0x44,0x33,0x05,0x03,0x4F,0x4F,0x33,0x17,0x1D,0x43,0x6E}}, //set GVDDP=4.1V, GVDDN=-4.1V, VGHO=12V, VGLO=-11V
	{0xD5, 14,{0x8B,0x00,0x00,0x00,0x01,0x7D,0x01,0x7D,0x01,0x7D,0x00,0x00,0x00,0x00}}, //set VCOM
	{0xEC,  7,{0x76,0x1E,0x32,0x00,0x46,0x00,0x02}}, //black display while video stop
	{0xEF,  1,{0x8F}}, //power saving
									
	{0xC7,122,{0x00,0x00,0x00,0x12,0x00,0x35,0x00,0x53,0x00,0x69,0x00,0x7C,0x00,0x8D,0x00,
	           0x9D,0x00,0xAB,0x00,0xDB,0x01,0x02,0x01,0x3E,0x01,0x6E,0x01,0xBC,0x01,0xFA,
	           0x01,0xFC,0x02,0x37,0x02,0x75,0x02,0x9B,0x02,0xD0,0x02,0xF4,0x03,0x27,0x03,
	           0x37,0x03,0x49,0x03,0x5D,0x03,0x74,0x03,0x90,0x03,0xB1,0x03,0xD4,0x03,0xE6,
	           0x00,0x00,0x00,0x12,0x00,0x35,0x00,0x53,0x00,0x69,0x00,0x7C,0x00,0x8D,0x00,
	           0x9D,0x00,0xAB,0x00,0xDB,0x01,0x02,0x01,0x3E,0x01,0x6E,0x01,0xBC,0x01,0xFA,
	           0x01,0xFC,0x02,0x37,0x02,0x75,0x02,0x9B,0x02,0xD0,0x02,0xF4,0x03,0x27,0x03,
	           0x37,0x03,0x49,0x03,0x5D,0x03,0x74,0x03,0x90,0x03,0xB1,0x03,0xD4,0x03,0xE6,
	           0x01,0x01}},
	
	{0xE5, 73,{0x6D,0x92,0x80,0x34,0x76,0x12,0x12,0x00,0x00,0x36,0x36,0x24,0x24,0x26,0xF6,
	           0xF6,0xF6,0xF6,0xF6,0xF6,0x6D,0x9B,0x89,0x34,0x76,0x1B,0x1B,0x09,0x09,0x3F,
	           0x3F,0x2D,0x2D,0x26,0xF6,0xF6,0xF6,0xF6,0xF6,0xF6,0x04,0x40,0x90,0x00,0x68,
	           0x00,0x00,0x00,0x00,0x01,0x01,0x44,0x00,0x68,0x00,0x00,0x00,0x00,0x00,0x00,
	           0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x07}},
	
	{0xEE,  2,{0x22,0x10}},
	
	{0xEA, 66,{0x70,0x00,0x03,0x03,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,
	           0x00,0x00,0x00,0x89,0x8A,0x05,0x05,0x22,0x0a,0x0a,0x0b,0x00,0x08,0x00,0x30,
	           0x01,0x09,0x00,0x40,0x80,0xc0,0x00,0x00,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,
	           0xef,0xDD,0x22,0x22,0xCC,0xDD,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCC,
	           0x33,0x33,0x33,0x33,0x33,0x33}},
	
	{0xEB, 32,{0xa3,0xcf,0x73,0x18,0x54,0x55,0x55,0x55,0x55,0x00,0x55,0x55,0x55,0x55,0x55,
	           0x25,0x0D,0x0F,0x00,0x00,0x00,0x00,0x00,0x55,0x55,0x55,0x55,0x32,0x3E,0x55,
	           0x43,0x55}},
	
	//only for test purpose
	{0xDF, 11,{0x00,0x00,0x00,0x06,0x01,0x10,0x00,0x00,0x00,0x00,0xFE}}, //to measure GVDDP & GVDDN
	
	
	{REGFLAG_DELAY, 10, {} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} },

*/

};;

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
	{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type = LCM_TYPE_DSI;
	
		params->width = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;
	
#if (LCM_DSI_CMD_MODE)
		params->dsi.mode = CMD_MODE;
		params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
		params->dsi.mode = SYNC_PULSE_VDO_MODE;
		params->dsi.switch_mode = CMD_MODE;
#endif
		params->dsi.switch_mode_enable = 0;
	
		/* DSI */
		/* Command mode setting */
		params->dsi.LANE_NUM = LCM_FOUR_LANE;
		/* The following defined the fomat for data coming from LCD engine. */
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	
		/* Highly depends on LCD driver capability. */
		params->dsi.packet_size = 256;
		/* video mode timing */
	
		params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	
		params->dsi.vertical_sync_active = 4;
		params->dsi.vertical_backporch = 32;
		params->dsi.vertical_frontporch = 32;
		params->dsi.vertical_active_line = FRAME_HEIGHT;
	
		params->dsi.horizontal_sync_active = 10;
		params->dsi.horizontal_backporch = 80;
		params->dsi.horizontal_frontporch = 80;
		params->dsi.horizontal_active_pixel = FRAME_WIDTH;
		/* params->dsi.ssc_disable														= 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
		params->dsi.PLL_CLOCK = 500;	/* this value must be in MTK suggested table */
#else
		params->dsi.PLL_CLOCK = 485;//450	/* this value must be in MTK suggested table */
#endif
#else
		params->dsi.pll_div1 = 0;
		params->dsi.pll_div2 = 0;
		params->dsi.fbk_div = 0x1;
#endif
	
		//params->dsi.cont_clock = 0;
		params->dsi.clk_lp_per_line_enable = 0;
		//params->dsi.noncont_clock = TRUE;
		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
		params->dsi.lcm_esd_check_table[0].count = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
	
	//params->dsi.lcm_esd_check_table[1].cmd			= 0x0D;
	//params->dsi.lcm_esd_check_table[1].count		= 1;
	///params->dsi.lcm_esd_check_table[1].para_list[0] = 0x00;
	
		params->dsi.lcm_esd_check_table[1].cmd			= 0xD2;
		params->dsi.lcm_esd_check_table[1].count		= 1;
		params->dsi.lcm_esd_check_table[1].para_list[0] = 0x03;

		params->dsi.lcm_esd_check_table[2].cmd			= 0x0D;
	    params->dsi.lcm_esd_check_table[2].count		= 1;
	    params->dsi.lcm_esd_check_table[2].para_list[0] = 0x00;

		//	  params->dsi.lcm_esd_check_table[2].para_list[1] = 0x8e;

	
	
	
	}




static void lcm_init(void)
	{
		unsigned char cmd = 0x0;
		unsigned char data = 0xFF;
		int ret = 0;
		cmd = 0x00;
		data = 0x0D;
#ifndef CONFIG_FPGA_EARLY_PORTING
		mt_set_gpio_mode(GPIO_65132_ENP_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_65132_ENP_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_65132_ENP_EN, GPIO_OUT_ONE);
		MDELAY(1);
		mt_set_gpio_mode(GPIO_65132_ENN_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_65132_ENN_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_65132_ENN_EN, GPIO_OUT_ONE);
#ifdef BUILD_LK
//		ret = TPS65132_write_byte(cmd, data);
#else
//		ret = tps65132_write_bytes(cmd, data);
#endif
	
		if (ret < 0)
			LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write error----\n", cmd);
		else
			LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write success----\n", cmd);
	
		cmd = 0x01;
		data = 0x0D;
	
#ifdef BUILD_LK
//		ret = TPS65132_write_byte(cmd, data);
#else
//		ret = tps65132_write_bytes(cmd, data);
#endif
	
		if (ret < 0)
			LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write error----\n", cmd);
		else
			LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write success----\n", cmd);
	
#endif
	
	  unsigned int data_array[32];
	  MDELAY(2);
		SET_RESET_PIN(1);
		MDELAY(1);
		SET_RESET_PIN(0);
		MDELAY(10);
	
		SET_RESET_PIN(1);
		MDELAY(120);
	
		push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	
		//push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
		 //BOE055

	
	
	}


static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	
#ifndef CONFIG_FPGA_EARLY_PORTING
	mt_set_gpio_mode(GPIO_65132_ENN_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENN_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENN_EN, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_mode(GPIO_65132_ENP_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENP_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENP_EN, GPIO_OUT_ZERO);
#endif


	/* SET_RESET_PIN(0); */
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);

static unsigned int lcm_compare_id(void)
	{
	unsigned int id = 0;
	unsigned char buffer[2];
	unsigned int array[16];
    int data[4] = {0,0,0,0};
    int res = 0;

    int rawdata = 0;
    int lcm_vol = 0;


	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(50);
	res = IMM_GetOneChannelValue(13,data,&rawdata);
	lcm_vol = data[0]*1000+data[1]*10;
	LCM_LOGI("[adc_uboot]: lcm_vol= %d\n",lcm_vol);


	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	LCM_LOGI("%s,ili9885 debug: HX8399 id = 0x%08x\n", __func__, id);

	read_reg_v2(0xBF, buffer, 2);
	id = buffer[0];		/* we only need ID */
	if (lcm_vol>500&&id == 0x98)
		return 1;
	else
		return 0;

}



/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	} else {
		LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
		return FALSE;
	}
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35595 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_setbacklight(unsigned int level)
{
	LCM_LOGI("%s,nt35595 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


LCM_DRIVER ili9885a_fhd_dsi_vdo_dj_asi_al1518_lcm_drv = {
	.name = "ili9885a_fhd_dsi_vdo_dj_asi_al1518",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	//.init_power = lcm_init_power,
	//.resume_power = lcm_resume_power,
	//.suspend_power = lcm_suspend_power,
	//.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
};

