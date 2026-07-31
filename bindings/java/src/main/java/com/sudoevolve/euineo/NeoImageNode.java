package com.sudoevolve.euineo;

public class NeoImageNode extends NeoNode {
    NeoImageNode(NeoUi ui) { super("image", ui); }

    public NeoImageNode source(String v)           { putProp("source", v); return this; }
    public NeoImageNode fit(NeoImageFit v)         { putProp("fit", v.jsonValue); return this; }
    public NeoImageNode flipVertically(boolean v)  { putProp("flipVertically", v); return this; }

    @Override public NeoImageNode id(String v)        { super.id(v); return this; }
    @Override public NeoImageNode width(float v)      { super.width(v); return this; }
    @Override public NeoImageNode width(String v)     { super.width(v); return this; }
    @Override public NeoImageNode height(float v)     { super.height(v); return this; }
    @Override public NeoImageNode height(String v)    { super.height(v); return this; }
    @Override public NeoImageNode fill()              { super.fill(); return this; }
    @Override public NeoImageNode opacity(float v)    { super.opacity(v); return this; }
    @Override public NeoImageNode flexGrow(float v)   { super.flexGrow(v); return this; }
}
