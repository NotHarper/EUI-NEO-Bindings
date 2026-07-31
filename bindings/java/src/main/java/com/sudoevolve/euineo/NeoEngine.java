package com.sudoevolve.euineo;

import com.sudoevolve.euineo.events.NeoEvent;
import com.sudoevolve.euineo.events.NeoEventType;
import com.sudoevolve.euineo.nodes.NeoNode;
import java.lang.ref.Cleaner;

public final class NeoEngine implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    static {
        NativeLoader.load();
    }

    private long handle;
    private final Thread ownerThread;
    private final State cleanupState;
    private final Cleaner.Cleanable cleanable;

    public NeoEngine() {
        this(new NeoConfig());
    }

    public NeoEngine(NeoConfig config) {
        if (config == null) throw new NullPointerException("config");
        ownerThread = Thread.currentThread();
        handle = nativeCreate(config.title, config.pageId, config.uiJson, config.width, config.height,
                config.framesPerSecond, config.clearRed, config.clearGreen, config.clearBlue,
                config.clearAlpha, config.resizable, config.decorated);
        if (handle == 0) throw new NeoException("Unable to create native engine", -1);
        cleanupState = new State(handle);
        cleanable = CLEANER.register(this, cleanupState);
    }

    public void initialize() {
        checkThread();
        check(nativeInitialize(handle));
    }

    public void pumpEvents(int waitTimeoutMillis) {
        checkThread();
        if (waitTimeoutMillis < 0) throw new IllegalArgumentException("waitTimeoutMillis must not be negative");
        check(nativePumpEvents(handle, waitTimeoutMillis));
    }

    public NeoFrameInfo frame() {
        checkThread();
        long[] values = nativeFrame(handle);
        return new NeoFrameInfo(values[0], (int) values[1], (int) values[2], Float.intBitsToFloat((int) values[3]),
                values[4] != 0, values[5] != 0);
    }

    public void setUiJson(String json) {
        checkThread();
        check(nativeSetUiJson(handle, json));
    }

    public void setUi(NeoUi ui, NeoNode root) {
        if (ui == null || root == null) throw new NullPointerException("ui and root must not be null");
        setUiJson(root.toJson());
    }

    public NeoEvent pollEvent() {
        checkThread();
        long[] raw = nativePollEvent(handle);
        if (raw == null) return new NeoEvent(NeoEventType.NONE, "", 0, 0, 0, 0, "");
        NeoEventType type = NeoEventType.fromCode((int) raw[0]);
        float x  = Float.intBitsToFloat((int) raw[1]);
        float y  = Float.intBitsToFloat((int) raw[2]);
        float dx = Float.intBitsToFloat((int) raw[3]);
        float dy = Float.intBitsToFloat((int) raw[4]);
        String handlerId = type == NeoEventType.NONE ? "" : nativeLastEventHandlerId(handle);
        String text      = type == NeoEventType.TEXT_INPUT ? nativeLastEventTextInput(handle) : "";
        return new NeoEvent(type, handlerId, x, y, dx, dy, text);
    }

    public void drainEvents(NeoUi ui) {
        NeoEvent evt;
        while (!(evt = pollEvent()).isNone()) {
            if (ui != null) ui.dispatchEvent(evt);
        }
    }

    public void setWindowTitle(String title) {
        checkThread();
        if (title == null) throw new NullPointerException("title");
        check(nativeSetWindowTitle(handle, title));
    }

    public void setWindowSize(int width, int height) {
        checkThread();
        check(nativeSetWindowSize(handle, width, height));
    }

    public void beginWindowDrag() {
        checkThread();
        check(nativeBeginWindowDrag(handle));
    }

    public static int apiVersion() {
        return nativeApiVersion();
    }

    public void requestUpdate() {
        check(nativeRequestUpdate(handle));
    }

    public boolean isRunning() {
        return nativeIsRunning(handle) != 0;
    }

    public String lastError() {
        return nativeLastError(handle);
    }

    /**
     * Get the current cursor position in screen coordinates.
     * Must be called from the owner thread.
     * @return double array [x, y] or null on error
     */
    public double[] getCursorPosition() {
        checkThread();
        return nativeGetCursorPosition(handle);
    }

    /**
     * Center the window on the primary monitor with the specified size.
     * Must be called from the owner thread.
     */
    public void centerWindow(int width, int height) {
        checkThread();
        check(nativeCenterWindow(handle, width, height));
    }

    /**
     * Set the system clipboard text.
     * Must be called from the owner thread.
     */
    public void setClipboardText(String text) {
        checkThread();
        check(nativeSetClipboardText(handle, text));
    }

    /**
     * Get the system clipboard text.
     * Must be called from the owner thread.
     * @return clipboard text or empty string
     */
    public String getClipboardText() {
        checkThread();
        return nativeGetClipboardText(handle);
    }

    @Override
    public void close() {
        if (handle == 0) return;
        if (Thread.currentThread() != ownerThread) {
            throw new NeoException("NeoEngine.close() must be called from the owner thread", 3);
        }
        long h = handle;
        handle = 0;
        cleanupState.handle = 0;
        try {
            nativeShutdown(h);
        } finally {
            nativeDestroy(h);
            cleanable.clean();
        }
    }

    public static String version() {
        return nativeVersion();
    }

    private void checkThread() {
        if (Thread.currentThread() != ownerThread) {
            throw new NeoException("NeoEngine must be used from its creating thread", 3);
        }
        if (handle == 0) throw new NeoException("NeoEngine is closed", 2);
    }

    private void check(int result) {
        if (result != 0) throw new NeoException(lastError(), result);
    }

    private static final class State implements Runnable {
        private volatile long handle;
        State(long value) { handle = value; }
        @Override public void run() {
            if (handle != 0) {
                System.err.println("[EUI-NEO] WARNING: NeoEngine was not closed explicitly. " +
                                   "Call close() on the owner thread before the engine is GC'd. " +
                                   "Native GUI resources cannot be safely freed from the Cleaner thread.");
                // Cannot call nativeDestroy here: native engine has GUI thread affinity.
                // The native window and OpenGL context must be released on the creating thread.
                handle = 0;
            }
        }
    }

    private static native long nativeCreate(String title, String pageId, String uiJson, int width, int height,
                                             double fps, float red, float green, float blue, float alpha,
                                             boolean resizable, boolean decorated);
    private static native int nativeInitialize(long handle);
    private static native int nativePumpEvents(long handle, int waitTimeoutMillis);
    private static native long[] nativeFrame(long handle);
    private static native int nativeSetUiJson(long handle, String json);
    private static native int nativeRequestUpdate(long handle);
    private static native int nativeIsRunning(long handle);
    private static native int nativeShutdown(long handle);
    private static native void nativeDestroy(long handle);
    private static native String nativeLastError(long handle);
    private static native String nativeVersion();
    private static native long[] nativePollEvent(long handle);
    private static native String nativeLastEventHandlerId(long handle);
    private static native String nativeLastEventTextInput(long handle);
    private static native int nativeSetWindowTitle(long handle, String title);
    private static native int nativeSetWindowSize(long handle, int width, int height);
    private static native int nativeBeginWindowDrag(long handle);
    private static native double[] nativeGetCursorPosition(long handle);
    private static native int nativeCenterWindow(long handle, int width, int height);
    private static native int nativeSetClipboardText(long handle, String text);
    private static native String nativeGetClipboardText(long handle);
    private static native int nativeApiVersion();
}
