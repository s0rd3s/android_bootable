#  L2 Cache Share Sram contains:
#  4KB GFH              0x0020_0000 ~ 0x0020_1000
#  212KB Preloader      0x0020_1000 ~ 0x0023_6000
#  40KB BSS             0x0023_6000 ~ 0x0023_FFFF

# RAM size include bss section listed in link_descriptor.ld
# bss size is around 35KB, thus we evaluate RAM size upto 40KB
PL_MAX_RAM_SIZE=40960

# Keep ROM size smaller than 192 KB (Move Preloader BSS into Share SRAM)
# Keep ROM size smaller than 0x0023_6000 - 0x0020_1000 =0x35000 (217088) Bytes
# start address is 0x0020_1000, GFH file is placed in the front of the start address
#  - Hash(32) - Signature(260) ) (292)
PL_MAX_ROM_SIZE=$(( 217088 - 292 ))

ROM_RPT_SIZE=500
RAM_RPT_SIZE=500

size `ls ${PRELOADER_OUT}/obj/*.o` > ${PRELOADER_OUT}/report-objsize.txt

# check image rom size
PL_ROM_SIZE=$(stat -c%s "${PL_IMAGE}")

if [ $PL_ROM_SIZE -gt $PL_MAX_ROM_SIZE ]; then
    echo "===================== Building Fail ==========================="
    echo " IMG ROM Size: ${PL_ROM_SIZE} bytes > ${PL_MAX_ROM_SIZE} bytes!! "
    echo " WARNING: Reduce your code size first then compile again!! "
    echo "==============================================================="
    echo "                    ROM SIZE REPORT (>${ROM_RPT_SIZE} bytes)   "
    echo "---------------------------------------------------------------"
    cat report-codesize.txt | awk -F' ' '{if ($3=="FUNC" && $2>sz) print $0}' sz=${ROM_RPT_SIZE}
    echo "---------------------------------------------------------------"
    rm ${PL_IMAGE}
    echo "BUILD FAIL."
    exit 1;
fi

# check image ram size
PL_RAM_SIZE=$(mawk '{if($3=="_bss_start") {BSS_START=("0x" $1)+0} ;
             if($3=="_bss_end") {BSS_END=("0x" $1)+0}}
             END{printf("%d\n",BSS_END-BSS_START)}' ${PL_FUN_MAP})
if [ $PL_RAM_SIZE -gt $PL_MAX_RAM_SIZE ]; then
    echo "===================== Building Fail ==========================="
    echo " IMG RAM Size: ${PL_RAM_SIZE} bytes > ${PL_MAX_RAM_SIZE} bytes!!"
    echo " WARNING: Reduce your code size first then compile again!! "
    echo "==============================================================="
    echo "                    RAM SIZE REPORT (>${RAM_RPT_SIZE} bytes)   "
    echo "---------------------------------------------------------------"
    cat report-codesize.txt | awk -F' ' '{if ($3=="OBJECT" && $2>sz) print $0}' sz=${RAM_RPT_SIZE}
    echo "---------------------------------------------------------------"
    rm -f ${PL_IMAGE}
    echo "BUILD FAIL."
    exit 1;
fi

# check image dram size
PL_DRAM_SIZE=$(mawk '{if($3=="_dram_start") {DRAM_START=("0x" $1)+0} ;
             if($3=="_dram_end") {DRAM_END=("0x" $1)+0}}
             END{printf("%d\n",DRAM_END-DRAM_START)}' ${PL_FUN_MAP})

PL_SIZE=`expr $PL_RAM_SIZE + $PL_ROM_SIZE`
echo "===================== Building Success ==========================="
echo " PL_MAX_ROM_SIZE:  ${PL_MAX_ROM_SIZE} bytes"
echo " PL ROM size:  ${PL_ROM_SIZE} bytes"
echo " PL RAM size:  ${PL_RAM_SIZE} bytes"
echo " PL DRAM size: ${PL_DRAM_SIZE} bytes"
echo " PL ROM+RAM size: ${PL_SIZE} bytes"
echo "=================================================================="


