package com.example.androidapp;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.TextView;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        TextView textView = new TextView(this);
        textView.setText(getString(R.string.bootstrap_message));
        textView.setGravity(Gravity.CENTER);
        textView.setTextSize(18f);
        setContentView(textView);
    }
}
