package com.sudoevolve.euineo;

public class NeoLoaderNode extends NeoNode {
    NeoLoaderNode(NeoUi ui) { super("loader", ui); }

    public NeoLoaderNode active(boolean v) { putProp("active", v); return this; }
    public NeoLoaderNode mode(String v)    { putProp("mode", v); return this; }

    @Override public NeoLoaderNode id(String v)       { super.id(v); return this; }
    @Override public NeoLoaderNode width(float v)     { super.width(v); return this; }
    @Override public NeoLoaderNode height(float v)    { super.height(v); return this; }
    @Override public NeoLoaderNode fill()             { super.fill(); return this; }
    @Override public NeoLoaderNode color(float r,float g,float b,float a) { super.color(r,g,b,a); return this; }
    @Override public NeoLoaderNode opacity(float v)   { super.opacity(v); return this; }
    @Override public NeoLoaderNode flexGrow(float v)  { super.flexGrow(v); return this; }
}
