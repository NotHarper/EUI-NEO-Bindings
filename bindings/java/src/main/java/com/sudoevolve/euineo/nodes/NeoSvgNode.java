package com.sudoevolve.euineo.nodes;

import com.sudoevolve.euineo.NeoUi;

public class NeoSvgNode extends NeoNode {
    public NeoSvgNode(NeoUi ui) { super("svg", ui); }

    public NeoSvgNode source(String v) { putProp("source", v); return this; }

    @Override public NeoSvgNode id(String v)       { super.id(v); return this; }
    @Override public NeoSvgNode width(float v)     { super.width(v); return this; }
    @Override public NeoSvgNode width(String v)    { super.width(v); return this; }
    @Override public NeoSvgNode height(float v)    { super.height(v); return this; }
    @Override public NeoSvgNode height(String v)   { super.height(v); return this; }
    @Override public NeoSvgNode fill()             { super.fill(); return this; }
    @Override public NeoSvgNode opacity(float v)   { super.opacity(v); return this; }
    @Override public NeoSvgNode flexGrow(float v)  { super.flexGrow(v); return this; }
    @Override public NeoSvgNode color(float r,float g,float b,float a) { super.color(r,g,b,a); return this; }
}
