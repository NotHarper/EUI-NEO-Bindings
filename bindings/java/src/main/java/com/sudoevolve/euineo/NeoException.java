package com.sudoevolve.euineo;

public final class NeoException extends RuntimeException {
    private final int code;

    public NeoException(String message, int code) {
        super(message);
        this.code = code;
    }

    public int code() {
        return code;
    }
}
