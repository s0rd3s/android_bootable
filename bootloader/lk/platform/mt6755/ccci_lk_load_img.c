//#ifdef LOAD_MD_AT_LK
#if 1
#include <sys/types.h>
#include <stdint.h>
#include <platform/partition.h>
#include <platform/mt_typedefs.h>
#include <platform/boot_mode.h>
#include <platform/mt_reg_base.h>
#include <platform/errno.h>
#include <printf.h>
#include <string.h>
#include <malloc.h>
#include <libfdt.h>
#include <platform/mt_gpt.h>
#include <platform/emi_mpu.h>
#define MODULE_NAME "LK_LD_MD"
/* Log part */
#define ALWAYS_LOG(fmt, args...) do {dprintf(ALWAYS, fmt, ##args);} while (0)
#define DBG_LOG(fmt, args...) do {dprintf(INFO, fmt, ##args);} while (0)

/***************************************************************************************************
** Option setting part
***************************************************************************************************/
#define ENABLE_EMI_PROTECTION
//#define ENABLE_DBG_LOG
//#define ENABLE_DT_DEB_LOG

/***************************************************************************************************
** Common part
***************************************************************************************************/
static unsigned int md_load_status_flag; /* MD load sucess flags */

static unsigned int ap_md1_smem_size_at_lk_env;
static unsigned int md1_md3_smem_size_at_lk_env;
static unsigned int ap_md3_smem_size_at_lk_env;

static unsigned int ap_md1_smem_size_at_img;
static unsigned int md1_md3_smem_size_at_img;
static unsigned int ap_md3_smem_size_at_img;
#define AP_MD1_SMEM_SIZE    0x200000
#define MD1_MD3_SMEM_SIZE   0x200000
#define AP_MD3_SMEM_SIZE    0x200000
#define MAX_SMEM_SIZE       0x8000000/*Change from 6M to 128M,0x600000*/
static int md_reserve_mem_size_fixup(void *fdt);
static int ccci_load_raw_data(char *part_name, unsigned char *mem_addr, unsigned int offset, int size)
{
	part_dev_t *dev;
	part_t *part;
	int len;
	#ifdef MTK_EMMC_SUPPORT
	u64 storage_start_addr;
	#else
	unsigned long storage_start_addr;
	#endif

	printf("load_md_partion_hdr, name[%s], mem_addr[%p], offset[%x], size[%x]\n", part_name, mem_addr, offset, size);

	dev = mt_part_get_device();
	if (!dev)
	{    
		printf("load_md_partion_hdr, dev = NULL\n");
		return -ENODEV;
	}

	part = mt_part_get_partition(part_name);
	if (!part)
	{
		printf("mt_part_get_partition (%s), part = NULL\n",part_name);
		return -ENOENT;
	}

	#ifdef MTK_EMMC_SUPPORT
	storage_start_addr = (u64)part->start_sect * BLK_SIZE;
	#else
	storage_start_addr = part->start_sect * BLK_SIZE;
	#endif
	printf("image[%s](%d) addr is 0x%llx in partition\n", part_name, offset, (u64)storage_start_addr);
	printf(" > to 0x%llx (at dram), size:0x%x, part id:%d\n",(u64)((unsigned long)mem_addr), size, part->part_id);

	#if defined(MTK_EMMC_SUPPORT) && defined(MTK_NEW_COMBO_EMMC_SUPPORT)
	len = dev->read(dev, storage_start_addr+offset, mem_addr, size, part->part_id);
	#else
	len = dev->read(dev, storage_start_addr+offset, mem_addr, size);
	#endif

	if (len < 0) {
		printf("[%s] %s boot image header read error. LINE: %d\n", MODULE_NAME, part_name, __LINE__);
		return -1;
	}

	return len;
}

extern BOOT_ARGUMENT *g_boot_arg;
static void *ccci_request_mem(unsigned int mem_size, unsigned long align)
{
	void *resv_addr;
	resv_addr = (void *)((unsigned long)mblock_reserve(&g_boot_arg->mblock_info, mem_size, align, 0x100000000L, RANKMAX));
    	printf("request size: 0x%08x, get start address: %p\n", mem_size, resv_addr);
	return resv_addr;
}

static int ccci_resize_reserve_mem(unsigned char *addr, int old_size, int new_size)
{
	return mblock_resize(&g_boot_arg->mblock_info, &g_boot_arg->orig_dram_info, (u64)(unsigned long)addr,
				(u64)old_size, (u64)new_size);
}

extern int mblock_create(mblock_info_t *mblock_info, dram_info_t *orig_dram_info, u64 addr, u64 size);
static int ccci_retrieve_mem(unsigned char *addr, int size)
{
	return mblock_create(&g_boot_arg->mblock_info, &g_boot_arg->orig_dram_info, (u64)(unsigned long)addr,(u64)size);
}

/* modem check header */
struct md_check_header_comm {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	  /* header structure version number */
	unsigned int  product_ver;	   /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;		/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];	  /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	 /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id;	   /* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	unsigned char ext_attr;		  /* no shrink: 0, shrink: 1*/
	unsigned char reserved[2];	   /* for reserved */

	unsigned int  mem_size;		  /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;	   /* md image size, exclude head size*/
}__attribute__((packed));

struct md_check_header_v1 {
	struct md_check_header_comm common; /* common part */
	unsigned int  ap_md_smem_size;     /* share memory size */
	unsigned int  size;           /* the size of this structure */
} __attribute__((packed));

struct md_check_header_v3 {
	struct md_check_header_comm common; /* common part */
	unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	unsigned char reserved2[88];

	unsigned int  size;			  /* the size of this structure */
}__attribute__((packed));

typedef struct _md_regin_info {
	unsigned int region_offset;
	unsigned int region_size;
}md_regin_info;

struct md_check_header_v4 {
	struct md_check_header_comm common; /* common part */
	unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	
	unsigned int  region_num;	 /* total region number */
	md_regin_info region_info[8]; 	 /* max support 8 regions */
	unsigned int  domain_attr[4];	 /* max support 4 domain settings, each region has 4 control bits*/	 

	unsigned char reserved2[4];

	unsigned int  size;			  /* the size of this structure */
}__attribute__((packed));

typedef struct _free_padding_block {
	unsigned int start_offset;
	unsigned int length;
} free_padding_block_t;

struct md_check_header_v5 {
	struct md_check_header_comm common; /* common part */
	unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	
	unsigned int  region_num;	 /* total region number */
	md_regin_info region_info[8]; 	 /* max support 8 regions */
	unsigned int  domain_attr[4];	 /* max support 4 domain settings, each region has 4 control bits*/	 

	unsigned int  arm7_img_offset;
	unsigned int  arm7_img_size;

	free_padding_block_t padding_blk[3];
	unsigned int  ap_md_smem_size;
	unsigned int  md_to_md_smem_size;
	unsigned char reserved_1[24];

	unsigned int  size; /* the size of this structure */
};

/* dsp check header */
struct dsp_check_header {
	unsigned char check_header[8];
	unsigned char file_info_H[12];
	unsigned char unknown[52];
	unsigned char chip_id[16];
	unsigned char dsp_info[48];
};

static void *ccci_get_md_header(void *tail_addr, int *header_ver)
{
	int size;
	void *header_addr = NULL;
	printf("ccci_get_md_header tail_addr[%p]\n", tail_addr);
	if (!tail_addr)
		return NULL;

	size = *((unsigned int *)(tail_addr - 4));
	printf("ccci_get_md_header check headr v1(%d), v3(%d), v5(%d), curr(%d)\n",
		sizeof(struct md_check_header_v1), sizeof(struct md_check_header_v3),
		sizeof(struct md_check_header_v5), size);

	if (size == sizeof(struct md_check_header_v1)) {
		header_addr = tail_addr - sizeof(struct md_check_header_v1);
		*header_ver = 1;
	} else if (size == sizeof(struct md_check_header_v3)) {
		header_addr = tail_addr - sizeof(struct md_check_header_v3);
		*header_ver = 3;
	} else if (size == sizeof(struct md_check_header_v5)) {
		header_addr = tail_addr - sizeof(struct md_check_header_v5);
		*header_ver = 5;
	}

	return header_addr;
}

static int ccci_md_header_parser(void *header_addr, int ver)
{
	#ifdef ENABLE_DBG_LOG
	//int size;
	struct md_check_header_v1 *v1_hdr;
	struct md_check_header_v5 *v5_hdr;
	struct md_check_header_v3 *v3_hdr;
	int i;
	if (!header_addr)
		return -1;

	switch(ver) {
	case 1:
		/* parsing md check header */
		v1_hdr = (struct md_check_header_v1 *)header_addr;
		printf("===== Dump v1 md header =====\n");
		printf("[%s] MD IMG  - Magic          : %s\n"    , MODULE_NAME, v1_hdr->common.check_header);
		printf("[%s] MD IMG  - header_verno   : 0x%08X\n", MODULE_NAME, v1_hdr->common.header_verno);
		printf("[%s] MD IMG  - product_ver    : 0x%08X\n", MODULE_NAME, v1_hdr->common.product_ver);
		printf("[%s] MD IMG  - image_type     : 0x%08X\n", MODULE_NAME, v1_hdr->common.image_type);
		printf("[%s] MD IMG  - mem_size       : 0x%08X\n", MODULE_NAME, v1_hdr->common.mem_size);
		printf("[%s] MD IMG  - md_img_size    : 0x%08X\n", MODULE_NAME, v1_hdr->common.md_img_size);
		printf("[%s] MD IMG  - ap_md_smem_size: 0x%08X\n", MODULE_NAME, v1_hdr->ap_md_smem_size);
		printf("=============================\n");
		break;
	case 3: /* V3 */
	case 4:
		/* parsing md check header */
		v3_hdr = (struct md_check_header_v3 *)header_addr;
		printf("===== Dump v3 md header =====\n");
		printf("[%s] MD IMG  - Magic          : %s\n"    , MODULE_NAME, v3_hdr->common.check_header);
		printf("[%s] MD IMG  - header_verno   : 0x%08X\n", MODULE_NAME, v3_hdr->common.header_verno);
		printf("[%s] MD IMG  - product_ver    : 0x%08X\n", MODULE_NAME, v3_hdr->common.product_ver);
		printf("[%s] MD IMG  - image_type     : 0x%08X\n", MODULE_NAME, v3_hdr->common.image_type);
		printf("[%s] MD IMG  - mem_size       : 0x%08X\n", MODULE_NAME, v3_hdr->common.mem_size);
		printf("[%s] MD IMG  - md_img_size    : 0x%08X\n", MODULE_NAME, v3_hdr->common.md_img_size);
		printf("[%s] MD IMG  - dsp offset     : 0x%08X\n", MODULE_NAME, v3_hdr->dsp_img_offset);
		printf("[%s] MD IMG  - dsp size       : 0x%08X\n", MODULE_NAME, v3_hdr->dsp_img_size);
		printf("[%s] MD IMG  - rpc_sec_mem_addr : 0x%08X\n", MODULE_NAME, v3_hdr->rpc_sec_mem_addr);
		printf("=============================\n");
		break;
	case 5:
		/* parsing md check header */
		v5_hdr = (struct md_check_header_v5 *)header_addr;
		printf("===== Dump v5 md header =====\n");
		printf("[%s] MD IMG  - Magic          : %s\n"    , MODULE_NAME, v5_hdr->common.check_header);
		printf("[%s] MD IMG  - header_verno   : 0x%08X\n", MODULE_NAME, v5_hdr->common.header_verno);
		printf("[%s] MD IMG  - product_ver    : 0x%08X\n", MODULE_NAME, v5_hdr->common.product_ver);
		printf("[%s] MD IMG  - image_type     : 0x%08X\n", MODULE_NAME, v5_hdr->common.image_type);
		printf("[%s] MD IMG  - mem_size       : 0x%08X\n", MODULE_NAME, v5_hdr->common.mem_size);
		printf("[%s] MD IMG  - md_img_size    : 0x%08X\n", MODULE_NAME, v5_hdr->common.md_img_size);
		printf("[%s] MD IMG  - dsp offset     : 0x%08X\n", MODULE_NAME, v5_hdr->dsp_img_offset);
		printf("[%s] MD IMG  - dsp size       : 0x%08X\n", MODULE_NAME, v5_hdr->dsp_img_size);
		printf("[%s] MD IMG  - rpc_sec_mem_addr : 0x%08X\n", MODULE_NAME, v5_hdr->rpc_sec_mem_addr);
		printf("[%s] MD IMG  - armv7 offset   : 0x%08X\n", MODULE_NAME, v5_hdr->arm7_img_offset);
		printf("[%s] MD IMG  - armv7 size     : 0x%08X\n", MODULE_NAME, v5_hdr->arm7_img_size);
		printf("[%s] MD IMG  - region num     : 0x%08X\n", MODULE_NAME, v5_hdr->region_num);
		printf("[%s] MD IMG  - ap_md_smem_size: 0x%08X\n", MODULE_NAME, v5_hdr->ap_md_smem_size);
		printf("[%s] MD IMG  - md_md_smem_size: 0x%08X\n", MODULE_NAME, v5_hdr->md_to_md_smem_size);
		for (i = 0; i < 8; i++) {
 			printf("[%s] MD IMG  - region[%d] off : 0x%08X\n", MODULE_NAME, i,
								v5_hdr->region_info[i].region_offset);
			printf("[%s] MD IMG  - region[%d] size: 0x%08X\n", MODULE_NAME, i,
								v5_hdr->region_info[i].region_size);
		}
		for (i = 0; i < 4; i++) {
 			printf("[%s] MD IMG  - domain_attr[%d] : 0x%08X\n", MODULE_NAME, i,
								v5_hdr->domain_attr[i]);
		}
		printf("=============================\n");
		break;
	default:
		break;
	}
	return 0;
	#else
	return 0;
	#endif
}

/* Parameters from LK to modem */
typedef struct _modem_info
{
	unsigned long long base_addr;
	unsigned int size;
	char md_id;
	char errno;
	char md_type;
	char ver;
	unsigned int reserved[2];
}modem_info_t;

static modem_info_t header_tbl[4];
static int hdr_count;

#define MAX_LK_INFO_SIZE	(0x10000)
#define CCCI_TAG_NAME_LEN	(16)
typedef struct _ccci_lk_info
{
	unsigned long long lk_info_base_addr;
	unsigned int       lk_info_size;
	unsigned int       lk_info_tag_num;
}ccci_lk_info_t;
typedef struct _ccci_tag
{
	char tag_name[CCCI_TAG_NAME_LEN];
	unsigned int data_offset;
	unsigned int data_size;
	unsigned int next_tag_offset;
}ccci_tag_t; 
static ccci_lk_info_t ccci_lk_inf;
static unsigned int curr_offset;

static void ccci_lk_info_init(unsigned long long base_addr)
{
	ccci_lk_inf.lk_info_base_addr = base_addr;
	ccci_lk_inf.lk_info_size = 0;
	ccci_lk_inf.lk_info_tag_num = 0;
	curr_offset = 0;
}

static int insert_ccci_tag_inf(char *name, char *data, unsigned int size)
{
	int i;
	ccci_tag_t *tag = (ccci_tag_t *)((unsigned long)(ccci_lk_inf.lk_info_base_addr + curr_offset));
	char* buf = (char *)((unsigned long)(ccci_lk_inf.lk_info_base_addr + curr_offset + sizeof(ccci_tag_t)));
	int total_size = (size + sizeof(ccci_tag_t) + 7)&(~7); /* make sure 8 bytes align */

	if (total_size >= MAX_LK_INFO_SIZE) {
		printf("not enought memory to insert(%d)\n", MAX_LK_INFO_SIZE - total_size);
		return -1;
	}

	/* Copy name */
	for (i=0; i<CCCI_TAG_NAME_LEN-1; i++) {
		if (name[i] == 0) {
			tag->tag_name[i] = 0;
			break;
		}
		tag->tag_name[i] = name[i];
	}

	/* Set offset */
	tag->data_offset = curr_offset + sizeof(ccci_tag_t);
	/* Set data size */
	tag->data_size = size;
	/* Set next offset */
	tag->next_tag_offset = curr_offset + total_size;
	/* Copy data */
	memcpy(buf, data, size);

	/* update control structure */
	ccci_lk_inf.lk_info_size += total_size;
	ccci_lk_inf.lk_info_tag_num++;
	curr_offset += total_size;

	printf("tag insert(%d), [name]:%s [4 bytes]:%x [size]:%d\n", ccci_lk_inf.lk_info_tag_num, name, *(unsigned int *)buf, size);

	return 0;
}

static int find_ccci_tag_inf(char *name, char *buf, unsigned int size)
{
	unsigned int i;
	ccci_tag_t *tag;
	unsigned int cpy_size;

	tag = (ccci_tag_t *)((unsigned long)ccci_lk_inf.lk_info_base_addr);
	for (i=0; i<ccci_lk_inf.lk_info_tag_num; i++) {
		if (strcmp(tag->tag_name, name) != 0) {
			tag = (ccci_tag_t *)((unsigned long)(ccci_lk_inf.lk_info_base_addr + tag->next_tag_offset));
			continue;
		}
		/* found it */
		cpy_size = size>tag->data_size?tag->data_size:size;
		memcpy(buf, (void*)((unsigned long)(ccci_lk_inf.lk_info_base_addr+tag->data_offset)), cpy_size);

		return cpy_size;;
	}
	return -1;
}

unsigned int *update_md_hdr_info_to_dt(unsigned int *ptr, void *fdt)
{
	int i;
	unsigned int *local_ptr;

	if (hdr_count == 0)
		return ptr;

	/* update hdr_count info */
	if (insert_ccci_tag_inf("hdr_count", (char*)&hdr_count, sizeof(unsigned int)) < 0)
		printf("insert hdr_count fail\n");

	/* update hdr tbl info */
	if (insert_ccci_tag_inf("hdr_tbl_inf", (char*)header_tbl, sizeof(modem_info_t)*hdr_count) < 0)
		printf("insert hdr_tbl_inf fail\n");

	local_ptr = (unsigned int *)&ccci_lk_inf;
	for(i = 0; i < (int)(sizeof(ccci_lk_info_t)/sizeof(unsigned int)); i++) {
		*ptr = local_ptr[i];
		ptr++;
	}

	/* update kernel dt */
	md_reserve_mem_size_fixup(fdt);

	return ptr;
}

static void insert_hdr_info(modem_info_t *hdr)
{
	header_tbl[hdr_count].base_addr = hdr->base_addr;
	header_tbl[hdr_count].size = hdr->size;
	header_tbl[hdr_count].errno = hdr->errno;
	header_tbl[hdr_count].md_id = hdr->md_id;
	header_tbl[hdr_count].ver = 0;
	header_tbl[hdr_count].md_type = hdr->md_type;
	hdr_count++;
}
		

/* image types */
#define IMG_TYPE_ID_OFFSET (0)
#define IMG_TYPE_RESERVED0_OFFSET (8)
#define IMG_TYPE_RESERVED1_OFFSET (16)
#define IMG_TYPE_GROUP_OFFSET (24)

#define IMG_TYPE_ID_MASK (0xffU << IMG_TYPE_ID_OFFSET)
#define IMG_TYPE_RESERVED0_MASK (0xffU << IMG_TYPE_RESERVED0_OFFSET)
#define IMG_TYPE_RESERVED1_MASK (0xffU << IMG_TYPE_RESERVED1_OFFSET)
#define IMG_TYPE_GROUP_MASK (0xffU << IMG_TYPE_GROUP_OFFSET)

#define IMG_TYPE_GROUP_AP (0x00U << IMG_TYPE_GROUP_OFFSET)
#define IMG_TYPE_GROUP_MD (0x01U << IMG_TYPE_GROUP_OFFSET)
#define IMG_TYPE_GROUP_CERT (0x02U << IMG_TYPE_GROUP_OFFSET)

/* AP group */
#define IMG_TYPE_IMG_AP_BIN (0x00 | IMG_TYPE_GROUP_AP)

/* MD group */
#define IMG_TYPE_IMG_MD_LTE (0x00 | IMG_TYPE_GROUP_MD)
#define IMG_TYPE_IMG_MD_C2K (0x01 | IMG_TYPE_GROUP_MD)

/* CERT group */
#define IMG_TYPE_CERT1 (0x00 | IMG_TYPE_GROUP_CERT)
#define IMG_TYPE_CERT1_MD (0x01 | IMG_TYPE_GROUP_CERT)
#define IMG_TYPE_CERT2 (0x02 | IMG_TYPE_GROUP_CERT)

#define SEC_IMG_TYPE_LTE	(0x01000000)
#define SEC_IMG_TYPE_C2K	(0x01000001)
#define SEC_IMG_TYPE_DSP	(0x00000000)
#define SEC_IMG_TYPE_ARMV7	(0x00000000)

#define IMG_MAGIC			0x58881688
#define EXT_MAGIC			0x58891689

#define IMG_NAME_SIZE		32
#define IMG_HDR_SIZE		512
typedef union
{
	struct {
		unsigned int magic;		/* always IMG_MAGIC */
		unsigned int dsize;		/* image size, image header and padding are not included */
		char name[IMG_NAME_SIZE];
		unsigned int maddr;		/* image load address in RAM */
		unsigned int mode;		/* maddr is counted from the beginning or end of RAM */
		/* extension */
		unsigned int ext_magic;		/* always EXT_MAGIC */
		unsigned int hdr_size;		/* header size is 512 bytes currently, but may extend in the future */
		unsigned int hdr_version;	/* see HDR_VERSION */
		unsigned int img_type;		/* please refer to #define beginning with SEC_IMG_TYPE_ */
		unsigned int img_list_end;	/* end of image list? 0: this image is followed by another image 1: end */
		unsigned int align_size;	/* image size alignment setting in bytes, 16 by default for AES encryption */
		unsigned int dsize_extend;	/* high word of image size for 64 bit address support */
		unsigned int maddr_extend;	/* high word of image load address in RAM for 64 bit address support */
	} info;
	unsigned char data[IMG_HDR_SIZE];
} IMG_HDR_T;

enum {
	main_img = 1,
	dsp_img,
	armv7_img,
	max_img_num
};

enum {
	MD_SYS1 = 0,
	MD_SYS2,
	MD_SYS3,
	MD_SYS4,
};
typedef struct _download_info
{
	int	img_type;	/* Main image, or plug-in image */
	char	*partition_name;
	char	*image_name;	
	int	max_size;
}download_info_t;


/*------------------------------------------------------*/
/* Share memory cal                                     */
/*------------------------------------------------------*/
static unsigned int str2uint(char *str)
{
	/* max 32bit integer is 4294967296, buf[16] is enough */
	char buf[16];
	int i;
	int num;
	int base = 10;
	int ret_val;
	if (NULL == str)
		return 0;

	i = 0;
	while (i<16) {
		/* Format it */
		if ((str[i] == 'X') || (str[i] == 'x')) {
			buf[i] = 'x';
			if (i != 1)
				return 0; /* not 0[x]XXXXXXXX */
			else if (buf[0] != '0')
				return 0; /* not [0]xXXXXXXXX */
			else
				base = 16;
		} else if ((str[i] >= '0') && (str[i] <= '9'))
			buf[i] = str[i];
		else if ((str[i] >= 'a') && (str[i] <= 'f')) {
			if (base != 16)
				return 0;
			buf[i] = str[i];
		} else if ((str[i] >= 'A') && (str[i] <= 'F')) {
			if (base != 16)
				return 0;
			buf[i] = str[i] - 'A' + 'a';
		} else if (str[i] == 0) {
			buf[i] = 0;
			i++;
			break;
		} else
			return 0;

		i++;
	}

	num = i-1;
	ret_val = 0;
	if (base == 10) {
		for (i=0; i<num; i++)
			ret_val = ret_val*10 + buf[i] - '0';
	} else if (base == 16) {
		for (i=2; i<num; i++) {
			if (buf[i] >= 'a')
				ret_val = ret_val*16 + buf[i] - 'a' + 10;
			else
				ret_val = ret_val*16 + buf[i] - '0';
		}
	}
	return ret_val;	
}

typedef struct _smem_layout
{
	unsigned long long base_addr;
	unsigned int ap_md1_smem_offset;
	unsigned int ap_md1_smem_size;
	unsigned int ap_md3_smem_offset;
	unsigned int ap_md3_smem_size;
	unsigned int md1_md3_smem_offset;
	unsigned int md1_md3_smem_size;
	unsigned int total_smem_size;
}smem_layout_t;
static smem_layout_t smem_info;

extern char *get_env(char *name);
static unsigned int cal_share_mem_layout(unsigned char* base_addr)
{
	/* MD Share memory layout */
	/*    AP    <-->   MD1    */
	/*    MD1   <-->   MD3    */
	/*    AP    <-->   MD3    */
	smem_info.base_addr = (unsigned long long)((unsigned long)base_addr);
	/* read env setting for md1 and md3 dhl */
	ap_md1_smem_size_at_lk_env = str2uint(get_env("apmd1_smem"));
	md1_md3_smem_size_at_lk_env = str2uint(get_env("md1md3_smem"));
	ap_md3_smem_size_at_lk_env = str2uint(get_env("apmd3_smem"));
	#ifdef ENABLE_DT_DEB_LOG
	printf("env[apmd1_smem]%x.\n", ap_md1_smem_size_at_lk_env);
	printf("env[md1md3_smem]%x.\n", md1_md3_smem_size_at_lk_env);
	printf("env[apmd3_smem]%x.\n", ap_md3_smem_size_at_lk_env);
	#endif
	if (md_load_status_flag & (1<<MD_SYS3)) {
		smem_info.ap_md1_smem_offset = 0;
		if (ap_md1_smem_size_at_lk_env)
			smem_info.ap_md1_smem_size = ap_md1_smem_size_at_lk_env;
		else if (ap_md1_smem_size_at_img)
			smem_info.ap_md1_smem_size = ap_md1_smem_size_at_img;
		else
			smem_info.ap_md1_smem_size  = AP_MD1_SMEM_SIZE;

		smem_info.md1_md3_smem_offset = (smem_info.ap_md1_smem_offset + smem_info.ap_md1_smem_size + 0x10000 - 1)&(~(0x10000 - 1));
		if (md1_md3_smem_size_at_lk_env)
			smem_info.md1_md3_smem_size = md1_md3_smem_size_at_lk_env;
		else if (md1_md3_smem_size_at_img)
			smem_info.md1_md3_smem_size = md1_md3_smem_size_at_img;
		else
			smem_info.md1_md3_smem_size = MD1_MD3_SMEM_SIZE;

		smem_info.ap_md3_smem_offset = smem_info.md1_md3_smem_offset + smem_info.md1_md3_smem_size;
		if (ap_md3_smem_size_at_lk_env)
			smem_info.ap_md3_smem_size = ap_md3_smem_size_at_lk_env;
		else if (ap_md3_smem_size_at_img)
			smem_info.ap_md3_smem_size = ap_md3_smem_size_at_img;
		else
			smem_info.ap_md3_smem_size = AP_MD3_SMEM_SIZE;

		smem_info.total_smem_size = smem_info.ap_md3_smem_offset + smem_info.ap_md3_smem_size;
	} else {
		smem_info.ap_md1_smem_offset = 0;
		if (ap_md1_smem_size_at_lk_env)
			smem_info.ap_md1_smem_size = ap_md1_smem_size_at_lk_env;
		else if (ap_md1_smem_size_at_img)
			smem_info.ap_md1_smem_size = ap_md1_smem_size_at_img;
		else
			smem_info.ap_md1_smem_size  = AP_MD1_SMEM_SIZE;

		smem_info.md1_md3_smem_offset = 0;
		smem_info.md1_md3_smem_size = 0;
		smem_info.ap_md3_smem_offset = 0;
		smem_info.ap_md3_smem_size = 0;
		smem_info.total_smem_size = smem_info.ap_md1_smem_size;
	}
	printf("smem_info.ap_md1_smem_offset:%x\n", smem_info.ap_md1_smem_offset);
	printf("smem_info.ap_md1_smem_size:%x\n", smem_info.ap_md1_smem_size);
	printf("smem_info.md1_md3_smem_offset:%x\n", smem_info.md1_md3_smem_offset);
	printf("smem_info.md1_md3_smem_size:%x\n", smem_info.md1_md3_smem_size);
	printf("smem_info.ap_md3_smem_offset:%x\n", smem_info.ap_md3_smem_offset);
	printf("smem_info.ap_md3_smem_size:%x\n", smem_info.ap_md3_smem_size);
	printf("smem_info.total_smem_size:%x\n", smem_info.total_smem_size);

	/* insert share memory layout to lk info */
	if (insert_ccci_tag_inf("smem_layout", (char*)&smem_info, sizeof(smem_layout_t)) < 0)
		printf("insert smem_layout fail\n");

	return smem_info.total_smem_size;
}


/*------------------------------------------------------------------------------------------------*/
int md_reserve_mem_size_fixup(void *fdt)
{
	int offset, sub_offset;

	/* -------- Remove device tree item ----------------------- */
	offset = fdt_path_offset(fdt, "/reserved-memory");

	sub_offset = fdt_subnode_offset(fdt, offset, "reserve-memory-ccci_md1");
	if (sub_offset) {
		#ifdef ENABLE_DT_DEB_LOG
		printf("get md1 reserve-mem success. offset: %d, sub_off: %d\n",
			offset, sub_offset);
		#endif
		/* kill this node */
		fdt_del_node(fdt, sub_offset);
	} else
		printf("ignore md1 reserve-mem.\n");

	sub_offset = fdt_subnode_offset(fdt, offset, "reserve-memory-ccci_md3_ccif");
	if (sub_offset) {
		#ifdef ENABLE_DT_DEB_LOG
		printf("get md3 reserve-mem success. offset: %d, sub_off: %d\n",
			offset, sub_offset);
		#endif
		/* kill this node */
		fdt_del_node(fdt, sub_offset);
	} else
		printf("ignore md3 reserve-mem.\n");

	sub_offset = fdt_subnode_offset(fdt, offset, "reserve-memory-ccci_share");
	if (sub_offset) {
		#ifdef ENABLE_DT_DEB_LOG
		printf("get ap md reserve-smem szie success. offset: %d, sub_off: %d\n",
			offset, sub_offset);
		#endif
		/* kill this node */
		fdt_del_node(fdt, sub_offset);
	} else
		printf("ignore reserve-share-mem.\n");

	return 0;
}

/*------------------------------------------------------------------------------------------------*/
typedef struct _mpu_cfg {
	unsigned int start;
	unsigned int end;
	int region;
	unsigned int permission;
	int relate_region; /* Using same behavior and setting */
} mpu_cfg_t;

#define MAX_PADDING_NUM     3

#define MPU_64K_ALIGN_MASK  (~(0x10000-1))
#define MPU_64K_VALUE       (0x10000)
#define MEM_4K_ALIGN_MASK   (~(0x1000-1))
#define MEM_4K_VALUE        (0x1000)

#define MAX_CFG_LIST_NUM    (8)

/* This function only support 3 cases
**   case 0: |======|======| ==> |++====|======|
**   case 1: |======|======| ==> |==++==|======|
**   case 2: |======|======| ==> |====++|======|
**   case 4: |======|======| ==> |=====+|+=====| NOT suppose this case !!!!
**
** return value:
**   1 : retrieve success and used one mpu region
**   0 : retrieve success but not used one mpu region
**  -1 : error
*/
int free_padding_mem_blk_match(mpu_cfg_t cfg_list[], free_padding_block_t *free_mem, int free_mpu_region)
{
	int i;
	int mem_blk_num = 0;
	unsigned int tmp_start_addr, tmp_end_addr;
	unsigned int old_start, old_end;

	ALWAYS_LOG("== Get retrieve 0x%08x~0x%08x =======\n", free_mem->start_offset,
	           free_mem->start_offset+free_mem->length);

	if (free_mem->length <= MPU_64K_VALUE) {
		ALWAYS_LOG("free memory too small\n");
		return -1;
	}

	while (cfg_list[mem_blk_num].region != -1)
		mem_blk_num++;

	/* Find free memory position */
	for (i = 0; i < mem_blk_num; i++) {
		if ((free_mem->start_offset >= cfg_list[i].start) &&
		        (free_mem->start_offset <= cfg_list[i].end))
			break;
	}

	if (i == mem_blk_num) {
		ALWAYS_LOG("invalid free memory slot info, not at any block\n");
		return -1;
	}

	old_start = cfg_list[i].start;
	old_end = cfg_list[i].end;
	DBG_LOG("[0x%08x-0x%08x] ==>\n", old_start, old_end);
	if (free_mem->start_offset == cfg_list[i].start) {
		/* case 0, no need use one mpu region */
		if ((free_mem->start_offset + free_mem->length) >= cfg_list[i].end) {
			ALWAYS_LOG("free memory size too large\n");
			return -1;
		}
		/* Adjust mpu start address */
		tmp_start_addr  = cfg_list[i].start;
		cfg_list[i].start += free_mem->length;
		cfg_list[i].start &= MPU_64K_ALIGN_MASK;
		ALWAYS_LOG("[0x%08x-Retrieve-0x%08x|0x%08x-Reserved-0x%08x]\n",
		           free_mem->start_offset, cfg_list[i].start-1,
		           cfg_list[i].start, cfg_list[i].end);
		ALWAYS_LOG("  0x%xB retrieved by AP\n", cfg_list[i].start - tmp_start_addr);
		ccci_retrieve_mem((unsigned char*)free_mem->start_offset, cfg_list[i].start - tmp_start_addr);
		return 0;
	}

	if ((free_mem->start_offset + free_mem->length -1) < cfg_list[i].end) {
		/* case 1, need use one mpu region, seperate it to three part */
		if ((MAX_CFG_LIST_NUM <= mem_blk_num) || (free_mpu_region == -1)) {
			ALWAYS_LOG("need more mpu region\n");
			return -1;
		}
		tmp_end_addr = cfg_list[i].end;
		cfg_list[i].end = ((free_mem->start_offset + MPU_64K_VALUE - 1) & MPU_64K_ALIGN_MASK) - 1;
		tmp_start_addr = (free_mem->start_offset + free_mem->length) & MPU_64K_ALIGN_MASK;
		if (((cfg_list[i].end - cfg_list[i].start + 1) > MPU_64K_VALUE) &&
		        ((tmp_end_addr - tmp_start_addr +1) > MPU_64K_VALUE)) {
			ALWAYS_LOG("[0x%08x-Reserved-0x%08x|0x%08x-Retrieve-0x%08x|0x%08x-Reserved-0x%08x]\n",
			           cfg_list[i].start, cfg_list[i].end, cfg_list[i].end + 1, tmp_start_addr - 1,
			           tmp_start_addr, tmp_end_addr);
			ALWAYS_LOG("  0x%xB retrieved by AP\n", tmp_start_addr - (cfg_list[i].end + 1));
			ccci_retrieve_mem((unsigned char*)(cfg_list[i].end + 1), tmp_start_addr - (cfg_list[i].end + 1));
			cfg_list[mem_blk_num].start = tmp_start_addr;
			cfg_list[mem_blk_num].end = tmp_end_addr;
			cfg_list[mem_blk_num].permission = cfg_list[i].permission;
			cfg_list[mem_blk_num].region = free_mpu_region;
			cfg_list[mem_blk_num].relate_region = cfg_list[i].region;
			cfg_list[i].relate_region = free_mpu_region;
			mem_blk_num++;
			cfg_list[mem_blk_num].region = -1; /* Mark for new end */
		}
		return 1;
	}

	if ((free_mem->start_offset + free_mem->length -1) == cfg_list[i].end) {
		/* case 2, no need use one mpu region */
		tmp_end_addr = cfg_list[i].end;
		cfg_list[i].end = ((free_mem->start_offset + MPU_64K_VALUE - 1) & MPU_64K_ALIGN_MASK) - 1;
		ALWAYS_LOG("[0x%08x-Reserved-0x%08x|0x%08x-Retrieve-0x%08x]\n",
		           cfg_list[i].start, cfg_list[i].end, cfg_list[i].end + 1, tmp_end_addr);
		ALWAYS_LOG("  0x%xB retrieved by AP\n", tmp_end_addr - cfg_list[i].end);
		ccci_retrieve_mem((unsigned char*)(cfg_list[i].end + 1), tmp_end_addr - cfg_list[i].end);
		return 0;
	}

	if ((free_mem->start_offset + free_mem->length) > cfg_list[i].end) {
		ALWAYS_LOG("over two region\n");
		return -1;
	}

	return -1;
}

/* Remove smallest padding memory that need additional MPU region if available MPU region enough.
** The dat of free_slot maybe modified by this function
*/
#define VALID_PADDING       (1<<0)
#define NEED_ADDITIONAL_MPU (1<<1)
#define NEED_REMOVE         (1<<2)

struct padding_tag {
	free_padding_block_t padding_mem;
	int status;
};
int padding_mem_pre_process(mpu_cfg_t cfg_list[], free_padding_block_t free_slot[], int available_mpu[])
{
	int i, j;
	int mem_blk_num = 0;
	int padding_mem_num = 0;
	int available_mpu_num = 0;
	free_padding_block_t *padding_mem;
	struct padding_tag padding_tag_tbl[MAX_PADDING_NUM+1], *small_ptr = NULL;
	int padding_with_additional_num = 0;
	unsigned int small_length = 0;

	while (cfg_list[mem_blk_num].region != -1)
		mem_blk_num++;

	while (free_slot[padding_mem_num].start_offset != ((unsigned int)-1)) {
		padding_tag_tbl[padding_mem_num].padding_mem.start_offset = free_slot[padding_mem_num].start_offset;
		padding_tag_tbl[padding_mem_num].padding_mem.length = free_slot[padding_mem_num].length;
		padding_tag_tbl[padding_mem_num].status = VALID_PADDING;
		padding_mem_num++;
		if (padding_mem_num >= MAX_PADDING_NUM)
			/* For current design, only have MAX_PADDING_NUM number padding */
			break;
	}
	for (i = padding_mem_num; i < (MAX_PADDING_NUM+1); i++)
		padding_tag_tbl[i].status = 0;

	while (available_mpu[available_mpu_num] != -1)
		available_mpu_num++;

	for (j = 0; j < padding_mem_num; j++) {
		padding_mem = &padding_tag_tbl[j].padding_mem;
		/* Find free memory position */
		for (i = 0; i < mem_blk_num; i++) {
			if ((padding_mem->start_offset >= cfg_list[i].start) &&
			        (padding_mem->start_offset <= cfg_list[i].end))
				break;
		}
		if (i == mem_blk_num)
			continue;

		if (padding_mem->start_offset == cfg_list[i].start)
			continue;/* case 0, no need use one mpu region */
		if ((padding_mem->start_offset + padding_mem->length -1) < cfg_list[i].end) {
			/* case 1, need use one mpu region, seperate it to three part */
			padding_tag_tbl[j].status |= NEED_ADDITIONAL_MPU;
			padding_with_additional_num++;
		}
		if ((padding_mem->start_offset + padding_mem->length -1) == cfg_list[i].end)
			continue;/* case 2, no need use one mpu region */
		if ((padding_mem->start_offset + padding_mem->length) > cfg_list[i].end)
			continue;/* over two region */
	}

	/* If mpu region not enough, remove the small padding memory part */
	while (padding_with_additional_num > available_mpu_num) {
		/* Find first tag that with additional region */
		for (i = 0; i < (MAX_PADDING_NUM + 1); i++) {
			if (padding_tag_tbl[i].status & NEED_ADDITIONAL_MPU) {
				small_ptr = &padding_tag_tbl[i];
				small_length = small_ptr->padding_mem.length;
				break;
			}
		}

		for (j = i; j < padding_mem_num; j++) {
			/* Find the smallest padding with mpu */
			if ((padding_tag_tbl[j].status & NEED_ADDITIONAL_MPU) &&
			        (padding_tag_tbl[j].padding_mem.length < small_length)) {
				small_ptr = &padding_tag_tbl[j];
				small_length = small_ptr->padding_mem.length;
			}
		}
		small_ptr->status |= NEED_REMOVE;
		small_ptr->status &= (~NEED_ADDITIONAL_MPU);
		padding_mem_num--;
		padding_with_additional_num--;
		ALWAYS_LOG("MPU region not enough, cancel to retrieve padding 0x%08x ~ 0x%08x\n",
		           small_ptr->padding_mem.start_offset,
		           small_ptr->padding_mem.start_offset+small_ptr->padding_mem.length);
	}

	/* Update final padding list
	** Remove smallest item, j always <= i */
	j = 0;
	for (i = 0; i < (MAX_PADDING_NUM + 1) ; i++) {
		if ((padding_tag_tbl[i].status & VALID_PADDING) == 0)
			continue;
		if (padding_tag_tbl[i].status & NEED_REMOVE)
			continue;
		if (i == j) {/* No need to copy */
			j++;
			continue;
		}
		free_slot[j].start_offset = padding_tag_tbl[i].padding_mem.start_offset;
		free_slot[j].length = padding_tag_tbl[i].padding_mem.length;
		j++;
	}

	free_slot[j].start_offset = -1;/* mark for new end */

	return j;
}

int retrieve_free_padding_mem(int md_id, unsigned char *start_addr,
                              mpu_cfg_t cfg_list[], void *hdr, int region_list[])
{
	int i, j, ret;
	int free_mpu_region_num = 0;
	int retrieve_blk_num = 0;
	int mpu_region_id;
	struct md_check_header_v5 *chk_hdr = (struct md_check_header_v5 *)hdr;
	free_padding_block_t free_slot[MAX_PADDING_NUM+1]; /* Maximux is 3, that confirned with MD */

	j = 0;
	switch (md_id) {
		case MD_SYS1:
			for (i = 0; i < MAX_PADDING_NUM; i++) {
				if ((chk_hdr->padding_blk[i].start_offset == 0) &&
				        (chk_hdr->padding_blk[i].length == 0))
					continue;
				else {
					free_slot[j].start_offset =
					    (unsigned int)start_addr + chk_hdr->padding_blk[i].start_offset;
					free_slot[j].length = chk_hdr->padding_blk[i].length;
					j++;
				}
			}
			free_slot[j].start_offset = -1; /* Mark for last */
			break;
		case MD_SYS3:
			break;
		default:
			ALWAYS_LOG("[error]invalid md_id(%d) for MPU protect\n", md_id+1);
			return -1;
	}

	while (region_list[free_mpu_region_num] != -1)
		free_mpu_region_num++;

	if (!free_mpu_region_num) {
		ALWAYS_LOG("no free mpu region to use\n");
	}

	if (0 == j) {
		ALWAYS_LOG("no free padding memmory to retrieve\n");
		return -1;
	}

	retrieve_blk_num = padding_mem_pre_process(cfg_list, free_slot, region_list);

	j = 0;
	for (i = 0; i < retrieve_blk_num; i++) {
		if (j < free_mpu_region_num)
			mpu_region_id = region_list[j];
		else
			mpu_region_id = -1;
		ret = free_padding_mem_blk_match(cfg_list, &free_slot[i], mpu_region_id);
		if (ret > 0)
			j++;
	}

	return 0;
}

/***************************************************************************************************
** Platform part
***************************************************************************************************/
extern u32 sec_img_auth_init(u8* part_name, u8* img_name);
extern u32 sec_img_auth(u8 *buf, u32 buf_sz);

extern u32 sec_get_c2kmd_sbcen(void);
extern u32 sec_get_ltemd_sbcen(void);
extern u32 get_policy_entry_idx(u8* part_nam);
extern u32 get_vfy_policy(u8* part_nam);

extern unsigned int ddr_enable_4gb(void)__attribute__((weak));
static int is_4gb_ddr_support_en(void)
{
	int ret;
	if (ddr_enable_4gb) {
		ret = ddr_enable_4gb();
		printf("ddr_enable_4gb() ret:%d.\n", ret);
		return ret;
	} else {
		printf("4gb not enable.\n");
		return 0;
	}
}

/*-------- Register base part -------------------------------*/
/* HW remap for MD1 */
#define MD1_BANK0_MAP0 (0x10001300)
#define MD1_BANK0_MAP1 (0x10001304)
#define MD1_BANK1_MAP0 (0x10001308)
#define MD1_BANK1_MAP1 (0x1000130C)
#define MD1_BANK4_MAP0 (0x10001310)
#define MD1_BANK4_MAP1 (0x10001314)

/* HW remap for MD3(C2K) */
#define MD2_BANK0_MAP0 (0x10001320)
#define MD2_BANK0_MAP1 (0x10001324)
#define MD2_BANK4_MAP0 (0x10001330)
#define MD2_BANK4_MAP1 (0x10001334)

/* HW remap lock register */
#define MD_HW_REMAP_LOCK (0x10001F80)
#define MD1_LOCK         (1<<16)
#define MD3_LOCK         (1<<17)
static int md_mem_ro_rw_remapping(unsigned int md_id, unsigned int addr)
{
	unsigned int md_img_start_addr;
	unsigned int hw_remapping_bank0_map0;
	unsigned int hw_remapping_bank0_map1 = 0;
	unsigned int write_val;

	switch(md_id) {
	case 0: // MD1
		hw_remapping_bank0_map0 = MD1_BANK0_MAP0;
		hw_remapping_bank0_map1 = MD1_BANK0_MAP1;
		break;
	case 2: // MD3
		hw_remapping_bank0_map0 = MD2_BANK0_MAP0;
		hw_remapping_bank0_map1 = MD2_BANK0_MAP1;
		break;
	default:
		printf("Invalid md id:%d\n", md_id);
		return -1;
	}

	if (is_4gb_ddr_support_en()) {
		md_img_start_addr = addr & 0xFFFFFFFF;
		printf("---> Map 0x00000000 to 0x%x for MD%d(4GB M)\n", addr, md_id+1);
	} else {
		md_img_start_addr = addr - 0x40000000;
		printf("---> Map 0x00000000 to 0x%x for MD%d\n", addr, md_id+1);
	}

	/* For MDx_BANK0_MAP0 */
	write_val = (((md_img_start_addr >> 24) | 1) & 0xFF) \
				+ ((((md_img_start_addr + 0x02000000) >> 16) | 1<<8) & 0xFF00) \
				+ ((((md_img_start_addr + 0x04000000) >> 8) | 1<<16) & 0xFF0000) \
				+ ((((md_img_start_addr + 0x06000000) >> 0) | 1<<24) & 0xFF000000);
	DRV_WriteReg32(hw_remapping_bank0_map0, write_val);
	printf("BANK0_MAP0 value:0x%X\n", DRV_Reg32(hw_remapping_bank0_map0));

	/* For MDx_BANK0_MAP1 */
	write_val = ((((md_img_start_addr + 0x08000000) >> 24) | 1) & 0xFF) \
				+ ((((md_img_start_addr + 0x0A000000) >> 16) | 1<<8) & 0xFF00) \
				+ ((((md_img_start_addr + 0x0C000000) >> 8) | 1<<16) & 0xFF0000) \
				+ ((((md_img_start_addr + 0x0E000000) >> 0) | 1<<24) & 0xFF000000);
	DRV_WriteReg32(hw_remapping_bank0_map1, write_val);
	printf("BANK0_MAP1 value:0x%X\n", DRV_Reg32(hw_remapping_bank0_map1));

	return 0;
}

static int md_smem_rw_remapping(unsigned int md_id, unsigned int addr)
{
	unsigned int md_smem_start_addr;
	unsigned int hw_remapping_bank4_map0;
	unsigned int hw_remapping_bank4_map1 = 0;
	unsigned int write_val;

	switch(md_id) {
	case 0: // MD1
		hw_remapping_bank4_map0 = MD1_BANK4_MAP0;
		hw_remapping_bank4_map1 = MD1_BANK4_MAP1;
		break;
	case 2: // MD3
		hw_remapping_bank4_map0 = MD2_BANK4_MAP0;
		hw_remapping_bank4_map1 = MD2_BANK4_MAP1;
		break;
	default:
		printf("Invalid md id:%d\n", md_id);
		return -1;
	}

	if (is_4gb_ddr_support_en()) {
		md_smem_start_addr = addr & 0xFFFFFFFF;
		printf("---> Map 0x40000000 to 0x%x for MD%d(4G)\n", addr, md_id+1);
	} else {
		md_smem_start_addr = addr - 0x40000000;
		printf("---> Map 0x40000000 to 0x%x for MD%d\n", addr, md_id+1);
	}
	/* For MDx_BANK0_MAP0 */
	write_val = (((md_smem_start_addr >> 24) | 1) & 0xFF) \
				+ ((((md_smem_start_addr + 0x02000000) >> 16) | 1<<8) & 0xFF00) \
				+ ((((md_smem_start_addr + 0x04000000) >> 8) | 1<<16) & 0xFF0000) \
				+ ((((md_smem_start_addr + 0x06000000) >> 0) | 1<<24) & 0xFF000000);
	DRV_WriteReg32(hw_remapping_bank4_map0, write_val);
	printf("BANK4_MAP0 value:0x%X\n", DRV_Reg32(hw_remapping_bank4_map0));

	/* For MDx_BANK0_MAP1 */
	write_val = ((((md_smem_start_addr + 0x08000000) >> 24) | 1) & 0xFF) \
				+ ((((md_smem_start_addr + 0x0A000000) >> 16) | 1<<8) & 0xFF00) \
				+ ((((md_smem_start_addr + 0x0C000000) >> 8) | 1<<16) & 0xFF0000) \
				+ ((((md_smem_start_addr + 0x0E000000) >> 0) | 1<<24) & 0xFF000000);
	DRV_WriteReg32(hw_remapping_bank4_map1, write_val);
	printf("BANK4_MAP1 value:0x%X\n", DRV_Reg32(hw_remapping_bank4_map1));

	return 0;
}
#define MD_HW_REMAP_LOCK (0x10001F80)
#define MD1_LOCK         (1<<16)
#define MD3_LOCK         (1<<17)
static void md_emi_remapping_lock(unsigned int md_id)
{
	unsigned int reg_val;
	unsigned int lock_bit;

	switch(md_id) {
	case 0: // MD1
		lock_bit = MD1_LOCK;
		break;
	case 2: // MD3
		lock_bit = MD3_LOCK;
		break;
	default:
		printf("Invalid md id:%d for lock\n", md_id);
		return;
	}

	reg_val = DRV_Reg32(MD_HW_REMAP_LOCK);
	printf("before hw remap lock: MD1[%d], MD3[%d]\n", !!(reg_val&MD1_LOCK), !!(reg_val&MD3_LOCK));
	DRV_WriteReg32(MD_HW_REMAP_LOCK, (reg_val|lock_bit));
	reg_val = DRV_Reg32(MD_HW_REMAP_LOCK);
	printf("before hw remap lock: MD1[%d], MD3[%d]\n", !!(reg_val&MD1_LOCK), !!(reg_val&MD3_LOCK));
}

/* =================================================== */
/* MPU Region defination                               */
/* =================================================== */
#define MPU_REGION_ID_SEC_OS            0
#define MPU_REGION_ID_ATF               1
/* #define MPU_REGION_ID_MD32_SMEM     2 */
#define MPU_REGION_ID_TRUSTED_UI        3
#define MPU_REGION_ID_MD1_SEC_SMEM      4

#define MPU_REGION_ID_MD1_SMEM          5
#define MPU_REGION_ID_MD3_SMEM          6
#define MPU_REGION_ID_MD1MD3_SMEM       7
#define MPU_REGION_ID_MD1_MCURW_HWRW    8
#define MPU_REGION_ID_MD1_ROM           9  /* contain DSP in Jade */
#define MPU_REGION_ID_MD1_MCURW_HWRO    10
#define MPU_REGION_ID_MD1_MCURO_HWRW    11
#define MPU_REGION_ID_WIFI_EMI_FW       12
#define MPU_REGION_ID_WMT               13
#define MPU_REGION_ID_MD1_RW            14
#define MPU_REGION_ID_MD3_ROM           15
#define MPU_REGION_ID_MD3_RW            16
#define MPU_REGION_ID_RESERVED1         17
#define MPU_REGION_ID_RESERVED2         18
#define MPU_REGION_ID_AP                19
#define MPU_REGION_ID_TOTAL_NUM         (MPU_REGION_ID_AP + 1)

#define MPU_MDOMAIN_ID_AP		0
#define MPU_MDOMAIN_ID_MD		1
#define MPU_MDOMAIN_ID_MD3		5
#define MPU_MDOMAIN_ID_MDHW		7
#define MPU_MDOMAIN_ID_TOTAL_NUM	8

static const unsigned int mpu_att_default[MPU_REGION_ID_TOTAL_NUM][MPU_MDOMAIN_ID_TOTAL_NUM] = {
/*===================================================================================================================*/
/* No |  | D0(AP)    | D1(MD1)      | D2(CONN) | D3(Res)  | D4(MM)       | D5(MD3 )      | D6(MFG)      | D7(MDHW)  |*/
/*--------------+----------------------------------------------------------------------------------------------------*/
/* 0*/{ SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN, SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/* 1*/{ SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/* 2*/{ SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN, SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/* 3*/{ SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN, SEC_RW       , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/* 4*/{ SEC_R_NSEC_R , NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , NO_PROTECTION, FORBIDDEN    , FORBIDDEN},
/* 5*/{ NO_PROTECTION, NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/* 6*/{ NO_PROTECTION, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, FORBIDDEN    , NO_PROTECTION, FORBIDDEN    , FORBIDDEN},
/* 7*/{ NO_PROTECTION , NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , NO_PROTECTION, FORBIDDEN    , FORBIDDEN},
/* 8*/{ SEC_R_NSEC_R , NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, NO_PROTECTION},
/* 9*/{ SEC_R_NSEC_R , SEC_R_NSEC_R , FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, SEC_R_NSEC_R },
/*10*/{ SEC_R_NSEC_R , NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, SEC_R_NSEC_R },
/*11*/{ SEC_R_NSEC_R , NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, NO_PROTECTION},
/*12*/{ FORBIDDEN    , FORBIDDEN    , NO_PROTECTION, FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/*13*/{ NO_PROTECTION, FORBIDDEN    , NO_PROTECTION, FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN    , FORBIDDEN},
/*14*/{ SEC_R_NSEC_R , NO_PROTECTION, FORBIDDEN    , FORBIDDEN, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, SEC_R_NSEC_R },
/*15*/{ SEC_R_NSEC_R , FORBIDDEN    , FORBIDDEN    , FORBIDDEN, FORBIDDEN    , SEC_R_NSEC_R , FORBIDDEN    , FORBIDDEN},
/*16*/{ SEC_R_NSEC_R , FORBIDDEN    , FORBIDDEN    , FORBIDDEN, FORBIDDEN    , NO_PROTECTION, FORBIDDEN    , FORBIDDEN},
/*17*/{ NO_PROTECTION, FORBIDDEN    , FORBIDDEN    , FORBIDDEN, NO_PROTECTION, FORBIDDEN    , NO_PROTECTION, FORBIDDEN},
/*18*/{ NO_PROTECTION, SEC_R_NSEC_R , FORBIDDEN    , FORBIDDEN, NO_PROTECTION, FORBIDDEN    , NO_PROTECTION, SEC_R_NSEC_R},
}; /*=================================================================================================================*/

/*==============================================================================*/
/* region             | bits  | AP(0) | MD1(1) | MDHW(7)                        */
/* md1_rom(dsp_rom)(9)|  3:0  | f     | f      | f                              */
/* md1_mcurw_hwro(10) |  7:4  | f     | f      | f                              */
/* md1_mcuro_hwrw(11) | 11:8  | f     | f      | f                              */
/* md1_mcurw_hwrw(8)  | 15:12 | f     | f      | f                              */
/* md1_rw(14)         | 19:16 | f     | f      | f                              */
static const char bit_map_for_region_at_hdr_domain[MPU_REGION_ID_TOTAL_NUM] = {
	0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,/* 0~7 */
	3, 0, 1, 2,/* 8~11 */
	0x7F, 0x7F,/* 12~13 */
	4,/* 14 */
	0x7F, 0x7F, 0x7F};

static const unsigned char region_mapping_at_hdr[] = {
	MPU_REGION_ID_MD1_ROM, MPU_REGION_ID_MD1_MCURW_HWRO, MPU_REGION_ID_MD1_MCURO_HWRW,
	MPU_REGION_ID_MD1_MCURW_HWRW, MPU_REGION_ID_MD1_RW};

static void ccci_mem_access_cfg(mpu_cfg_t *mpu_cfg_list, int clear)
{
	#ifdef ENABLE_EMI_PROTECTION
	mpu_cfg_t *curr;
	unsigned int curr_attr;
	if (NULL == mpu_cfg_list)
		return;

	curr = mpu_cfg_list;
	curr_attr = SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
				NO_PROTECTION, NO_PROTECTION, NO_PROTECTION);
	while (curr->region != -1) {
		if (clear) {
			emi_mpu_set_region_protection(	0,		/*START_ADDR*/
							0,		/*END_ADDR*/
							curr->region,	/*region*/
							curr_attr);
			printf("Clr MPU:S:0x%x E:0x%x A:<%d>[0~7]%d-%d-%d-%d-%d-%d-%d-%d\n",  0, 0,
							curr->region, curr_attr&7, (curr_attr>>3)&7, (curr_attr>>6)&7,
							(curr_attr>>9)&7, (curr_attr>>12)&7, (curr_attr>>15)&7,
							(curr_attr>>18)&7, (curr_attr>>21)&7);
		} else {
			emi_mpu_set_region_protection(	curr->start,	/*START_ADDR*/
							curr->end,	/*END_ADDR*/
							curr->region,	/*region*/
							curr->permission);
			curr_attr = curr->permission;
			printf("Set MPU:S:0x%x E:0x%x A:<%d>[0~7]%d-%d-%d-%d-%d-%d-%d-%d\n", curr->start,
							curr->end, curr->region, curr_attr&7, (curr_attr>>3)&7,
							(curr_attr>>6)&7, (curr_attr>>9)&7, (curr_attr>>12)&7,
							(curr_attr>>15)&7, (curr_attr>>18)&7, (curr_attr>>21)&7);
		}
		curr++;
	}
	#endif
}

static unsigned int extract_region_value(unsigned int domain_set, int region_id)
{
	unsigned int ret;
	unsigned int shift_num = region_id * 4;

	ret = (domain_set&(0x0000000F<<shift_num))>>shift_num;

	return ret;
}

/* region_id: 0~7 0=MPU_DOMAIN_INFO_ID_MD1 ->CHEAD_MPU_DOMAIN_ID[MPU_DOMAIN_INFO_ID_MD1]-- domain 1 MD1*/
static unsigned int chk_hdr_region_attr_paser(unsigned region_id, void *hdr)
{
	unsigned int i;
	unsigned int region_attr_merged, extract_value;
	struct md_check_header_v5 *chk_hdr = (struct md_check_header_v5 *)hdr;
	unsigned int region_attr[8];
	int mapped_region_id;
	unsigned int flag = 0;

	mapped_region_id = bit_map_for_region_at_hdr_domain[region_id];
	if (mapped_region_id != 0x7F) {
		/* region for domain0 */
		extract_value = extract_region_value(chk_hdr->domain_attr[0], mapped_region_id);
		if (extract_value <= FORBIDDEN) {
			region_attr[0] = extract_value;
			printf("[0]using hdr val:0x%x\n", extract_value);
			flag |= 1<<0;
		}
		/* region for domain1 */
		extract_value = extract_region_value(chk_hdr->domain_attr[1], mapped_region_id);
		if (extract_value <= FORBIDDEN) {
			region_attr[1] = extract_value;
			printf("[1]using hdr val:0x%x\n", extract_value);
			flag |= 1<<1;
		}
		/* region for domain7 */
		extract_value = extract_region_value(chk_hdr->domain_attr[2], mapped_region_id);
		if (extract_value <= FORBIDDEN) {
			region_attr[7] = extract_value;
			printf("[7]using hdr val:0x%x\n", extract_value);
			flag |= 1<<7;
		}
	}

	for (i = 0; i<8; i++) {
		if (flag & (1<<i)) continue;
		region_attr[i] = mpu_att_default[region_id][i];
	}

	region_attr_merged = SET_ACCESS_PERMISSON(region_attr[7], region_attr[6], region_attr[5], region_attr[4],
					region_attr[3], region_attr[2], region_attr[1], region_attr[0]);

	return region_attr_merged;
}

static void ccci_mem_access_protection_init(int md_id, unsigned char *start_addr, int size,
						void *hdr, mpu_cfg_t cfg_list[])
{
	unsigned int i;
	struct md_check_header_v5 *chk_hdr = (struct md_check_header_v5 *)hdr;
	struct md_check_header_v1 *chk_hdr_v1 = (struct md_check_header_v1 *)hdr;

	switch (md_id) {
	/*
	 * if set start=0x0, end=0x10000, the actural protected area will be 0x0-0x1FFFF,
	 * here we use 64KB align, MPU actually request 32KB align since MT6582, but this works...
	 * we assume emi_mpu_set_region_protection will round end address down to 64KB align.
	 */
	case MD_SYS1:
		for (i = 0; i < (sizeof(region_mapping_at_hdr)/sizeof(unsigned char)); i++) {
			/* set 9, 8, 10, 14, 11 */
			cfg_list[i].permission = chk_hdr_region_attr_paser(cfg_list[i].region, hdr);
			cfg_list[i].start = (unsigned int)start_addr + chk_hdr->region_info[i].region_offset;
			cfg_list[i].end = cfg_list[i].start + chk_hdr->region_info[i].region_size;
			/* 64K align */
			cfg_list[i].end = ((cfg_list[i].end + 0xFFFF)&(~0xFFFF)) - 1; 
			cfg_list[i].relate_region = 0;
		}
		cfg_list[i].region = -1; /* Mark for last */
		cfg_list[i].relate_region = 0;
		break;
	case MD_SYS3:
		/* RO part */
		cfg_list[0].permission = SET_ACCESS_PERMISSON(mpu_att_default[cfg_list[0].region][7],
								mpu_att_default[cfg_list[0].region][6],
								mpu_att_default[cfg_list[0].region][5],
								mpu_att_default[cfg_list[0].region][4],
								mpu_att_default[cfg_list[0].region][3],
								mpu_att_default[cfg_list[0].region][2],
								mpu_att_default[cfg_list[0].region][1],
								mpu_att_default[cfg_list[0].region][0]);
		cfg_list[0].start = (unsigned int)start_addr;
		cfg_list[0].end = cfg_list[0].start + chk_hdr_v1->common.md_img_size;
		/* 64K align */
		cfg_list[0].end = ((cfg_list[0].end + 0xFFFF)&(~0xFFFF)) - 1; 
		cfg_list[0].relate_region = 0;

		/* RW part */
		cfg_list[1].permission = SET_ACCESS_PERMISSON(mpu_att_default[cfg_list[1].region][7],
								mpu_att_default[cfg_list[1].region][6],
								mpu_att_default[cfg_list[1].region][5],
								mpu_att_default[cfg_list[1].region][4],
								mpu_att_default[cfg_list[1].region][3],
								mpu_att_default[cfg_list[1].region][2],
								mpu_att_default[cfg_list[1].region][1],
								mpu_att_default[cfg_list[1].region][0]);
		cfg_list[1].start = (unsigned int)start_addr + chk_hdr_v1->common.md_img_size;
		cfg_list[1].end = (unsigned int)start_addr + chk_hdr_v1->common.mem_size;
		/* 64K align */
		cfg_list[1].start = ((cfg_list[1].start + 0xFFFF)&(~0xFFFF));
		cfg_list[1].end = ((cfg_list[1].end + 0xFFFF)&(~0xFFFF)) - 1;
		cfg_list[1].relate_region = 0;
		break;
	default:
		printf("[error]invalid md_id(%d) for MPU protect\n", md_id+1);
		return;
	}

}

static void ccci_share_mem_access_protection_init(mpu_cfg_t cfg_list[])
{
	int i = 0;

	/*
	 * if set start=0x0, end=0x10000, the actural protected area will be 0x0-0x1FFFF,
	 * here we use 64KB align, MPU actually request 32KB align since MT6582, but this works...
	 * we assume emi_mpu_set_region_protection will round end address down to 64KB align.
	 */
	if (md_load_status_flag & (1<<MD_SYS1)) {
		ALWAYS_LOG("ap md1 share mem MPU need configure\n");
		cfg_list[i].region = MPU_REGION_ID_MD1_SMEM;
		cfg_list[i].permission = SET_ACCESS_PERMISSON(mpu_att_default[cfg_list[i].region][7],
		                         mpu_att_default[cfg_list[i].region][6],
		                         mpu_att_default[cfg_list[i].region][5],
		                         mpu_att_default[cfg_list[i].region][4],
		                         mpu_att_default[cfg_list[i].region][3],
		                         mpu_att_default[cfg_list[i].region][2],
		                         mpu_att_default[cfg_list[i].region][1],
		                         mpu_att_default[cfg_list[i].region][0]);
		cfg_list[i].start = (unsigned int)smem_info.base_addr + smem_info.ap_md1_smem_offset;
		cfg_list[i].end = (unsigned int)smem_info.base_addr + smem_info.ap_md1_smem_offset
		                  + smem_info.ap_md1_smem_size;
		cfg_list[i].end = ((cfg_list[i].end + 0xFFFF)&(~0xFFFF)) - 1;
		i++;
	}

	if (md_load_status_flag & (1<<MD_SYS3)) {
		ALWAYS_LOG("md1 md3 share mem MPU need configure\n");
		cfg_list[i].region = MPU_REGION_ID_MD1MD3_SMEM;
		cfg_list[i].permission = SET_ACCESS_PERMISSON(mpu_att_default[cfg_list[i].region][7],
		                         mpu_att_default[cfg_list[i].region][6],
		                         mpu_att_default[cfg_list[i].region][5],
		                         mpu_att_default[cfg_list[i].region][4],
		                         mpu_att_default[cfg_list[i].region][3],
		                         mpu_att_default[cfg_list[i].region][2],
		                         mpu_att_default[cfg_list[i].region][1],
		                         mpu_att_default[cfg_list[i].region][0]);
		cfg_list[i].start = (unsigned int)smem_info.base_addr + smem_info.md1_md3_smem_offset;
		cfg_list[i].end = (unsigned int)smem_info.base_addr + smem_info.md1_md3_smem_offset
		                  + smem_info.md1_md3_smem_size;
		cfg_list[i].end = ((cfg_list[i].end + 0xFFFF)&(~0xFFFF)) - 1;
		i++;
		ALWAYS_LOG("ap md3 share mem MPU need configure\n");
		cfg_list[i].region = MPU_REGION_ID_MD3_SMEM;
		cfg_list[i].permission = SET_ACCESS_PERMISSON(mpu_att_default[cfg_list[i].region][7],
		                         mpu_att_default[cfg_list[i].region][6],
		                         mpu_att_default[cfg_list[i].region][5],
		                         mpu_att_default[cfg_list[i].region][4],
		                         mpu_att_default[cfg_list[i].region][3],
		                         mpu_att_default[cfg_list[i].region][2],
		                         mpu_att_default[cfg_list[i].region][1],
		                         mpu_att_default[cfg_list[i].region][0]);
		cfg_list[i].start = (unsigned int)smem_info.base_addr + smem_info.ap_md3_smem_offset;
		cfg_list[i].end = (unsigned int)smem_info.base_addr + smem_info.ap_md3_smem_offset
		                  + smem_info.ap_md3_smem_size;
		cfg_list[i].end = ((cfg_list[i].end + 0xFFFF)&(~0xFFFF)) - 1;
		i++;
	}

	cfg_list[i].region = -1; /* mark for end */
}

/* Image list that need to download */
static download_info_t md1_download_list[] = {
	/* img type | partition | image name | addr | len | verify */
	{main_img, "md1img", "md1rom", 0xA000000},
	{dsp_img, "md1dsp", "md1dsp", 0x200000},
	{armv7_img, "md1arm7", "md1arm7", 0x200000},
	{0, NULL, NULL, 0},
};
static download_info_t md3_download_list[] = {
	/* img type | partition | image name | addr | len | verify */
	{main_img, "md3img", "md3rom", 0xC00000},
	{0, NULL, NULL, 0},
};

extern void arch_sync_cache_range(addr_t start, size_t len);

int load_img(download_info_t img_list[], unsigned char **load_addr, void *hdr)
{
	IMG_HDR_T *p_hdr = NULL;
	int load_size;
	int md_mem_resv_size = 0;
	int curr_img_size;
	int md_hdr_ver;
	int ret = 0;
	int i;
	download_info_t *curr;
	struct md_check_header_comm *common_hdr;
	struct md_check_header_v5 *chk_hdr_v5;
	struct md_check_header_v1 *chk_hdr_v1;
	unsigned char *img_load_addr[max_img_num];
	int img_size[max_img_num];

	/* Security policy */
	#ifdef MTK_SECURITY_SW_SUPPORT		
	unsigned int policy_entry_idx = 0;			
	unsigned int img_auth_required = 0;	
	unsigned int verify_hash = 0;
	unsigned int lte_sbc_en = sec_get_ltemd_sbcen();			
	unsigned int c2k_sbc_en = sec_get_c2kmd_sbcen();
	int time_md_auth = get_timer(0);
	int time_md_auth_init = get_timer(0);		
	#endif

	for (i=0; i < max_img_num; i++) {
		img_load_addr[i] = NULL;
		img_size[i] = 0;
	}

	printf("enter load_img function\n");

	if (img_list == NULL) {
		printf("image list is NULL!\n");
		ret = -1;
		return ret;
	}

	/* allocate partition header to reserved memory fisrt */
	p_hdr = (IMG_HDR_T *)malloc(sizeof(IMG_HDR_T));
	if (p_hdr==NULL) {
		printf("alloc mem for hdr fail\n");
		ret = -2;
		goto _MD_Exit;
	}

	/* load image one by one */
	curr = img_list;
	while (curr->img_type != 0) {
		/* do fisrt step check */
		printf("load ptr[%s] hdr\n", curr->partition_name);

		if ((curr->img_type != main_img) &&
			((img_load_addr[curr->img_type] == NULL) || (img_size[curr->img_type] == 0))) {
			/* update image pointer to next*/
			curr++;
			continue;
		}
		load_size = (int)ccci_load_raw_data(curr->partition_name, (unsigned char*)p_hdr, 0,
					sizeof(IMG_HDR_T));
		if (load_size != sizeof(IMG_HDR_T)) {
			printf("load hdr fail(%d)\n", load_size);
			ret = -3;
			goto _MD_Exit;
		}
		if (p_hdr->info.magic!=IMG_MAGIC) {
			printf("invalid magic(%x):(%x)ref\n", p_hdr->info.magic, IMG_MAGIC);
			ret = -4;
			goto _MD_Exit;
		}
		if ((curr->img_type == main_img) && (p_hdr->info.dsize < 0x100000)) { /* 1MB */
			printf("img size abnormal,size(0x%x)\n", p_hdr->info.dsize);
			ret = -5;
			goto _MD_Exit;
		}

                /* Check sec policy to see if verification is required */
		#ifdef MTK_SECURITY_SW_SUPPORT			
		policy_entry_idx = get_policy_entry_idx((unsigned char *)curr->partition_name);
		printf("policy_entry_idx = %d \n", policy_entry_idx);

		img_auth_required = get_vfy_policy(policy_entry_idx);	
		printf("img_auth_required = %d \n", img_auth_required);	
		
		/* do verify md cert-chain if need */
		
		time_md_auth_init = get_timer(0);		
		if (img_auth_required) {
			
			ret = sec_img_auth_init((unsigned char *)curr->partition_name,
							(unsigned char *)curr->image_name);
			if (0 != ret) {
				printf("img cert-chain verification fail: %d\n", ret);
				ret = -6;
				goto _MD_Exit;
			}
			printf("Verify MD cert chain %s cost %d ms\n",curr->partition_name, (int)get_timer(time_md_auth_init));
		}
		#endif
              

		/* Verity pass, then begin load to dram step */
		printf("dump p_hdr info\n");
		printf(" p_hdr->info.magic:%x\n", p_hdr->info.magic);
		printf(" p_hdr->info.dsize:%x\n", p_hdr->info.dsize);
		printf(" p_hdr->info.name:%s\n", p_hdr->info.name);
		printf(" p_hdr->info.mode:%x\n", p_hdr->info.mode);
		printf(" p_hdr->info.hdr_size:%x\n", p_hdr->info.hdr_size);

		/* load data to memory */
		curr_img_size = (int)p_hdr->info.dsize;
		if (curr_img_size > curr->max_size) {
			printf("image size abnormal r:[0x%x]<>a:[0x%x]\n",
				p_hdr->info.dsize, curr->max_size);
			ret = -7;
			goto _MD_Exit;
		}

		if (curr->img_type == main_img) { /* Main image need allocate memory first */
			/* update hdr tbl info */
			if (insert_ccci_tag_inf(curr->partition_name, (char*)&curr_img_size, sizeof(unsigned int)) < 0)
				printf("insert %s size fail\n", curr->partition_name);
			/* Reserve memory with max requirement */
			img_load_addr[main_img] = ccci_request_mem(curr->max_size, 0x2000000L);
			if (img_load_addr[main_img] == NULL) {
				printf("allocate MD memory fail\n");
				ret = -8;
				goto _MD_Exit;
			}
			md_mem_resv_size = curr->max_size;
		} else {
			if (curr_img_size > img_size[curr->img_type]) {
				printf("actual size mis-sync with reserved size,a:[0x%x]<>r:[0x%x]\n",
					curr_img_size, img_size[curr->img_type]);
				ret = -9;
				goto _MD_Exit;
			}
		}

		load_size = ccci_load_raw_data(curr->partition_name, img_load_addr[curr->img_type],
					sizeof(IMG_HDR_T), p_hdr->info.dsize);
		if (load_size != curr_img_size) {
			printf("load image fail:%d\n", load_size);
			ret = -10;
			goto _MD_Exit;
		}

                /* Calcualte size that add padding */
 		curr_img_size = (curr_img_size + p_hdr->info.align_size - 1) & (~(p_hdr->info.align_size -1));
		/* Clear padding data to 0 */
		for (i = 0; i < curr_img_size - (int)p_hdr->info.dsize; i++)
			(img_load_addr[curr->img_type])[p_hdr->info.dsize + i] = 0;

		#ifdef MTK_SECURITY_SW_SUPPORT				
		/* Verify image hash value if needed */
		if (img_auth_required) {
			/* Verify md image hash value */
			/* When SBC_EN is fused, bypass sec_img_auth */					
			verify_hash = 1;			
			time_md_auth = get_timer(0);			

 			if (!strncmp((char const *)curr->partition_name, "md1img",6)){
				if (0x01 == lte_sbc_en){
					printf("LTE now, and lte sbc en = 0x1 \n");
					/* Bypass image hash verification at AP side*/
					verify_hash = 0;
				}
				else		
					printf("LTE now, and lte sbc en != 0x1 \n");
			}
			if (!strncmp((char const *)curr->partition_name, "md3img",6)){
				if (0x01 == c2k_sbc_en){
					printf("C2K now, and c2k sbc en = 0x1 \n");
					/* Bypass image hash verification at AP side*/
					verify_hash = 0;
				}
				else		
					printf("C2K now, and c2k sbc en != 0x1 \n");
			}			
			if (1 == verify_hash){	
				time_md_auth = get_timer(0);	
				ret = sec_img_auth(img_load_addr[curr->img_type], curr_img_size);
				if (0 != ret) {
					printf("image hash verification fail: %d\n",ret);
					ret = -11;
					goto _MD_Exit;
				}
				printf("Image hash verification success: %d\n",ret);
				printf("Verify MD image hash %s cost %d ms\n",curr->partition_name, (int)get_timer(time_md_auth));
			}			
		}
		#endif
		if (curr->img_type == main_img) { /* Main image need parse header */
			/* Parse check header */
			common_hdr = ccci_get_md_header(img_load_addr[curr->img_type] + p_hdr->info.dsize , &md_hdr_ver);

			if (common_hdr == NULL) {
				printf("parse check header fail\n");
				ret = -12;
				goto _MD_Exit;
			}
			if (strncmp((char const *)common_hdr->check_header, "CHECK_HEADER", 12)) {
				printf("invald md check header str[%s]\n", common_hdr->check_header);
				ret = -13;
				goto _MD_Exit;
			}

			/* Dump header info */
			ccci_md_header_parser(common_hdr, md_hdr_ver);

			/* Resize if acutal memory less then reserved*/
			md_mem_resv_size = (int)common_hdr->mem_size;
			if (md_mem_resv_size == 0) {
				printf("md required memory is 0, using defautl [0x%x]:[0x%x]\n",
					md_mem_resv_size, curr->max_size);
				md_mem_resv_size = curr->max_size;
			} else if (md_mem_resv_size < curr->max_size) {
				printf("md required memory more less[0x%x]:[0x%x]\n",
					md_mem_resv_size, curr->max_size);
				/* Resize reserved memory */
				ccci_resize_reserve_mem(img_load_addr[curr->img_type],
							curr->max_size, md_mem_resv_size);
			} else if (md_mem_resv_size > curr->max_size) {
				printf("md required memory abnormal[0x%x]:[0x%x]\n",
					md_mem_resv_size, curr->max_size);
				md_mem_resv_size = 0;
				/* Resize reserved memory */
				ccci_resize_reserve_mem(img_load_addr[curr->img_type],
							curr->max_size, 0);
				ret = -14;
				goto _MD_Exit;
			}

			/* Update plugin image info */
			if (md_hdr_ver == 5) {
				chk_hdr_v5 = (struct md_check_header_v5 *)common_hdr;
				img_load_addr[dsp_img] = img_load_addr[curr->img_type] + chk_hdr_v5->dsp_img_offset;
				img_size[dsp_img] = chk_hdr_v5->dsp_img_size;
				img_load_addr[armv7_img] = img_load_addr[curr->img_type] +chk_hdr_v5->arm7_img_offset;
				img_size[armv7_img] =chk_hdr_v5->arm7_img_size;
				img_size[main_img] = chk_hdr_v5->common.md_img_size;
				ap_md1_smem_size_at_img = chk_hdr_v5->ap_md_smem_size;
				md1_md3_smem_size_at_img = chk_hdr_v5->md_to_md_smem_size;

				/* copy check header */
				memcpy(hdr, chk_hdr_v5, sizeof(struct md_check_header_v5));
			} else {
				chk_hdr_v1 = (struct md_check_header_v1 *)common_hdr;
				ap_md3_smem_size_at_img = chk_hdr_v1->ap_md_smem_size;

				/* copy check header */
				memcpy(hdr, chk_hdr_v1, sizeof(struct md_check_header_v1));
			}
		}

		/* update image pointer */
		curr++;
	}

	/* Free part header memory */
	if (p_hdr) {
		free(p_hdr);
		p_hdr = NULL;
	}

	/* Sync cache to make sure all data flash to DRAM to avoid MPU violation */
	arch_sync_cache_range((addr_t)img_load_addr[main_img], (size_t)md_mem_resv_size);

	*load_addr = img_load_addr[main_img];
	return md_mem_resv_size;
_MD_Exit:
	if (p_hdr) {
		free(p_hdr);
		p_hdr = NULL;
	}

	if (img_load_addr[main_img]) {
		printf("Free reserved memory\n");
		ccci_resize_reserve_mem(img_load_addr[main_img], md_mem_resv_size, 0);
	}

	return ret;
}

void load_modem_image(void)
{
	struct md_check_header_v5 *chk_hdr = NULL;
	struct md_check_header_v1 *chk_hdr_v1;
	modem_info_t info;
	int md_mem_resv_size;
	unsigned char *load_addr;
	unsigned char *smem_addr;
	unsigned int smem_size;
	int success = 0;
	unsigned int i;
	mpu_cfg_t mem_access_list[MAX_CFG_LIST_NUM];
	int free_mpu_region[] = {MPU_REGION_ID_RESERVED1, -1};
	int time_lk_md_init = get_timer(0);
	int retrieve_padding_ret;

	printf("CCCI:Enter load_modem_image\n");
	ap_md1_smem_size_at_lk_env = 0;
	md1_md3_smem_size_at_lk_env = 0;
	ap_md3_smem_size_at_lk_env = 0;
	ap_md1_smem_size_at_img = 0;
	md1_md3_smem_size_at_img = 0;
	ap_md3_smem_size_at_img = 0;

	/* smem_addr = ccci_request_mem(MAX_SMEM_SIZE + md1_dhl_buf_size + md3_dhl_buf_size, 0x2000000L); */
	smem_addr = ccci_request_mem(MAX_SMEM_SIZE, 0x2000000L);
	if (smem_addr == NULL) {
		printf("allocate MD share memory fail\n");
		goto _free_memory;
	}

	chk_hdr = (struct md_check_header_v5 *)malloc(sizeof(struct md_check_header_v5));
	if (chk_hdr == NULL) {
		printf("allocate check header memory fail\n");
		goto _free_memory;
	}
	chk_hdr_v1 = (struct md_check_header_v1 *)chk_hdr;

	ccci_lk_info_init((unsigned long long)((unsigned long)smem_addr));

	printf("-- MD1 --\n");
	for (i = 0; i < (sizeof(region_mapping_at_hdr)/sizeof(unsigned char)); i++)
		mem_access_list[i].region = (int)region_mapping_at_hdr[i];

	mem_access_list[i].region = -1;

	/* Clear MPU setting */
	ccci_mem_access_cfg(mem_access_list, 1);
	/* Load image */
	md_mem_resv_size = load_img(md1_download_list, &load_addr, chk_hdr);

	if (md_mem_resv_size > 0) {
		info.base_addr = (unsigned long long)((unsigned long)load_addr);
		info.size = md_mem_resv_size;
		info.errno = 0;
		info.md_type = chk_hdr->common.image_type;
		success = 1;
	} else {
		info.base_addr = 0LL;
		info.size = 0;
		info.errno = (char)md_mem_resv_size;
		info.md_type = 0;
	}
	info.md_id = MD_SYS1;
	insert_hdr_info(&info);

	if (success) {
		/* Configure HW remap */
		md_mem_ro_rw_remapping(MD_SYS1, (unsigned int)load_addr);
		/* Enable MPU setting */
		ccci_mem_access_protection_init(MD_SYS1, load_addr, md_mem_resv_size, chk_hdr, mem_access_list);
		retrieve_padding_ret = retrieve_free_padding_mem(MD_SYS1, load_addr,
		                       mem_access_list, chk_hdr, free_mpu_region);
		if (retrieve_padding_ret == 0) {
			if (insert_ccci_tag_inf("md1_mpu", (char*)mem_access_list, sizeof(mem_access_list)) < 0)
				ALWAYS_LOG("insert md1_mpu fail\n");
		}
		ccci_mem_access_cfg(mem_access_list, 0);
		if (retrieve_padding_ret == 0) {
			/* For MD has pre-fetch, padding memory may be read access by MD,
			** add a mpu region setting for MD read only requirement */
			mem_access_list[0].region = MPU_REGION_ID_RESERVED2;
			mem_access_list[0].permission = SET_ACCESS_PERMISSON(
			                                mpu_att_default[mem_access_list[0].region][7],
			                                mpu_att_default[mem_access_list[0].region][6],
			                                mpu_att_default[mem_access_list[0].region][5],
			                                mpu_att_default[mem_access_list[0].region][4],
			                                mpu_att_default[mem_access_list[0].region][3],
			                                mpu_att_default[mem_access_list[0].region][2],
			                                mpu_att_default[mem_access_list[0].region][1],
			                                mpu_att_default[mem_access_list[0].region][0]);
			mem_access_list[0].start = (unsigned int)load_addr;
			mem_access_list[0].end = mem_access_list[0].start + md_mem_resv_size;
			/* 64K align */
			mem_access_list[0].end = ((mem_access_list[0].end + 0xFFFF)&(~0xFFFF)) - 1;
			mem_access_list[0].relate_region = 0;
			mem_access_list[1].region = -1;
			ccci_mem_access_cfg(mem_access_list, 0);
		}

		/* update load success flag */
		md_load_status_flag |= 1<<MD_SYS1;
		/* update tag info */
		if (insert_ccci_tag_inf("md1_chk", (char*)chk_hdr, sizeof(struct md_check_header_v5)) < 0)
			printf("insert md1_chk fail\n");
	}

	printf("-- MD3 --\n");
	success = 0;
	/* Init MPU region */
	mem_access_list[0].region = MPU_REGION_ID_MD3_ROM;
	mem_access_list[1].region = MPU_REGION_ID_MD3_RW;
	mem_access_list[2].region = -1;

	/* Clear MPU setting */
	ccci_mem_access_cfg(mem_access_list, 1);

	/* Load image */
	md_mem_resv_size = load_img(md3_download_list, &load_addr, chk_hdr_v1);
	if (md_mem_resv_size > 0) {
		info.base_addr = (unsigned long long)((unsigned long)load_addr);
		info.size = md_mem_resv_size;
		info.errno = 0;
		info.md_type = chk_hdr_v1->common.image_type;
		success = 1;
	} else {
		info.base_addr = 0LL;
		info.size = 0;
		info.errno = (char)md_mem_resv_size;
		info.md_type = 0;
	}
	info.md_id = MD_SYS3;
	insert_hdr_info(&info);

	if (success) {
		/* Configure HW remap */
		md_mem_ro_rw_remapping(MD_SYS3, (unsigned int)load_addr);
		/* Enable MPU setting */
		ccci_mem_access_protection_init(MD_SYS3, load_addr, md_mem_resv_size, chk_hdr_v1, mem_access_list);
		ccci_mem_access_cfg(mem_access_list, 0);

		/* update load success flag */
		md_load_status_flag |= 1<<MD_SYS3;

		/* update tag info */
		if (insert_ccci_tag_inf("md3_chk", (char*)chk_hdr_v1, sizeof(struct md_check_header_v1)) < 0)
			printf("insert md3_chk fail\n");
	}
	printf("Exit load_modem_image\n");

	if (!(md_load_status_flag & (1<<MD_SYS1))) {
		/* check md1 enough, no image load, need free share memory too */
		goto _free_memory;
	}

	/* share memory setting */
	smem_size = cal_share_mem_layout(smem_addr);
	if (smem_size < MAX_SMEM_SIZE) {
		printf("re-size share memory form %x to %x\n", MAX_SMEM_SIZE, smem_size);
		ccci_resize_reserve_mem(smem_addr, MAX_SMEM_SIZE, smem_size);
	}

	/* Configure smem hw remap and lock it */
	if (md_load_status_flag & (1<<MD_SYS1)) {
		md_smem_rw_remapping(MD_SYS1, (unsigned int)smem_info.base_addr + smem_info.ap_md1_smem_offset);
		md_emi_remapping_lock(MD_SYS1);
	}

	if (md_load_status_flag & (1<<MD_SYS3)) {
		md_smem_rw_remapping(MD_SYS3, (unsigned int)smem_info.base_addr + smem_info.md1_md3_smem_offset);
		md_emi_remapping_lock(MD_SYS3);
	}

	ccci_share_mem_access_protection_init(mem_access_list);
	ccci_mem_access_cfg(mem_access_list, 0);

	if (chk_hdr)
		free(chk_hdr);

	printf("load_modem_image init cost %d ms ----\n", (int)get_timer(time_lk_md_init));

	return;

_free_memory:
	if (smem_addr != NULL) {
		memset(smem_addr, 0, curr_offset);
		ccci_lk_info_init(0LL);
		ccci_resize_reserve_mem(smem_addr, MAX_SMEM_SIZE, 0);
	}

	if (chk_hdr)
		free(chk_hdr);

	printf("load_modem_image init cost %d ms ----\n", (int)get_timer(time_lk_md_init));
}

#endif

