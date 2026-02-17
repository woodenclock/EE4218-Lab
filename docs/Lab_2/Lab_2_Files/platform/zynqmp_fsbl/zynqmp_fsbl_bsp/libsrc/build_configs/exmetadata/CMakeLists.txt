
cmake_minimum_required(VERSION 3.15)
project(bsp)

# Use CMAKE_LIBRARY_PATH in link_directories
link_directories(${CMAKE_LIBRARY_PATH})
        
set(DRIVER_TARGETS llfifo;scugic;zdma;sysmonpsu;axipmon;coresightps_dcc;resetps;clockps;csudma;ddrcpsu;gpiops;iicps;ipipsu;rtcpsu;ttcps;uartps;wdtps)
set(DRIVER_LOCATIONS C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/llfifo_v6_0;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/scugic_v5_6;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/zdma_v1_21;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/sysmonpsu_v3_2;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/axipmon_v6_13;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/coresightps_dcc_v1_13;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/resetps_v1_7;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/clockps_v1_6;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/csudma_v1_18;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/ddrcpsu_v1_8;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/gpiops_v3_14;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/iicps_v3_22;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/ipipsu_v2_18;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/rtcpsu_v1_16;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/ttcps_v3_22;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/uartps_v3_17;C:/AMDDesignTools/2025.2/Vitis/data/embeddedsw/XilinxProcessorIPLib/drivers/wdtps_v3_8)

list(LENGTH DRIVER_TARGETS no_of_drivers)
set(index 0)

while(${index} LESS ${no_of_drivers})
    list(GET DRIVER_TARGETS ${index} drv)
    list(GET DRIVER_LOCATIONS ${index} drv_dir)
    set(exp_dir "${drv_dir}/examples")
    add_custom_target(
        ${drv}_example ALL
        COMMAND lopper -f -O C:/Users/ELVIS/EE4218-Lab/EE4218-Lab/docs/Lab_2/Lab_2_Files/platform/zynqmp_fsbl/zynqmp_fsbl_bsp/libsrc/${drv} C:/Users/ELVIS/EE4218-Lab/EE4218-Lab/docs/Lab_2/Lab_2_Files/platform/zynqmp_fsbl/zynqmp_fsbl_bsp/hw_artifacts/psu_cortexa53_0_baremetal.dts -- bmcmake_metadata_xlnx psu_cortexa53_0 ${exp_dir} drvcmake_metadata
        BYPRODUCTS x${drv}_exlist.yaml)
    MATH(EXPR index "${index}+1")
endwhile()
