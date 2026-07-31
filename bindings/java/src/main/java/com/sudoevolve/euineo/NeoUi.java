package com.sudoevolve.euineo;

import com.sudoevolve.euineo.events.NeoEvent;
import com.sudoevolve.euineo.events.NeoEventType;
import com.sudoevolve.euineo.nodes.*;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

public final class NeoUi {
    private final Map<String, Object> callbacks = new HashMap<>();
    private final AtomicInteger counter = new AtomicInteger(0);

    // ---- Factory methods ----

    public NeoLayoutNode column() { return new NeoLayoutNode("column", this); }
    public NeoLayoutNode row()    { return new NeoLayoutNode("row", this); }
    public NeoLayoutNode stack()  { return new NeoLayoutNode("stack", this); }
    public NeoLayoutNode flow()   { return new NeoLayoutNode("flow", this); }
    public NeoRectNode   rect()   { return new NeoRectNode(this); }
    public NeoTextNode   text()   { return new NeoTextNode(this); }
    public NeoImageNode  image()  { return new NeoImageNode(this); }
    public NeoSvgNode    svg()    { return new NeoSvgNode(this); }
    public NeoPolygonNode polygon() { return new NeoPolygonNode(this); }
    public NeoLoaderNode loader()   { return new NeoLoaderNode(this); }

    // ---- Callback registry ----

    public String registerCallback(Object handler) {
        String id = "h_" + counter.getAndIncrement();
        callbacks.put(id, handler);
        return id;
    }

    public void dispatchEvent(NeoEvent event) {
        if (event == null || event.isNone()) return;
        Object handler = callbacks.get(event.handlerId);
        if (handler == null) return;

        switch (event.type) {
            case CLICK:
                if (handler instanceof Runnable) ((Runnable) handler).run();
                break;
            case PRESS:
            case RELEASE:
            case SCROLL:
            case DRAG:
            case TEXT_INPUT:
                if (handler instanceof Consumer) {
                    @SuppressWarnings("unchecked") Consumer<NeoEvent> c = (Consumer<NeoEvent>) handler;
                    c.accept(event);
                }
                break;
            case HOVER_ENTER:
                if (handler instanceof Consumer) {
                    @SuppressWarnings("unchecked") Consumer<Boolean> c = (Consumer<Boolean>) handler;
                    c.accept(true);
                }
                break;
            case HOVER_LEAVE:
                if (handler instanceof Consumer) {
                    @SuppressWarnings("unchecked") Consumer<Boolean> c = (Consumer<Boolean>) handler;
                    c.accept(false);
                }
                break;
            default:
                break;
        }
    }
}
