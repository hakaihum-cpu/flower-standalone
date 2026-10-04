package com.example.epsampler;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.media.MediaPlayer;
import android.media.PlaybackParams;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Surface;
import android.graphics.SurfaceTexture;
import android.view.TextureView;
import android.view.View;
import android.widget.FrameLayout;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

final class PerformanceVideoLayer extends FrameLayout implements TextureView.SurfaceTextureListener {
    private static final String ASSET_NAME = "violin_bg.mp4";
    private static final int VIDEO_DURATION_MS = 8000;

    private final TextureView textureView;
    private final DreamyOverlay dreamyOverlay;
    private final Handler handler = new Handler(Looper.getMainLooper());

    private MediaPlayer player;
    private SurfaceTexture surfaceTexture;
    private File cachedVideo;
    private boolean fileReady = false;
    private boolean prepared = false;
    private boolean pausedByLifecycle = false;

    private final boolean[] held = new boolean[128];
    private int heldCount = 0;
    private int lastNote = -1;
    private int lastVelocity = 96;
    private long lastNoteOnMs = 0L;
    private final long[] recentNoteTimes = new long[12];
    private int recentWrite = 0;
    private int eventCounter = 0;

    private boolean reverseMode = false;
    private int dreamyX = 36;
    private int dreamyY = 36;

    PerformanceVideoLayer(Context context) {
        super(context);
        setBackgroundColor(0xFF000000);

        textureView = new TextureView(context);
        textureView.setSurfaceTextureListener(this);
        addView(textureView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        dreamyOverlay = new DreamyOverlay(context, textureView);
        addView(dreamyOverlay, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        prepareAssetAsync(context.getApplicationContext());
    }

    boolean isVideoReady() {
        return prepared && textureView.isAvailable();
    }

    void noteOn(int note, int velocity) {
        if (note >= 0 && note < held.length && !held[note]) {
            held[note] = true;
            heldCount++;
        }

        long now = SystemClock.uptimeMillis();
        recentNoteTimes[recentWrite] = now;
        recentWrite = (recentWrite + 1) % recentNoteTimes.length;
        eventCounter++;

        int interval = lastNote >= 0 ? Math.abs(note - lastNote) : 0;
        if (prepared && interval >= 7) {
            int jumpMs = 260 + Math.min(18, interval) * 42;
            jumpBy(note >= lastNote ? jumpMs : -jumpMs);
        }

        if (prepared && recentCount(now, 500) >= 4) {
            int jumpMs = 140 + (eventCounter % 5) * 70;
            jumpBy((eventCounter & 1) == 0 ? jumpMs : -jumpMs);
        }

        lastNote = note;
        lastVelocity = Math.max(1, Math.min(127, velocity));
        lastNoteOnMs = now;
        updateMotion();
        handler.postDelayed(this::updateMotion, 520L);
        handler.postDelayed(this::updateMotion, 950L);
    }

    void noteOff(int note) {
        if (note >= 0 && note < held.length && held[note]) {
            held[note] = false;
            heldCount = Math.max(0, heldCount - 1);
        }
        updateMotion();
    }

    void allNotesOff() {
        for (int i = 0; i < held.length; i++) held[i] = false;
        heldCount = 0;
        reverseMode = false;
        handler.removeCallbacks(reverseTick);
        updateMotion();
    }

    void setDreamy(boolean on) {
        dreamyOverlay.setDreamy(on);
    }

    void setDreamyXY(int x, int y) {
        dreamyX = Math.max(0, Math.min(127, x));
        dreamyY = Math.max(0, Math.min(127, y));
        dreamyOverlay.setAmount(dreamyX, dreamyY);
    }

    void pauseForLifecycle() {
        pausedByLifecycle = true;
        handler.removeCallbacks(reverseTick);
        if (player != null && prepared) {
            try { player.pause(); } catch (IllegalStateException ignored) { }
        }
    }

    void resumeFromLifecycle() {
        pausedByLifecycle = false;
        updateMotion();
    }

    void release() {
        handler.removeCallbacksAndMessages(null);
        prepared = false;
        if (player != null) {
            try { player.release(); } catch (Exception ignored) { }
            player = null;
        }
        dreamyOverlay.release();
    }

    private void prepareAssetAsync(Context context) {
        new Thread(() -> {
            try {
                File out = new File(context.getCacheDir(), "violin_bg_original.mp4");
                if (!out.exists() || out.length() < 1_600_000L) {
                    try (InputStream in = context.getAssets().open(ASSET_NAME);
                         FileOutputStream fos = new FileOutputStream(out, false)) {
                        byte[] buffer = new byte[64 * 1024];
                        int n;
                        while ((n = in.read(buffer)) >= 0) fos.write(buffer, 0, n);
                        fos.flush();
                    }
                }
                cachedVideo = out;
                fileReady = out.exists() && out.length() > 1_600_000L;
            } catch (Exception ignored) {
                fileReady = false;
            }
            post(this::maybeStartPlayer);
        }, "ViolinVideoAsset").start();
    }

    private void maybeStartPlayer() {
        if (!fileReady || surfaceTexture == null || player != null) return;

        try {
            MediaPlayer p = new MediaPlayer();
            Surface surface = new Surface(surfaceTexture);
            p.setSurface(surface);
            surface.release();
            p.setDataSource(cachedVideo.getAbsolutePath());
            p.setVolume(0.0f, 0.0f);
            p.setLooping(true);
            p.setOnPreparedListener(mp -> {
                prepared = true;
                applyCenterCrop(mp.getVideoWidth(), mp.getVideoHeight());
                if (!pausedByLifecycle) {
                    try {
                        mp.start();
                        setForwardSpeed(0.75f);
                    } catch (Exception ignored) { }
                    updateMotion();
                }
                invalidate();
            });
            p.setOnVideoSizeChangedListener((mp, width, height) -> applyCenterCrop(width, height));
            p.prepareAsync();
            player = p;
        } catch (Exception ignored) {
            prepared = false;
            if (player != null) {
                try { player.release(); } catch (Exception ignored2) { }
                player = null;
            }
        }
    }

    private void applyCenterCrop(int videoW, int videoH) {
        if (videoW <= 0 || videoH <= 0 || getWidth() <= 0 || getHeight() <= 0) return;

        float viewW = getWidth();
        float viewH = getHeight();
        float sx = viewW / videoW;
        float sy = viewH / videoH;
        float scale = Math.max(sx, sy);

        float xScale = scale / sx;
        float yScale = scale / sy;
        Matrix matrix = new Matrix();
        matrix.setScale(xScale, yScale, viewW * 0.5f, viewH * 0.5f);
        textureView.setTransform(matrix);
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (player != null && prepared) applyCenterCrop(player.getVideoWidth(), player.getVideoHeight());
    }

    private void updateMotion() {
        if (!prepared || player == null || pausedByLifecycle) return;

        boolean wantReverse = heldCount >= 3;
        if (wantReverse) {
            if (!reverseMode) {
                reverseMode = true;
                try { player.pause(); } catch (IllegalStateException ignored) { }
                handler.removeCallbacks(reverseTick);
                handler.post(reverseTick);
            }
            return;
        }

        if (reverseMode) {
            reverseMode = false;
            handler.removeCallbacks(reverseTick);
            try { player.start(); } catch (IllegalStateException ignored) { }
        }

        long now = SystemClock.uptimeMillis();
        int recent = recentCount(now, 500);
        float speed;

        if (heldCount == 0) {
            speed = 0.55f;
        } else if (recent >= 4) {
            speed = 2.0f;
        } else if (lastVelocity >= 108) {
            speed = 1.85f;
        } else if (now - lastNoteOnMs > 900L) {
            speed = 0.60f;
        } else {
            speed = 0.82f + 0.35f * (lastVelocity / 127.0f);
        }

        setForwardSpeed(speed);
    }

    private void setForwardSpeed(float speed) {
        if (player == null || !prepared) return;
        speed = Math.max(0.50f, Math.min(2.0f, speed));
        try {
            PlaybackParams params = player.getPlaybackParams();
            params.setSpeed(speed);
            params.setPitch(1.0f);
            player.setPlaybackParams(params);
            if (!player.isPlaying() && !pausedByLifecycle) player.start();
        } catch (Exception ignored) {
            try {
                if (!player.isPlaying() && !pausedByLifecycle) player.start();
            } catch (Exception ignored2) { }
        }
    }

    private final Runnable reverseTick = new Runnable() {
        @Override public void run() {
            if (!reverseMode || pausedByLifecycle || player == null || !prepared) return;
            try {
                int step = 90 + Math.round(70f * (lastVelocity / 127f));
                int pos = player.getCurrentPosition() - step;
                if (pos < 0) pos = Math.max(0, VIDEO_DURATION_MS + pos);
                player.seekTo(pos, MediaPlayer.SEEK_CLOSEST);
            } catch (Exception ignored) { }
            handler.postDelayed(this, 70L);
        }
    };

    private void jumpBy(int deltaMs) {
        if (player == null || !prepared) return;
        try {
            int duration = player.getDuration();
            if (duration <= 0) duration = VIDEO_DURATION_MS;
            int pos = player.getCurrentPosition() + deltaMs;
            while (pos < 0) pos += duration;
            while (pos >= duration) pos -= duration;
            player.seekTo(pos, MediaPlayer.SEEK_CLOSEST);
        } catch (Exception ignored) { }
    }

    private int recentCount(long now, long windowMs) {
        int count = 0;
        for (long t : recentNoteTimes) {
            if (t > 0L && now - t <= windowMs) count++;
        }
        return count;
    }

    @Override public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        surfaceTexture = surface;
        maybeStartPlayer();
    }

    @Override public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
        if (player != null && prepared) applyCenterCrop(player.getVideoWidth(), player.getVideoHeight());
    }

    @Override public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        surfaceTexture = null;
        prepared = false;
        handler.removeCallbacks(reverseTick);
        if (player != null) {
            try { player.release(); } catch (Exception ignored) { }
            player = null;
        }
        return true;
    }

    @Override public void onSurfaceTextureUpdated(SurfaceTexture surface) { }

    private static final class DreamyOverlay extends View {
        private final TextureView source;
        private final Paint red = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint green = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint blue = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint block = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Rect srcRect = new Rect();
        private final RectF dstRect = new RectF();

        private Bitmap capture;
        private boolean dreamy = false;
        private int x = 36;
        private int y = 36;
        private long lastCaptureMs = 0L;
        private int frameCounter = 0;

        DreamyOverlay(Context context, TextureView source) {
            super(context);
            this.source = source;
            setWillNotDraw(false);
            red.setColorFilter(channelFilter(0));
            green.setColorFilter(channelFilter(1));
            blue.setColorFilter(channelFilter(2));
            red.setAlpha(100);
            green.setAlpha(78);
            blue.setAlpha(100);
            block.setAlpha(190);
        }

        void setDreamy(boolean on) {
            dreamy = on;
            setVisibility(on ? VISIBLE : INVISIBLE);
            if (on) postInvalidateOnAnimation();
        }

        void setAmount(int x, int y) {
            this.x = Math.max(0, Math.min(127, x));
            this.y = Math.max(0, Math.min(127, y));
            if (dreamy) postInvalidateOnAnimation();
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            if (!dreamy || !source.isAvailable() || getWidth() <= 0 || getHeight() <= 0) return;

            long now = SystemClock.uptimeMillis();
            if (capture == null) {
                capture = Bitmap.createBitmap(320, 320, Bitmap.Config.ARGB_8888);
            }
            if (now - lastCaptureMs >= 120L) {
                try {
                    source.getBitmap(capture);
                    lastCaptureMs = now;
                    frameCounter++;
                } catch (Exception ignored) { }
            }

            if (capture == null || capture.isRecycled()) return;

            float amountX = x / 127f;
            float amountY = y / 127f;
            float shift = getWidth() * (0.004f + 0.025f * amountX);
            RectF full = new RectF(0f, 0f, getWidth(), getHeight());

            dstRect.set(full);
            dstRect.offset(-shift, 0f);
            canvas.drawBitmap(capture, null, dstRect, red);
            dstRect.set(full);
            canvas.drawBitmap(capture, null, dstRect, green);
            dstRect.set(full);
            dstRect.offset(shift, 0f);
            canvas.drawBitmap(capture, null, dstRect, blue);

            int blocks = 2 + Math.round(amountY * 10f);
            int bw = capture.getWidth();
            int bh = capture.getHeight();
            long state = 0x9E3779B97F4A7C15L ^ ((long) frameCounter * 1103515245L);

            for (int i = 0; i < blocks; i++) {
                state = nextRandom(state);
                int sw = Math.max(10, (int) (bw * (0.08f + ((state >>> 8) & 255) / 255f * 0.23f)));
                state = nextRandom(state);
                int sh = Math.max(6, (int) (bh * (0.025f + ((state >>> 12) & 255) / 255f * 0.10f)));
                state = nextRandom(state);
                int sx = (int) Math.floorMod(state, Math.max(1, bw - sw));
                state = nextRandom(state);
                int sy = (int) Math.floorMod(state, Math.max(1, bh - sh));

                float scaleX = getWidth() / (float) bw;
                float scaleY = getHeight() / (float) bh;
                float dx = (((state >>> 18) & 255) / 255f - 0.5f) *
                        getWidth() * (0.03f + amountX * 0.14f);
                float dy = (((state >>> 28) & 127) / 127f - 0.5f) *
                        getHeight() * amountY * 0.04f;

                srcRect.set(sx, sy, sx + sw, sy + sh);
                dstRect.set(
                        sx * scaleX + dx,
                        sy * scaleY + dy,
                        (sx + sw) * scaleX + dx,
                        (sy + sh) * scaleY + dy);
                canvas.drawBitmap(capture, srcRect, dstRect, block);
            }

            postInvalidateDelayed(60L);
        }

        void release() {
            if (capture != null && !capture.isRecycled()) capture.recycle();
            capture = null;
        }

        private static ColorMatrixColorFilter channelFilter(int channel) {
            float[] m = new float[] {
                    channel == 0 ? 1f : 0f, 0f, 0f, 0f, 0f,
                    0f, channel == 1 ? 1f : 0f, 0f, 0f, 0f,
                    0f, 0f, channel == 2 ? 1f : 0f, 0f, 0f,
                    0f, 0f, 0f, 1f, 0f
            };
            return new ColorMatrixColorFilter(new ColorMatrix(m));
        }

        private static long nextRandom(long x) {
            return x * 6364136223846793005L + 1442695040888963407L;
        }
    }
}
