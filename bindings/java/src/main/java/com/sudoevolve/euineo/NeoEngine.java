package com.sudoevolve.euineo;

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
                config.clearAlpha, config.resizable);
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

    public void requestUpdate() {
        check(nativeRequestUpdate(handle));
    }

    public boolean isRunning() {
        return nativeIsRunning(handle) != 0;
    }

    public String lastError() {
        return nativeLastError(handle);
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
                                             boolean resizable);
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
}
