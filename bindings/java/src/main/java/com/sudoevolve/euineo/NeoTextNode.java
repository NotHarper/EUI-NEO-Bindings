package com.sudoevolve.euineo;

public class NeoTextNode extends NeoNode {
    NeoTextNode(NeoUi ui) { super("text", ui); }

    public NeoTextNode text(String v)         { putProp("text", v); return this; }
    public NeoTextNode fontSize(float v)      { putProp("fontSize", v); return this; }
    public NeoTextNode fontWeight(int v)      { putProp("fontWeight", v); return this; }
    public NeoTextNode fontFamily(String v)   { putProp("fontFamily", v); return this; }
    public NeoTextNode textColor(float r,float g,float b,float a) { putProp("textColor", new float[]{r,g,b,a}); return this; }
    public NeoTextNode wrap(boolean v)        { putProp("wrap", v); return this; }
    public NeoTextNode lineHeight(float v)    { putProp("lineHeight", v); return this; }
    public NeoTextNode horizontalAlign(NeoAlign v) { putProp("horizontalAlign", v.jsonValue); return this; }
    public NeoTextNode verticalAlign(NeoAlign v)   { putProp("verticalAlign", v.jsonValue); return this; }

    @Override public NeoTextNode id(String v)        { super.id(v); return this; }
    @Override public NeoTextNode width(float v)      { super.width(v); return this; }
    @Override public NeoTextNode width(String v)     { super.width(v); return this; }
    @Override public NeoTextNode height(float v)     { super.height(v); return this; }
    @Override public NeoTextNode height(String v)    { super.height(v); return this; }
    @Override public NeoTextNode maxWidth(float v)   { super.maxWidth(v); return this; }
    @Override public NeoTextNode opacity(float v)    { super.opacity(v); return this; }
    @Override public NeoTextNode flexGrow(float v)   { super.flexGrow(v); return this; }
    @Override public NeoTextNode onClick(java.util.function.Runnable h) { super.onClick(h); return this; }
}
