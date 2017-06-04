#include <platform/partition.h>
#include <platform/partition_wp.h>
#include <printf.h>
#include <platform/boot_mode.h>
#include <platform/mtk_wdt.h>
#include <platform/env.h>


#ifdef MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
extern int is_protect2_ready_for_wp(void);
extern int sync_sml_data(void);
#endif

int set_write_protect()
{
	int err = 0;
	if(g_boot_mode == NORMAL_BOOT){
		/*Group 1*/
		dprintf(CRITICAL, "[%s]: Lock boot region \n", __func__);
		err = partition_write_prot_set("preloader", "preloader", WP_POWER_ON);
	
		if(err != 0){
			dprintf(CRITICAL, "[%s]: Lock boot region failed: %d\n", __func__,err);
			return err;
		}
		/*Group 2*/
	#ifdef MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
        /* sync protect1 sml data to protect2 if needed */
        int sync_ret = 0;   

        mtk_wdt_disable();
        
        sync_ret = sync_sml_data();
        if(0 != sync_ret)
        {
            dprintf(INFO,"sml data not sync \n");
        }else{
            dprintf(INFO,"sml data sync \n");
        }
        
        mtk_wdt_init();
        
        if(g_boot_mode == NORMAL_BOOT) {
            if (0 == is_protect2_ready_for_wp()) {
                dprintf(CRITICAL,"[%s]: protect2 is fmt \n", __func__);
                dprintf(CRITICAL, "[%s]: Lock protect2 \n", __func__);
                err = partition_write_prot_set("protect2", "protect2" , WP_POWER_ON);
                if(err != 0){
                    dprintf(CRITICAL, "[%s]: Lock protect region failed:%d\n", __func__,err);
                    return err;
                }
            }
        }

	#endif
	
		/*Group 4*/
		if(TRUE == seclib_sec_boot_enabled(TRUE)){
			dprintf(CRITICAL, "[%s]: Lock oemkeystore->system \n", __func__);
			err = partition_write_prot_set("oemkeystore", "system", WP_POWER_ON);
			if(err != 0){
				dprintf(CRITICAL, "[%s]: Lock oemkeystore->system failed:%d\n", __func__,err);
				return err;
			}
		} else {
			dprintf(CRITICAL, "[%s]: Lock oemkeystore->keystore \n", __func__);
			err = partition_write_prot_set("oemkeystore", "keystore", WP_POWER_ON);
			if(err != 0){
				dprintf(CRITICAL, "[%s]: Lock oemkeystore->keystore failed:%d\n", __func__,err);
				return err;
			}
		}
	}

	/*Group 3*/
	dprintf(CRITICAL, "[%s]: Lock seccfg \n", __func__);
	err = partition_write_prot_set("seccfg", "seccfg", WP_POWER_ON);
	if(err != 0){
		dprintf(CRITICAL, "[%s]: Lock seccfg  failed:%d\n", __func__,err);
		return err;
	}
	


	return 0;
}

void write_protect_flow()
{
	int bypass_wp = 0;
	int ret = 0;

#ifndef USER_BUILD
	bypass_wp= atoi(get_env("bypass_wp"));
	dprintf(CRITICAL, "bypass write protect flag = %d! \n",bypass_wp);
#endif

	if(!bypass_wp){
		ret = set_write_protect();
		if(ret != 0)
			dprintf(CRITICAL, "write protect fail! \n");
		dprintf(CRITICAL, "write protect Done! \n");
	} else
		dprintf(CRITICAL, "Bypass write protect! \n");
}

