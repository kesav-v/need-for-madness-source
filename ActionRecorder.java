import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Sidecar action log for human / policy demos (pairs with a .nfmst).
 *
 * Format (big-endian):
 *   magic "NFMA" (4 bytes)
 *   int version (=1)
 *   int stage, car, nplayers
 *   repeating until EOF:
 *     5 bytes: left, right, up, down, handb  (0 or 1 each)
 *   = 5 bytes per frame (player 0 controls)
 */
public class ActionRecorder {
    public static final int VERSION = 1;

    private final DataOutputStream out;
    private int frames;
    private boolean finished;

    public ActionRecorder(final String path, final int stage, final int car, final int nplayers)
            throws IOException {
        final File file = new File(path);
        final File parent = file.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }
        this.frames = 0;
        this.finished = false;
        this.out = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(file), 1 << 16));
        this.out.writeBytes("NFMA");
        this.out.writeInt(VERSION);
        this.out.writeInt(stage);
        this.out.writeInt(car);
        this.out.writeInt(nplayers);
        System.out.println("[human] writing actions -> " + file.getAbsolutePath());
    }

    public void capture(final Control u) throws IOException {
        if (this.finished || u == null) {
            return;
        }
        this.out.writeByte(u.left ? 1 : 0);
        this.out.writeByte(u.right ? 1 : 0);
        this.out.writeByte(u.up ? 1 : 0);
        this.out.writeByte(u.down ? 1 : 0);
        this.out.writeByte(u.handb ? 1 : 0);
        ++this.frames;
    }

    public int frameCount() {
        return this.frames;
    }

    public void finish() {
        if (this.finished) {
            return;
        }
        this.finished = true;
        try {
            this.out.flush();
            this.out.close();
        }
        catch (IOException ex) {}
        System.out.println("[human] action frames=" + this.frames);
    }

    /** Derive sidecar path: foo.nfmst -> foo.nfmac */
    public static String sidecarPath(final String nfmstPath) {
        if (nfmstPath.endsWith(".nfmst")) {
            return nfmstPath.substring(0, nfmstPath.length() - 6) + ".nfmac";
        }
        return nfmstPath + ".nfmac";
    }
}
