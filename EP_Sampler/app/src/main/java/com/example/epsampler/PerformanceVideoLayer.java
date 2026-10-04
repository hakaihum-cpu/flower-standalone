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
    private static final String[] ASSET_NAMES = new String[] {
            "violin_bg.mp4",
            "flute_bg.mp4",
            "saxophone_bg.mp4",
            "felt_piano_bg.mp4",
            "pianica_accordion_bg.mp4",
            "xylophone_bg.mp4",
            "wood_bass_bg.mp4",
            "drums_bg.mp4"
    };
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
    private int instrumentMode = 0;
    private int assetGeneration = 0;

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

        prepareAssetAsync(context.getApplicationContext(), 0);
    }

    boolean isVideoReady() {
        return prepared && textureView.isAvailable();
    }

    void setInstrument(int mode) {
        mode = Math.max(0, Math.min(ASSET_NAMES.length - 1, mode));
        if (mode == instrumentMode && (fileReady || prepared)) return;

        instrumentMode = mode;
        allNotesOff();
        resetPlayerForSourceChange();
        prepareAssetAsync(getContext().getApplicationContext(), mode);
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
        dreamyOverlay.noteOn(note, velocity, heldCount);
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

    private void prepareAssetAsync(Context context, int requestedMode) {
        final int generation = ++assetGeneration;
        final int mode = Math.max(0, Math.min(ASSET_NAMES.length - 1, requestedMode));

        new Thread(() -> {
            File readyFile = null;
            try {
                String requested = ASSET_NAMES[mode];
                String chosen = requested;
                InputStream probe;
                try {
                    probe = context.getAssets().open(requested);
                } catch (Exception missing) {
                    chosen = ASSET_NAMES[0];
                    probe = context.getAssets().open(chosen);
                }

                File out = new File(context.getCacheDir(),
                        "instrument_bg_" + mode + "_" + chosen.replace('.', '_'));
                try (InputStream in = probe;
                     FileOutputStream fos = new FileOutputStream(out, false)) {
                    byte[] buffer = new byte[64 * 1024];
                    int n;
                    while ((n = in.read(buffer)) >= 0) fos.write(buffer, 0, n);
                    fos.flush();
                }

                if (out.exists() && out.length() > 1024L) readyFile = out;
            } catch (Exception ignored) { }

            final File result = readyFile;
            post(() -> {
                if (generation != assetGeneration) return;
                cachedVideo = result;
                fileReady = result != null;
                if (fileReady) maybeStartPlayer();
            });
        }, "InstrumentVideoAsset").start();
    }

    private void resetPlayerForSourceChange() {
        prepared = false;
        fileReady = false;
        reverseMode = false;
        handler.removeCallbacks(reverseTick);
        if (player != null) {
            try { player.release(); } catch (Exception ignored) { }
            player = null;
        }
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
                try {
                    mp.seekTo(0, MediaPlayer.SEEK_CLOSEST);
                } catch (Exception ignored) { }
                if (!pausedByLifecycle) updateMotion();
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

        // Silence means a true freeze. Do not let the background drift when
        // nothing is being played.
        if (heldCount == 0) {
            reverseMode = false;
            handler.removeCallbacks(reverseTick);
            try {
                if (player.isPlaying()) player.pause();
            } catch (IllegalStateException ignored) { }
            return;
        }

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
        }

        long now = SystemClock.uptimeMillis();
        int recent = recentCount(now, 500);
        float speed;

        if (recent >= 4) {
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
                int duration = player.getDuration();
                if (duration <= 0) duration = VIDEO_DURATION_MS;
                int pos = player.getCurrentPosition() - step;
                while (pos < 0) pos += duration;
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
        private static final int HISTORY = 6;
        private static final int CAPTURE_SIZE = 480;

        private final TextureView source;
        private final Paint red = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint green = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint blue = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint echo = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint slice = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint block = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Paint scanline = new Paint();
        private final Rect srcRect = new Rect();
        private final RectF dstRect = new RectF();

        private final Bitmap[] history = new Bitmap[HISTORY];
        private int historyWrite = 0;
        private int historyCount = 0;

        private boolean dreamy = false;
        private int x = 36;
        private int y = 36;
        private long lastCaptureMs = 0L;
        private long holdUntilMs = 0L;
        private int holdBack = 2;
        private int frameCounter = 0;
        private int eventCounter = 0;
        private float notePulse = 0.0f;

        DreamyOverlay(Context context, TextureView source) {
            super(context);
            this.source = source;
            setWillNotDraw(false);

            red.setColorFilter(channelFilter(0));
            green.setColorFilter(channelFilter(1));
            blue.setColorFilter(channelFilter(2));

            red.setAlpha(84);
            green.setAlpha(42);
            blue.setAlpha(84);
            echo.setAlpha(58);
            slice.setAlpha(148);
            block.setAlpha(178);
            scanline.setColor(0x28FFFFFF);
            scanline.setStrokeWidth(1.0f);
        }

        void setDreamy(boolean on) {
            dreamy = on;
            setVisibility(on ? VISIBLE : INVISIBLE);
            if (!on) {
                holdUntilMs = 0L;
                notePulse = 0.0f;
            }
            if (on) postInvalidateOnAnimation();
        }

        void setAmount(int x, int y) {
            this.x = Math.max(0, Math.min(127, x));
            this.y = Math.max(0, Math.min(127, y));
            if (dreamy) postInvalidateOnAnimation();
        }

        void noteOn(int note, int velocity, int voices) {
            if (!dreamy) return;

            eventCounter++;
            notePulse = Math.max(notePulse, Math.max(1, Math.min(127, velocity)) / 127.0f);

            // MIYAKO-style temporal fragment: not every note causes a glitch.
            // Higher Y makes the short held fragment happen more often and last
            // a little longer, while X selects a deeper point in recent history.
            int densityDivisor = Math.max(2, 7 - Math.round((y / 127.0f) * 4.0f));
            if ((eventCounter % densityDivisor) == 0 || voices >= 3) {
                long now = SystemClock.uptimeMillis();
                holdUntilMs = now + 55L + Math.round((y / 127.0f) * 155.0f);
                holdBack = 1 + Math.round((x / 127.0f) * 3.0f);
            }
            postInvalidateOnAnimation();
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            if (!dreamy || !source.isAvailable() || getWidth() <= 0 || getHeight() <= 0) return;

            final long now = SystemClock.uptimeMillis();
            final float amountX = x / 127.0f;
            final float amountY = y / 127.0f;

            // Y follows the audio Dreamy idea: higher values mean shorter,
            // denser fragments. Capture faster as Y rises.
            final long captureInterval = 135L - Math.round(amountY * 75.0f);
            if (now - lastCaptureMs >= captureInterval) {
                captureHistory();
                lastCaptureMs = now;
            }

            Bitmap current = historyBack(0);
            if (current == null) {
                postInvalidateDelayed(50L);
                return;
            }

            Bitmap recent = historyBack(1 + Math.round(amountX * 1.0f));
            Bitmap older = historyBack(3 + Math.round(amountX * 2.0f));
            if (recent == null) recent = current;
            if (older == null) older = recent;

            RectF full = new RectF(0f, 0f, getWidth(), getHeight());

            // Two time-offset ghosts correspond to the two-fragment character
            // of MIYAKO Dreamy. They are intentionally subtle rather than a
            // permanent full-screen RGB split.
            float drift = getWidth() * (0.003f + 0.024f * amountX);
            float pulse = 0.45f + 0.55f * notePulse;
            int channelAlpha = Math.min(130, Math.round((38f + 58f * amountY) * pulse));
            red.setAlpha(channelAlpha);
            blue.setAlpha(channelAlpha);

            dstRect.set(full);
            dstRect.offset(-drift, -getHeight() * 0.003f * amountX);
            canvas.drawBitmap(recent, null, dstRect, red);

            dstRect.set(full);
            dstRect.offset(drift, getHeight() * 0.002f * amountX);
            canvas.drawBitmap(older, null, dstRect, blue);

            // A very faint green temporal echo gives a smeared, not neon-RGB,
            // centre to the image.
            green.setAlpha(Math.min(72, Math.round(20f + 34f * amountY)));
            dstRect.set(full);
            dstRect.inset(-getWidth() * 0.004f * amountX, -getHeight() * 0.004f * amountX);
            canvas.drawBitmap(recent, null, dstRect, green);

            // Short held fragment: the live full-quality MP4 remains below;
            // only the Dreamy layer momentarily freezes an older image.
            if (now < holdUntilMs) {
                Bitmap held = historyBack(holdBack);
                if (held != null) {
                    echo.setAlpha(Math.min(118, Math.round(44f + 58f * amountY)));
                    float jx = getWidth() * 0.006f * (float)Math.sin(frameCounter * 0.71);
                    float jy = getHeight() * 0.004f * (float)Math.cos(frameCounter * 0.53);
                    dstRect.set(full);
                    dstRect.offset(jx, jy);
                    canvas.drawBitmap(held, null, dstRect, echo);
                }
            }

            drawSliceDrift(canvas, recent, older, amountX, amountY);
            drawSparseBlocks(canvas, older, amountX, amountY);
            drawScanlines(canvas, amountY);

            // Brief brightness instability rather than constant flicker.
            if (((frameCounter + eventCounter * 3) % 17) == 0 && amountY > 0.18f) {
                echo.setAlpha(Math.round(10f + 30f * amountY));
                canvas.drawBitmap(current, null, full, echo);
            }

            notePulse *= 0.90f;
            frameCounter++;
            postInvalidateDelayed(50L);
        }

        private void captureHistory() {
            Bitmap target = history[historyWrite];
            if (target == null || target.isRecycled()) {
                target = Bitmap.createBitmap(CAPTURE_SIZE, CAPTURE_SIZE, Bitmap.Config.ARGB_8888);
                history[historyWrite] = target;
            }
            try {
                source.getBitmap(target);
                historyWrite = (historyWrite + 1) % HISTORY;
                historyCount = Math.min(HISTORY, historyCount + 1);
            } catch (Exception ignored) { }
        }

        private Bitmap historyBack(int back) {
            if (historyCount <= 0) return null;
            back = Math.max(0, Math.min(historyCount - 1, back));
            int index = historyWrite - 1 - back;
            while (index < 0) index += HISTORY;
            return history[index % HISTORY];
        }

        private void drawSliceDrift(Canvas canvas, Bitmap a, Bitmap b, float amountX, float amountY) {
            if (historyCount < 2 || amountY < 0.06f) return;

            int slices = 1 + Math.round(amountY * 7.0f);
            int bw = a.getWidth();
            int bh = a.getHeight();
            long state = 0xD1B54A32D192ED03L ^ ((long)frameCounter * 1103515245L)
                    ^ ((long)eventCounter * 2654435761L);

            slice.setAlpha(Math.min(185, Math.round(72f + 88f * amountY)));

            for (int i = 0; i < slices; i++) {
                state = nextRandom(state);
                int sh = Math.max(3, Math.round(bh * (0.010f + (((state >>> 12) & 255) / 255f) * 0.055f)));
                int sy = (int)Math.floorMod(state >>> 20, Math.max(1, bh - sh));
                Bitmap src = ((i & 1) == 0) ? a : b;

                float scaleY = getHeight() / (float)bh;
                float dy0 = sy * scaleY;
                float dy1 = (sy + sh) * scaleY;

                state = nextRandom(state);
                float direction = ((state & 1L) == 0L) ? -1f : 1f;
                float dx = direction * getWidth()
                        * (0.004f + amountX * 0.055f)
                        * (0.4f + (((state >>> 8) & 255) / 255f) * 0.6f);

                srcRect.set(0, sy, bw, sy + sh);
                dstRect.set(dx, dy0, getWidth() + dx, dy1);
                canvas.drawBitmap(src, srcRect, dstRect, slice);
            }
        }

        private void drawSparseBlocks(Canvas canvas, Bitmap sourceBitmap, float amountX, float amountY) {
            if (amountY < 0.18f) return;

            // Sparse anomaly events, closer to MIYAKO/Twilight than a constant
            // block-noise overlay.
            int gatePeriod = Math.max(4, 12 - Math.round(amountY * 7.0f));
            if (((frameCounter + eventCounter) % gatePeriod) != 0) return;

            int blocks = 1 + Math.round(amountY * 5.0f);
            int bw = sourceBitmap.getWidth();
            int bh = sourceBitmap.getHeight();
            long state = 0x9E3779B97F4A7C15L ^ ((long)frameCounter * 6364136223846793005L);

            block.setAlpha(Math.min(205, Math.round(105f + 82f * amountY)));

            for (int i = 0; i < blocks; i++) {
                state = nextRandom(state);
                int sw = Math.max(12, Math.round(bw * (0.05f + (((state >>> 8) & 255) / 255f) * 0.18f)));
                state = nextRandom(state);
                int sh = Math.max(8, Math.round(bh * (0.025f + (((state >>> 12) & 255) / 255f) * 0.09f)));
                state = nextRandom(state);
                int sx = (int)Math.floorMod(state, Math.max(1, bw - sw));
                state = nextRandom(state);
                int sy = (int)Math.floorMod(state, Math.max(1, bh - sh));

                float scaleX = getWidth() / (float)bw;
                float scaleY = getHeight() / (float)bh;
                float dx = ((((state >>> 18) & 255) / 255f) - 0.5f)
                        * getWidth() * (0.02f + amountX * 0.13f);
                float dy = ((((state >>> 28) & 127) / 127f) - 0.5f)
                        * getHeight() * amountY * 0.035f;

                srcRect.set(sx, sy, sx + sw, sy + sh);
                dstRect.set(
                        sx * scaleX + dx,
                        sy * scaleY + dy,
                        (sx + sw) * scaleX + dx,
                        (sy + sh) * scaleY + dy);
                canvas.drawBitmap(sourceBitmap, srcRect, dstRect, block);
            }
        }

        private void drawScanlines(Canvas canvas, float amountY) {
            if (amountY < 0.25f) return;
            int count = 2 + Math.round(amountY * 4.0f);
            scanline.setAlpha(Math.min(62, Math.round(16f + 32f * amountY)));

            long state = 0x94D049BB133111EBL ^ ((long)frameCounter * 0x9E3779B97F4A7C15L);
            for (int i = 0; i < count; i++) {
                state = nextRandom(state);
                float yy = (Math.floorMod(state, 10000L) / 10000f) * getHeight();
                canvas.drawLine(0f, yy, getWidth(), yy, scanline);
            }
        }

        void release() {
            for (int i = 0; i < HISTORY; i++) {
                Bitmap bitmap = history[i];
                if (bitmap != null && !bitmap.isRecycled()) bitmap.recycle();
                history[i] = null;
            }
            historyCount = 0;
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
