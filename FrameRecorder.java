import java.awt.Image;
import java.awt.image.PixelGrabber;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Records game frames as fast as the sim can tick, then encodes an mp4 whose
 * duration matches the sum of the game's intended per-frame sleeps.
 */
public class FrameRecorder
{
    private final int width;
    private final int height;
    private final String outPath;
    private File rawFile;
    private OutputStream rawOut;
    private int frames;
    private long totalSleepMs;
    private final int[] pixels;
    private final byte[] rgb;
    private boolean started;
    private boolean finished;
    private String ffmpegBin;

    public FrameRecorder(final String outPath, final int width, final int height, final int ignoredFps) {
        this.outPath = outPath;
        this.width = width;
        this.height = height;
        this.pixels = new int[width * height];
        this.rgb = new byte[width * height * 3];
        this.frames = 0;
        this.totalSleepMs = 0L;
        this.started = false;
        this.finished = false;
    }

    public void start() throws IOException {
        if (this.started) {
            return;
        }
        final File out = new File(this.outPath);
        final File parent = out.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }
        this.ffmpegBin = findFfmpeg();
        if (this.ffmpegBin == null) {
            throw new IOException("ffmpeg not found on PATH (needed to encode video)");
        }
        this.rawFile = File.createTempFile("nfm_frames_", ".rgb", parent != null ? parent : new File("."));
        this.rawFile.deleteOnExit();
        this.rawOut = new BufferedOutputStream(new FileOutputStream(this.rawFile), 1 << 20);
        this.started = true;
        System.out.println("[record] buffering frames to encode " + out.getAbsolutePath()
            + " (fast sim, realtime pacing in video)");
    }

    /**
     * @param intendedSleepMs how long the game would have slept after this frame
     */
    public void capture(final Image image, final int intendedSleepMs) throws IOException {
        if (!this.started || this.finished || image == null) {
            return;
        }
        final PixelGrabber grabber = new PixelGrabber(image, 0, 0, this.width, this.height, this.pixels, 0, this.width);
        try {
            if (!grabber.grabPixels()) {
                return;
            }
        }
        catch (InterruptedException ex) {
            Thread.currentThread().interrupt();
            return;
        }
        int o = 0;
        for (int i = 0; i < this.pixels.length; ++i) {
            final int p = this.pixels[i];
            this.rgb[o++] = (byte)((p >> 16) & 0xFF);
            this.rgb[o++] = (byte)((p >> 8) & 0xFF);
            this.rgb[o++] = (byte)(p & 0xFF);
        }
        this.rawOut.write(this.rgb);
        ++this.frames;
        this.totalSleepMs += Math.max(1, intendedSleepMs);
    }

    /** Back-compat: assume ~30fps timing if sleep unknown. */
    public void capture(final Image image) throws IOException {
        this.capture(image, 33);
    }

    public int frameCount() {
        return this.frames;
    }

    public long totalSleepMs() {
        return this.totalSleepMs;
    }

    /** In-place terminal progress bar (stderr). pct in [0,1]. */
    public static void printBar(final float pct, final String detail) {
        final int width = 28;
        final float clamped = Math.max(0.0f, Math.min(1.0f, pct));
        final int filled = Math.round(clamped * width);
        final StringBuilder sb = new StringBuilder(96);
        sb.append(Madness.recordNoRender ? "\r[sim] |" : (Madness.replaying ? "\r[video] |" : "\r[record] |"));
        for (int i = 0; i < width; ++i) {
            sb.append(i < filled ? '#' : '-');
        }
        sb.append("| ");
        sb.append(String.format("%3.0f%% ", clamped * 100.0f));
        if (detail != null) {
            sb.append(detail);
        }
        sb.append("   ");
        System.err.print(sb.toString());
        System.err.flush();
    }

    public static void printBarDone() {
        System.err.println();
    }

    public void finish() {
        if (!this.started || this.finished) {
            return;
        }
        this.finished = true;
        try {
            this.rawOut.flush();
            this.rawOut.close();
        }
        catch (IOException ex) {}
        if (this.frames == 0 || this.totalSleepMs <= 0L) {
            System.err.println("[record] no frames captured");
            this.rawFile.delete();
            return;
        }
        final double fps = this.frames * 1000.0 / this.totalSleepMs;
        System.out.println("[record] encoding " + this.frames + " frames, intended "
            + (this.totalSleepMs / 1000.0) + "s @ " + String.format("%.2f", fps) + " fps");
        try {
            final List<String> cmd = new ArrayList<String>();
            cmd.add(this.ffmpegBin);
            cmd.add("-y");
            cmd.add("-f");
            cmd.add("rawvideo");
            cmd.add("-pix_fmt");
            cmd.add("rgb24");
            cmd.add("-s");
            cmd.add(this.width + "x" + this.height);
            cmd.add("-r");
            cmd.add(String.format(java.util.Locale.US, "%.6f", fps));
            cmd.add("-i");
            cmd.add(this.rawFile.getAbsolutePath());
            cmd.add("-an");
            cmd.add("-c:v");
            cmd.add("libx264");
            cmd.add("-preset");
            cmd.add("veryfast");
            cmd.add("-pix_fmt");
            cmd.add("yuv420p");
            cmd.add(new File(this.outPath).getAbsolutePath());
            final ProcessBuilder pb = new ProcessBuilder(cmd);
            pb.redirectError(ProcessBuilder.Redirect.to(new File(this.outPath + ".ffmpeg.log")));
            final Process p = pb.start();
            final int code = p.waitFor();
            System.out.println("[record] done frames=" + this.frames + " exit=" + code + " -> " + this.outPath);
        }
        catch (Exception ex) {
            System.err.println("[record] encode failed: " + ex.getMessage());
        }
        finally {
            this.rawFile.delete();
        }
    }

    private static String findFfmpeg() {
        final String[] candidates = {
            "ffmpeg",
            "/opt/homebrew/bin/ffmpeg",
            "/usr/local/bin/ffmpeg",
            "/usr/bin/ffmpeg"
        };
        for (final String c : candidates) {
            try {
                final Process p = new ProcessBuilder(c, "-version").redirectErrorStream(true).start();
                final int code = p.waitFor();
                if (code == 0 || code == 1) {
                    return c;
                }
            }
            catch (Exception ex) {}
        }
        return null;
    }
}
