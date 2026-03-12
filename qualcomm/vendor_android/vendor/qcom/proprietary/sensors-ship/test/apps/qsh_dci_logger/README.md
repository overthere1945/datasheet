Notes:
1) This Application forwards Sensors F3 DEBUG Message Log packets from DSP to Logcat for accesability.
2) It removes the need to have QXDM for viewing DEBUG Messages from DSP.
3) It uses the DCI interface provided by DIAG Library to route Log Packets.
4) Only non qshrink encoded F3 message packets can be decoded by this application.
5) Only one DCI based client application should be run at a time.
6) Only Log packets enabled in the diag config file will be routed.
7) Logs with SENSOR_INT SSID will not be processed.


Step 1: Run Application from adb shell < Terminal #1>:
    adb wait-for-device root
    adb shell
    qsh_dci_logger [Press Ctrl + C to exit when done...]

Step 2: Run Logcat to view Sensor F3 DEBUG Messages < Terminal #2>:
    adb wait-for-device root
    adb logcat -v raw -s QSH_LOG:I

Usage for qsh_dci_logger:
-h  --usage:     Display supported options    (optional)
-p  --f3msg:     Printf forwarded F3 Messages (optional)
-m  --mfile:     Custom DIAG Mask CFG file    (optional)
                 Default DIAG Mask CFG file is: /data/vendor/sensors/diag_cfg/qsh_F3_logs.cfg
