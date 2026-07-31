package com.sudoevolve.euineo.events;

public enum NeoEventType {
    NONE(0), CLICK(1), PRESS(2), RELEASE(3),
    HOVER_ENTER(4), HOVER_LEAVE(5), TEXT_INPUT(6), SCROLL(7), DRAG(8);

    public final int code;

    NeoEventType(int code) { this.code = code; }

    public static NeoEventType fromCode(int code) {
        for (NeoEventType t : values()) if (t.code == code) return t;
        return NONE;
    }
}
