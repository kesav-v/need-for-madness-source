import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.DataLine;
import javax.sound.sampled.AudioFormat;
import java.io.ByteArrayInputStream;
import javax.sound.sampled.SourceDataLine;

public class SuperClip implements Runnable
{
    int skiprate;
    Thread cliper;
    int stoped;
    SourceDataLine source;
    ByteArrayInputStream stream;
    int rollBackPos;
    int rollBackTrig;
    boolean changeGain;
    /** Invoked once after {@link SourceDataLine#start()} succeeds (use to sync {@link RadicalMod#playing}). */
    Runnable onLineOpened;
    /** Invoked from the player thread's {@code finally} when the thread exits. */
    Runnable onThreadDone;
    
    public SuperClip(final byte[] array, final int n, final int skiprate) {
        this.skiprate = 0;
        this.stoped = 1;
        this.source = null;
        this.rollBackPos = 0;
        this.rollBackTrig = 0;
        this.changeGain = false;
        this.stoped = 2;
        this.skiprate = skiprate;
        this.stream = new ByteArrayInputStream(array, 0, n);
    }
    
    @Override
    public void run() {
        try {
            final AudioFormat audioFormat = new AudioFormat(AudioFormat.Encoding.PCM_SIGNED, this.skiprate, 16, 1, 2, this.skiprate, false);
            this.source = (SourceDataLine)AudioSystem.getLine(new DataLine.Info(SourceDataLine.class, audioFormat));
            final int bufferBytes = Math.max(8192, this.skiprate * 2);
            this.source.open(audioFormat, bufferBytes);
            this.source.start();
            if (this.onLineOpened != null) {
                this.onLineOpened.run();
            }
        }
        catch (Exception ex2) {
            System.err.println("[NFM] Could not open audio line for music: " + ex2.getMessage());
            ex2.printStackTrace();
            this.stoped = 1;
        }
        try {
            while (this.stoped == 0 && this.source != null) {
                try {
                    final int skiprate = this.skiprate;
                    int available = this.stream.available();
                    if (available <= 0) {
                        this.stream.reset();
                        if (this.rollBackPos != 0) {
                            this.stream.skip(this.rollBackPos);
                        }
                        available = this.stream.available();
                    }
                    if (available % 2 != 0) {
                        ++available;
                    }
                    final int chunk = (available > skiprate) ? skiprate : available;
                    if (chunk <= 0) {
                        Thread.sleep(5L);
                        continue;
                    }
                    final byte[] array = new byte[chunk];
                    int read = this.stream.read(array, 0, chunk);
                    final boolean jumpLoop = read == -1 || (this.rollBackPos != 0 && available < this.rollBackTrig);
                    if (jumpLoop) {
                        if (read > 0) {
                            this.source.write(array, 0, read);
                        }
                        this.stream.reset();
                        if (this.rollBackPos != 0) {
                            this.stream.skip(this.rollBackPos);
                        }
                        int av2 = this.stream.available();
                        if (av2 % 2 != 0) {
                            ++av2;
                        }
                        final int ch2 = (av2 > skiprate) ? skiprate : av2;
                        if (ch2 > 0) {
                            final byte[] buf2 = new byte[ch2];
                            read = this.stream.read(buf2, 0, ch2);
                            if (read > 0) {
                                this.source.write(buf2, 0, read);
                            }
                        }
                    }
                    else if (read > 0) {
                        this.source.write(array, 0, read);
                    }
                }
                catch (Exception ex) {
                    System.out.println("Play error: " + ex);
                    this.stoped = 1;
                }
                try {
                    Thread.sleep(5L);
                }
                catch (InterruptedException ex3) {}
            }
        }
        finally {
            if (this.source != null) {
                try {
                    this.source.stop();
                    this.source.close();
                }
                catch (Exception ignored) {}
                this.source = null;
            }
            this.stoped = 2;
            if (this.onThreadDone != null) {
                this.onThreadDone.run();
            }
        }
    }
    
    public void play() {
        if (this.stoped == 2) {
            this.stoped = 0;
            try {
                this.stream.reset();
            }
            catch (Exception ex) {}
            (this.cliper = new Thread(this, "nfm-music")).start();
        }
    }
    
    public void resume() {
        if (this.stoped == 2) {
            this.stoped = 0;
            try {
                if (this.stream.available() == 0) {
                    this.stream.reset();
                    if (this.rollBackPos != 0) {
                        this.stream.skip(this.rollBackPos);
                    }
                }
            }
            catch (Exception ex) {}
            (this.cliper = new Thread(this, "nfm-music")).start();
        }
    }
    
    public void stop() {
        if (this.stoped == 0) {
            this.stoped = 1;
            if (this.source != null) {
                this.source.stop();
            }
        }
    }
    
    public void close() {
        try {
            this.stream.close();
            this.stream = null;
        }
        catch (Exception ex) {}
    }
}
