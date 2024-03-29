/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2011
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE. 
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

#include "sec_platform.h"
#include "nand_core.h"
#include "boot_device.h"
#include "cust_bldr.h"
#include "cust_sec_ctrl.h"
#include "sec_boot.h"
#include "sec.h"

/******************************************************************************
 * CONSTANT DEFINITIONS                                                       
 ******************************************************************************/
#define MOD                         "LIB"

/******************************************************************************
 * DEBUG
 ******************************************************************************/
#define SEC_DEBUG                   (FALSE)
#define SMSG                        print
#if SEC_DEBUG
#define DMSG                        print
#else
#define DMSG 
#endif

/******************************************************************************
 *  EXTERNAL VARIABLES
 ******************************************************************************/
extern struct nand_chip             g_nand_chip;
extern boot_arg_t                   bootarg;

/******************************************************************************
 *  INTERNAL VARIABLES
 ******************************************************************************/
BOOL bDumpPartInfo                  = FALSE;
static BOOL bUsbHandshakeSuccess    = FALSE;

/******************************************************************************
 *  RETURN AVAILABLE BUFFER FOR S-BOOT CHECK
 ******************************************************************************/
U8* sec_util_get_secro_buf (void)
{
    return (U8*) SEC_SECRO_BUF;
}
 
U8* sec_util_get_img_buf (void)
{
    return (U8*) SEC_IMG_BUF;
}

U8* sec_util_get_working_buf (void)
{
    return (U8*) SEC_WORKING_BUF;
}

/******************************************************************************
 *  READ IMAGE FOR S-BOOT CHECK (FROM NAND or eMMC DEVICE)
 ******************************************************************************/
U32 sec_util_read_image (U8* img_name, U8 *buf, U64 offset, U32 size)
{
    BOOL ret            = SEC_OK;
    U32 i               = 0;
    U32 cnt             = 0;

    U32 total_pages     = 0;
    blkdev_t *bootdev   = NULL;
    part_t *part        = NULL;
    U64 src;


    if (NULL == (bootdev = blkdev_get(CFG_BOOT_DEV))) 
    {
        SMSG("[%s] can't find boot device(%d)\n", MOD, CFG_BOOT_DEV);
        ASSERT(0);
    }

    /* ======================== */
    /* get part info            */
    /* ======================== */
    /* part_get should be device abstraction function */    
    if(NULL == (part = part_get(img_name)))
    {
        SMSG("[%s] part_get fail\n", MOD);
    }

    /* ======================== */
    /* read part data           */
    /* ======================== */
    /* part_load should be device abstraction function */ 
    if(TRUE == bDumpPartInfo)
    {
        SMSG("[%s] part load '0x%x'\n", MOD, part->start_sect * bootdev->blksz);
        bDumpPartInfo = FALSE;
    }
    src = part->start_sect * bootdev->blksz + offset;

    if (0 != blkdev_read(bootdev, src, size, buf, part->part_id))
    {
        SMSG("[%s] part_load fail\n", MOD);
        ASSERT(0);        
    }

    if (NULL != part)
    {
        put_part(part);
    }
    
    return ret;
}

/******************************************************************************
 *  WRITE IMAGE FOR S-BOOT USAGE (FROM NAND or eMMC DEVICE)
 ******************************************************************************/
static U32 sec_util_write_image (U8* img_name, U8 *buf, U64 offset, U32 size)
{
    BOOL ret            = SEC_OK;
    U32 i               = 0;
    U32 cnt             = 0;

    U32 total_pages     = 0;
    blkdev_t *bootdev   = NULL;
    part_t *part        = NULL;
    U64 dest;


    if (NULL == (bootdev = blkdev_get(CFG_BOOT_DEV))) 
    {
        SMSG("[%s] can't find boot device(%d)\n", MOD, CFG_BOOT_DEV);
        ASSERT(0);
    }

    /* ======================== */
    /* get part info            */
    /* ======================== */
    /* part_get should be device abstraction function */    
    if(NULL == (part = part_get (img_name)))
    {
        SMSG("[%s] part_get fail\n", MOD);
        ASSERT(0);        
    }

    /* ======================== */
    /* write part data           */
    /* ======================== */
    /* part_load should be device abstraction function */ 
    if(TRUE == bDumpPartInfo)
    {
        SMSG("[%s] part load '0x%x'\n", MOD, part->start_sect * bootdev->blksz);
        bDumpPartInfo = FALSE;
    }
    dest = part->start_sect * bootdev->blksz + offset;
    
    if (0 != blkdev_write(bootdev, dest, size, buf, part->part_id))
    {
        SMSG("[%s] part_store fail\n", MOD);
        ASSERT(0);        
    }

    if (NULL != part)
    {
        put_part(part);
    }
    
    return ret;
}

static BOOL sec_util_force_brom_download_recovery(void)
{    
    #define SEC_PL_ERASE_SIZE 2048
    u8 *sec_buf = sec_util_get_img_buf();

    memset(sec_buf,0,SEC_PL_ERASE_SIZE);

    if(SEC_OK != sec_util_write_image (SBOOT_PART_PL, sec_buf, 0, SEC_PL_ERASE_SIZE)) 
    {
        SMSG("[%s] Write image fail for seek offset 0x%x\n",MOD,0); 
        return FALSE;    
    }

    SMSG("[%s] Force brom download recovery success\n", MOD);
    return TRUE;
}

BOOL sec_util_brom_download_recovery_check(void)
{
#ifdef KPD_DL_KEY2    	
    if (mtk_detect_key (KPD_DL_KEY2) && FALSE==bUsbHandshakeSuccess 
        && is_BR_cmd_disabled())
    {
        SMSG("[%s] Start checking (1500 ms)\n", MOD);
        mdelay(1500);

        if(false == mtk_detect_key (KPD_DL_KEY2))
        {        
            SMSG("[%s] Key is not detected, wait for 1500ms \n", MOD);
            mdelay(1500);
            if(mtk_detect_key (KPD_DL_KEY2))            
        {        
            SMSG("[%s] Key is detected\n", MOD);
            return sec_util_force_brom_download_recovery();
        }
        else
        {
            SMSG("[%s] Key is not detected\n", MOD);
            return FALSE;
        }
    }
        else
        {
            SMSG("[%s] Key is detected\n", MOD);
            return FALSE;
        }
    }
#endif
    return FALSE;
}

void sec_set_usb_handshake_status(BOOL status_ok)
{
    bUsbHandshakeSuccess = status_ok;
}

void sec_util_force_entering_fastboot_mode(void){
    u32 addr = CFG_UBOOT_MEMADDR;
    blkdev_t *bootdev;

    g_boot_mode = FASTBOOT;
    platform_set_boot_args();
    bldr_jump(addr, &bootarg, sizeof(boot_arg_t));
       
error:   
    print("error on jumping to fastboot mode\n");
    while(1);
}



