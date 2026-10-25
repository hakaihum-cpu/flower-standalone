package com.posewalk.studio;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.graphics.Color;
import android.content.res.Configuration;

public final class MainActivity extends Activity {
    private WebView browser;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setNavigationBarColor(Color.rgb(234, 233, 228));
        getWindow().setStatusBarColor(Color.rgb(234, 233, 228));
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);

        browser = new WebView(this);
        browser.setBackgroundColor(Color.rgb(234, 233, 228));
        browser.setOverScrollMode(View.OVER_SCROLL_NEVER);
        WebSettings webSettings = browser.getSettings();
        webSettings.setJavaScriptEnabled(true);
        webSettings.setAllowFileAccess(true);
        webSettings.setAllowContentAccess(false);
        webSettings.setDomStorageEnabled(false);
        webSettings.setMediaPlaybackRequiresUserGesture(true);
        browser.loadUrl("file:///android_asset/index.html");
        setContentView(browser);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    protected void onDestroy() {
        if (browser != null) {
            browser.loadUrl("about:blank");
            browser.destroy();
            browser = null;
        }
        super.onDestroy();
    }
}
