package com.sudoevolve.euineo.nodes;

import com.sudoevolve.euineo.NeoUi;
import com.sudoevolve.euineo.events.NeoEvent;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

public class NeoNode {
    protected final Map<String, Object> props = new LinkedHashMap<>();
    protected final List<NeoNode> childList   = new ArrayList<>();
    protected final NeoUi ui;

    protected NeoNode(String type, NeoUi ui) {
        this.ui = ui;
        props.put("type", type);
    }

    protected NeoNode putProp(String key, Object value) { props.put(key, value); return this; }

    public NeoNode id(String v)         { return putProp("id", v); }
    public NeoNode width(float v)       { return putProp("width", v); }
    public NeoNode width(String v)      { return putProp("width", v); }
    public NeoNode height(float v)      { return putProp("height", v); }
    public NeoNode height(String v)     { return putProp("height", v); }
    public NeoNode fill()               { props.put("width","fill"); props.put("height","fill"); return this; }
    public NeoNode x(float v)           { return putProp("x", v); }
    public NeoNode y(float v)           { return putProp("y", v); }
    public NeoNode padding(float v)     { return putProp("padding", v); }
    public NeoNode paddingX(float v)    { return putProp("paddingX", v); }
    public NeoNode paddingY(float v)    { return putProp("paddingY", v); }
    public NeoNode paddingLeft(float v)  { return putProp("paddingLeft", v); }
    public NeoNode paddingTop(float v)   { return putProp("paddingTop", v); }
    public NeoNode paddingRight(float v) { return putProp("paddingRight", v); }
    public NeoNode paddingBottom(float v){ return putProp("paddingBottom", v); }
    public NeoNode margin(float v)      { return putProp("margin", v); }
    public NeoNode marginX(float v)     { return putProp("marginX", v); }
    public NeoNode marginY(float v)     { return putProp("marginY", v); }
    public NeoNode marginLeft(float v)  { return putProp("marginLeft", v); }
    public NeoNode marginTop(float v)   { return putProp("marginTop", v); }
    public NeoNode marginRight(float v) { return putProp("marginRight", v); }
    public NeoNode marginBottom(float v){ return putProp("marginBottom", v); }
    public NeoNode gap(float v)         { return putProp("gap", v); }
    public NeoNode lineGap(float v)     { return putProp("lineGap", v); }
    public NeoNode radius(float v)      { return putProp("radius", v); }
    public NeoNode opacity(float v)     { return putProp("opacity", v); }
    public NeoNode flexGrow(float v)    { return putProp("flexGrow", v); }
    public NeoNode flexShrink(float v)  { return putProp("flexShrink", v); }
    public NeoNode zIndex(int v)        { return putProp("zIndex", v); }
    public NeoNode ignoreLayout(boolean v) { return putProp("ignoreLayout", v); }
    public NeoNode clip(boolean v)      { return putProp("clip", v); }
    public NeoNode minWidth(float v)    { return putProp("minWidth", v); }
    public NeoNode minHeight(float v)   { return putProp("minHeight", v); }
    public NeoNode maxWidth(float v)    { return putProp("maxWidth", v); }
    public NeoNode maxHeight(float v)   { return putProp("maxHeight", v); }
    public NeoNode color(float r, float g, float b, float a) { return putProp("color", new float[]{r,g,b,a}); }
    public NeoNode hoverColor(float r, float g, float b, float a)   { return putProp("hoverColor",   new float[]{r,g,b,a}); }
    public NeoNode pressedColor(float r, float g, float b, float a) { return putProp("pressedColor", new float[]{r,g,b,a}); }
    public NeoNode interactive()        { return putProp("interactive", true); }
    public NeoNode focusable()          { return putProp("focusable", true); }
    public NeoNode disabled(boolean v)  { return putProp("disabled", v); }
    public NeoNode blur(float v)        { return putProp("blur", v); }
    public NeoNode translate(float x, float y) { return putProp("translate", new float[]{x, y}); }
    public NeoNode scale(float s)       { return putProp("scale", s); }
    public NeoNode scale(float sx, float sy) { return putProp("scale", new float[]{sx, sy}); }
    public NeoNode rotate(float radians){ return putProp("rotate", radians); }
    public NeoNode perspective(float v) { return putProp("perspective", v); }
    public NeoNode rotateX(float v)     { return putProp("rotateX", v); }
    public NeoNode rotateY(float v)     { return putProp("rotateY", v); }
    public NeoNode transformOrigin(float x, float y) { return putProp("transformOrigin", new float[]{x, y}); }

    public NeoNode shadow(float blur, float offsetX, float offsetY, float r, float g, float b, float a, boolean inset) {
        Map<String,Object> s = new LinkedHashMap<>();
        s.put("blur", blur); s.put("offsetX", offsetX); s.put("offsetY", offsetY);
        s.put("color", new float[]{r,g,b,a}); s.put("inset", inset);
        return putProp("shadow", s);
    }
    public NeoNode shadow(float blur, float offsetX, float offsetY, float r, float g, float b, float a) {
        return shadow(blur, offsetX, offsetY, r, g, b, a, false);
    }

    public NeoNode border(float width, float r, float g, float b, float a) {
        Map<String,Object> m = new LinkedHashMap<>();
        m.put("width", width); m.put("color", new float[]{r,g,b,a});
        return putProp("border", m);
    }

    public NeoNode gradient(float sr, float sg, float sb, float sa,
                             float er, float eg, float eb, float ea, boolean vertical) {
        Map<String,Object> m = new LinkedHashMap<>();
        m.put("start", new float[]{sr,sg,sb,sa}); m.put("end", new float[]{er,eg,eb,ea});
        m.put("direction", vertical ? "vertical" : "horizontal");
        return putProp("gradient", m);
    }

    public NeoNode transition(float duration, NeoEase ease, float delay) {
        Map<String,Object> m = new LinkedHashMap<>();
        m.put("duration", duration); m.put("ease", ease.jsonValue); m.put("delay", delay);
        return putProp("transition", m);
    }
    public NeoNode transition(float duration, NeoEase ease) { return transition(duration, ease, 0); }
    public NeoNode transition(float duration) { return transition(duration, NeoEase.LINEAR, 0); }

    public NeoNode scrollState(String id, float offset, float maxOffset, float step) {
        Map<String,Object> m = new LinkedHashMap<>();
        m.put("id", id); m.put("offset", offset); m.put("maxOffset", maxOffset); m.put("step", step);
        return putProp("scrollState", m);
    }
    public NeoNode scrollContentFrom(String sourceId) { return putProp("scrollContentFrom", sourceId); }

    public NeoNode onClick(Runnable handler) {
        props.put("onClick", ui.registerCallback(handler)); return this;
    }
    public NeoNode onPress(Consumer<NeoEvent> handler) {
        props.put("onPress", ui.registerCallback(handler)); return this;
    }
    public NeoNode onRelease(Consumer<NeoEvent> handler) {
        props.put("onRelease", ui.registerCallback(handler)); return this;
    }
    public NeoNode onHover(Consumer<Boolean> handler) {
        props.put("onHover", ui.registerCallback(handler)); return this;
    }
    public NeoNode onScroll(Consumer<NeoEvent> handler) {
        props.put("onScroll", ui.registerCallback(handler)); return this;
    }
    public NeoNode onDrag(Consumer<NeoEvent> handler) {
        props.put("onDrag", ui.registerCallback(handler)); return this;
    }
    public NeoNode onTextInput(Consumer<NeoEvent> handler) {
        props.put("onTextInput", ui.registerCallback(handler)); return this;
    }

    public NeoNode add(NeoNode child) { childList.add(child); return this; }
    public NeoNode add(NeoNode... children) { for (NeoNode c : children) childList.add(c); return this; }

    public String toJson() {
        StringBuilder sb = new StringBuilder();
        appendJson(sb);
        return sb.toString();
    }

    void appendJson(StringBuilder sb) {
        sb.append('{');
        boolean first = true;
        for (Map.Entry<String,Object> e : props.entrySet()) {
            if (!first) sb.append(',');
            first = false;
            appendJsonString(sb, e.getKey());
            sb.append(':');
            appendJsonValue(sb, e.getValue());
        }
        if (!childList.isEmpty()) {
            if (!first) sb.append(',');
            sb.append("\"children\":[");
            for (int i = 0; i < childList.size(); i++) {
                if (i > 0) sb.append(',');
                childList.get(i).appendJson(sb);
            }
            sb.append(']');
        }
        sb.append('}');
    }

    private static void appendJsonValue(StringBuilder sb, Object v) {
        if (v == null)            { sb.append("null"); }
        else if (v instanceof String)  { appendJsonString(sb, (String) v); }
        else if (v instanceof Boolean) { sb.append((Boolean)v ? "true" : "false"); }
        else if (v instanceof Integer) { sb.append((Integer)v); }
        else if (v instanceof float[]) {
            float[] arr = (float[]) v;
            sb.append('[');
            for (int i = 0; i < arr.length; i++) {
                if (i > 0) sb.append(',');
                sb.append(arr[i]);
            }
            sb.append(']');
        } else if (v instanceof float[][]) {
            float[][] arr = (float[][]) v;
            sb.append('[');
            for (int i = 0; i < arr.length; i++) {
                if (i > 0) sb.append(',');
                appendJsonValue(sb, arr[i]);
            }
            sb.append(']');
        } else if (v instanceof Map) {
            @SuppressWarnings("unchecked") Map<String,Object> m = (Map<String,Object>) v;
            sb.append('{');
            boolean f = true;
            for (Map.Entry<String,Object> e : m.entrySet()) {
                if (!f) sb.append(',');
                f = false;
                appendJsonString(sb, e.getKey());
                sb.append(':');
                appendJsonValue(sb, e.getValue());
            }
            sb.append('}');
        } else {
            sb.append(v);
        }
    }

    private static void appendJsonString(StringBuilder sb, String s) {
        sb.append('"');
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if      (c == '"')  { sb.append("\\\""); }
            else if (c == '\\') { sb.append("\\\\"); }
            else if (c == '\n') { sb.append("\\n"); }
            else if (c == '\r') { sb.append("\\r"); }
            else if (c == '\t') { sb.append("\\t"); }
            else                { sb.append(c); }
        }
        sb.append('"');
    }
}
