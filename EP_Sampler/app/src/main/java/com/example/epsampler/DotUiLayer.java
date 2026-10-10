package com.example.epsampler;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.widget.FrameLayout;

/**
 * Visual-only UI postprocessor. Children retain their original hit targets,
 * listeners, positions and z-order; only the final pixels are transformed.
 */
final class DotUiLayer extends FrameLayout {
    private static final int[] BAYER={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    private static final int[] COLORS={0xff100e08,0xff80671e,0xffffdb46,0xffffe57a};
    private boolean dotEnabled;
    private Bitmap source, rendered;
    private int[] srcPixels, dstPixels;
    private final Paint blit=new Paint();

    DotUiLayer(Context context) {
        super(context);
        setWillNotDraw(false);
        setClipChildren(false);
    }

    void setDotEnabled(boolean enabled) {
        if (dotEnabled==enabled) return;
        dotEnabled=enabled;
        invalidate();
    }

    @Override protected void onDetachedFromWindow() {
        if(source!=null) {source.recycle();source=null;}
        if(rendered!=null) {rendered.recycle();rendered=null;}
        srcPixels=null;
        dstPixels=null;
        super.onDetachedFromWindow();
    }

    @Override protected void dispatchDraw(Canvas canvas) {
        if (!dotEnabled || getWidth()<1 || getHeight()<1) {
            super.dispatchDraw(canvas);
            return;
        }
        final int w=Math.max(1,Math.min(512,getWidth()));
        final int h=Math.max(1,Math.min(384,getHeight()));
        if (source==null || source.getWidth()!=w || source.getHeight()!=h) {
            if(source!=null) source.recycle();
            if(rendered!=null) rendered.recycle();
            source=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
            rendered=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
            srcPixels=new int[w*h];
            dstPixels=new int[w*h];
        }
        source.eraseColor(Color.TRANSPARENT);
        Canvas offscreen=new Canvas(source);
        offscreen.scale(w/(float)getWidth(),h/(float)getHeight());
        super.dispatchDraw(offscreen);
        source.getPixels(srcPixels,0,w,0,0,w,h);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            int i=y*w+x,c=srcPixels[i],a=Color.alpha(c);
            if(a<16) {dstPixels[i]=Color.TRANSPARENT;continue;}
            float l=(0.2126f*Color.red(c)+0.7152f*Color.green(c)+0.0722f*Color.blue(c))/255f;
            float threshold=0.13f+0.63f*BAYER[(x&3)+((y&3)<<2)]/16f;
            int out=COLORS[l<threshold?0:l>0.88f?3:l>0.63f?2:1];
            dstPixels[i]=(Math.min(255,a)<<24)|(out&0x00ffffff);
        }
        rendered.setPixels(dstPixels,0,w,0,0,w,h);
        blit.setFilterBitmap(false);
        canvas.drawBitmap(rendered,new Rect(0,0,w,h),
                new RectF(0,0,getWidth(),getHeight()),blit);
    }
}
