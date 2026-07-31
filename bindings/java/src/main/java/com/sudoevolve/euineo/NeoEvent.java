package com.sudoevolve.euineo;

public final class NeoEvent {
    public final NeoEventType type;
    public final String handlerId;
    public final float x, y, deltaX, deltaY;
    public final String textInput;

    public NeoEvent(NeoEventType type, String handlerId,
                    float x, float y, float deltaX, float deltaY, String textInput) {
        this.type      = type;
        this.handlerId = handlerId != null ? handlerId : "";
        this.x = x; this.y = y; this.deltaX = deltaX; this.deltaY = deltaY;
        this.textInput = textInput != null ? textInput : "";
    }

    public boolean isNone() { return type == NeoEventType.NONE; }

    @Override
    public String toString() {
        return "NeoEvent{type=" + type + ", handlerId='" + handlerId + "', x=" + x + ", y=" + y + "}";
    }
}
