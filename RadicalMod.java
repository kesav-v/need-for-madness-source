import java.io.File;
import java.io.ByteArrayInputStream;

/**
 * Music player — replaces the original MOD/XM tracker implementation.
 *
 * Instead of decoding tracker modules (MOD/XM) via SuperClip, this plays
 * pre-converted MP3 files from the music_mp3/ directory using ffplay as a
 * background subprocess.  The public API (constructors, play/resume/stop/
 * unload) is identical to the original so xtGraphics.java needs no changes.
 *
 * ZIP path mapping:  "music/stageN.zip" → "<fpath>music_mp3/stageN.mp3"
 *                    "music/interface.zip" → "<fpath>music_mp3/interface.mp3"
 *                    "music/party.zip"    → "<fpath>music_mp3/party.mp3"
 */
public class RadicalMod
{
    boolean playing;
    int     loaded;
    int     rvol;
    String  imod;   // path to mp3 (resolved at construct/loadimod time)
    String  pmod;   // path to mp3 (resolved at construct/loadpmod time)

    /**
     * Stub kept for binary compatibility with StageMaker.java, which accesses
     * track.sClip.stream.available() to drive a progress bar.  The stream is
     * always empty so the bar stays at position 0; that is acceptable since
     * we can no longer seek inside an ffplay subprocess.
     */
    SuperClip sClip = new SuperClip(new byte[0], 0, 22000);

    /**
     * Stage 26 lightning was keyed to SuperClip PCM {@code stream.available()}.
     * With ffplay there is no PCM cursor, so we advance a virtual remaining-byte
     * count once per game tick (see {@link #pollLightningAvailable()}).
     */
    private static final int LIGHTNING_STREAM_LEN = 7_000_000;
    private int lightningRemaining = LIGHTNING_STREAM_LEN;
    private int lightningBytesPerTick = 485; // ~8000 Hz * 2 bytes / 33 fps
    private boolean lightningArmed;

    private Process  ffplay;      // the running ffplay subprocess
    private String   mp3Path;     // resolved absolute mp3 path

    // ---------------------------------------------------------------
    // Constructors — mirror the original API exactly
    // ---------------------------------------------------------------

    /** No-op default constructor (used when music is disabled). */
    public RadicalMod() {
        this.playing = false;
        this.loaded  = 0;
        this.rvol    = 0;
        this.imod    = "";
        this.pmod    = "";
        this.lightningArmed = false;
    }

    /**
     * Immediate-load constructor — original took (zipPath, vol, freq, bpm, b1, b2).
     * We ignore vol/freq/bpm/b2 and just resolve the mp3.
     */
    public RadicalMod(String zipPath, int vol, int freq, final int bpm,
                      final boolean normalize, final boolean remote) {
        this.playing = false;
        this.loaded  = 0;
        this.rvol    = 128; // dummy value, kept for API compat
        this.imod    = "";
        this.pmod    = "";
        this.lightningArmed = false;
        this.lightningBytesPerTick = bytesPerTickForFreq(freq);
        this.mp3Path = resolveMp3(zipPath, remote);
        if (this.mp3Path != null) {
            this.loaded = 2;
        }
    }

    /** Lazy-load interface constructor: RadicalMod("music/interface.zip"). */
    public RadicalMod(final String zipPath) {
        this.playing = false;
        this.loaded  = 0;
        this.rvol    = 0;
        this.imod    = "";
        this.pmod    = "";
        this.lightningArmed = false;
        this.loaded  = 1;
        this.imod    = resolveMp3(zipPath, false);
        if (this.imod == null) this.imod = "";
    }

    /** Lazy pmod constructor used for some multiplayer tracks. */
    public RadicalMod(final String zipPath, final boolean isPmod) {
        this.playing = false;
        this.loaded  = 0;
        this.rvol    = 128;
        this.imod    = "";
        this.pmod    = "";
        this.lightningArmed = false;
        this.loaded  = 1;
        this.pmod    = resolveMp3(zipPath, false);
        if (this.pmod == null) this.pmod = "";
        this.loadpmod(true);
    }

    private static int bytesPerTickForFreq(final int freq) {
        final int hz = freq > 0 ? freq : 8000;
        final int fps = Madness.autorace ? Math.max(1, Madness.recordFps) : 33;
        return Math.max(1, (hz * 2) / fps);
    }

    /**
     * Advance the virtual PCM cursor one game tick and return remaining bytes.
     * Used by stage-26 lightning sync in {@code xtGraphics.stat}.
     */
    public int pollLightningAvailable() {
        if (this.lightningArmed) {
            this.lightningRemaining -= this.lightningBytesPerTick;
            if (this.lightningRemaining <= 0) {
                this.lightningRemaining = LIGHTNING_STREAM_LEN;
            }
        }
        return this.lightningRemaining;
    }

    private void armLightningCursor() {
        this.lightningArmed = true;
        this.lightningRemaining = LIGHTNING_STREAM_LEN;
    }

    // ---------------------------------------------------------------
    // Lazy-load helpers
    // ---------------------------------------------------------------

    public void loadimod(final boolean normalize) {
        if (this.loaded == 1 && !this.imod.isEmpty()) {
            this.mp3Path = this.imod;
            this.loaded  = 2;
        }
    }

    public void loadpmod(final boolean normalize) {
        if (this.loaded == 1 && !this.pmod.isEmpty()) {
            this.mp3Path = this.pmod;
            this.loaded  = 2;
        }
    }

    // ---------------------------------------------------------------
    // Playback
    // ---------------------------------------------------------------

    public void play() {
        // Always arm the lightning cursor (stage 26), even when audio is muted.
        this.armLightningCursor();
        if (Madness.recordNoRender || (Madness.autorace && Madness.recordMute)) {
            return;
        }
        if (!this.playing && this.loaded == 2 && this.mp3Path != null) {
            startFfplay();
        }
    }

    public void resume() {
        this.lightningArmed = true;
        if (Madness.recordNoRender || (Madness.autorace && Madness.recordMute)) {
            return;
        }
        if (!this.playing && this.loaded == 2 && this.mp3Path != null) {
            startFfplay();
        }
    }

    public void stop() {
        // Keep lightningArmed so mute/replay still sync stage-26 flashes.
        if (this.playing) {
            killFfplay();
            this.playing = false;
        }
    }

    // ---------------------------------------------------------------
    // Unload
    // ---------------------------------------------------------------

    protected void unloadimod() {
        stop();
        this.lightningArmed = false;
        this.mp3Path = null;
        this.loaded  = 1; // back to lazy-load state so loadimod can re-arm it
    }

    protected void unload() {
        stop();
        this.lightningArmed = false;
        this.mp3Path = null;
        this.imod    = null;
        this.pmod    = null;
        this.loaded  = 0;
    }

    // ---------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------

    /** Candidate paths for ffplay — searched in order. */
    private static final String[] FFPLAY_CANDIDATES = {
        "ffplay",                        // already on PATH
        "/opt/homebrew/bin/ffplay",      // Homebrew on Apple Silicon
        "/usr/local/bin/ffplay",         // Homebrew on Intel / manual install
        "/usr/bin/ffplay",               // Linux system package
    };

    private static String findFfplay() {
        for (String cmd : FFPLAY_CANDIDATES) {
            try {
                Process p = new ProcessBuilder(cmd, "-version")
                    .redirectErrorStream(true)
                    .start();
                p.waitFor();
                if (p.exitValue() == 0) return cmd;
            } catch (Exception ignored) {}
        }
        return null;
    }

    private static final String FFPLAY_CMD = findFfplay();

    private void startFfplay() {
        killFfplay(); // ensure no previous instance
        if (FFPLAY_CMD == null) {
            System.err.println("[NFM-MUSIC] ffplay not found — music disabled. Install ffmpeg to enable music.");
            return;
        }
        try {
            // -nodisp   = no video window
            // -loop 0   = loop forever
            // -loglevel quiet = suppress console spam
            // -volume N = 0-100
            ProcessBuilder pb = new ProcessBuilder(
                FFPLAY_CMD, "-nodisp", "-loop", "0",
                "-loglevel", "quiet",
                "-volume", "70",
                this.mp3Path
            );
            pb.redirectErrorStream(true);
            pb.redirectOutput(ProcessBuilder.Redirect.DISCARD);
            this.ffplay  = pb.start();
            this.playing = true;
        } catch (Exception e) {
            System.err.println("[NFM-MUSIC] ffplay launch failed: " + e.getMessage());
            this.playing = false;
        }
    }

    private void killFfplay() {
        if (this.ffplay != null) {
            this.ffplay.destroy();
            this.ffplay = null;
        }
    }

    /**
     * Maps a zip path like "music/stageN.zip" or "music/interface.zip" to an
     * absolute path for the corresponding mp3.  Returns null if the file
     * cannot be found.
     */
    private static String resolveMp3(String zipPath, boolean remote) {
        if (zipPath == null || zipPath.isEmpty()) return null;

        // Strip directory prefix and extension to get the base name
        // e.g. "music/stage3.zip" → "stage3"
        //      "mystages/mymusic/foo.zip" → "foo"
        String base = zipPath;
        int slash = base.lastIndexOf('/');
        if (slash >= 0) base = base.substring(slash + 1);
        if (base.endsWith(".zip")) base = base.substring(0, base.length() - 4);
        // URL-style names use underscores instead of spaces
        base = base.replace('_', ' ').trim();
        // Normalise back (ffplay handles spaces in paths fine)
        base = base.replace(' ', '_');

        String dir = Madness.fpath + "music_mp3/";
        String candidate = dir + base + ".mp3";
        File f = new File(candidate);
        if (f.exists()) return f.getAbsolutePath();

        System.err.println("[NFM-MUSIC] MP3 not found: " + candidate);
        return null;
    }
}
