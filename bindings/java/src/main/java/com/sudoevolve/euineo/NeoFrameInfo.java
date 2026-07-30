package com.sudoevolve.euineo;

public record NeoFrameInfo(long frameNumber, int framebufferWidth, int framebufferHeight,
                           float dpiScale, boolean rendered, boolean running) {
}
