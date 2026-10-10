package com.example.epsampler;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.TextureView;
import android.view.View;

/** Read-only visual adapter. Never touches audio, transport, or media decoding. */
final class DotVisualOverlay extends View {
    private static final int W = 512, H = 384;
    private static final int[] BAYER = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    private static final int[] PALETTE = {0xff100e08,0xff80671e,0xffffdb46,0xffffe57a};
    static Bitmap makeDotBitmap(Bitmap original) {
        int w=Math.max(1,Math.min(512,original.getWidth()));
        int h=Math.max(1,Math.min(512,original.getHeight()));
        Bitmap scaled=Bitmap.createScaledBitmap(original,w,h,false);
        int[] input=new int[w*h], result=new int[w*h];
        scaled.getPixels(input,0,w,0,0,w,h);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            int i=y*w+x,c=input[i];
            float l=(0.2126f*Color.red(c)+0.7152f*Color.green(c)+0.0722f*Color.blue(c))/255f;
            float threshold=0.13f+0.63f*BAYER[(x&3)+((y&3)<<2)]/16f;
            result[i]=PALETTE[l<threshold?0:l>0.88f?3:l>0.63f?2:1];
        }
        Bitmap out=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
        out.setPixels(result,0,w,0,0,w,h);
        if(scaled!=original) scaled.recycle();
        return out;
    }

    private final TextureView source;
    private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final Paint uiPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Bitmap sample;
    private Bitmap output;
    private int[] pixels;
    private int[] dots;
    private boolean active;
    private int tick;
    private final Runnable update = new Runnable() {
        @Override public void run() {
            if (!active) return;
            invalidate();
            postDelayed(this, 100L);
        }
    };

    DotVisualOverlay(Context context, TextureView source) {
        super(context);
        this.source = source;
        setClickable(false);
        setFocusable(false);
        setVisibility(GONE);
    }

    void setDotEnabled(boolean enabled) {
        active = enabled;
        setVisibility(enabled ? VISIBLE : GONE);
        removeCallbacks(update);
        if (enabled) post(update);
    }

    @Override protected void onDetachedFromWindow() {
        removeCallbacks(update);
        if(sample!=null) {sample.recycle();sample=null;}
        if(output!=null) {output.recycle();output=null;}
        pixels=null;
        dots=null;
        super.onDetachedFromWindow();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!active || getWidth() < 1 || getHeight() < 1) return;
        int w = Math.min(W, Math.max(1, getWidth()));
        int h = Math.min(H, Math.max(1, getHeight()));
        if (sample == null || sample.getWidth() != w || sample.getHeight() != h) {
            if (sample != null) sample.recycle();
            if (output != null) output.recycle();
            sample = Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
            output = Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
            pixels = new int[w*h];
            dots = new int[w*h];
        }
        if (source.isAvailable()) {
            Bitmap frame = source.getBitmap(sample);
            if (frame != null) {
                frame.getPixels(pixels,0,w,0,0,w,h);
                for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
                    int i=y*w+x,c=pixels[i];
                    float l=(0.2126f*Color.red(c)+0.7152f*Color.green(c)+0.0722f*Color.blue(c))/255f;
                    float threshold=0.13f+0.63f*BAYER[(x&3)+((y&3)<<2)]/16f;
                    dots[i]=PALETTE[l<threshold?0:l>0.88f?3:l>0.63f?2:1];
                }
                output.setPixels(dots,0,w,0,0,w,h);
            }
        }
        canvas.drawColor(PALETTE[0]);
        paint.setFilterBitmap(false);
        canvas.drawBitmap(output,new Rect(0,0,w,h),new RectF(0,0,getWidth(),getHeight()),paint);
        uiPaint.setColor(PALETTE[2]);
        uiPaint.setStyle(Paint.Style.STROKE);
        uiPaint.setStrokeWidth(2f);
        canvas.drawRect(3,3,getWidth()-3,getHeight()-3,uiPaint);
        uiPaint.setStyle(Paint.Style.FILL);
        uiPaint.setTypeface(android.graphics.Typeface.MONOSPACE);
        uiPaint.setTextSize(15f);
        canvas.drawText("DOT  /  EP-SAMPLE",14,24,uiPaint);
        tick++;
    }
}
