package com.sudoevolve.euineo;

public class NeoLayoutNode extends NeoNode {
    NeoLayoutNode(String type, NeoUi ui) { super(type, ui); }

    public NeoLayoutNode justify(NeoAlign v)   { putProp("justify", v.jsonValue); return this; }
    public NeoLayoutNode align(NeoAlign v)     { putProp("align", v.jsonValue); return this; }

    @Override public NeoLayoutNode id(String v)          { super.id(v); return this; }
    @Override public NeoLayoutNode width(float v)        { super.width(v); return this; }
    @Override public NeoLayoutNode width(String v)       { super.width(v); return this; }
    @Override public NeoLayoutNode height(float v)       { super.height(v); return this; }
    @Override public NeoLayoutNode height(String v)      { super.height(v); return this; }
    @Override public NeoLayoutNode fill()                { super.fill(); return this; }
    @Override public NeoLayoutNode padding(float v)      { super.padding(v); return this; }
    @Override public NeoLayoutNode paddingX(float v)     { super.paddingX(v); return this; }
    @Override public NeoLayoutNode paddingY(float v)     { super.paddingY(v); return this; }
    @Override public NeoLayoutNode gap(float v)          { super.gap(v); return this; }
    @Override public NeoLayoutNode color(float r,float g,float b,float a) { super.color(r,g,b,a); return this; }
    @Override public NeoLayoutNode radius(float v)       { super.radius(v); return this; }
    @Override public NeoLayoutNode clip(boolean v)       { super.clip(v); return this; }
    @Override public NeoLayoutNode flexGrow(float v)     { super.flexGrow(v); return this; }
    @Override public NeoLayoutNode add(NeoNode child)    { super.add(child); return this; }
    @Override public NeoLayoutNode add(NeoNode... cs)    { super.add(cs); return this; }
}
