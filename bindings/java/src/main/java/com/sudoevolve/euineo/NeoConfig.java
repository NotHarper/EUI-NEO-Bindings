package com.sudoevolve.euineo;

public final class NeoConfig {
    String title = "EUI-NEO Java";
    String pageId = "java";
    String uiJson;
    int width = 960;
    int height = 640;
    double framesPerSecond = 60.0;
    float clearRed = 0.16f;
    float clearGreen = 0.18f;
    float clearBlue = 0.20f;
    float clearAlpha = 1.0f;
    boolean resizable = true;
    boolean decorated = true;

    public NeoConfig title(String value) { title = requireText(value, "title"); return this; }
    public NeoConfig pageId(String value) { pageId = requireText(value, "pageId"); return this; }
    public NeoConfig uiJson(String value) { uiJson = value; return this; }
    public NeoConfig size(int valueWidth, int valueHeight) {
        if (valueWidth <= 0 || valueHeight <= 0) throw new IllegalArgumentException("size must be positive");
        width = valueWidth;
        height = valueHeight;
        return this;
    }
    public NeoConfig framesPerSecond(double value) {
        if (!(value > 0.0) || Double.isInfinite(value) || Double.isNaN(value)) {
            throw new IllegalArgumentException("framesPerSecond must be positive");
        }
        framesPerSecond = value;
        return this;
    }
    public NeoConfig clearColor(float red, float green, float blue, float alpha) {
        clearRed = red;
        clearGreen = green;
        clearBlue = blue;
        clearAlpha = alpha;
        return this;
    }
    public NeoConfig resizable(boolean value) { resizable = value; return this; }
    public NeoConfig decorated(boolean value) { decorated = value; return this; }

    private static String requireText(String value, String name) {
        if (value == null || value.isEmpty()) throw new IllegalArgumentException(name + " must not be empty");
        return value;
    }
}
