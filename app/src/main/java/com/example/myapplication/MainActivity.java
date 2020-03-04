package com.example.myapplication;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.lody.whale.xposed.XC_MethodHook;
import com.lody.whale.xposed.XposedHelpers;
import com.lody.whale.xposed.callbacks.XCallback;

public class MainActivity extends AppCompatActivity {

    private TextView tvHelloword;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        tvHelloword.setText(getString());
    }


    private void initView() {
        tvHelloword = (TextView) findViewById(R.id.tv_helloword);
    }

    public void AddHook(View view) {
        XposedHelpers.findAndHookMethod(MainActivity.class,
                "getString",new Hook());
        tvHelloword.setText(getString());

    }

    public String getString(){
        return "1234";
    }
}
