/*
 * 목적: ADSP의 인터럽트 대안으로, Native C++ 백그라운드 스레드를 실행시켜 이미지를 가져오는 액티비티
 * 기능:
 * 1. 앱 실행 시 JNI로 startNativeThread()를 호출하여 C++ 백그라운드 감시 루프 시작
 * 2. C++ 스레드에서 직접 "바이트 배열(byte[])"을 콜백으로 넘겨줌
 * 3. 디스크 읽기를 생략하고 곧바로 바이트 배열을 비트맵으로 디코딩하여 화면에 즉시 갱신함
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

public class MainActivity extends AppCompatActivity {

    private ImageView imageView;
    private static final String TAG = "VISION_TEST";

    // Used to load the 'vision' library on application startup.
    static {
        System.loadLibrary("vision");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        checkStoragePermission();

        // Example of a call to a native method
        TextView tv = binding.sampleText;

        tv.setText("start");

        imageView = findViewById(R.id.myimageview); // layout에 정의된 ID

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

         /* // 기존 1초마다 도는 루프 로직은 제거 (불필요한 CPU 소모 방지)
		runnable = new Runnable() {
            @Override
            public void run() {
                boolean bReceivedNewImage = false;
                try {
                    // JNI 함수 호출 (이전 단계에서 만든 데이터 읽기 함수)
                    bReceivedNewImage = readFromUio(false);
                } catch (Exception e) {
                    Log.e(TAG, "JNI Call Failed: " + e.getMessage());
                }

                if (bReceivedNewImage)
                {
                    updateImage();
                    Log.d(TAG, "Received New VGA Image");
                }
                else
                {
                    Log.d(TAG, "No Update VGA Image");
                }
                // 1000ms(1초) 후에 자기 자신을 다시 호출
                handler.postDelayed(this, 1000);
            }
        };
		*/
        // change(add)-hyungchul-20260306-1400 끝
    }
    
    private void checkStoragePermission() {
        // Android 11 (API 30) 이상은 READ_EXTERNAL_STORAGE가 필요합니다.
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.MANAGE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {

            // 권한 팝업 띄우기
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
    protected void onResume() {
        super.onResume();
        // 화면이 보일 때 호출 시작
        // handler.post(runnable); // 기존 코드 비활성화
        
        // change(add)-hyungchul-20260306-1400 시작
        // 설명: 화면이 켜지면 센서(인터럽트) 이벤트를 수신하도록 리스너를 등록합니다.
        Log.d(TAG, "Starting Native Polling Thread...");
        startNativeThread(true); // true = 유사도 필터링 켜기
        // change(add)-hyungchul-20260306-1400 끝
    }

    @Override
    protected void onPause() {
        super.onPause();
        // 화면이 가려지면 배터리 절약을 위해 중지
        // handler.removeCallbacks(runnable); // 기존 코드 비활성화

        // change(add)-hyungchul-20260306-1400 시작
        Log.d(TAG, "Stopping Native Polling Thread...");
        stopNativeThread();
    }

    // change(add)-hyungchul-20260306-1720 시작
    // 설명: C++ 스레드로부터 파일시스템을 거치지 않고 메모리 상의 원본 JPEG 데이터를 직접 수신합니다.
    // 레이스 컨디션에 의한 디코딩 실패 확률이 0%가 되며, UI 반응 속도가 비약적으로 상승합니다.
    public void onImageReceived(byte[] jpegData) {
        Log.d(TAG, "Callback Received with " + jpegData.length + " bytes! Decoding image...");
        
        // C++ 백그라운드 스레드에서 곧바로 비트맵으로 디코딩 (메인 스레드의 부하를 줄이기 위함)
        final Bitmap bitmap = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.length);
        
        // UI 변경은 반드시 메인(UI) 스레드에서 수행해야 하므로 runOnUiThread를 사용합니다.
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (bitmap != null) {
                    imageView.setImageBitmap(bitmap);
                    Log.d(TAG, "Image successfully updated on screen!");
                } else {
                    Log.e(TAG, "Bitmap decoding FAILED! Invalid JPEG data.");
                }
            }
        });
    }
    // change(add)-hyungchul-20260306-1720 끝
    /**
     * JNI 선언부 변경: 단발성 호출이 아닌 스레드 관리 함수로 변경
     */
    public native void startNativeThread(boolean filtered);
    public native void stopNativeThread();
    // change(add)-hyungchul-20260306-1610 끝


    /**
     * A native method that is implemented by the 'vision' native library,
     * which is packaged with this application.
     */
//    public native boolean readFromUio(boolean fillted);
}