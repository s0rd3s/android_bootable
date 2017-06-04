// Dummy AP
#include <platform/boot_mode.h>
#include <debug.h>
#include <dev/uart.h>
#include <platform/mtk_key.h>
#include <target/cust_key.h>
#include <platform/mt_gpio.h>
#include <sys/types.h>
#include <debug.h>
#include <err.h>
#include <reg.h>
#include <string.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_irq.h>
#include <platform/mt_pmic.h>
#include <platform/timer.h>
#include <sys/types.h>
#include <arch/ops.h>
#include <platform/mt_pmic.h> 
#include <platform/upmu_common.h> 
#include <platform/upmu_hw.h>
#include <platform/pmic_wrap_init.h>

//=====================================================================
// Feature option switch part
//=====================================================================
#define THIS_IS_EVB
#define BOTH_MD_ON
//#define CCIF_DVT
//#define DEFAULT_META
//#define ENABLE_MD_RESET_SPM
//#define ENABLE_MD_RESET_RGU
#define IGNORE_MD_WDT
//#define IGNORE_MD1_WDT
//#define IGNORE_MD2_WDT
//#define ALWAYS_META
//#define DVT_512M_SUPPORT
#define AP_MD_SAME_UART
#define ALWAYS_USING_42000000_AS_MD1_MD3_SHARE

#ifdef ENABLE_MD_RESET_SPM
#include <platform/spm.h>
#endif

#define MAX_MD_NUM			(2)
#define MAX_IMG_NUM			(8)
#define PART_HEADER_MAGIC	(0x58881688)


//------- IRQ ID part ---------------------------------------
#define GIC_PRIVATE_SIGNALS	(32)
#define MT_MD_WDT1_IRQ_ID	(223+GIC_PRIVATE_SIGNALS)
#define MT_MD_WDT2_IRQ_ID	(220+GIC_PRIVATE_SIGNALS)


//-------- Register base part -------------------------------
// HW remap for MD1
#define MD1_BANK0_MAP0 (0x10001300)
#define MD1_BANK0_MAP1 (0x10001304)
#define MD1_BANK1_MAP0 (0x10001308)
#define MD1_BANK1_MAP1 (0x1000130C)
#define MD1_BANK4_MAP0 (0x10001310)
#define MD1_BANK4_MAP1 (0x10001314)

// HW remap for MD3(C2K)
#define MD2_BANK0_MAP0 (0x10001320)
#define MD2_BANK0_MAP1 (0x10001324)
#define MD2_BANK4_MAP0 (0x10001330)
#define MD2_BANK4_MAP1 (0x10001334)

// C2K_CONFIG (0x10001360)
#define  AP_C2K_CONFIG (0x10001360)

// AP RGU
#define TOP_RGU_WDT_MODE (TOPRGU_BASE+0x0)
#define TOP_RGU_WDT_SWRST (TOPRGU_BASE+0x14)
#define TOP_RGU_WDT_SWSYSRST (TOPRGU_BASE+0x18)
#define TOP_RGU_WDT_NONRST_REG (TOPRGU_BASE+0x20)

// MD RGU PCore
#define MD_WDT_BASE (0x200F0000)  // P Core 0x55000000
#define MD_WDT_STA  (MD_WDT_BASE+0x0C)

// MD RGU L1 Core
#define MD_L1WDT_BASE (0x26010000) //2200//L Core

#define PLL_TYPE	(volatile unsigned int *)

#define MD_CLKSW_BASE			(0x20150000)
#define MD_GLOBAL_CON_DCM_BASE		(0x20130000)
#define PSMCU_MISC_BASE			(0x20200000)
#define MD_PERI_MISC_BASE		(0x20060000)
#define MDL1A0_BASE			(0x260F0000)
#define BASE_ADDR_MDTOP_PLLMIXED	(0x20140000)
#define BASE_ADDR_MDSYS_CLKCTL		(0x20120000)
#define L1_BASE_MADDR_MDL1_CONF 	(0x260F0000)

#define R_CLKSEL_CTL			(PLL_TYPE(MD_CLKSW_BASE+0x0024))
#define R_FLEXCKGEN_SEL0		(PLL_TYPE(MD_CLKSW_BASE+0x0028))
#define R_FLEXCKGEN_SEL1		(PLL_TYPE(MD_CLKSW_BASE+0x002C))
#define R_FLEXCKGEN_SEL2		(PLL_TYPE(MD_CLKSW_BASE+0x0044))

#define R_PLL_STS			(PLL_TYPE(MD_CLKSW_BASE+0x0040))

#define R_FLEXCKGEN_STS0		(PLL_TYPE(MD_CLKSW_BASE+0x0030))
#define R_FLEXCKGEN_STS1		(PLL_TYPE(MD_CLKSW_BASE+0x0034))
#define R_FLEXCKGEN_STS2		(PLL_TYPE(MD_CLKSW_BASE+0x0048))

/*PSMCU DCM*/
#define R_PSMCU_DCM_CTL0		(PLL_TYPE(MD_GLOBAL_CON_DCM_BASE+0x0010))
#define R_PSMCU_DCM_CTL1		(PLL_TYPE(MD_GLOBAL_CON_DCM_BASE+0x0014))

#define R_ARM7_DCM_CTL0			(PLL_TYPE(MD_GLOBAL_CON_DCM_BASE+0x0020))
#define R_ARM7_DCM_CTL1			(PLL_TYPE(MD_GLOBAL_CON_DCM_BASE+0x0024))
#define MD_GLOBAL_CON_DUMMY		(PLL_TYPE(MD_GLOBAL_CON_DCM_BASE+0x1000))
#define MD_PLL_MAGIC_NUM 		(0x67550000)

#define R_DCM_SHR_SET_CTL		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0004))
#define R_LTEL2_BUS_DCM_CTL		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0010))
#define R_MDDMA_BUS_DCM_CTL		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0014))
#define R_MDREG_BUS_DCM_CTL		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0018))
#define R_MODULE_BUS2X_DCM_CTL		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x001C))
#define R_MODULE_BUS1X_DCM_CTL		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0020))
#define R_MDINFRA_CKEN			(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0044))		
#define R_MDPERI_CKEN			(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0048))
#define R_MDPERI_DCM_MASK		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x0064))

#define R_PSMCU_AO_CLK_CTL 		(PLL_TYPE(BASE_ADDR_MDSYS_CLKCTL+0x00C0))

#define R_L1_PMS			(PLL_TYPE(MD_PERI_MISC_BASE+0x00C4))


#define REG_DCM_PLLCK_SEL		(PLL_TYPE(MDL1A0_BASE+0x0188))
#define R_L1MCU_PWR_AWARE		(PLL_TYPE(MDL1A0_BASE+0x0190))
#define R_L1AO_PWR_AWARE		(PLL_TYPE(MDL1A0_BASE+0x0194))
#define R_BUSL2DCM_CON3			(PLL_TYPE(MDL1A0_BASE+0x0198))
#define R_L1MCU_DCM_CON2		(PLL_TYPE(MDL1A0_BASE+0x0184))
#define R_L1MCU_DCM_CON			(PLL_TYPE(MDL1A0_BASE+0x0180))

// C2K boot
#define UINT32P			(volatile unsigned int *)
#define SLEEP_BASE		(0x10006000)
#define APMIXED_BASE		(0x1000C000)
#define TOPRGU_BASE		(0x10007000)
#define INFRACFG_AO_BASE	(0x10001000)
#define C2KSYS_BASE		(0x38000000)
#define C2K_CGBR1                (UINT32P (C2KSYS_BASE+0x0200B004))
#define WDT_SWSYSRST             (UINT32P (TOPRGU_BASE+0x018))
#define INFRA_TOPAXI_PROTECTEN_1 (UINT32P (INFRACFG_AO_BASE+0x250))
#define C2K_SPM_CTRL             (UINT32P (INFRACFG_AO_BASE+0x368))
#define C2K_STATUS               (UINT32P (INFRACFG_AO_BASE+0x364))
#define POWERON_CONFIG_EN        (UINT32P (SLEEP_BASE+0x000))
#define C2K_PWR_CON              (UINT32P (SLEEP_BASE+0x328))
#define PWR_STATUS               (UINT32P (SLEEP_BASE+0x180))
#define PWR_STATUS_2ND           (UINT32P (SLEEP_BASE+0x184))
#define INFRA_MISC2		(UINT32P (INFRACFG_AO_BASE+0xF0C))
#define AP_PLL_CON0		(UINT32P (APMIXED_BASE+0x0))
#define PWR_RST_B     0
#define PWR_ISO       1
#define PWR_ON        2
#define PWR_ON_2ND    3
#define PWR_CLK_DIS   4
#define C2K          28

#define C2K_MAGIC_NUM 0xC275
#define INFRACFG_AO_BASE (0x10001000)
#define UINT16P      volatile unsigned short *
#define UINT32P      (volatile unsigned int *)
#define C2KSYS_BASE (0x38000000)
#define C2K_SBC_KEY0    (UINT32P(INFRACFG_AO_BASE+0x8B0))
#define C2K_SBC_KEY1    (UINT32P(INFRACFG_AO_BASE+0x8B4))
#define C2K_SBC_KEY2    (UINT32P(INFRACFG_AO_BASE+0x8B8))
#define C2K_SBC_KEY3    (UINT32P(INFRACFG_AO_BASE+0x8BC))
#define C2K_SBC_KEY4    (UINT32P(INFRACFG_AO_BASE+0x8C0))
#define C2K_SBC_KEY5    (UINT32P(INFRACFG_AO_BASE+0x8C4))
#define C2K_SBC_KEY6    (UINT32P(INFRACFG_AO_BASE+0x8C8))
#define C2K_SBC_KEY7    (UINT32P(INFRACFG_AO_BASE+0x8CC))
#define C2K_SBC_KEY_LOCK        (UINT32P(INFRACFG_AO_BASE+0x8D0))
#define C2K_CONFIG      (UINT32P(INFRACFG_AO_BASE+0x360))
#define C2K_C2K_PLL_CON3        (UINT32P(C2KSYS_BASE+0x02013008))
#define C2K_C2K_PLL_CON2        (UINT32P(C2KSYS_BASE+0x02013004))
#define C2K_C2K_PLLTD_CON0      (UINT32P(C2KSYS_BASE+0x02013074))
#define C2K_CLK_CTRL9   (UINT32P(C2KSYS_BASE+0x0200029C))
#define C2K_CLK_CTRL4   (UINT32P(C2KSYS_BASE+0x02000010))
#define C2K_CG_ARM_AMBA_CLKSEL  (UINT32P(C2KSYS_BASE+0x02000234))
#define C2K_C2K_C2KPLL1_CON0    (UINT32P(C2KSYS_BASE+0x02013018))
#define C2K_C2K_CPPLL_CON0      (UINT32P(C2KSYS_BASE+0x02013040))
#define C2K_C2K_DSPPLL_CON0     (UINT32P(C2KSYS_BASE+0x02013050))
#define C2K 28

#define MDPLL1_CON0              (UINT32P (APMIXED_BASE+0x02C8))

// SIM GPIO setcion
// -- SIM1
#define SIM1_SIO_GPIO_ID	(41)
#define SIM1_SRST_GPIO_ID	(40)
#define SIM1_SCLK_GPIO_ID	(39)
#define SIM1_HOT_PLUG_GPIO_ID	(46)
// -- SIM2
#define SIM2_SIO_GPIO_ID	(36)
#define SIM2_SRST_GPIO_ID	(37)
#define SIM2_SCLK_GPIO_ID	(38)
#define SIM2_HOT_PLUG_GPIO_ID	(45)
// -- Connection


typedef enum{
	DUMMY_AP_IMG = 0,
	MD1_IMG,
	MD1_RAM_DISK,
	MD2_IMG,
	MD2_RAM_DISK,
	MD_DSP,
	MAX_IMG_IDX,
}img_idx_t;

typedef struct _map
{
	char		name[32];
	img_idx_t	idx;
}map_t;


extern BOOT_ARGUMENT	*g_boot_arg;
static unsigned int img_load_flag = 0;
static part_hdr_t *img_info_start = NULL;
static unsigned int img_addr_tbl[MAX_IMG_NUM];
static unsigned int img_size_tbl[MAX_IMG_NUM];
static map_t map_tbl[] = 
{
	{"DUMMY_AP",		DUMMY_AP_IMG},
	{"MD_IMG",		MD1_IMG},
	{"MD_RAM_DISK",		MD1_RAM_DISK},
	{"MD_DSP",		MD_DSP},
	{"MD2_IMG",		MD2_IMG},
	{"MD2_RAM_DISK",	MD2_RAM_DISK},
};

struct md_check_header_v3 {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	  /* header structure version number */
	unsigned int  product_ver;	   /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;		/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];	  /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	 /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id;	   /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr;		  /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2];	   /* for reserved */

	unsigned int  mem_size;		  /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;	   /* md image size, exclude head size */
	unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	unsigned char reserved2[88];

	unsigned int  size;			  /* the size of this structure */
};

struct _md_regin_info {
	unsigned int region_offset;
	unsigned int region_size;
};

struct md_check_header_v5 {
	unsigned char check_header[12]; /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno; /* header structure version number */
	unsigned int  product_ver; /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type; /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16]; /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64]; /* build time string */
	unsigned char build_ver[64]; /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id; /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr; /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2]; /* for reserved */

	unsigned int  mem_size; /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size; /* md image size, exclude head size */
	unsigned int  rpc_sec_mem_addr; /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;

	unsigned int  region_num; /* total region number */
	struct _md_regin_info region_info[8]; /* max support 8 regions */
	unsigned int  domain_attr[4]; /* max support 4 domain settings, each region has 4 control bits*/

	unsigned int  arm7_img_offset;
	unsigned int  arm7_img_size;

	unsigned char reserved_1[56];


	unsigned int  size; /* the size of this structure */
};

extern int mt_set_gpio_mode_chip(unsigned int pin, unsigned int mode);
void pmic_init_sequence(void);
void md_uart_config(int md_id, int boot_mode);
void md1_pll_init(void);
void md1_pmic_setting(void);

int parse_img_header(unsigned int *start_addr, unsigned int img_num)
{
	unsigned int i, j;
	int idx;

	if(start_addr == NULL) {
		printf("parse_img_header get invalid parameters!\n");
		return -1;
	}
	img_info_start = (part_hdr_t*)start_addr;
	for(i=0; i<img_num; i++) {
		if(img_info_start[i].info.magic != PART_HEADER_MAGIC)
			continue;

		for(j=0; j<(sizeof(map_tbl)/sizeof(map_t)); j++) {
			if(strcmp(img_info_start[i].info.name, map_tbl[j].name) == 0) {
				idx = map_tbl[j].idx;
				img_addr_tbl[idx] = img_info_start[i].info.maddr;
				img_size_tbl[idx] = img_info_start[i].info.dsize;
				img_load_flag |= (1<<idx);
				printf("[%s] idx:%d, addr:0x%x, size:0x%x\n", map_tbl[j].name, idx, img_addr_tbl[idx], img_size_tbl[idx]);
			}
		}
	}
	return 0;
}

unsigned int * query_img_setting(int img_idx, unsigned int *size)
{
	unsigned int i, j;
	int idx;

	if(img_info_start == NULL) {
		printf("parse_img_header not init yet!\n");
		return NULL;
	}

 	if(img_load_flag & (1<<img_idx)) {
 		if(size)
			*size = img_size_tbl[img_idx];
		return img_addr_tbl[img_idx];
 	}

	return NULL;
}

int mem_cpy_check(void* des, void *src, unsigned int  sz) // sz must multi-4
{
	unsigned int offset = 0;
	unsigned int reg_val_des;
	unsigned int reg_val_src;
	unsigned int i;
	
	if( (des==NULL) || (src==NULL))
		return 0;
	for(i=0; i<(sz>>2);i++) {
		reg_val_src = DRV_Reg32((unsigned int)src+offset);
		DRV_WriteReg32((unsigned int)des+offset, reg_val_src);
		dsb();
		reg_val_des = DRV_Reg32((unsigned int)des+offset);
		if(reg_val_src != reg_val_des)
			printf("data-miss-match:d:%x<>s:%x\n", reg_val_des, reg_val_src);
		offset+=4;
	}

	offset = 0;
	for(i=0; i<(sz>>2);i++) {
		reg_val_des = DRV_Reg32((unsigned int)des+offset);
		reg_val_src = DRV_Reg32((unsigned int)src+offset);
		if(reg_val_des != reg_val_src ) {
			printf("Read back check fail[%x] d:%x <-> [%x] s:%x\n",  (unsigned int)des+offset, reg_val_des, (unsigned int)src+offset, reg_val_src);
			return 0;
		}	
		offset+=4;
	}
	printf("mem_cpy_check success\n");
	return sz;
}


static int meta_detection(void)
{
	int boot_mode = 0;
	
	if(g_boot_arg->boot_mode != NORMAL_BOOT)
		boot_mode = 1;
	printf("Meta mode: %d, boot_mode: %d\n", boot_mode, g_boot_arg->boot_mode);
	return boot_mode;
}

static void md_gpio_get(GPIO_PIN pin, char *tag)
{
	printf("GPIO(%X)(%s): mode=%d,dir=%d,in=%d,out=%d,pull_en=%d,pull_sel=%d,smt=%d\n",
			pin, tag,
			mt_get_gpio_mode(pin),
			mt_get_gpio_dir(pin),
			mt_get_gpio_in(pin),
			mt_get_gpio_out(pin),
			mt_get_gpio_pull_enable(pin),
			mt_get_gpio_pull_select(pin),
			mt_get_gpio_smt(pin));
}

static void md_gpio_set(GPIO_PIN pin, GPIO_MODE mode, GPIO_DIR dir, GPIO_OUT out, GPIO_PULL_EN pull_en, GPIO_PULL pull, GPIO_SMT smt)
{
	mt_set_gpio_mode(pin, mode);
	if(dir != GPIO_DIR_UNSUPPORTED)
		mt_set_gpio_dir(pin, dir);

	if(dir == GPIO_DIR_OUT) {
		mt_set_gpio_out(pin, out);
	}
	if(dir == GPIO_DIR_IN) {
		mt_set_gpio_smt(pin, smt);
	}
	if(pull_en != GPIO_PULL_EN_UNSUPPORTED) {
		mt_set_gpio_pull_enable(pin, pull_en);
		mt_set_gpio_pull_select(pin, pull);
	}
	md_gpio_get(pin, "-");
}

static void md_gpio_config(unsigned int boot_mode_case)
{
	// init sim1
	mt_set_gpio_dir(SIM1_SCLK_GPIO_ID, GPIO_DIR_OUT);
	mt_set_gpio_dir(SIM1_SRST_GPIO_ID, GPIO_DIR_OUT);	
	mt_set_gpio_pull_enable(SIM1_SIO_GPIO_ID, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(SIM1_SIO_GPIO_ID, GPIO_PULL_UP);
	mt_set_gpio_dir(SIM1_SIO_GPIO_ID, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(SIM1_HOT_PLUG_GPIO_ID, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(SIM1_HOT_PLUG_GPIO_ID, GPIO_PULL_UP);
	mt_set_gpio_dir(SIM1_HOT_PLUG_GPIO_ID, GPIO_DIR_IN);
	// init sim2
	mt_set_gpio_dir(SIM2_SCLK_GPIO_ID, GPIO_DIR_OUT);
	mt_set_gpio_dir(SIM2_SRST_GPIO_ID, GPIO_DIR_OUT);	 
	mt_set_gpio_pull_enable(SIM2_SIO_GPIO_ID, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(SIM2_SIO_GPIO_ID, GPIO_PULL_UP);
	mt_set_gpio_dir(SIM2_SIO_GPIO_ID, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(SIM2_HOT_PLUG_GPIO_ID, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(SIM2_HOT_PLUG_GPIO_ID, GPIO_PULL_UP);
	mt_set_gpio_dir(SIM2_HOT_PLUG_GPIO_ID, GPIO_DIR_IN);

	//	md_gpio_set(GPIO96, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO39, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO40, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO41, GPIO_MODE_05, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO38, GPIO_MODE_06, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO37, GPIO_MODE_06, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO36, GPIO_MODE_06, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO46, GPIO_MODE_01, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO45, GPIO_MODE_01, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);

	md_gpio_set(GPIO13, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO14, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO15, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO16, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);

	md_gpio_set(GPIO27, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO28, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO34, GPIO_MODE_06, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO30, GPIO_MODE_06, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO33, GPIO_MODE_06, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO32, GPIO_MODE_06, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO35, GPIO_MODE_06, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO31, GPIO_MODE_06, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO110, GPIO_MODE_05, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO108, GPIO_MODE_05, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO111, GPIO_MODE_05, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO107, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO8, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO18, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO17, GPIO_MODE_05, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO25, GPIO_MODE_05, GPIO_DIR_OUT, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO26, GPIO_MODE_05, GPIO_DIR_IN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_UP, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO116, GPIO_MODE_00, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_UNSUPPORTED);

	md_gpio_set(GPIO82, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO81, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO80, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO79, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO62, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO61, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO60, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO59, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO58, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO57, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO56, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO55, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO54, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO53, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO52, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO51, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO90, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO89, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO88, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO87, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO86, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO85, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO84, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO83, GPIO_MODE_02, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO159, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO160, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO69, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO68, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO67, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO66, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO65, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO64, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO63, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO78, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO77, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO76, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
	md_gpio_set(GPIO75, GPIO_MODE_01, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);

//	md_gpio_set(GPIO51, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO52, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO53, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO54, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);

//	md_gpio_set(GPIO107, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO108, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO161, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO109, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO110, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO111, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO42, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO43, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO157, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO44, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO158, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);

//	md_gpio_set(GPIO17, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO18, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO19, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO20, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO21, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO22, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO23, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO24, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);
//	md_gpio_set(GPIO25, GPIO_MODE_07, GPIO_DIR_OUT, GPIO_OUT_ZERO, GPIO_PULL_EN_UNSUPPORTED, GPIO_PULL_UNSUPPORTED, GPIO_SMT_UNSUPPORTED);

	switch(boot_mode_case) {
	case 0: 
		//SIM1=> MD1 SIM1IF		   
		mt_set_gpio_mode(SIM1_SCLK_GPIO_ID, 1);
		mt_set_gpio_mode(SIM1_SRST_GPIO_ID, 1);
		mt_set_gpio_mode(SIM1_SIO_GPIO_ID, 1);
		mt_set_gpio_mode(SIM1_HOT_PLUG_GPIO_ID, 1);
		//SIM2=> MD1 SIM2IF
		mt_set_gpio_mode(SIM2_SCLK_GPIO_ID, 1);
		mt_set_gpio_mode(SIM2_SRST_GPIO_ID, 1);
		mt_set_gpio_mode(SIM2_SIO_GPIO_ID, 1);
		mt_set_gpio_mode(SIM2_HOT_PLUG_GPIO_ID, 1);
		break;
	case 1:
		//SIM1=> MD2 UIM0IF
		mt_set_gpio_mode(SIM1_SCLK_GPIO_ID, 5);
		mt_set_gpio_mode(SIM1_SRST_GPIO_ID, 5);
		mt_set_gpio_mode(SIM1_SIO_GPIO_ID, 5);
		mt_set_gpio_mode(SIM1_HOT_PLUG_GPIO_ID, 1);
		//SIM2=> MD2 UIM1IF
		mt_set_gpio_mode(SIM2_SCLK_GPIO_ID, 6);
		mt_set_gpio_mode(SIM2_SRST_GPIO_ID, 6);
		mt_set_gpio_mode(SIM2_SIO_GPIO_ID, 6);
		mt_set_gpio_mode(SIM2_HOT_PLUG_GPIO_ID, 1);
		break;
	case 2:
		//SIM1=> MD1 SIM1IF
		mt_set_gpio_mode(SIM1_SCLK_GPIO_ID, 1);
		mt_set_gpio_mode(SIM1_SRST_GPIO_ID, 1);
		mt_set_gpio_mode(SIM1_SIO_GPIO_ID, 1);
		mt_set_gpio_mode(SIM1_HOT_PLUG_GPIO_ID, 1);
		//SIM2=> MD2 UIM0IF
		mt_set_gpio_mode(SIM2_SCLK_GPIO_ID, 5);
		mt_set_gpio_mode(SIM2_SRST_GPIO_ID, 5);
		mt_set_gpio_mode(SIM2_SIO_GPIO_ID, 5);
		mt_set_gpio_mode(SIM2_HOT_PLUG_GPIO_ID, 1);
		break;
	default:
		break;
	}
	
	md_gpio_get(SIM1_SCLK_GPIO_ID, "sclk");
	md_gpio_get(SIM1_SRST_GPIO_ID, "srst");
	md_gpio_get(SIM1_SIO_GPIO_ID, "sio");
	md_gpio_get(SIM1_HOT_PLUG_GPIO_ID, "hp");
	md_gpio_get(SIM2_SCLK_GPIO_ID, "sclk2");
	md_gpio_get(SIM2_SRST_GPIO_ID, "srst2");
	md_gpio_get(SIM2_SIO_GPIO_ID, "sio2");
	md_gpio_get(SIM2_HOT_PLUG_GPIO_ID, "hp2");


}

static unsigned int md1_md3_share_memory_addr;
static void md_emi_remapping(unsigned int boot_md_id)
{
	unsigned int md_img_start_addr = 0;
	unsigned int md_emi_remapping_addr_bank0 = 0;
	unsigned int md_emi_remapping_addr_bank4 = 0;
	int i;

	// Using MD1 last 32MB for MD1 and MD3 share memory
	

	switch(boot_md_id) {
	case 0: // MD1
		md_img_start_addr = img_addr_tbl[MD1_IMG] - 0x40000000;
		md_emi_remapping_addr_bank0 = MD1_BANK0_MAP0;
		md_emi_remapping_addr_bank4 = MD1_BANK4_MAP0;
		#ifdef ALWAYS_USING_42000000_AS_MD1_MD3_SHARE
		md1_md3_share_memory_addr = 0x42000000 - 0x40000000;
		memset((void*)md1_md3_share_memory_addr, 0, 0x200000);
		#else
		md1_md3_share_memory_addr = md_img_start_addr+0x0E000000;
		#endif
		break;
	case 1: // MD2
		md_img_start_addr = img_addr_tbl[MD2_IMG] - 0x40000000;
		md_emi_remapping_addr_bank0 = MD2_BANK0_MAP0;
		md_emi_remapping_addr_bank4 = MD2_BANK4_MAP0;
		break;
	default:
		break;
	}

	printf("---> Map 0x00000000 to 0x%x for MD%d\n", md_img_start_addr+0x40000000, boot_md_id+1);
	
	// For MDx_BANK0_MAP0 
	*((volatile unsigned int*)md_emi_remapping_addr_bank0) = (((md_img_start_addr >> 24) | 1) & 0xFF) \
	+ ((((md_img_start_addr + 0x02000000) >> 16) | 1<<8) & 0xFF00) \
	+ ((((md_img_start_addr + 0x04000000) >> 8) | 1<<16) & 0xFF0000) \
	+ ((((md_img_start_addr + 0x06000000) >> 0) | 1<<24) & 0xFF000000);
	
	// For MDx_BANK0_MAP1
	*((volatile unsigned int*)(md_emi_remapping_addr_bank0 + 0x4)) = ((((md_img_start_addr + 0x08000000) >> 24) | 1) & 0xFF) \
	+ ((((md_img_start_addr + 0x0A000000) >> 16) | 1<<8) & 0xFF00) \
	+ ((((md_img_start_addr + 0x0C000000) >> 8) | 1<<16) & 0xFF0000) \
	+ ((((md_img_start_addr + 0x0E000000) >> 0) | 1<<24) & 0xFF000000);
	
	printf("---> MD_BANK0_MAP0=0x%x, MD_BANK0_MAP1=0x%x\n",
		*((volatile unsigned int*)md_emi_remapping_addr_bank0),
		*((volatile unsigned int*)(md_emi_remapping_addr_bank0 + 0x4)));

	#ifdef DVT_512M_SUPPORT
	if (boot_md_id==0) {
		/* MD1 bank1 map0 */ 
		*((volatile unsigned int*)MD1_BANK1_MAP0) = ((((md_img_start_addr + 0x10000000) >> 24) | 1) & 0xFF) \
			+ ((((md_img_start_addr + 0x12000000) >> 16) | 1<<8) & 0xFF00) \
			+ ((((md_img_start_addr + 0x14000000) >> 8) | 1<<16) & 0xFF0000) \
			+ ((((md_img_start_addr + 0x16000000) >> 0) | 1<<24) & 0xFF000000);
		/* MD1 bank1 map1 */
		*((volatile unsigned int*)MD1_BANK1_MAP1) = ((((md_img_start_addr + 0x18000000) >> 24) | 1) & 0xFF) \
			+ ((((md_img_start_addr + 0x1A000000) >> 16) | 1<<8) & 0xFF00) \
			+ ((((md_img_start_addr + 0x1C000000) >> 8) | 1<<16) & 0xFF0000) \
			+ ((((md_img_start_addr + 0x1E000000) >> 0) | 1<<24) & 0xFF000000);

		printf("---> MD_BANK0_MAP0=0x%x, MD_BANK0_MAP1=0x%x\n",
			*((volatile unsigned int*)MD1_BANK1_MAP0),
			*((volatile unsigned int*)MD1_BANK1_MAP1));
	}
	#endif

	#ifndef DVT_512M_SUPPORT /* HVT don't using this part */
	// For MDx_BANK4_MAP0 1st 32MB
	*((volatile unsigned int*)md_emi_remapping_addr_bank4) = (((md1_md3_share_memory_addr >> 24) | 1) & 0xFF);
	
	printf("---> MD_BANK4_MAP0=0x%x\n", *((volatile unsigned int*)md_emi_remapping_addr_bank4));
	#endif
}

static void md_power_up_mtcmos(unsigned int boot_md_id)
{
	volatile unsigned int loop = 10000;
	
	loop =10000;
	while(loop-->0);

	switch(boot_md_id) {
	case 0://MD 1
#ifdef ENABLE_MD_RESET_SPM
		spm_mtcmos_ctrl_mdsys1(STA_POWER_ON);
#else
		// default on
#endif
		break;		
	case 1:// MD2
#ifdef ENABLE_MD_RESET_SPM
		spm_mtcmos_ctrl_mdsys2(STA_POWER_ON);
#else
		// YP Lin will power it on in preloader
#endif
		break;
	default:
		break;  
	}
}

static void c2k_boot(int boot_mode);

int md_jtag_config(int boot_md_id)
{
	return 0;
}

int get_input(void)
{
	return uart_getc();
}

void apply_env_setting(int case_id)
{
	printf("Apply case:%d setting for dummy AP!\n", case_id);
}

void md_uart_config(int type_id, int boot_mode)
{
	switch(type_id) {
	case -1: // for AP only
		printf("md_uart_config:%d, UART1->[AP], UART2->AP, UART3->XXX, UART4->C2K\n", type_id);
		mt_set_gpio_mode(105, 1); //URXD0 / MD_URXD0
		mt_set_gpio_mode(106, 1); //UTXD0 / MD_UTXD0
		mt_set_gpio_mode(19, 2); //URXD1
		mt_set_gpio_mode(20, 2); //UTXD1
		//mt_set_gpio_mode(13, 4); //C2K_URXD0
		//mt_set_gpio_mode(14, 4); //C2K_UTXD0
		//mt_set_gpio_mode(17, 5); //C2K_URXD0
		//mt_set_gpio_mode(18, 5); //C2K_UTXD0
		break;
	case 0: // for AP & MD1
	case 1: // for AP & MD2201
		if(boot_mode==0) { // Normal boot
			printf("[Normal]md_uart_config:%d, UART1->[MD1], UART2->AP, UART3->XXX, UART4->C2K\n", type_id);
			#ifdef AP_MD_SAME_UART
			mt_set_gpio_mode(105, 3); //URXD0 / MD_URXD0
			mt_set_gpio_mode(106, 3); //UTXD0 / MD_UTXD0
			mt_set_gpio_mode(13, 0); //URXD0 / MD_URXD0
			mt_set_gpio_mode(14, 0); //UTXD0 / MD_UTXD0
			#else
			mt_set_gpio_mode(105, 1); //URXD0 / MD_URXD0
			mt_set_gpio_mode(106, 1); //UTXD0 / MD_UTXD0
			mt_set_gpio_mode(13, 4); //URXD0 / MD_URXD0
			mt_set_gpio_mode(14, 4); //UTXD0 / MD_UTXD0
			#endif
			//mt_set_gpio_mode(105, 1); //URXD0 / MD_URXD0
			//mt_set_gpio_mode(106, 1); //UTXD0 / MD_UTXD0
			mt_set_gpio_mode(19, 2); //URXD1
			mt_set_gpio_mode(20, 2); //UTXD1
			//mt_set_gpio_mode(13, 4); //C2K_URXD0
			//mt_set_gpio_mode(14, 4); //C2K_UTXD0
			//mt_set_gpio_mode(17, 5); //C2K_URXD0
			//mt_set_gpio_mode(18, 5); //C2K_UTXD0
		} else {
		printf("[Meta]md_uart_config:%d, UART1->[MD1], UART2->AP, UART3->XXX, UART4->C2K\n", type_id);
			mt_set_gpio_mode(105, 3); //URXD0 / MD_URXD0
			mt_set_gpio_mode(106, 3); //UTXD0 / MD_UTXD0
			mt_set_gpio_mode(13, 0); //URXD0 / MD_URXD0
			mt_set_gpio_mode(14, 0); //UTXD0 / MD_UTXD0
			//mt_set_gpio_mode(105, 1); //URXD0 / MD_URXD0
			//mt_set_gpio_mode(106, 1); //UTXD0 / MD_UTXD0
			mt_set_gpio_mode(19, 2); //URXD1
			mt_set_gpio_mode(20, 2); //UTXD1
			//mt_set_gpio_mode(13, 4); //C2K_URXD0
			//mt_set_gpio_mode(14, 4); //C2K_UTXD0
			//mt_set_gpio_mode(17, 5); //C2K_URXD0
			//mt_set_gpio_mode(18, 5); //C2K_UTXD0
		}
		break;
	case 2: // for both MD1 and MD2
		printf("md_uart_config:%d, UART1->[MD1], UART2->AP, UART3->XXX, UART4->C2K\n", type_id);
		mt_set_gpio_mode(105, 3); //URXD0 / MD_URXD0
		mt_set_gpio_mode(106, 3); //UTXD0 / MD_UTXD0
		mt_set_gpio_mode(13, 0); //MD_URXD0 ==> MD1
		mt_set_gpio_mode(14, 0); //MD_UTXD0 ==> MD1
		mt_set_gpio_mode(19, 2); //URXD1
		mt_set_gpio_mode(20, 2); //UTXD1
		//mt_set_gpio_mode(13, 4); //C2K_URXD0
		//mt_set_gpio_mode(14, 4); //C2K_UTXD0
		//mt_set_gpio_mode(17, 5); //C2K_URXD0
		//mt_set_gpio_mode(18, 5); //C2K_UTXD0
	default:
		break;
	}
}

static void let_md_go(int md_id)
{
	unsigned int reg_val;
	switch(md_id)
	{
	case 0:
		// Trigger MD run
		DRV_WriteReg32(0x20000024, 1);
		break;
	case 1:
		DRV_WriteReg32(AP_C2K_CONFIG, DRV_Reg32(AP_C2K_CONFIG)|(1<<3));
  		/* AP release C2K ARM core to let C2K go */
		reg_val = DRV_Reg32(C2K_CONFIG);
		reg_val &= ~(0x1 << 1);
		DRV_WriteReg32(C2K_CONFIG, reg_val);
		break;
	default:
		break;
	}
}


int md_common_setting(int boot_md_comb)
{
	unsigned int reg_value;
	// step 3: MD srcclkena setting
	if((boot_md_comb&0x3)==1)
		*((volatile unsigned int*)0x10001000+0xF0C) = 0x29;
	else {
		reg_value = *((volatile unsigned int*)0x10001000+0xF0C);
		reg_value &= 0x6D;
		reg_value |= 0x29;
		*((volatile unsigned int*)0x10001000+0xF0C) = reg_value;
	}
	printf("MD srcclkena setting:0x%x\n", *((volatile unsigned int*)0x10001000+0xF0C));
	return 0;
}
static void config_md_boot_env(int md_id, int boot_mode)
{
	unsigned int reg_value;
	
	// Configure EMI remapping setting
	printf("Configure EMI remapping for MD%d...\n", md_id+1);
	md_emi_remapping(md_id);

	switch(md_id) {
	case 0:// For MD1
		// step 0: PMIC setting
		md1_pmic_setting();
		// step 1: Power on MTCMOS
		md_power_up_mtcmos(md_id);

		// Temp Setting:
		// Request by Yuyang Hsiao and Adder
		DRV_WriteReg32(0x10006008,0x215830); 
		
		// step 2: pll init
		md1_pll_init();
		// step 3: set META Register
		if(boot_mode) {
			reg_value = DRV_Reg32(0x20000010);
			DRV_WriteReg32(0x20000010, (reg_value |0x1)); // Bit0, Meta mode flag, this need sync with MD init owner
		}

		// step 4: Disabel MD WDT
		//  -- PCore
		//DRV_WriteReg32(MD_WDT_BASE, 0x55000000);
		//  -- MD RGU L1 Core
		//DRV_WriteReg32(MD_L1WDT_BASE ,0x2200); //L Core
		
		
		break;
	case 1:// For MD2
		pmic_init_sequence();
		c2k_boot(boot_mode);
		break;
	default:
		break;
	}

	// Configure DAP for ICE to connect to MD
	printf("Configure DAP for ICE to connect to MD!\n");
	md_jtag_config(md_id);
}


void md_wdt_init(void)
{
	if(img_load_flag &(1<<MD1_IMG)) {	
		mt_irq_set_sens(MT_MD_WDT1_IRQ_ID, MT65xx_EDGE_SENSITIVE);
		mt_irq_set_polarity(MT_MD_WDT1_IRQ_ID, MT65xx_POLARITY_LOW);
		mt_irq_unmask(MT_MD_WDT1_IRQ_ID);
	}
	if(img_load_flag &(1<<MD2_IMG)) {	
		mt_irq_set_sens(MT_MD_WDT2_IRQ_ID, MT65xx_EDGE_SENSITIVE);
		mt_irq_set_polarity(MT_MD_WDT2_IRQ_ID, MT65xx_POLARITY_LOW);
		mt_irq_unmask(MT_MD_WDT2_IRQ_ID);
	}
}

#ifdef CCIF_DVT
/******************************************************************************/
#define ccif_debug(fmt, args...) printf("ts_ccif: "fmt, ##args)
#define ccif_error(fmt, args...) printf("[CCIF][Error] "fmt, ##args)
#define __IO_WRITE32(a, v) *((volatile unsigned int*)a)=(v)
#define __IO_READ32(a) (*((volatile unsigned int*)a))

#define CCIF_IRQ_ID   171
#define CCIF_REG_BASE 0x10218000
#define CCIF_MAX_PHY 16
#define CCIF_DATA_REG_LENGTH (512) 
#define CCIF_CON             (CCIF_REG_BASE + 0x0000)
#define CCIF_BUSY            (CCIF_REG_BASE + 0x0004)
#define CCIF_START           (CCIF_REG_BASE + 0x0008)
#define CCIF_TCHNUM          (CCIF_REG_BASE + 0x000c)
#define CCIF_RCHNUM          (CCIF_REG_BASE + 0x0010)
#define CCIF_ACK             (CCIF_REG_BASE + 0x0014)
#define CCIF_DATA            (CCIF_REG_BASE + 0x0100)
#define CCIF_TXCHDATA_OFFSET (0)
#define CCIF_RXCHDATA_OFFSET (CCIF_DATA_REG_LENGTH/2)
#define CCIF_TXCHDATA (CCIF_DATA+CCIF_TXCHDATA_OFFSET)
#define CCIF_RXCHDATA (CCIF_DATA + CCIF_RXCHDATA_OFFSET)

typedef struct
{
    unsigned int data[2];
    unsigned int channel;
    unsigned int reserved;
} CCCI_BUFF_T;

typedef struct
{
    unsigned int ap_state;
    unsigned int md_state;
    unsigned int test_case;
}sync_flag;

enum {    AP_NOT_READY=0x9abcdef, AP_READY, AP_SYNC_ID_DONE, AP_WAIT_BEGIN, AP_BEGIN,
    AP_REQUST, AP_REQUST_ONGOING, AP_REQUST_DONE
};
enum {    MD_NOT_READY=0xfedcba9, MD_READY, MD_SYNC_ID_DONE, MD_WAIT_BEGIN, MD_GEGIN,
    MD_WAIT_REQUST, MD_ACK_DONE
};

enum {    TC_00=0,
    TC_01=1, TC_02, TC_03, TC_04, TC_05, TC_06, TC_07, TC_08, TC_09, TC_10, 
    TC_11, TC_12, TC_13, TC_14, TC_15, TC_16, TC_17, TC_18, TC_19, TC_20, 
    TC_21, TC_22, TC_23, TC_24, TC_25, TC_26, TC_27, TC_28, TC_29, TC_30,
};

static CCCI_BUFF_T ccif_msg[CCIF_MAX_PHY];
static volatile unsigned int *test_heap_ptr;
static volatile unsigned int *test_nc_heap_ptr;
static void (*CCIF_isr_process_func)(void);
static void(*CCIF_call_back_func)(unsigned int);
static unsigned int using_user_isr=0;
static unsigned int rx_ch_flag = 1;
static unsigned int rx_ch_index = 0;
static volatile unsigned int *ap_md_share_heap_ptr;
static volatile sync_flag *sync_data_region;
static unsigned int *AP_MD_Result_Share_Region;
static const unsigned int phy_ch_seq_mode_seq[CCIF_MAX_PHY] = { 13, 5, 7, 11, 4,12, 0, 6, 3, 14, 9,2, 15,8,10,1 };
static const unsigned int data_pattern[] = {
    0x500af0f0, 0x501af0f0, 0x502af0f0, 0x503af0f0, 0x504af0f0,0x505af0f0,0x506af0f0,0x507af0f0,
    0x508af0f0, 0x509af0f0, 0x50aaf0f0, 0x50baf0f0, 0x50caf0f0,0x50daf0f0,0x50eaf0f0,0x50faf0f0,
    0x510af0f0, 0x511af0f0, 0x512af0f0, 0x513af0f0, 0x514af0f0,0x515af0f0,0x516af0f0,0x517af0f0,
    0x518af0f0, 0x519af0f0, 0x51aaf0f0, 0x51baf0f0, 0x51caf0f0,0x51daf0f0,0x51eaf0f0,0x51faf0f0,
};
static const unsigned int channel_bits_map[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};
static volatile unsigned int isr_work_done[30];
static const unsigned int result_unknow = 0;
static const unsigned int result_pass = 1<<16;
static const unsigned int result_fail = 2<<16;
static unsigned int rx_seq_for_isr = 0;
static unsigned int rx_last_time_val = 0;
static unsigned int irq_is_masked = 0;
static const int ccif_phy_arb_mode_seq[CCIF_MAX_PHY] = { 12, 3, 15, 1, 11, 8, 14, 2, 13, 4, 10, 7, 9, 5, 0, 6};

static void CCIF_arb_isr_func(void)
{
    // 1. Get channel
    unsigned int ch_flag = __IO_READ32(CCIF_RCHNUM);
    ccif_debug("CCIF_arb_isr_func() ch:%d\n", ch_flag);

    while(ch_flag){
        if(ch_flag & rx_ch_flag){
            // 2. Call callback function
            if(CCIF_call_back_func)
                (*CCIF_call_back_func)(rx_ch_index);
    
            // 3. Ack channel
            __IO_WRITE32(CCIF_ACK, rx_ch_flag);
            
            // 4. Clear rx channel flag
            ch_flag &= (~rx_ch_flag);
        }
        
        // 5. Updata index and ch flag
        rx_ch_index++;
        rx_ch_index &= 0xf;

        rx_ch_flag =1<<rx_ch_index;   
    }
}

static void CCIF_default_seq_cb_func()
{
    // 1. Get channel
    unsigned int ch = __IO_READ32(CCIF_RCHNUM);
    ccif_debug("CCIF_default_seq_cb_func() ch:%d\n", ch);
	
    // 2. Call callback function
    if(CCIF_call_back_func)
        (*CCIF_call_back_func)(ch);
    
    // 3. Ack channel
    __IO_WRITE32(CCIF_ACK, channel_bits_map[ch]);
}

void CCIF_Set_Con(unsigned int setting)
{
    __IO_WRITE32(CCIF_CON, setting);

    if(using_user_isr)
        return;

    if(setting&0x1){    // arb mode
        CCIF_isr_process_func = CCIF_arb_isr_func;
    }else{            // seq mode
        CCIF_isr_process_func = CCIF_default_seq_cb_func;
    }
}

void CCIF_Register_User_Isr(void(*pfunc)(void))
{
    static void (*pfunc_bak)(void);
    if(pfunc){
        using_user_isr = 1;
        pfunc_bak = CCIF_isr_process_func;
        CCIF_isr_process_func = pfunc;
    }else{
        using_user_isr = 0;
        CCIF_isr_process_func = pfunc_bak;
    }
}

int CCIF_Register(void(*pfunc)(unsigned int))
{
    CCIF_call_back_func = pfunc;
    return 0;
}

unsigned int CCIF_Get_Busy(void)
{
    return __IO_READ32(CCIF_BUSY);
}

unsigned int CCIF_Get_Start(void)
{
    return __IO_READ32(CCIF_START);
}

void CCIF_Set_Busy(unsigned int setting)
{
    __IO_WRITE32(CCIF_BUSY, setting);
}

void CCIF_Set_TxData(unsigned int ch, char buff[])
{
    unsigned int *src_ptr;
    unsigned int *des_ptr;
    int i;

    src_ptr = (unsigned int *)&buff[0];
    des_ptr = (unsigned int *)(CCIF_TXCHDATA + (16*ch));

    for(i=0; i<4; i++)
        des_ptr[i] = src_ptr[i];
}

void CCIF_Set_Tch_Num(unsigned int ch)
{
    __IO_WRITE32(CCIF_TCHNUM, ch);
}

int CCIF_Init()
{
    // 1. Mask CCIF Irq
    CCIF_Mask_Irq(CCIF_IRQ_ID);
    
    // 2. Set to arb mode
    CCIF_Set_Con(1);
    
    // 3. Ack all channel
    //CCIF_Set_Ack(0xff);
    
    // 4. Register interrupt to system
    //IRQ_Register_LISR(CCIF_IRQ_ID, CCIF_Isr, "CCIF");  // FIXME
    mt_irq_set_sens(CCIF_IRQ_ID, MT65xx_LEVEL_SENSITIVE);
    mt_irq_set_polarity(CCIF_IRQ_ID, MT65xx_POLARITY_LOW);
    //IRQClearInt(CCIF_IRQ_ID);

    // 5. Un-mask Irq
    CCIF_UnMask_Irq(CCIF_IRQ_ID);
}

void CCIF_Set_ShareData(unsigned int offset, unsigned int setting)
{
    __IO_WRITE32((CCIF_DATA+(offset&(~0x003))), setting);
}

unsigned int CCIF_Get_ShareData(unsigned int offset)
{
    return __IO_READ32( (CCIF_DATA+(offset&(~0x003))) );
}

void CCIF_Get_RxData(unsigned int ch, char buff[])
{
    unsigned int *src_ptr;
    unsigned int *des_ptr;
    int i;

    des_ptr = (unsigned int *)&buff[0];
    src_ptr = (unsigned int *)(CCIF_RXCHDATA + (16*ch));

    for(i=0; i<4; i++)
        des_ptr[i] = src_ptr[i];
}

void CCIF_Isr(unsigned int irq)
{
	if(irq == CCIF_IRQ_ID) {
		printf("CCIF ISR enter!\n");
    	(*CCIF_isr_process_func)();
		mt_irq_ack(CCIF_IRQ_ID);
		mt_irq_unmask(CCIF_IRQ_ID);
		printf("CCIF ISR exit!\n");
	}
}

void CCIF_Mask_Irq()
{
    mt_irq_mask(CCIF_IRQ_ID);
    irq_is_masked = 1;
}
void CCIF_UnMask_Irq()
{
    mt_irq_unmask(CCIF_IRQ_ID);
    irq_is_masked = 0;
}

void CCIF_Set_Ack(unsigned int ch)
{
    __IO_WRITE32(CCIF_ACK, ch);
}

unsigned int CCIF_Get_Tch_Num()
{
    return __IO_READ32(CCIF_TCHNUM);
}

unsigned int CCIF_Get_Rch_Num(void)
{
    return __IO_READ32(CCIF_RCHNUM);
}

void AP_MD_Test_Sync_Init(unsigned int addr)
{
    ccif_debug("AP_MD_Test_Sync_Init 10 \n");
    sync_data_region = (sync_flag *)addr;
    sync_data_region->ap_state = AP_NOT_READY;
    sync_data_region->md_state = MD_NOT_READY;
    sync_data_region->test_case = 0;
    ccif_debug("AP_MD_Test_Sync_Init 99 \n");
}

void Set_AP_MD_Sync_Share_Memory(unsigned int ptr)
{
    enum {AP_IN_INIT_DONE=0x12345678, AP_WRITE_DATA_DONE};
    enum {MD_IN_INIT_DONE=0x87654321, MD_GET_DATA_DONE};

    ccif_debug("Check if MD init done 0x%x\n", CCIF_Get_ShareData(CCIF_RXCHDATA_OFFSET));
    // 1. Notify MD that AP init done
    ccif_debug("Notify MD that AP init done, addr:0x%x, value:0x%x\n", CCIF_DATA,AP_IN_INIT_DONE);
    CCIF_Set_ShareData(CCIF_TXCHDATA_OFFSET, AP_IN_INIT_DONE);
    // 2. Check MD is init done
    ccif_debug("Check if MD init done, addr:0x%x, value:0x%x\n", (CCIF_DATA + (CCIF_RXCHDATA_OFFSET & (~0x003))), MD_IN_INIT_DONE);
    while(CCIF_Get_ShareData(CCIF_RXCHDATA_OFFSET)!=MD_IN_INIT_DONE);
    ccif_debug("Check if MD init done 0x%x\n", CCIF_Get_ShareData(CCIF_RXCHDATA_OFFSET));

    // 3. MD ready, notify share memory start addr
    ccif_debug("AP MD share memroy start addr:%x\n", (unsigned int)ptr);

    ptr -= img_addr_tbl[MD2_IMG]; // FIXME, hardcode;
    
    CCIF_Set_ShareData(CCIF_TXCHDATA_OFFSET+4, (unsigned int)(ptr));
    ccif_debug("Notify MD get share memroy start addr:0x%x\n",(unsigned int) ptr);
    CCIF_Set_ShareData(0, AP_WRITE_DATA_DONE);
    
    // 4. Wait MD get data done
    ccif_debug("Check if MD get data done\n");
    while(CCIF_Get_ShareData(CCIF_RXCHDATA_OFFSET)!=MD_GET_DATA_DONE);
    
    // 5. Set share memory done, clear ccif shareram
    ccif_debug("Set_AP_MD_Sync_Share_Memory done\n");
    CCIF_Set_ShareData(CCIF_TXCHDATA_OFFSET, 0);
    CCIF_Set_ShareData(CCIF_TXCHDATA_OFFSET+4, 0);
    CCIF_Set_ShareData(CCIF_RXCHDATA_OFFSET, 0);
}

void CCIF_Test_Env_Init(unsigned int ptr)
{
    ccif_debug("CCIF_Test_Env_Init 1\n");
    AP_MD_Test_Sync_Init(ptr);

    ccif_debug("CCIF_Test_Env_Init 2\n");
    Set_AP_MD_Sync_Share_Memory(ptr);

    ccif_debug("CCIF_Test_Env_Init 3\n");
    //INFRA_disable_clock(MT65XX_PDN_INFRA_CCIF0);
    
    ccif_debug("CCIF_Test_Env_Init 4\n");
    AP_MD_Result_Share_Region = (unsigned int*)(ptr+sizeof(sync_flag)*2);
}

int ts_init_handler()
{
    int ret_val = 0;
    unsigned int *ptr, *nc_ptr;
	
    memset(ccif_msg,0,sizeof(ccif_msg));
    ccif_debug("TC init\n");

    test_heap_ptr=(unsigned int *)malloc(1024*2);
    test_nc_heap_ptr=(unsigned int *)malloc(1024*2);

    ccif_debug("\tTC init step 3\n");    
	CCIF_Init();
    ccif_debug("\tTC init step 4\n");
    ap_md_share_heap_ptr = (void *)(img_addr_tbl[MD2_IMG]+0x100000); // FIXME, hardcode
    ccif_debug("\tTC init step 5\n");
    //Let_MD_to_Run();
    ccif_debug("\tTC init step 6\n");
    CCIF_Test_Env_Init((unsigned int)ap_md_share_heap_ptr);
    ccif_debug("TC init done.\n");
    return ret_val;
}

void AP_MD_test_case_sync_AP_Side(unsigned int tc)
{
    int i =0;
    ccif_debug("Enter set_ccif_test_case_sync \n");
    // 1. Notify MD that AP ready
    ccif_debug("Notify MD that AP ready, addr:0x%x, val:0x%x, disr:0x%x\n",(unsigned int)(&(sync_data_region->ap_state)), sync_data_region->ap_state, AP_READY);
    sync_data_region->ap_state = AP_READY;
	//arch_sync_cache_range(&sync_data_region, sizeof(sync_data_region));

    // 2. Check MD is ready
    ccif_debug("Check if MD is ready, addr:0x%x, val:0x%x, disr:0x%x\n", (unsigned int)(&(sync_data_region->md_state)),sync_data_region->md_state, AP_READY);
    while(sync_data_region->md_state!=MD_READY) {
        ccif_debug("Check AP addr:0x%x, State:%x\n", &(sync_data_region->ap_state),
                            sync_data_region->ap_state);
        ccif_debug("Check MD addr:0x%x, State:%x, expeted:0x%x\n", &(sync_data_region->md_state),
                            sync_data_region->md_state, MD_READY);
    };
    // 3. MD ready, notify MD test case ID
    sync_data_region->test_case = tc;
    ccif_debug("Notify MD test id :%x\n", tc);
    sync_data_region->ap_state = AP_SYNC_ID_DONE;

    // 4. Wait MD get tc id done
    ccif_debug("Check if MD ready to test\n");
    while(sync_data_region->md_state!=MD_SYNC_ID_DONE){
       ccif_debug("Check AP 2 addr:0x%x, State:%x\n", &(sync_data_region->ap_state),
                            sync_data_region->ap_state);
        ccif_debug("Wait MD get TC id Done: addr:0x%x, State:%x, expeted:0x%x\n", &(sync_data_region->md_state),
                            sync_data_region->md_state, MD_SYNC_ID_DONE);
    };

    // 5. Finish sysn
    ccif_debug("AP_MD_test_case_sync_AP_Side done\n");
}

void AP_MD_Ready_to_Test_Sync_AP_Side()
{
    ccif_debug("Enter AP_MD_Ready_to_Test_Sync_AP_Side \n");
    // 1. Notify MD that AP ready
    ccif_debug("Notify MD that AP ready\n");
    sync_data_region->ap_state = AP_WAIT_BEGIN;

    // 2. Check MD is ready
    ccif_debug("Check if MD is ready\n");
    while(sync_data_region->md_state!=MD_WAIT_BEGIN);
    sync_data_region->ap_state=AP_NOT_READY;
    sync_data_region->md_state=MD_NOT_READY;
    // 3. MD also ready
    ccif_debug("Begin to test\n");
}

int AP_Get_MD_Test_Result(unsigned int buf[], unsigned int length)
{
    int act_get = 0;
    unsigned int j;
    ccif_debug("Enter AP_Get_MD_Test_Result \n");
    // 1. Notify MD that AP request test result
    ccif_debug("Notify MD that AP request test result\n");
    sync_data_region->ap_state = AP_REQUST;

    // 2. Check MD is wait request
    ccif_debug("Check if MD is wait send result\n");
    while(sync_data_region->md_state!=MD_WAIT_REQUST);

    sync_data_region->ap_state = AP_REQUST_ONGOING;
    // 3. Wait MD write result done
    ccif_debug("Wait MD write result done\n");
    while(sync_data_region->md_state!=MD_ACK_DONE);

    // 4. Get Test result
    for(j=0; (j<AP_MD_Result_Share_Region[0])&&(j<length); j++)
        buf[j] = AP_MD_Result_Share_Region[j+1];
    
    // 5. Notify MD AP get result done
    ccif_debug("Notify MD AP get result done buf[0](%d)\n", buf[0]);
    sync_data_region->ap_state = AP_REQUST_DONE;
    
    return j;
}

void Set_Isr_Work_Done(unsigned int tc)
{
	printf("CCIF ISR done for TC%d\n", tc);
    isr_work_done[tc] = 1;
}
void Reset_Isr_Work_State(unsigned int tc)
{
    isr_work_done[tc] = 0;
}
int Is_Isr_Work_Done(unsigned int tc)
{
    return isr_work_done[tc];
}

static void ccif_seq_isr_cb_for_tc02(unsigned int ch)
{
    unsigned int data[4];
    unsigned int seq = test_heap_ptr[0];
    unsigned int mis_match = 0;

    // 1. Read Data Send from MD
    CCIF_Get_RxData(ch, (unsigned char*)data);

    // 2. Check sequence
    if(ch != phy_ch_seq_mode_seq[seq])
    {
        mis_match = (1<<0);
        ccif_error("AP <== MD seq mode channel seq mis-match(%d!=%d), seq:%d\n",ch,phy_ch_seq_mode_seq[seq], seq);
    }
    // 3. Check rx data
    if( (data[0]==data[1])&&(data[1]==data[2])&&(data[2]==data[3])&&
        (data[3]==(data_pattern[TC_02]+ch))&&(result_unknow==test_heap_ptr[ch+1]) )
        test_heap_ptr[ch+1] = result_pass;
    else
        {
            test_heap_ptr[ch+1] = result_fail;
            ccif_error("%x != %x \r\n",data[0],data_pattern[TC_02]+ch);
        }

    // 4 Update mis-match info
    if(mis_match){
        test_heap_ptr[ch+1] += 1;
        mis_match = 0;
    }

    // 5. Check whether test done
    seq++;
    test_heap_ptr[0] = seq;
    if(test_heap_ptr[0] == CCIF_MAX_PHY)
        Set_Isr_Work_Done(TC_02);
}

static void ccif_arb_isr_for_tc04()
{
    unsigned int ch;
    // 1. Get Channel
    ch = CCIF_Get_Rch_Num();
    if (ch < 1) {
        ccif_debug("ch = 0, exit abr isr\r\n");
        return ;
    }
    // 2. Check Rx seq
    if(ch!=rx_last_time_val){
        // Has new data
        // if( (ch^rx_last_time_val)==(unsigned int)(1<<ccif_phy_arb_mode_seq[rx_seq_for_isr]) )
        //     rx_seq_for_isr++;
        if( (ch)==(unsigned int)(1<<ccif_phy_arb_mode_seq[rx_seq_for_isr]) )
            rx_seq_for_isr++;
        rx_last_time_val = ch;
        CCIF_Set_Ack(ch);
        CCIF_Get_Rch_Num();
        CCIF_Get_Rch_Num();
    }
    // 3. Check if RX all channel have data
    if((ch==0xFFFF) ||(rx_seq_for_isr == CCIF_MAX_PHY)){
        Set_Isr_Work_Done(TC_04);
        CCIF_Mask_Irq();
        // CCIF_Set_Ack(0xFFFFFFFF);
        ccif_debug("Output: seq_for_isr:0x%x\r\n", rx_seq_for_isr);
    }
}

int ccif_ap_test_case_01(void)
{
    unsigned int irq, data[4], phy;
    int i;
    unsigned int md_result;
    unsigned int reg_busy=0, reg_start=0;
    unsigned int tmp_busy, tmp_start;
    unsigned int ret=0;
    unsigned int last_busy_val=0;

    ccif_debug("[TC01]AP ==> MD sequence test\n");
    AP_MD_test_case_sync_AP_Side(TC_01);
    CCIF_Set_Con(0);
    CCIF_Register(0);
    AP_MD_Ready_to_Test_Sync_AP_Side();

    for (i = 0; i < CCIF_MAX_PHY; i++) {        
        phy = phy_ch_seq_mode_seq[i];
        data[0] = data[1] = data[2] = data[3] = data_pattern[TC_01]+phy;
        // 1. Busy bit should be zero
        tmp_busy = CCIF_Get_Busy();
        tmp_start = CCIF_Get_Start();
        if(tmp_busy&(1<<phy)){
            ccif_error("\tphysical ch %d busy now, abnormal\n", phy);
            ret=(1<<0);
        }
        if(tmp_start&(1<<phy)){
            ccif_error("\tphysical ch %d start bit is 1 now, abnormal\n", phy);
            ret=(1<<1);
        }

        // 2. Busy bits for this channel will change to 1
        CCIF_Set_Busy(1 << phy);
        reg_busy |= 1<<phy;
        if( (reg_busy&CCIF_Get_Busy())!= CCIF_Get_Busy() ){
            ccif_error("\tphysical ch %d busy bit change to 1 fail\n", phy);
            ret=(1<<3);
        }
        CCIF_Set_TxData(phy, (unsigned char*)data);
        CCIF_Set_Tch_Num(phy);
        //CTP_Wait_msec(1000);
        ccif_debug("send data to physical ch %d, data:0x%x, 0x%x, 0x%x, 0x%x\n", phy, data[0], data[1], data[2], data[3]);

        // 3. Start bit for this ch should change to 1
        reg_start|= 1<<phy;
        if( (reg_start&CCIF_Get_Start())!= CCIF_Get_Start() ){
            ccif_error("physical ch %d start bit change to 1 fail\n", phy);
            ret=(1<<4);
        }
    }
    last_busy_val = reg_busy;

    i=20;
    // while(i-- && (last_busy_val=CCIF_Get_Busy())) {delay_a_while(500000) ;}
    while(i-- && (last_busy_val=CCIF_Get_Busy())) {mdelay(200) ; ccif_debug("[TC01]Test,2 CCIF_Get_Busy():0x%x \n", CCIF_Get_Busy());}
    if (last_busy_val)
    {
        ccif_error("last_busy_val=%x  Failed to clear \n",last_busy_val);
        ret=(1<<5);
    }
    AP_Get_MD_Test_Result(&md_result, 1);
    if( (0==md_result)&&(0==ret) ){
        ccif_debug("[TC01]Test pass\n");
        return 0;
    }else{
        ccif_error("[TC01]Test fail,md_result=%d,ap_result=%d\n",md_result,ret);
        return -1;
    }
}

int ccif_ap_test_case_02(void)
{
    int i;
    unsigned int md_result=0;
    int error_val=0;

    ccif_debug("[TC02]AP <== MD sequence test\n");
    AP_MD_test_case_sync_AP_Side(TC_02);
    CCIF_Set_Con(0);
    CCIF_Register(0);
    for(i=1; i<=CCIF_MAX_PHY; i++)
        test_heap_ptr[i] = result_unknow;
    test_heap_ptr[0] = 0;
    Reset_Isr_Work_State(TC_02);

    CCIF_Register(ccif_seq_isr_cb_for_tc02);

    CCIF_Mask_Irq(CCIF_IRQ_ID);

    AP_MD_Ready_to_Test_Sync_AP_Side();

    // Sleep 500ms to let MD side send all 8 channel data done
    //CTP_Wait_msec(500);

    CCIF_UnMask_Irq(CCIF_IRQ_ID);

    // Waiting test done
    ccif_debug("\tWaiting md test done\n");
    while(!Is_Isr_Work_Done(TC_02));

    // Check result
    ccif_debug("\tCheck AP <== MD result\n");
    for(i=1; i<=CCIF_MAX_PHY; i++){
        if(result_fail == (test_heap_ptr[i]&0xffff0000)){
            ccif_error("\tAP <== MD seq mode data check fail\n");
            error_val = -1;
            goto _Result;
        }
        if(0 != (test_heap_ptr[i]&0x0000ffff)){
            ccif_error("\tAP <== MD seq mode check seq fail\n");
            error_val = -2;
            goto _Result;
        }
    }
    if(i!=CCIF_MAX_PHY+1){
        ccif_error("\tAP <== MD seq mode channel num fail\n");
        error_val = -3;
    }

_Result:
    ccif_debug("\tCheck AP ==> MD result\n");
    AP_Get_MD_Test_Result(&md_result, 1);
    if( (0==md_result)&&(0==error_val) ){
        ccif_debug("[TC02]Test pass\n");
        return 0;
    }else{
        ccif_error("[TC02]Test fail,md_result=%d,ret=\n",md_result,error_val);
        return -1;
    }
}

int ccif_ap_test_case_03(void)
{
    unsigned int irq, data[4], phy;
    unsigned int i;
    unsigned int reg_busy=0, reg_start=0;
    unsigned int tmp_busy, tmp_start;
    unsigned int ret=0,md_result = 0;
    unsigned int last_busy_val=0;

    ccif_debug("[TC03]AP ==> MD arbitration test\n");
    CCIF_Set_Con(1);
    AP_MD_test_case_sync_AP_Side(TC_03);
    
    CCIF_Register(0);
    AP_MD_Ready_to_Test_Sync_AP_Side();

    for (i = 0; i < CCIF_MAX_PHY; i++) {
            data[0] = data[1] = data[2] = data[3] = data_pattern[TC_03]+i;
            phy = ccif_phy_arb_mode_seq[i];

        // 1. Busy bit should be zero
        tmp_busy = CCIF_Get_Busy();
        tmp_start = CCIF_Get_Start();
        if(tmp_busy&(1<<phy)){
            ccif_error("\tphysical ch %d busy now, abnormal\n", phy);
            ret=(1<<0);
        }
        if(tmp_start&(1<<phy)){
            ccif_error("\tphysical ch %d start bit is 1 now, abnormal\n", phy);
            ret=(1<<1);
        }

        // 2. Busy bits for this channel will change to 1
        CCIF_Set_Busy(1<<phy);
        reg_busy |= 1<<phy;
        if( (reg_busy&CCIF_Get_Busy())!= CCIF_Get_Busy() ){
            ccif_error("\tphysical ch %d busy bit change to 1 fail\n", phy);
            ret=(1<<2);
        }

        CCIF_Set_TxData(phy, (unsigned char*)data);
        CCIF_Set_Tch_Num(phy);
        ccif_debug("send data to physical ch %d\n", phy);

        // 3. Start bit for this ch should change to 1
        reg_start|= 1<<phy;
        if( (reg_start&CCIF_Get_Start())!= CCIF_Get_Start() ){
            ccif_error("\tphysical ch %d start bit change to 1 fail\n", phy);
            ret=(1<<3);
        }
    }
    last_busy_val = reg_busy;

    i=20;
    while(i-- && (last_busy_val=CCIF_Get_Busy())) {mdelay(20);}
    if (last_busy_val)
    {
        ccif_debug("last_busy_val=%x  Failed to clear \n",last_busy_val);
        ret=(1<<4);
    }
    AP_Get_MD_Test_Result(&md_result, 1);

    if(ret||md_result){
        ccif_error("[TC03]Test fail, md_result=%d,ret=%d\n",md_result,ret);
        return -1;
    }else{
        ccif_debug("[TC03]Test pass\n");
        return 0;
    }
}


int ccif_ap_test_case_04(void)
{
    unsigned int irq, data[4], phy;
    int i;
    unsigned int ret=0, md_result=0;
    ccif_debug("[TC04]AP <== MD arbitration test\n");
    rx_seq_for_isr = 0;
    rx_last_time_val = 0;
    //CCIF_CHECK_POINT_IN_BEGIN();
    AP_MD_test_case_sync_AP_Side(TC_04);
    CCIF_Set_Con(1);
    CCIF_UnMask_Irq();
    mdelay(1000);
    ccif_debug("delay for some time \n");
    CCIF_Register(0);
    for(i=1; i<=CCIF_MAX_PHY; i++)
        test_heap_ptr[i] = result_unknow;
    test_heap_ptr[0] = 0;
    Reset_Isr_Work_State(TC_04);

    ccif_debug("\tCCIF_Register_User_Isr \n");
    CCIF_Register_User_Isr(ccif_arb_isr_for_tc04);

    ccif_debug("\tAP_MD_Ready_to_Test_Sync_AP_Side \n");
    AP_MD_Ready_to_Test_Sync_AP_Side();

    // Waiting test done
    while(!Is_Isr_Work_Done(TC_04));
    
    ccif_debug("\tIsr work done \n");

    // Check Data
    for(i=0; i<CCIF_MAX_PHY; i++){
        CCIF_Get_RxData(ccif_phy_arb_mode_seq[i], (unsigned char*)data);
        if( (data[0]==data[1])&&(data[1]==data[2])&&(data[2]==data[3])&&
            (data[3]==(data_pattern[TC_04]+i)) );
        else{
            ret=1;
        }
        // Ack AP
        CCIF_Set_Ack(1<<ccif_phy_arb_mode_seq[i]);
        mdelay(1);
    }

    CCIF_Register_User_Isr(0);
    rx_seq_for_isr = 0;
    rx_last_time_val = 0;

    AP_Get_MD_Test_Result(&md_result, 1);
    //CCIF_CHECK_POINT_IN_END();
    if(ret||md_result){
        ccif_error("[TC04]Test fail,md_result=%d,ret=%d\n",md_result,ret);
        return -1;
    }else{
        ccif_debug("[TC04]Test pass\n");
        return 0;
    }
}


/******************************************************************************/
#endif

void dummy_ap_entry(void)
{
	int md_check_tbl[] = {1<<MD1_IMG, 1<<MD2_IMG};
	int i=0;
	int get_val=0;
	unsigned int dsp_size;
	unsigned int dsp_offset;
	unsigned int *dsp_load_addr;
	unsigned int *md_load_addr;
	unsigned int md_size;
	struct md_check_header_v3 *p_v3header;
	struct md_check_header_v5 *p_v5header;
	int boot_mode;
	unsigned int *size_ptr;

	// reinit UART, overwrite DWS setting
	md_uart_config(-1, 0);

	// Disable AP WDT
	*(volatile unsigned int *)(TOPRGU_BASE) = 0x22000000; 

	printf("Welcome to use dummy AP!\n");
	//get_val = get_input();

	apply_env_setting(get_val);

	// 0, Parse header info
	printf("Parsing image info!\n");
	parse_img_header((unsigned int*)g_boot_arg->part_info, (unsigned int)g_boot_arg->part_num);
	#ifndef DVT_512M_SUPPORT /* HVT don't using dsp */
	dsp_load_addr = query_img_setting(MD_DSP, NULL);
	md_load_addr = query_img_setting(MD1_IMG, &md_size);
	printf("DSP[%x],MD[%x](%x)!\n", dsp_load_addr, md_load_addr, md_size);
	if((dsp_load_addr==NULL) || (md_load_addr==NULL)){
		printf("Skip move dsp\n");
	} else {
		size_ptr = (unsigned int *)((unsigned int)md_load_addr+md_size-sizeof(int));
		if(*size_ptr == sizeof(struct md_check_header_v3)) {
			printf("v3 header!\n");
			p_v3header = (struct md_check_header_v3 *)((unsigned int)md_load_addr+md_size-sizeof(struct md_check_header_v3));
			printf("p_header addr:%x\n", p_v3header);
			if(strncmp(p_v3header->check_header, "CHECK_HEADER", 12)==0) {
				printf("Header[%s]\n", p_v3header->check_header);
				dsp_offset = p_v3header->dsp_img_offset;
				printf("dsp offset:%x\n", dsp_offset);
				dsp_size = p_v3header->dsp_img_size;
				printf("move %x to %x(%x)\n", dsp_load_addr, (unsigned int)md_load_addr+dsp_offset, dsp_size);
				mem_cpy_check(((unsigned int)md_load_addr+dsp_offset), dsp_load_addr, dsp_size);
				printf("copy done\n");
			} else
				printf("no valid header[%s]\n", p_v3header->check_header);
		} else if(*size_ptr == sizeof(struct md_check_header_v5)) {
			printf("v5 header!\n");
			p_v5header = (struct md_check_header_v5 *)((unsigned int)md_load_addr+md_size-sizeof(struct md_check_header_v5));
			printf("p_header addr:%x\n", p_v5header);
			if(strncmp(p_v5header->check_header, "CHECK_HEADER", 12)==0) {
				printf("Header[%s]\n", p_v5header->check_header);
				dsp_offset = p_v5header->dsp_img_offset;
				printf("dsp offset:%x\n", dsp_offset);
				dsp_size = p_v5header->dsp_img_size;
				printf("move %x to %x(%x)\n", dsp_load_addr, (unsigned int)md_load_addr+dsp_offset, dsp_size);
				mem_cpy_check(((unsigned int)md_load_addr+dsp_offset), dsp_load_addr, dsp_size);
				printf("copy done\n");
			} else
				printf("no valid header[%s]\n", p_v5header->check_header);
		} else {
			printf("header version can't detect\n");
		}
	}
	#endif

	printf("Begin to configure MD run env!\n");

	// 1, Setup special GPIO request (RF/SIM/UART ... etc)
	printf("Configure GPIO!\n");
	if((img_load_flag&((1<<MD1_IMG)|(1<<MD2_IMG))) == ((1<<MD1_IMG)|(1<<MD2_IMG))) {
		md_gpio_config(2);
	} else if (img_load_flag & (1<<MD1_IMG)) {
		md_gpio_config(0);
	} else if (img_load_flag & (1<<MD2_IMG)) {
		md_gpio_config(1);
	}

	// 2, Check boot Mode
#ifdef DEFAULT_META
	boot_mode = 1;
#else
	boot_mode = meta_detection();
#endif
	printf("Get boot mode is %d\n", boot_mode);

	// 3, MD WDT ISR init
	printf("Init MD WDT\n");
	md_wdt_init();

	// 4. Common Env setting
	md_common_setting(img_load_flag);

	// 5. Setup per-MD env before boot up MD
	for(i=0; i<MAX_MD_NUM; i++) {
		if(img_load_flag & md_check_tbl[i]) {
			printf("MD%d Enabled\n", i+1);
			config_md_boot_env(i, boot_mode);
		}
	}

	// 6, Switch UART
	printf("Switch UART!\n");
	if((img_load_flag&((1<<MD1_IMG)|(1<<MD2_IMG))) == ((1<<MD1_IMG)|(1<<MD2_IMG))) {
		md_uart_config(2, boot_mode);
	} else if (img_load_flag & (1<<MD1_IMG)) {
		md_uart_config(0, boot_mode);
	} else if (img_load_flag & (1<<MD2_IMG)) {
		md_uart_config(1, boot_mode);
	}

	#ifdef ALWAYS_USING_42000000_AS_MD1_MD3_SHARE
	memset((void*)0x42000000, 0, 0x200000);
	#endif

	// 7, Trigger modem to run
	for(i=0; i<MAX_MD_NUM; i++) {
		if(img_load_flag & md_check_tbl[i]) {
			printf("MD%d go\n", i+1);
			let_md_go(i);
		}
	}

#if CCIF_DVT
	arch_disable_cache(UCACHE);
	ts_init_handler();
	while(1) {
		printf("select test case:\n");
		get_val = get_input();
		uart_putc(get_val);
		switch(get_val) {
		case '1':
			ccif_ap_test_case_01();
			break;
		case '2':
			ccif_ap_test_case_02();
			break;
		case '3':
			ccif_ap_test_case_03();
			break;
		case '4':
			ccif_ap_test_case_04();
			break;
		case 'q':
			goto nothing;
		default:
			break;
		};
	};
#endif
nothing:
	printf("enter while(1), ^O^!!\n");
	while(1);
}

void bus_protection_en(int md_id)
{
	if(md_id == 0)
	{
		/* enable protection for MD1 */
		printf("enable protection for md\n");
		DRV_WriteReg32(0x10001250, 0x003F0000);
		/* Polling protection ready */
		printf("wait protection ....\n");
		while ((DRV_Reg32(0x10001258)&0x003F0000) != 0x003F0000);
		printf("protection done\n");
		return;
	}
}

void bus_protection_diable(int md_id)
{
	if(md_id == 0)
	{
		/* enable protection for MD1 */
		printf("disable protection for md\n");
		DRV_WriteReg32(0x10001250, 0x00000000);
		/* Polling protection ready */
		printf("wait protection disable....\n");
		while ((DRV_Reg32(0x10001258)&0x003F0000) != 0x00000000);
		printf("protection disable done\n");
		return;
	}
}


void md_wdt_irq_handler(unsigned int irq)
{
	unsigned int reg_value;
	
#if defined(ENABLE_MD_RESET_SPM) || defined(ENABLE_MD_RESET_RGU)
	// update counter
	unsigned int cnt = *(volatile unsigned int *)(TOP_RGU_WDT_NONRST_REG);
	*(volatile unsigned int *)(TOP_RGU_WDT_NONRST_REG) = cnt+1;
	// reset UART config
	md_uart_config(-1, 0);
	printf("\n\n\n\nCurrent wdt cnt:%d\n", cnt+1);
	
	if(irq == MT_MD_WDT1_IRQ_ID) {
#ifdef ENABLE_MD_RESET_SPM
		printf("MD1 power off\n");
		spm_mtcmos_ctrl_mdsys1(STA_POWER_DOWN);
		mdelay(5);
#endif
#ifdef ENABLE_MD_RESET_RGU
		printf("MD1 reset\n");
		
		bus_protection_en(0);
		reg_value = *((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST);
		*((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST) = reg_value|0x88000000|(0x1<<7);
		mdelay(5);
		reg_value = *((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST);
		*((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST) = (reg_value|0x88000000)&(~(0x1<<7));
		bus_protection_diable(0);
#endif
		config_md_boot_env(0, 0);
		let_md_go(0);
	}
	if(irq == MT_MD_WDT2_IRQ_ID) {
#ifdef ENABLE_MD_RESET_SPM
		printf("MD2 power off\n");
		spm_mtcmos_ctrl_mdsys2(STA_POWER_DOWN);
		mdelay(5);
#endif
#ifdef ENABLE_MD_RESET_RGU
		printf("MD2 reset\n");
		reg_value = *((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST);
		*((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST) = reg_value|0x88000000|(0x1<<15);
		mdelay(5);
		reg_value = *((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST);
		*((volatile unsigned int*)TOP_RGU_WDT_SWSYSRST) = (reg_value|0x88000000)&(~(0x1<<15));
#endif
		config_md_boot_env(1, 0);
		let_md_go(1);
	}

	#if 1
	printf("Config UART after MD WDT! %d\n", cnt+1);
	if((img_load_flag&((1<<MD1_IMG)|(1<<MD2_IMG))) == ((1<<MD1_IMG)|(1<<MD2_IMG))) {
		md_uart_config(2, 0);
	} else if (img_load_flag & (1<<MD1_IMG)) {
		md_uart_config(0, 0);
	} else if (img_load_flag & (1<<MD2_IMG)) {
		md_uart_config(2, 0);
	}
	#endif
#else
	md_uart_config(-1, 0);
	printf("Get MD WDT irq, STA:%x!!\n", *((volatile unsigned int*)(MD_WDT_STA)));
#ifdef IGNORE_MD_WDT
	printf("ignore MD WDT\n");
#else
	printf("whole system reboot\n");
	*(volatile unsigned int *)(TOP_RGU_WDT_MODE) = 0x22000000;
	*(volatile unsigned int *)(TOP_RGU_WDT_SWRST) = 0x1209;
	while(1);
#endif
#endif	
}

void dummy_ap_irq_handler(unsigned int irq)
{
#ifdef CCIF_DVT
	CCIF_Isr(irq);
#endif

	switch(irq) {
	case MT_MD_WDT1_IRQ_ID:
		if(img_load_flag &(1<<MD1_IMG)) {
			#ifndef IGNORE_MD1_WDT
			md_wdt_irq_handler(MT_MD_WDT1_IRQ_ID);
			#else
			printf("ignore MD1 WDT\n");
			#endif
			mt_irq_ack(MT_MD_WDT1_IRQ_ID);
			mt_irq_unmask(MT_MD_WDT1_IRQ_ID);
		}
		break;
	case MT_MD_WDT2_IRQ_ID:
		if(img_load_flag &(1<<MD2_IMG)) {
			#ifndef IGNORE_MD2_WDT
			md_wdt_irq_handler(MT_MD_WDT2_IRQ_ID);
			#else
			printf("ignore MD2 WDT\n");
			#endif
			mt_irq_ack(MT_MD_WDT2_IRQ_ID);
			mt_irq_unmask(MT_MD_WDT2_IRQ_ID);
		}
		break;
	default:
		break;
	}
}

void pmic_init_sequence(void)
{
	  // Copy from Jade kernel code: alps\kernel-3.10\drivers\misc\mediatek\power\mt6755 =====
	  // provided by Argus
    pmic_config_interface(0x8,0x1,0x1,0);
    pmic_config_interface(0xA,0x1,0x1,1);
    pmic_config_interface(0xA,0x1,0x1,2);
    pmic_config_interface(0xA,0x1,0x1,3);
    pmic_config_interface(0xA,0x1,0x1,4);
    pmic_config_interface(0xA,0x1,0x1,5);
    pmic_config_interface(0xA,0x1,0x1,7);
    pmic_config_interface(0xA,0x1,0x1,8);
    pmic_config_interface(0xA,0x1,0x1,10);
    pmic_config_interface(0xA,0x1,0x1,11);
    pmic_config_interface(0xA,0x1,0x1,12);
    pmic_config_interface(0xA,0x1,0x1,13);
    pmic_config_interface(0xA,0x1,0x1,14);
    pmic_config_interface(0xA,0x1,0x1,15);
    pmic_config_interface(0x18,0x1,0x1,5);
    pmic_config_interface(0x1C,0x1,0x1,7);
    pmic_config_interface(0x1E,0x1,0x1,0);
    pmic_config_interface(0x1E,0x1,0x1,1);
    pmic_config_interface(0x2C,0x1,0x1,15);
    pmic_config_interface(0x32,0x1,0x1,2);
    pmic_config_interface(0x32,0x1,0x1,3);
    pmic_config_interface(0x204,0x1,0x1,4);
    pmic_config_interface(0x204,0x1,0x1,5);
    pmic_config_interface(0x204,0x1,0x1,6);
    pmic_config_interface(0x226,0x1,0x1,0);
    pmic_config_interface(0x228,0x1,0x1,0);
    pmic_config_interface(0x228,0x1,0x1,1);
    pmic_config_interface(0x228,0x1,0x1,2);
    pmic_config_interface(0x228,0x1,0x1,3);
    pmic_config_interface(0x240,0x1,0x1,2);
    pmic_config_interface(0x240,0x1,0x1,3);
    pmic_config_interface(0x246,0x1,0x1,13);
    pmic_config_interface(0x246,0x1,0x1,14);
    pmic_config_interface(0x25E,0x1,0x1,9);
    pmic_config_interface(0x282,0x0,0x1,10);
    pmic_config_interface(0x282,0x0,0x1,11);
    pmic_config_interface(0x410,0x8,0x3F,8);
    pmic_config_interface(0x414,0x3,0x3,4);
    pmic_config_interface(0x436,0x0,0x3,2);
    pmic_config_interface(0x450,0xF,0xF,11);
    pmic_config_interface(0x452,0x1,0x1,3);
    pmic_config_interface(0x45C,0x1,0x1,1);
    pmic_config_interface(0x45E,0x400,0xFFFF,0);
    pmic_config_interface(0x464,0xF,0xF,11);
    pmic_config_interface(0x466,0x1,0x1,3);
    pmic_config_interface(0x470,0x1,0x1,1);
    pmic_config_interface(0x472,0x400,0xFFFF,0);
    pmic_config_interface(0x478,0xF,0xF,11);
    pmic_config_interface(0x47A,0x2,0x7,6);
    pmic_config_interface(0x47E,0x1,0x1,1);
    pmic_config_interface(0x480,0x1,0x7,4);
    pmic_config_interface(0x484,0x1,0x1,1);
    pmic_config_interface(0x48C,0xF,0xF,11);
    pmic_config_interface(0x48E,0x2,0x7,6);
    pmic_config_interface(0x492,0x1,0x1,1);
    pmic_config_interface(0x494,0x1,0x7,4);
    pmic_config_interface(0x498,0x1,0x1,1);
    pmic_config_interface(0x4A0,0xF,0xF,11);
    pmic_config_interface(0x4A2,0x2,0x7,6);
    pmic_config_interface(0x4A6,0x1,0x1,1);
    pmic_config_interface(0x4A8,0x1,0x7,4);
    pmic_config_interface(0x4AC,0x1,0x1,1);
    pmic_config_interface(0x4B6,0x4,0x7,6);
    pmic_config_interface(0x4C2,0x10,0xFFFF,0);
    pmic_config_interface(0x4CE,0x1,0x1,1);
    pmic_config_interface(0x4D0,0x1,0x7,4);
    pmic_config_interface(0x4DC,0x3,0x3,0);
    pmic_config_interface(0x4DC,0x0,0x1,10);
    pmic_config_interface(0x4E0,0x0,0x3,14);
    pmic_config_interface(0x4E2,0x81,0xFF,8);
    pmic_config_interface(0x600,0x1,0x1,1);
    pmic_config_interface(0x606,0x11,0x7F,0);
    pmic_config_interface(0x606,0x4,0x7F,8);
    pmic_config_interface(0x60C,0x10,0x7F,0);
    pmic_config_interface(0x612,0x3,0x3,0);
    pmic_config_interface(0x612,0x1,0x1,8);
    pmic_config_interface(0x61A,0x11,0x7F,0);
    pmic_config_interface(0x61A,0x4,0x7F,8);
    pmic_config_interface(0x620,0x10,0x7F,0);
    pmic_config_interface(0x626,0x3,0x3,0);
    pmic_config_interface(0x626,0x1,0x1,8);
    pmic_config_interface(0x628,0x1,0x1,1);
    pmic_config_interface(0x62E,0x11,0x7F,0);
    pmic_config_interface(0x62E,0x4,0x7F,8);
    //if (mt_get_chip_sw_ver() == CHIP_SW_VER_02) // For E2 code
	  //    ret = pmic_config_interface(0x632,0x30,0x7F,0);
    pmic_config_interface(0x634,0x10,0x7F,0);
    pmic_config_interface(0x63A,0x3,0x3,0);
    pmic_config_interface(0x63A,0x1,0x3,4);
    pmic_config_interface(0x63A,0x1,0x1,8);
    pmic_config_interface(0x63C,0x1,0x1,1);
    pmic_config_interface(0x642,0x11,0x7F,0);
    pmic_config_interface(0x642,0x4,0x7F,8);
    pmic_config_interface(0x646,0x30,0x7F,0);
    pmic_config_interface(0x648,0x10,0x7F,0);
    pmic_config_interface(0x64E,0x3,0x3,0);
    pmic_config_interface(0x64E,0x1,0x3,4);
    pmic_config_interface(0x64E,0x1,0x1,8);
    pmic_config_interface(0x650,0x1,0x1,1);
    pmic_config_interface(0x656,0x11,0x7F,0);
    pmic_config_interface(0x656,0x4,0x7F,8);
    pmic_config_interface(0x65A,0x40,0x7F,0);
    pmic_config_interface(0x65C,0x10,0x7F,0);
    pmic_config_interface(0x662,0x3,0x3,0);
    pmic_config_interface(0x662,0x1,0x3,4);
    pmic_config_interface(0x662,0x1,0x1,8);
    pmic_config_interface(0x676,0x1,0x1,8);
    pmic_config_interface(0x68A,0x1,0x1,8);
    pmic_config_interface(0x692,0x0,0x7F,0);
    pmic_config_interface(0x692,0x1,0x7F,8);
    pmic_config_interface(0x69E,0x0,0x3,0);
    pmic_config_interface(0x6A0,0x1,0x1,1);
    pmic_config_interface(0x6A6,0x11,0x7F,0);
    pmic_config_interface(0x6A6,0x4,0x7F,8);
    pmic_config_interface(0x6AC,0x35,0x7F,0);
    pmic_config_interface(0x6B2,0x3,0x3,0);
    pmic_config_interface(0x6B2,0x1,0x3,4);
    pmic_config_interface(0x6B2,0x1,0x1,8);
    pmic_config_interface(0xA02,0x1,0x1,9);
    pmic_config_interface(0xA04,0x1,0x1,3);
    pmic_config_interface(0xA04,0x0,0x7,11);
    pmic_config_interface(0xA06,0x1,0x1,9);
    pmic_config_interface(0xA08,0x1,0x1,3);
    pmic_config_interface(0xA08,0x1,0x7,11);
    pmic_config_interface(0xA0A,0x1,0x1,9);
    pmic_config_interface(0xA0E,0x1,0x1,9);
    pmic_config_interface(0xA14,0x1,0x1,9);
    pmic_config_interface(0xA18,0x1,0x1,9);
    pmic_config_interface(0xA1E,0x1,0x1,9);
    pmic_config_interface(0xA24,0x1,0x1,9);
    pmic_config_interface(0xA2A,0x1,0x1,9);
    pmic_config_interface(0xA30,0x1,0x1,9);
    pmic_config_interface(0xA36,0x1,0x1,9);
    pmic_config_interface(0xA3C,0x1,0x1,9);
    pmic_config_interface(0xA42,0x1,0x1,9);
    pmic_config_interface(0xA46,0x1,0x1,3);
    pmic_config_interface(0xA46,0x1,0x7,11);
    pmic_config_interface(0xA48,0x1,0x1,9);
    pmic_config_interface(0xA4E,0x1,0x1,9);
    pmic_config_interface(0xA54,0x1,0x1,9);
    pmic_config_interface(0xA5A,0x1,0x1,9);
    pmic_config_interface(0xA60,0x1,0x1,9);
    pmic_config_interface(0xA66,0x1,0x1,9);
    pmic_config_interface(0xA68,0x1,0x1,3);
    pmic_config_interface(0xA68,0x1,0x7,11);
    pmic_config_interface(0xA6A,0x1,0x1,9);
    pmic_config_interface(0xA70,0x1,0x1,9);
    pmic_config_interface(0xA74,0x1,0x1,1);
    pmic_config_interface(0xA74,0x1,0x1,2);
    pmic_config_interface(0xA74,0x0,0x1,3);
    pmic_config_interface(0xA74,0x0,0x7,5);
    pmic_config_interface(0xA76,0x1,0x1,9);
    pmic_config_interface(0xA7C,0x1,0x1,9);
    pmic_config_interface(0xA82,0x1,0x1,9);
    pmic_config_interface(0xA86,0x0,0x1,3);
    pmic_config_interface(0xA88,0x1,0x1,9);
    pmic_config_interface(0xA8E,0x1,0x1,9);
    pmic_config_interface(0xA94,0x1,0x1,9);
    pmic_config_interface(0xA9E,0x1,0x1,9);
    pmic_config_interface(0xAAC,0x1,0x1,9);
    pmic_config_interface(0xB10,0x2,0x7,8);
    pmic_config_interface(0xB24,0xF0,0xFF,2);
    pmic_config_interface(0xCC4,0x1,0x1,8);
    pmic_config_interface(0xCC4,0x1,0x1,9);
    pmic_config_interface(0xCC8,0x1F,0xFFFF,0);
    pmic_config_interface(0xCCA,0x14,0xFF,0);
    pmic_config_interface(0xCCC,0xFF,0xFF,8);
    pmic_config_interface(0xCE2,0x1,0x7FFF,0);
    pmic_config_interface(0xCE4,0xBCAC,0xFFFF,0);
    pmic_config_interface(0xEA2,0x0,0x1,13);
    pmic_config_interface(0xEA2,0x0,0x1,14);
    pmic_config_interface(0xEA2,0x0,0x1,15);
    pmic_config_interface(0xEAA,0x83,0xFFF,0);
    pmic_config_interface(0xEAA,0x1,0x1,13);
    pmic_config_interface(0xEAA,0x1,0x1,15);
    pmic_config_interface(0xEB2,0x1,0x3,4);
    pmic_config_interface(0xEB2,0x3,0x3,6);
    pmic_config_interface(0xEB2,0x1,0x3,8);
    pmic_config_interface(0xEB2,0x1,0x3,10);
    pmic_config_interface(0xEB2,0x1,0x3,12);
    pmic_config_interface(0xEB2,0x2,0x3,14);
    pmic_config_interface(0xEB4,0x1,0x3,0);
    pmic_config_interface(0xEB4,0x1,0x3,2);
    pmic_config_interface(0xEB4,0x1,0x3,4);
    pmic_config_interface(0xEB4,0x3,0x3,6);
    pmic_config_interface(0xEC6,0x1,0x1,14);
    pmic_config_interface(0xF16,0x40,0x3FF,0);
    pmic_config_interface(0xF16,0x1,0x1,15);
    pmic_config_interface(0xF1C,0x40,0x3FF,0);
    pmic_config_interface(0xF1C,0x1,0x1,15);
    pmic_config_interface(0xF20,0x1,0x1,2);
    pmic_config_interface(0xF7A,0xB,0xF,4);
    pmic_config_interface(0xF84,0x4,0xF,1);
    pmic_config_interface(0xF92,0x3,0xF,0);
    pmic_config_interface(0xFA0,0x1,0x1,1);
    pmic_config_interface(0xFA4,0x0,0x7,4);
    pmic_config_interface(0xFAA,0x1,0x1,2);
    pmic_config_interface(0xFAA,0x1,0x1,6);
    pmic_config_interface(0xFAA,0x1,0x1,7);
}

//=============================================
// MD1 PMIC setting
void md1_pmic_setting(void)
{
	pmic_config_interface(MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_ON, 0x40, 0x7F, 0); //E1 1.0V; offset:0x65A
	pmic_config_interface(MT6351_PMIC_BUCK_VMODEM_VOSEL_ON, 0x40, 0x7F, 0); // 1.0V; offset: 0x632
	pmic_config_interface(MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_CTRL, 1, 1, 1); // HW mode, bit[1]; offset: 0x650 
	pmic_config_interface(MT6351_PMIC_BUCK_VMD1_VOSEL_CTRL, 1, 1, 1); // HW mode, bit[1]; offset: 0x63C
	pmic_config_interface(MT6351_PMIC_BUCK_VMODEM_VOSEL_CTRL, 1, 1, 1); // HW mode, bit[1]; offset: 0x628
}

//=============================================
// MD1 pll init
void md1_pll_on(void)
{
	printf("md1_pll_on\n");
	/* Make md1 208M CG off, switch to software mode */
	*((volatile unsigned int *)0x20150020) |= (0x1<<26); /* turn off mdpll1 cg */
	*((volatile unsigned int *)0x20140010) |= (0x1<<16); /* let mdpll on ctrl into software mode */
	*((volatile unsigned int *)0x20140014) |= (0x1<<16); /* let mdpll enable into software mode */

	/* Turn on 208M */
	*MDPLL1_CON0 |= 0x00000001;
	DRV_WriteReg32(AP_PLL_CON0, (DRV_Reg32(AP_PLL_CON0)|(0x1<<1)));

	udelay(200);

	/* close 208M and autoK */
	*MDPLL1_CON0 &= 0xfffffffe;
	*MDPLL1_CON0 &= 0xffffff7f;

	*((volatile unsigned int *)0x20140010) &= ~(0x1<<16); /* let mdpll on ctrl into hardware mode */
	*((volatile unsigned int *)0x20140014) &= ~(0x1<<16); /* let mdpll enable into hardware mode */
	*((volatile unsigned int *)0x20150020) &= ~(0x1<<26); /* turn on mdpll1 cg */

	*MDPLL1_CON0 &= 0xfffffdff; /* set mdpll control by md1 and c2k */
}

void md1_pll_init(void)
{

	md1_pll_on();

	//enable L1 permission
	*R_L1_PMS |= 0x7;

	// modify PSMCU2EMI bus divider from 3 to 4.
	*R_PSMCU_AO_CLK_CTL = *R_PSMCU_AO_CLK_CTL | 0x83;
		
	*R_L1MCU_PWR_AWARE |=	(1<<16); //lock dcm bus
	*R_L1AO_PWR_AWARE |=	(1<<16); //lock dcm bus
	//busL2 DCM div 8/normal div 1/ clkslow_en/clock from PLL /debounce enable/ debounce time 7T 
	//*R_BUSL2DCM_CON3  = 0x0001FDE7; //L2DCM L1BUS div 16  v1.5
	//*R_BUSL2DCM_CON3  = 0x1001FDE7; //toggle setting // V1.5
	//*R_L1MCU_DCM_CON  = 0x0003FDE7; // DCM div 16/normal div 1/clkslow_en/ clock from PLL / dcm enable /debounce enable /debounce time 15T // V1.5
	*R_BUSL2DCM_CON3  = 0x0000FDE7; //L2DCM L1BUS div 16
	*R_BUSL2DCM_CON3  = 0x1000FDE7; //toggle setting
	*R_L1MCU_DCM_CON  = 0x0001FDE7; // DCM div 8/normal div 1/clkslow_en/ clock from PLL / dcm enable /debounce enable /debounce time 15T
	*R_L1MCU_DCM_CON2 = 0x00000000; //DCM config toggle = 0
	*R_L1MCU_DCM_CON2 = 0x80000000; //DCM config toggle  = 1 / old
	//*R_L1MCU_DCM_CON2 = 0x90000000; //DCM config toggle  = 1 / 
	//*R_L1MCU_PWR_AWARE &=	(1<<16); //un-lock dcm bus
	//*R_L1AO_PWR_AWARE &=	(1<<16); //un-lock dcm bus

	/*Wait PSMCU PLL ready*/
	printf("Wait PSMCU PLL ready\n");
	while ((*R_PLL_STS & 0x1) !=0x1); 		  	// Bit  0: PSMCUPLL_RDY
	printf("Got it\n");
	/*Switch clock, 0: 26MHz, 1: PLL*/
	*R_CLKSEL_CTL |=0x2;					// Bit	17: RF1_CKSEL
								// Bit	16: INTF_CKSEL
								// Bit	15: MD2G_CKSEL
								// Bit	14: DFE_CKSEL
								// Bit	13: CMP_CKSEL
								// Bit	12: ICC_CKSEL
								// Bit	11: IMC_CKSEL
								// Bit	10: EQ_CKSEL
								// Bit	9: BRP_CKSEL
								// Bit	8: L1MCU_CKSEL
								// Bit	6-5: ATB_SRC_CKSEL
								// Bit	4: ATB_CKSEL
								// Bit	3: DBG_CKSEL
								// Bit	2: ARM7_CKSEL
								// Bit	1: PSMCU_CKSEL
								// Bit	0: BUS_CKSEL

	/*Wait L1MCU PLL ready*/
	printf("Wait L1MCU PLL ready\n");
	while ((*R_PLL_STS & 0x2) != 0x2);		  	// Bit  1: L1MCUPLL_RDY
	printf("Got it\n");
	*R_CLKSEL_CTL |= 0x100;					// Bit	8: L1MCU_CK = L1MCUPLL
  
	/*DFE/CMP/ICC/IMC clock src select*/
	*R_FLEXCKGEN_SEL1 = 0x30302020;          		// Bit 29-28 DFE_CLK src = DFEPLL
	  							// Bit 21-20 CMP_CLK src = DFEPLL
	  							// Bit 13-12 ICC_CLK src = IMCPLL
	  							// Bit 5-4   IMC_CLK src = IMCPLL


	/*IMC/MD2G clock src select */
	*R_FLEXCKGEN_SEL2 = 0x00002030;          // Bit 13-12 INTF_CLK src = IMCPLL
      										   // Bit 5-4  MD2G_CLK src = DFEPLL

	/*Wait DFE/IMC PLL ready*/
	printf("Wait DFE/IMC PLL ready\n");
	while ((*R_PLL_STS & 0x90) !=0x90); 		  // Bit  7: DFEPLL_RDY
	                                              // Bit  4: IMCPLL_RDY
	printf("done\n");

	/*Wait L1SYS clock ready*/
	printf("Wait L1SYS clock ready\n");
	while ((*R_FLEXCKGEN_STS0 & 0x80800000) != 0x80800000);	// Bit  31: EQ_CK_RDY
								// Bit  23: BRP_CK_RDY
	printf("Done\n");

	printf("Wait R_FLEXCKGEN_STS1 & 0x80808080 ready\n");
	while ((*R_FLEXCKGEN_STS1 & 0x80808080) != 0x80808080);	// Bit  31: DFE_CK_RDY
								// Bit  23: CMP_CK_RDY	                                                              
								// Bit  15: ICC_CK_RDY	
								// Bit  7:  IMC_CK_RDY
	printf("Done\n");

	printf("Wait R_FLEXCKGEN_STS2 & 0x8080 ready\n");
	while ((*R_FLEXCKGEN_STS2 & 0x8080) != 0x8080);		// Bit  15: INTF_CK_RDY
								// Bit  23: MD2G_CK_RDY
	printf("Done\n");

	/*Switch L1SYS clock to PLL clock*/
	*R_CLKSEL_CTL |=0x3fe00;

	/*MD BUS/ARM7 clock src select */
	*R_FLEXCKGEN_SEL0 = 0x30203031;				// Bit  29-28: EQ_CLK src = EQPLL
	                                                        // Bit  26-24: EQ+DIVSEL, divided-by bit[2:0]+1
								// Bit  21-20: BRP_CLK src = IMCPLL
	                                            		// Bit  13-12: ARM7_CLK src = DFEPLL
	                          				// Bit  5-4: BUS_CLK src = EQPLL
	                                			// Bit  2-0: BUS_DIVSEL, divided-by bit[2:0]+1

	*MD_GLOBAL_CON_DUMMY = MD_PLL_MAGIC_NUM;
		#if 0
	/*PSMCU DCM*/
	*R_PSMCU_DCM_CTL0 |= 0x00F1F006;		  // Bit 26-20: DBC_CNT
							  //Bit 16-12: IDLE_FSEL
							  //Bit 2: DBC_EN
							  //Bit 1: DCM_EN
	*R_PSMCU_DCM_CTL1 |= 0x26;			  // Bit  5: APB_LOAD_TOG
							  // Bit  4-0: APB_SEL
	
	/*ARM7 DCM*/
	*R_ARM7_DCM_CTL0 |= 0x00F1F006;		// Bit 26-20: DBC_CNT
							  //Bit 16-12: IDLE_FSEL
							  //Bit 2: DBC_EN
							  //Bit 1: DCM_EN
	*R_ARM7_DCM_CTL1 |= 0x26; 			// Bit	5: APB_LOAD_TOG
							// Bit	4-0: APB_SEL
	
	*R_DCM_SHR_SET_CTL = 0x00014110;		  // Bit  16: BUS_PLL_SWITCH
							  // Bit  15: BUS_QUARTER_FREQ_SEL
							  // Bit  14: BUS_SLOW_FREQ
							  // Bit 12-8: HFBUS_SFSEL
							  // Bit   4-0: HFBUS_FSEL
	
	/*LTEL2 BUS DCM*/
	*R_LTEL2_BUS_DCM_CTL |= 0x1;			// Bit	0: DCM_EN

	/*MDDMA BUS DCM*/
	*R_MDDMA_BUS_DCM_CTL |= 0x1;			  // Bit  0: DCM_EN

	/*MDREG BUS DCM*/
	*R_MDREG_BUS_DCM_CTL |= 0x1;			  // Bit  0: DCM_EN

	/*MODULE BUS2X DCM*/
	*R_MODULE_BUS2X_DCM_CTL |= 0x1;		// Bit	0: DCM_EN

	/*MODULE BUS1X DCM*/
	*R_MODULE_BUS1X_DCM_CTL |= 0x1;			// Bit	0: DCM_EN

	/*MD perisys AHB master/slave DCM enable*/
	*R_MDINFRA_CKEN |= 0xC000001F;		// Bit	31: PSPERI_MAS_DCM_EN
							  // Bit  30: PSPERI_SLV_DCM_EN
							  // Bit  4: SOE_CKEN
							  // Bit  3: BUSREC_CKEN
							  // Bit  2: BUSMON_CKEN
							  // Bit  1: MDUART2_CKEN
							  // Bit  0: MDUART1_CKEN
	
	/*MD debugsys DCM enable*/
	*R_MDPERI_CKEN |= 0x8003FFFF; 		// Bit	31: MDDBGSYS_DCM_EN
							  // Bit  21: USB0_LINK_CK_SEL
							  // Bit  20: USB1_LINK_CK_SEL
							  // Bit  17: TRACE_CKEN
							  // Bit  16: MDPERI_MISCREG_CKEN
							  // Bit  15: PCCIF_CKEN
							  // Bit  14: MDEINT_CKEN
							  // Bit  13: MDCFGCTL_CKEN
							  // Bit  12: MDRGU_CKEN
							  // Bit  11: A7OST_CKEN
							  // Bit  10: MDOST_CKEN
							  // Bit  9: MDTOPSM_CKEN
							  // Bit  8: MDCIRQ_CKEN
							  // Bit  7: MDECT_CKEN
							  // Bit  6: USIM2_CKEN
							  // Bit  5: USIM1_CKEN
							  // Bit  4: GPTMLITE_CKEN
							  // Bit  3: MDGPTM_CKEN
							  // Bit  2: I2C_CKEN
							  // Bit  1: MDGDMA_CKEN
							  // Bit  0: MDUART0_CKEN
	
	/*SET MDRGU, MDTOPSM, MDOSTIMER, MDTOPSM DCM MASK*/
	*R_MDPERI_DCM_MASK |= 0x00001E00; 	  // Bit  12: MDRGU_DCM_MASK
							  // Bit  11: A7OST_DCM_MASK
							  // Bit  10: MDOST_DCM_MASK
							  // Bit  9: MDTOPSM_DCM_MASK
#endif
	*REG_DCM_PLLCK_SEL |= (1<<7); 	  // Bit  7: 0: clock do not from PLL, 1: clock from PLL							  

	// wait DCM config done, then switch BUS clock src to PLL
	printf("wait DCM config done\n");
	while ((*R_FLEXCKGEN_STS0 & 0x80) !=0x80); 		  // Bit  7: BUS_CK_RDY	 
	printf("done\n"); 													  
	*R_CLKSEL_CTL |=0x1;                                // Bit  1: BUS_CLK = EQPLL/2
}

//===========================================================
void c2k_boot(int boot_mode)
{
	unsigned int reg_value;

#if 1
	/* C2K CONFIGURE addr: 0x10001360 , Make C2K boot from 0 address at DRAM*/
	reg_value =*((volatile unsigned int*)0x10001360);
	reg_value &= (~(7<<8));
	reg_value |= (5<<8);
	*((volatile unsigned int*)0x10001360) = reg_value;
	printf("C2K[0x10001360]%x\n", *((volatile unsigned int*)0x10001360));
#endif

	/* Power on  C2K MTCMOS */
	/* Turn On SPM Reg Key*/
	*POWERON_CONFIG_EN = 0x0b160001;
	*C2K_PWR_CON |= (0x1 << PWR_ON);           // Assert pwr_on
	*C2K_PWR_CON |= (0x1 << PWR_ON_2ND);         // Assert pwr_on_2nd

	printf("C2K, wait PWR_SATUS and PWR_STATUS_2ND\n");
	while (
		((*PWR_STATUS & (0x1 << C2K)) != (0x1 << C2K)) ||
		((*PWR_STATUS_2ND & (0x1 << C2K)) != (0x1 << C2K))
	); // waiting for power ready
	printf("done\n");

	*C2K_PWR_CON &= ~(0x1 << PWR_CLK_DIS);       // Release clk_dis
	*C2K_PWR_CON &= ~(0x1 << PWR_ISO);           // Release pwr_iso
	*C2K_PWR_CON |= (0x1 << PWR_RST_B);          // Release pwr_rst_b

	/* AP config srcclkena selection mask (INFRA_AO, SLEEP_CON  register) */
	/* INFRA_MISC2 0x10001F0C */
	*INFRA_MISC2 |= 0x44;

	/* AP config ClkSQ register (APMixedsys register) */
	/* CLKSQ_LPF_EN (AP_PLL_CON0[1]) set to 1, @ 0x1000C000*/ 
	*AP_PLL_CON0 |= (0x1 << 1);
  
	/* AP Hold C2K ARM core	*/
	*C2K_CONFIG |= 0x1 << 1;

	/* AP_REQ_C2K, AP wakeup C2k */
	/* Enable C2K to Control MDPLL1 */
	*MDPLL1_CON0 &= 0XFFFFFDFF;
	/* Make sure release C2KSYS Reset */
	reg_value = *WDT_SWSYSRST;
	reg_value |= 0x88000000;
	reg_value &= ~(0x1<<15);
	*WDT_SWSYSRST = reg_value;

	/* AP Wakeup C2KSYS */
	*C2K_SPM_CTRL |= 0x1 << 1;
	printf("Wait C2K_STATUS\n");
	while (((*C2K_STATUS >> 1) & 0x1) == 0);
	printf("done\n");
	*C2K_SPM_CTRL &= ~(0x1 << 1);
	*INFRA_TOPAXI_PROTECTEN_1 &= 0xFE3FFFFF;
	/* Waiting for C2KSYS Bus ready for operation */
	printf("Waiting for C2KSYS Bus ready for operation\n");
	do {
		reg_value = *C2K_CGBR1;
	} while (reg_value != 0xFE8);
	printf("Done\n");

	/* C2K PLL init */
	*(UINT16P)C2K_C2K_PLL_CON3 = 0x8805; // CPPLL/DSPPLL ISO_EN -> hw mode
	*(UINT16P)C2K_C2K_PLL_CON3 = 0x0005; // CPPLL/DSPPLL PWR_ON -> hw mode
	*(UINT16P)C2K_C2K_PLL_CON3 = 0x0001; // MDPLL1 EN -> hw mode
	*(UINT16P)C2K_C2K_PLL_CON2 = 0x0000; // CPPLL/DSPPLL/C2KPLL_SUBPLL1/2 EN -> hw mode
	*(UINT16P)C2K_C2K_PLLTD_CON0 = 0x0010;//  bp_pll_dly -> 0

	*(UINT16P)C2K_C2K_CPPLL_CON0 |= 1 << 15;
	*(UINT16P)C2K_C2K_DSPPLL_CON0 |= 1 << 15;
	*(UINT16P)C2K_C2K_C2KPLL1_CON0 |= 1 << 15;

	/* Wait 20us */
	udelay(20);

	*(UINT16P)C2K_CG_ARM_AMBA_CLKSEL = 0xC124;
	*(UINT16P)C2K_CLK_CTRL4 = 0x8e43;
	*(UINT16P)C2K_CLK_CTRL9 = 0xA207;

	
}
