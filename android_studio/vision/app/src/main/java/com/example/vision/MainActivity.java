/*
 * 파일 목적: Sensor Interrupt 방식을 사용하여 ADSP로부터 1FPS 저빈도 이미지를 수신하는 메인 화면
 * 파일 기능:
 * 1. 시스템에 등록된 Wake-up 센서(CT7117X)를 찾아 리스너를 등록함
 * 2. 수면(Sleep) 상태에서 인터럽트 수신 시, CPU가 다시 잠들지 않도록 WakeLock을 획득함
 * 3. 인터럽트 수신 즉시 JNI(readFromUio)를 1회 호출하여 공유 메모리에서 이미지를 출력
 */

package com.example.vision;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.WindowManager;
import android.widget.TextView;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.widget.ImageView;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.content.pm.PackageManager;
import com.example.vision.databinding.ActivityMainBinding;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import java.util.List;
import android.os.PowerManager;

public class MainActivity extends AppCompatActivity implements SensorEventListener {

    private ImageView imageView;
    private static final String TAG = "VISION_TEST";

    static {
        System.loadLibrary("vision");
    }

    private ActivityMainBinding binding;
    private SensorManager sensorManager;
    private Sensor adspSensor;
    private boolean isSensorRegistered = false;

    // 라인 설명: Sleep 상태에서 인터럽트를 받았을 때 이미지 디코딩이 끝날 때까지 CPU를 깨워두는 WakeLock 객체
    private PowerManager.WakeLock wakeLock;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        checkStoragePermission();

        TextView tv = binding.sampleText;
        tv.setText("Sensor Interrupt Ready. Waiting for ADSP...");

        imageView = findViewById(R.id.myimageview);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // WakeLock 초기화
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "VisionApp::SensorWakeLock");
        }

        // 수동 강제 읽기 트리거 (디버깅용 유지)
        binding.getRoot().setOnClickListener(new android.view.View.OnClickListener() {
            @Override
            public void onClick(android.view.View v) {
                Log.d(TAG, "[Manual Trigger] Screen Tapped! Forcing JNI read...");
                byte[] jpegData = readFromUio(false);
                if (jpegData != null && jpegData.length > 0) {
                    final Bitmap bitmap = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.length);
                    if (bitmap != null) {
                        imageView.setImageBitmap(bitmap);
                        Log.d(TAG, "[Manual Trigger] SUCCESS! Image is in memory.");
                    }
                }
            }
        });

        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (sensorManager != null) {
            List<Sensor> sensorList = sensorManager.getSensorList(Sensor.TYPE_ALL);
            for (Sensor sensor : sensorList) {
                if (sensor.getName() != null && sensor.getName().contains("CT7117X")) {
                    adspSensor = sensor;
                    break;
                }
            }

            if (adspSensor != null) {
                Log.d(TAG, "Successfully found Sensor: " + adspSensor.getName());

                if (!isSensorRegistered) {
                    // change(fix)-hyungchul-20260312-1700 시작
                    // 설명: 1FPS 스트리밍의 안정성을 확보하고 ADSP 내부의 5Hz 속도 상한선 필터링(거부)을 완벽히 피하기 위해,
                    // 1,000,000 마이크로초(1Hz)로 속도를 고정 등록합니다.
                    isSensorRegistered = sensorManager.registerListener(this, adspSensor, 1000000, 0);
                    Log.d(TAG, "Sensor Listener Registration Status: " + isSensorRegistered);
                    // change(fix)-hyungchul-20260312-1700 끝
                }
            } else {
                Log.e(TAG, "Sensor not found in HAL.");
            }
        }
    }

    private void checkStoragePermission() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.MANAGE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.MANAGE_EXTERNAL_STORAGE}, 100);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivity(intent);
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (sensorManager != null && isSensorRegistered) {
            sensorManager.unregisterListener(this);
            isSensorRegistered = false;
            Log.d(TAG, "Sensor Listener Unregistered on App Destroy.");
        }
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        Log.d(TAG, "Hardware Interrupt Received! Dummy Value: " + event.values[0]);

        // Sleep 상태에서 이벤트 수신 시, JNI 작업이 끝날 때까지 1초간 CPU가 잠들지 않도록 강제 유지
        if (wakeLock != null && !wakeLock.isHeld()) {
            wakeLock.acquire(1000);
        }

        byte[] jpegData = readFromUio(true);

        if (jpegData != null && jpegData.length > 0) {
            final Bitmap bitmap = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.length);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (bitmap != null) {
                        imageView.setImageBitmap(bitmap);
                        Log.d(TAG, "Image successfully updated via Interrupt!");
                    } else {
                        Log.e(TAG, "Bitmap decoding FAILED!");
                    }
                }
            });
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    public native byte[] readFromUio(boolean filtered);
}