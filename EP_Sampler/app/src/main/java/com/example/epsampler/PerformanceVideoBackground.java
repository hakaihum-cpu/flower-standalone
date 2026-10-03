package com.example.epsampler;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.Base64;
import android.view.View;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

final class PerformanceVideoBackground {
    private static final int ASSET_CHUNKS = 2;

    private final View host;
    private volatile Bitmap[] frames;
    private volatile int frameCount = 0;
    private volatile int sourceFps = 10;

    private float framePosition = 0.0f;
    private long lastFrameNs = 0L;

    private final boolean[] held = new boolean[128];
    private int heldCount = 0;
    private int lastNote = -1;
    private int lastVelocity = 0;
    private long lastNoteOnMs = 0L;
    private int eventCounter = 0;
    private final long[] recentNoteTimes = new long[12];
    private int recentWrite = 0;

    private boolean dreamy = false;
    private int dreamyX = 36;
    private int dreamyY = 36;

    private final Paint normalPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint redPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint greenPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint bluePaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint blockPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint shadePaint = new Paint();

    private final Rect srcRect = new Rect();
    private final RectF dstRect = new RectF();

    PerformanceVideoBackground(Context context, View host) {
        this.host = host;
        setupDreamyPaints();
        loadAsync(context.getApplicationContext());
    }

    private void setupDreamyPaints() {
        redPaint.setColorFilter(channelFilter(0));
        greenPaint.setColorFilter(channelFilter(1));
        bluePaint.setColorFilter(channelFilter(2));
        blockPaint.setAlpha(205);
        shadePaint.setColor(Color.argb(38, 0, 0, 0));
    }

    private static ColorMatrixColorFilter channelFilter(int channel) {
        float[] m = new float[] {
                channel == 0 ? 1f : 0f, 0f, 0f, 0f, 0f,
                0f, channel == 1 ? 1f : 0f, 0f, 0f, 0f,
                0f, 0f, channel == 2 ? 1f : 0f, 0f, 0f,
                0f, 0f, 0f, 0.62f, 0f
        };
        return new ColorMatrixColorFilter(new ColorMatrix(m));
    }

    private void loadAsync(Context context) {
        new Thread(() -> {
            Bitmap[] loaded = null;
            int count = 0;
            int fps = 2;
            try {
                StringBuilder base64 = new StringBuilder(100000);
                for (int i = 0; i < ASSET_CHUNKS; i++) {
                    String name = String.format(java.util.Locale.US, "violin_bg_%02d.b64", i);
                    try (InputStream chunk = context.getAssets().open(name);
                         ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                        byte[] buffer = new byte[4096];
                        int n;
                        while ((n = chunk.read(buffer)) >= 0) out.write(buffer, 0, n);
                        base64.append(out.toString(StandardCharsets.US_ASCII.name()).trim());
                    }
                }

                byte[] packed = Base64.decode(base64.toString(), Base64.DEFAULT);
                try (BufferedInputStream in = new BufferedInputStream(
                        new ByteArrayInputStream(packed), 64 * 1024)) {
                    byte[] magic = new byte[4];
                    if (readFully(in, magic, 0, 4) != 4 ||
                            magic[0] != 'V' || magic[1] != 'B' || magic[2] != 'G' || magic[3] != '1') {
                        return;
                    }
                    count = readLeInt(in);
                    int width = readLeInt(in);
                    fps = readLeInt(in);
                    if (count <= 0 || count > 240 || width < 64 || width > 720 || fps < 1 || fps > 30) {
                        return;
                    }

                    loaded = new Bitmap[count];
                    for (int i = 0; i < count; i++) {
                        int size = readLeInt(in);
                        if (size <= 0 || size > 1024 * 1024) return;
                        byte[] encoded = new byte[size];
                        if (readFully(in, encoded, 0, size) != size) return;
                        Bitmap bitmap = BitmapFactory.decodeByteArray(encoded, 0, size);
                        if (bitmap == null) return;
                        loaded[i] = bitmap;
                    }
                }
            } catch (IOException | IllegalArgumentException ignored) {
                return;
            }

            final Bitmap[] readyFrames = loaded;
            final int readyCount = count;
            final int readyFps = fps;
            host.post(() -> {
                frames = readyFrames;
                frameCount = readyCount;
                sourceFps = readyFps;
                framePosition = 0.0f;
                lastFrameNs = System.nanoTime();
                host.invalidate();
            });
        }, "ViolinVideoLoad").start();
    }

    void noteOn(int note, int velocity) {
        if (note >= 0 && note < held.length && !held[note]) {
            held[note] = true;
            heldCount++;
        }

        final long now = SystemClock.uptimeMillis();
        recentNoteTimes[recentWrite] = now;
        recentWrite = (recentWrite + 1) % recentNoteTimes.length;
        eventCounter++;

        int interval = lastNote >= 0 ? Math.abs(note - lastNote) : 0;
        if (interval >= 7 && frameCount > 0) {
            // Melodic leaps cut to another part of the source video.
            float jump = sourceFps * (0.28f + Math.min(18, interval) * 0.035f);
            framePosition += note >= lastNote ? jump : -jump;
            wrapFramePosition();
        }

        int recent = recentCount(now, 500);
        if (recent >= 4 && frameCount > 0) {
            // Fast runs intentionally become a discontinuous frame sequence.
            int skip = 2 + (eventCounter % 5);
            framePosition += ((eventCounter & 1) == 0) ? skip : -skip;
            wrapFramePosition();
        }

        lastNote = note;
        lastVelocity = Math.max(1, Math.min(127, velocity));
        lastNoteOnMs = now;
        host.postInvalidateOnAnimation();
    }

    void noteOff(int note) {
        if (note >= 0 && note < held.length && held[note]) {
            held[note] = false;
            heldCount = Math.max(0, heldCount - 1);
        }
        host.postInvalidateOnAnimation();
    }

    void allNotesOff() {
        for (int i = 0; i < held.length; i++) held[i] = false;
        heldCount = 0;
        host.postInvalidateOnAnimation();
    }

    void setDreamy(boolean on) {
        dreamy = on;
        host.postInvalidateOnAnimation();
    }

    void setDreamyXY(int x, int y) {
        dreamyX = Math.max(0, Math.min(127, x));
        dreamyY = Math.max(0, Math.min(127, y));
        host.postInvalidateOnAnimation();
    }

    boolean draw(Canvas canvas, RectF destination) {
        Bitmap[] local = frames;
        int count = frameCount;
        if (local == null || count <= 0) return false;

        advance();

        int index = Math.max(0, Math.min(count - 1, (int) Math.floor(framePosition)));
        int nextIndex = (index + 1) % count;
        float blend = framePosition - (float) Math.floor(framePosition);
        Bitmap frame = local[index];
        Bitmap next = local[nextIndex];
        if (frame == null || next == null) return false;

        if (dreamy) {
            drawCrossfade(canvas, frame, next, destination, blend);
            drawDreamy(canvas, frame, destination, index);
        } else {
            drawCrossfade(canvas, frame, next, destination, blend);
        }

        // Keep the existing white UI legible without hiding the video.
        canvas.drawRect(destination, shadePaint);
        host.postInvalidateOnAnimation();
        return true;
    }

    private void advance() {
        long nowNs = System.nanoTime();
        if (lastFrameNs == 0L) {
            lastFrameNs = nowNs;
            return;
        }

        float dt = Math.min(0.10f, (nowNs - lastFrameNs) / 1_000_000_000.0f);
        lastFrameNs = nowNs;

        long nowMs = SystemClock.uptimeMillis();
        int recent = recentCount(nowMs, 500);

        float speed;
        float direction = 1.0f;

        if (heldCount == 0) {
            // Silence / breathing space: very slow drift.
            speed = 0.22f;
        } else if (heldCount >= 3) {
            // Dense chords reverse the source rather than merely speeding it up.
            direction = -1.0f;
            speed = 0.85f + 0.45f * (lastVelocity / 127.0f);
        } else if (recent >= 4) {
            // Fast runs: accelerated playback plus the discrete jumps in noteOn().
            speed = 1.75f + 0.75f * (lastVelocity / 127.0f);
        } else if (lastVelocity >= 108) {
            speed = 2.15f;
        } else if (heldCount > 0 && nowMs - lastNoteOnMs > 900L) {
            // A held tone gradually settles into slow motion.
            speed = 0.42f;
        } else {
            speed = 0.82f + 0.38f * (lastVelocity / 127.0f);
        }

        framePosition += direction * dt * sourceFps * speed;
        wrapFramePosition();
    }

    private void drawCrossfade(Canvas canvas, Bitmap a, Bitmap b, RectF destination, float blend) {
        blend = Math.max(0.0f, Math.min(1.0f, blend));
        normalPaint.setAlpha(255);
        canvas.drawBitmap(a, null, destination, normalPaint);
        if (blend > 0.01f) {
            normalPaint.setAlpha(Math.round(255.0f * blend));
            canvas.drawBitmap(b, null, destination, normalPaint);
            normalPaint.setAlpha(255);
        }
    }

    private void drawDreamy(Canvas canvas, Bitmap frame, RectF destination, int frameIndex) {
        float amountX = dreamyX / 127.0f;
        float amountY = dreamyY / 127.0f;
        float shift = destination.width() * (0.004f + 0.026f * amountX);

        normalPaint.setAlpha(135);
        canvas.drawBitmap(frame, null, destination, normalPaint);
        normalPaint.setAlpha(255);

        dstRect.set(destination);
        dstRect.offset(-shift, 0f);
        canvas.drawBitmap(frame, null, dstRect, redPaint);
        dstRect.set(destination);
        canvas.drawBitmap(frame, null, dstRect, greenPaint);
        dstRect.set(destination);
        dstRect.offset(shift, 0f);
        canvas.drawBitmap(frame, null, dstRect, bluePaint);

        int blocks = 2 + Math.round(amountY * 10.0f);
        int bw = frame.getWidth();
        int bh = frame.getHeight();
        long state = ((long) frameIndex * 1103515245L) ^ ((long) eventCounter * 2654435761L);

        for (int i = 0; i < blocks; i++) {
            state = nextRandom(state);
            int sw = Math.max(10, (int) (bw * (0.08f + ((state >>> 8) & 255) / 255.0f * 0.25f)));
            state = nextRandom(state);
            int sh = Math.max(6, (int) (bh * (0.025f + ((state >>> 10) & 255) / 255.0f * 0.11f)));
            state = nextRandom(state);
            int sx = (int) Math.floorMod(state, Math.max(1, bw - sw));
            state = nextRandom(state);
            int sy = (int) Math.floorMod(state, Math.max(1, bh - sh));

            float scaleX = destination.width() / bw;
            float scaleY = destination.height() / bh;
            float dx = (((state >>> 16) & 255) / 255.0f - 0.5f) *
                    destination.width() * (0.035f + amountX * 0.16f);
            float dy = (((state >>> 24) & 127) / 127.0f - 0.5f) *
                    destination.height() * amountY * 0.045f;

            srcRect.set(sx, sy, sx + sw, sy + sh);
            dstRect.set(
                    destination.left + sx * scaleX + dx,
                    destination.top + sy * scaleY + dy,
                    destination.left + (sx + sw) * scaleX + dx,
                    destination.top + (sy + sh) * scaleY + dy);
            canvas.drawBitmap(frame, srcRect, dstRect, blockPaint);
        }
    }

    private int recentCount(long nowMs, long windowMs) {
        int count = 0;
        for (long t : recentNoteTimes) {
            if (t > 0L && nowMs - t <= windowMs) count++;
        }
        return count;
    }

    private void wrapFramePosition() {
        int count = frameCount;
        if (count <= 0) return;
        while (framePosition < 0.0f) framePosition += count;
        while (framePosition >= count) framePosition -= count;
    }

    private static long nextRandom(long x) {
        return x * 6364136223846793005L + 1442695040888963407L;
    }

    void release() {
        Bitmap[] local = frames;
        frames = null;
        frameCount = 0;
        if (local != null) {
            for (Bitmap bitmap : local) {
                if (bitmap != null && !bitmap.isRecycled()) bitmap.recycle();
            }
        }
    }

    private static int readLeInt(InputStream in) throws IOException {
        int b0 = in.read();
        int b1 = in.read();
        int b2 = in.read();
        int b3 = in.read();
        if ((b0 | b1 | b2 | b3) < 0) throw new IOException("Unexpected EOF");
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    private static int readFully(InputStream in, byte[] buffer, int offset, int length) throws IOException {
        int total = 0;
        while (total < length) {
            int n = in.read(buffer, offset + total, length - total);
            if (n < 0) break;
            total += n;
        }
        return total;
    }
}
