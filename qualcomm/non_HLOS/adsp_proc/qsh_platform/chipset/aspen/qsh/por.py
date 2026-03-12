#==============================================================================
# POR Config Script
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#==============================================================================

include_sensor_vendor_libs = []
include_oem_libs = []
include_qsh_libs = []
include_qsh_algorithms_libs = []
exclude_libs = []
include_ssc_algorithm_libs = []
libs_from_island_to_bigimage = []
# Table to store the default mapping of directories to memory segment in the format
# { "directory_name_1" : "memory_segment_1", "directory_name_2" : "memory_segment_2", .... }
#
# Note :
# 1. The paths defined in the table must be relative to 'BUILD_ROOT'
# 2. This default mapping can be overwritten by using the optional "segment"
#    parameter passed to the add_ssc_su function
# 3. If a mapping is not defined in the table and not passed through add_ssc_su,
#    the default "ssc" memory segment will be used
# 4. Update the table in the target specific por.py
dir_to_segment = {}

# ==============================================================================
# Helper functions for common configuration
# ==============================================================================
    
def common_island_features(env):
    # 0. Framework
    env.AddUsesFlags(['SNS_ISLAND_INCLUDE_CM'])

    # 1. Island drivers
    # Temporarily removing ENG from island to accommodate memory for audio in island
    #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_STENG1AX'])


    # 2. Calibration
    # 3. Algorithms group 4
    #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_OEM1'])
    #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_AI_OEM1'])

    # 5. Test
    #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_TEST_SENSOR'])
    env.AddUsesFlags(['SNS_ISLAND_INCLUDE_DA_TEST'])

    # Move algorithm library to big image which is previously generated for 
    # island in pack build
    #libs_from_island_to_bigimage.extend(['sns_fmv'])
    #env.Replace(LIBS_FROM_ISLAND_TO_BIGIMAGE=libs_from_island_to_bigimage)

def target_qsh_sensors_list(env):
    if env.PathExists("${BUILD_ROOT}/qsh_drivers") and env.GetUsesFlag('USES_QSH_SENSORS') and not env.GetUsesFlag('SENSORS_DD_DEV_FLAG'):
        include_qsh_libs.extend(['qsh_wifi',
                                 'qsh_wifi_test',
                                 'qsh_wwan',
                                 'qsh_modem',
                                 'qsh_test'
                                 ])

        #qsh audio sensors
        include_qsh_libs.extend(['qsh_audio_utils',
                                 'qsh_audio_test',
                                 'qsh_audio_event',
                                 'qsh_audio_context',
                                 'qsh_audio_upd_proxy',
                                 'qsh_audio_data',
                                 'qsh_audio_ultrasound_renderer'])
                                 
        
        #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_QSH_AUDIO'])
        #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_QSH_TEST'])

        #if env.GetUsesFlag('USES_UART_ISLAND'):
            #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_QSH_BLE'])

        env.Replace(SSC_INCLUDE_QSH_LIBS=include_qsh_libs)
        env.Append(CPPDEFINES = ['SSC_QSH_SENSORS_LIST'])

    else:
        env.Replace(SSC_INCLUDE_QSH_LIBS={})
    if env.PathExists("${BUILD_ROOT}/ssc_algorithms") and env.GetUsesFlag('USES_QSH_SENSORS') and not env.GetUsesFlag('SENSORS_DD_DEV_FLAG'):
      
    #Currently no ssc algorthims are PORed on ADSP              

        env.Replace(SSC_INCLUDE_SSC_ALGORITHMS=include_ssc_algorithm_libs)      
        env.Append(CPPDEFINES = ['QSH_ALGORITHMS_LIST'])
        

def exists(env):
    return env.Detect('por')

def generate(env):

    env.Append(CCFLAGS= ' -ferror-limit=0')

    # ==========================================================================
    # Configuration common for target and sim
    # ==========================================================================
    
    # Thread configuration
    env.Append(CPPDEFINES = ['SNS_NUM_OF_WORKER_THREADS_POOL_H=2'])
    env.Append(CPPDEFINES = ['SNS_NUM_OF_WORKER_THREADS_POOL_L=2'])
    env.Append(CPPDEFINES = ['SNS_ISLAND_REMOVE_PROXY_VOTE'])
    
    #No CCD / SDC
    env.Append(CPPDEFINES = ['SSC_TARGET_NO_CCD'])

    
    # Enable batch sensor compilation
    env.AddUsesFlags(['USES_SNS_BATCH'])

    #Enable RFS file system 
    env.AddUsesFlags(['SSC_ENABLE_RFS_FILE_SYSTEM'])
    
    # SSC Algorithms list list
    env.Append(CPPDEFINES = ['SSC_ALGORITHMS_LIST'])

    if env.IsBuildInternal():
        #env.AddUsesFlags(['SNS_ENABLE_QSOCKET_TEST_CLIENT'])
        env.AddUsesFlags(['SNS_ENABLE_DCF_TEST_SENSOR'])
        #env.AddUsesFlags(['SNS_ENABLE_LATENCY_PROFILING'])
        #env.AddUsesFlags(['USES_ENABLE_BOOT_UP_PROFILING'])
        env.AddUsesFlags(['SNS_ENABLE_POWER_DEBUG'])
        env.AddUsesFlags(['USES_SNS_FW_DEBUG'])
        env.Append(CPPDEFINES = ['USES_DRIVER_CRASH_ON_INVALID_STATE'])


    # Sensor Utilities version
    env.Append(CPPDEFINES = ['SSC_ISLAND_3_0'])
    env.Append(CPPDEFINES = ['SSC_PWR_SLEEP_MGR_4_0'])
    env.Append(CPPDEFINES = ['SSC_TIMER_1_0'])
    env.AddUsesFlags(['SNS_QURT_HAS_SET_STACK_SIZE2'])

    # Enable F3s in Island Mode
    env.AddUsesFlags(['USES_SNS_ENABLE_ISLAND_F3'])

    env.Append(CPPDEFINES = ['SSC_TCM_BW_ENABLE'])
    env.Append(CPPDEFINES = ['SSC_LPI_LS_BW_DISABLED'])
    
    #Enable overwrite registry path 
    env.Append(CPPDEFINES = ['SNS_TARGET_OVERWRITE_REGISTRY_PATH'])
        
    env.Append(CPPDEFINES = ["SNS_TARGET_CONFIG_DIR=\\\"/vendor/etc/sensors/hub1/config\\\""])
    env.Append(CPPDEFINES = ["SNS_TARGET_REGISTRY_CONFIG=\\\"/vendor/etc/sensors/hub1/sns_reg_config\\\""])
    
    # SNS HW Convert NPM Mode to OPM
    env.Append(CPPDEFINES = ['SNS_PWR_OPM_ENABLED'])
    
    # Control new compiler option
    env.AddUsesFlags(['USES_NO_FRAME_ADDRESS'])

    # Diag disable flags.
    #env.Append(CPPDEFINES = ['SNS_PRINTF_DISABLED'])
    #env.Append(CPPDEFINES = ['SNS_LOG_DISABLED'])

    env.AddUsesFlags(['USES_QSH_IN_STD_ISLAND'])
    # Island Mode Configuration
    if 'USES_QSH_IN_STD_ISLAND' in env:
        env.Append(CPPDEFINES = ['ENABLE_QSH_IN_STD_ISLAND'])
        env.Append(CPPDEFINES = ['ENABLE_SSC_IN_STD_ISLAND'])
        env.Append(CPPDEFINES = ['ENABLE_QSHTECH_IN_STD_ISLAND'])
    
    env.Append(CPPDEFINES = ['QSHTECH_LLC_POOL_VOTE_DISABLE'])
    
    # Island features.
    common_island_features(env)

    # Update the directories to memory segment mapping
    dir_to_segment[env['SENSINGHUB_PLATFORM_WS_PATH'] + '/framework'] = 'qsh'
    dir_to_segment[env['SENSINGHUB_PLATFORM_WS_PATH'] + '/utils'] = 'qsh'
    dir_to_segment[env['SENSINGHUB_PLATFORM_WS_PATH'] + '/pal'] = 'qsh'
    dir_to_segment[env['SENSINGHUB_ALGORITHMS_WS_PATH']] = 'qshtech'
    dir_to_segment[env['SENSORS_ALGORITHMS_WS_PATH']] = 'ssc'
    env.Replace(SSC_DIR_TO_SEGMENT_MAPPING=dir_to_segment)

    # ==========================================================================
    # Configuration for target
    # ==========================================================================
    
    if 'SSC_TARGET_X86' not in env['CPPDEFINES']:
        
        if ('USES_BUILD_ISV' in env or 'SENSORS_ALGO_DEV_FLAG' in env):
          env.Append(CPPDEFINES = ['SENSORS_ALGO_DEV_FLAG'])
          env.AddUsesFlags(['SENSORS_ALGO_DEV_FLAG'])
    
        # Add USES_SNS_USE_RAMFS to target_variation.xml file
        # Uncomment USES_SNS_STANDALONE_SIM to enable OnlinePB on RUMI
        #env.AddUsesFlags(['USES_SNS_STANDALONE_SIM'])

        # POR sensors list
        #if 'USES_SNS_STANDALONE_SIM' not in env:
        include_sensor_vendor_libs.extend(['sns_da_test','sns_ct7117x'])
#        include_sensor_vendor_libs.extend(['sns_da_test','sns_himaxdsp'])
        # TO DO : ENG Sensor is disabled due to I2C_TIMEOUT crash seen on WRD device on SE3
        # However Issue is not seen on other devices ( WDP, IDPS )
        # Need to work with buses team for further analysis.

        env.Replace(SSC_INCLUDE_SENS_VEND_LIBS=include_sensor_vendor_libs)
        #Qsh_Router related
        env.Append(CPPDEFINES = ["SNS_REMOTE_REMOTE_LINK_NAME=\\\"wm\\\""])
        env.Append(CPPDEFINES = [('SNS_REMOTE_HUB_ID', '2')])
        env.Append(CPPDEFINES = [('SNS_REMOTE_NUM_OF_CHANNELS', '1')])
       
        # enable extra debug information in osa lock
        env.AddUsesFlags(['ENABLE_OSA_LOCK_DEBUG'])
        # Intercom Sensor
        #env.AddUsesFlags(['SNS_ISLAND_INCLUDE_SNS_INTERCOM'])
        env.AddUsesFlags(['SNS_INTERCOM_ENABLE'])
        env.Append(CPPDEFINES = ['SNS_IPC_GLINK_DEBUG'])
        env.Append(CPPDEFINES = ['SNS_ROUTER_BATCH_EVENT_INTENT_SIZE=4096'])
        env.Append(CPPDEFINES = ['SNS_GLINK_USES_TCM'])
        
        # Disable Island Vote in QSHTech
        env.Append(CPPDEFINES = ['QSHTECH_ISLAND_VOTE_UTIL_DISABLE'])
		
        #disable registry for BU 
        #env.AddUsesFlags(['SNS_DISABLE_REGISTRY'])
        
        # enable thread manager task start time debug time stamp
        env.AddUsesFlags(['ENABLE_TMGR_START_TS_DEBUG'])

        # enable auxiliary client manager debug time stamps
        env.AddUsesFlags(['ENABLE_CM_TS_DEBUG'])

        # enable auxiliary qcm debug time stamps
        env.AddUsesFlags(['ENABLE_QCM_TS_DEBUG'])

        #Enable this flag to achieve ADSP in standlone mode
        #Going forward STAND ALONE flags from target_variation.xml file are removed

        #env.AddUsesFlags(['USES_SNS_STANDALONE'])
        #env.Append(CPPDEFINES = ['SNS_RUMI_PLATFORM'])
        # The test_client is enabled by default in qsh_platform.scons through the definition 
        # of the SNS_ENABLE_QSOCKET_TEST_CLIENT flag. By declaring the flag below, we are disabling test_client.
        env.AddUsesFlags(['SNS_DISABLE_QSOCKET_TEST_CLIENT'])
        # exclude sns_ccd_proximity_proxy
        exclude_libs.extend(['sns_ccd_proximity_proxy'])
        # exclude sim libs
        exclude_libs.extend(['sns_sim', 'sensinghub_sim_utils','sns_test_sim'])
        # excluding test libs
        exclude_libs.extend(['sns_qsocket_test', 'cm_qsocket'])
        # exclude device_mode sensor
        exclude_libs.extend(['sns_device_mode'])
        # disable sns_sim island
        env.Append(CPPDEFINES = ['SNS_SIM_ISLAND_DISABLE'])
        # Enabling the island for ASCP as ENG sensor will support batching in the future.
        # disable async com port sensor island
        # env.AddUsesFlags(['SNS_ASYNC_COM_PORT_ISLAND_DISABLE'])
        # disable sns_batch sensor island
        env.AddUsesFlags(['SNS_BATCH_ISLAND_DISABLE'])
        # disable resampler sensor island
        env.AddUsesFlags(['SNS_RESAMPLER_ISLAND_DISABLE'])
        
        
        

        if 'SNS_RUMI_PLATFORM' in env['CPPDEFINES']:
            env.Append(CPPDEFINES = ['SNS_ENABLE_FIXED_CLOCK_VOTING'])
        else:
            # enable watchdog for development/debug
            env.Append(CPPDEFINES = ['SNS_ENABLE_WATCHDOG'])

        if 'USES_SNS_STANDALONE' in env:
            env.Append(CPPDEFINES = ['SNS_STANDALONE'])
            if 'USES_SNS_USE_RAMFS' not in env:
              env.AddUsesFlags(['SNS_DISABLE_REGISTRY'])
            env.AddUsesFlags(['USES_ENABLE_SNS_TEST_SENSOR'])

        if 'USES_SNS_USE_RAMFS' in env:
            env.Append(CPPDEFINES = ['SSC_ENABLE_RAMFS_FILE_SYSTEM'])
            env.Append(CPPDEFINES = ['SNS_TARGET_OVERWRITE_REGISTRY_PATH'])
            env.Append(CPPDEFINES = ["SNS_TARGET_CONFIG_DIR=\\\"/ramfs\\\""])
            env.Append(CPPDEFINES = ["SNS_TARGET_REGISTRY_CONFIG=\\\"\\\""])

        # Enable Additional DEBUG for FILE SERVICE
        env.AddUsesFlags(['USES_ENABLE_FILE_SERVICE_DEBUG'])

        # Multi Island Support with Dont Care vote
        env.Append(CPPDEFINES = ['SNS_USES_MULTI_ISLAND'])
        
        # OEM sensors list
        include_qsh_algorithms_libs.extend(['sns_ai_oem1','sns_oem1'])
        env.Replace(QSH_ALGORITHMS_INCLUDE_LIBS=include_qsh_algorithms_libs)

        env.AddUsesFlags(['OEM1_SUPPORT_DIRECT_SENSOR_REQUEST'])
        env.Append(CPPDEFINES = ['OEM1_SUPPORT_DIRECT_SENSOR_REQUEST'])

        # QSH sensors list
        target_qsh_sensors_list(env)

        env.Replace(SSC_EXCLUDE_LIBS=exclude_libs)

        # Library Registration count for algo sensors
        env.Replace(SNS_MULTIPLE_SENSORS_MAX_COUNT=2)

        if 'SSC_BUILD_TAGS' in env and env.IsKeyEnable(env['SSC_BUILD_TAGS']) is True:
            env.Append(CPPDEFINES = ['SSC_TARGET_HEXAGON_CORE_QDSP6_2_0'])
            env.Append(CPPDEFINES = ['SSC_TARGET_HEXAGON_CORE_QDSP6_3_0'])
            env.Append(CPPDEFINES = ['SSC_TARGET_PRAM_AVAILABLE'])

            env.Append(CPPDEFINES = ['SSC_TARGET_IPCC_AVAILABLE'])
            env.Append(CPPDEFINES = ['SNS_THREAD_UTILIZATION_ENABLED']) #to be removed once DCVS team adds clock voting

            if 'USES_CORE_MEMORY_OPT_CHRE' not in env:
                env.Append(CPPDEFINES = ['SNS_ISLAND_INCLUDE_DIAG'])

            #PEND: Remove following line once CoreBSP resolves compiler warnings
            env.Append(CPPDEFINES = ['SNS_USE_LOCAL_CLK_SRC'])
            # Temporarily removing I2C from island to accommodate memory for audio in island
            #env.Append(CPPDEFINES = ['SNS_ISLAND_INCLUDE_I2C'])
            env.Append(CPPDEFINES = ['SNS_INCLUDE_UART'])
            #env.Append(CPPDEFINES = ['SNS_ISLAND_INCLUDE_SPI'])

            # Dynamic libraries support
            env.AddUsesFlags(['SNS_SHARELIB_BUILDER'])
            #env.AddUsesFlags(['SNS_DYNLIB_GEOMAG_RV'])
            env.AddUsesFlags(['SNS_DYNLIB_TPPE'])

            env.AddUsesFlags(['USES_SNS_ULOG'])
            
            if 'QDSS_TRACER_SWE' in env:
                env.Append(CPPDEFINES = ['SNS_LOCAL_TRACER_IDS'])
                env.SWEBuilder(['${BUILDPATH}/sns_tracer_event_ids.h'],None)
                env.Append(CPPPATH = ['${BUILD_ROOT}/qsh_platform/build/${BUILDPATH}'])

        # Enable online playback compilation
        env.AddUsesFlags(['USES_ONLINE_PLAYBACK'])

        if 'qsh_ble' in include_qsh_libs:
            env.AddUsesFlags(['SNS_ENABLE_TPPE_SENSOR'])
            env.AddUsesFlags(['SNS_ENABLE_CLIENT_SENSOR'])

        if 'qsh_audio_data' in include_qsh_libs:
            env.AddUsesFlags(['SNS_QSH_AUDIO_ENABLED'])

        # This flag is used to determine if bus power needs to be turned on for 
        # IBI registration/de-registation
        if 'USES_BUSES_IBI_CTRL' in env:
          env.Append(CPPDEFINES = ['SNS_I2C_BUS_PWR_OFF'])

        # After turning on power rails wait for this amount of time before resetting the bus
        env.Append(CPPDEFINES = ['SNS_I2C_COM_PORT_OFF_TO_IDLE_NS=100000000'])

    # ==========================================================================
    # Configuration for sim
    # ==========================================================================

    else: #if 'SSC_TARGET_X86' not in env['CPPDEFINES']:

        if env.PathExists("${BUILD_ROOT}/qsh_drivers/qsh_ble"):
            env.AddUsesFlags(['SNS_ENABLE_TPPE_SENSOR'])
            env.AddUsesFlags(['SNS_ENABLE_CLIENT_SENSOR'])
            env.AddUsesFlags(['SNS_ISLAND_INCLUDE_TPPE'])

        if env.PathExists("${BUILD_ROOT}/qsh_drivers/qsh_audio"):
            env.AddUsesFlags(['SNS_QSH_AUDIO_ENABLED'])
        
        # enable watchdog for development/debug
        env.Append(CPPDEFINES = ['SNS_ENABLE_WATCHDOG'])

    # ==========================================================================
    # Shared configuration for target and Sim
    # ==========================================================================
    if 'SNS_DISABLE_REGISTRY' in env:
        env.Append(CPPDEFINES = ['SNS_DISABLE_REGISTRY'])
    else:
        # Enable Additional DEBUG for Registry Sensor
        env.AddUsesFlags(['USES_SNS_REGISTRY_DEBUG'])

