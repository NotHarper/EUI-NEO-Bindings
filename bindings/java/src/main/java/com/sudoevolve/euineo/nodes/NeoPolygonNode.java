package com.sudoevolve.euineo.nodes;

import com.sudoevolve.euineo.NeoUi;

public class NeoPolygonNode extends NeoNode {
    public NeoPolygonNode(NeoUi ui) { super("polygon", ui); }

    public NeoPolygonNode points(float[]... pts) { putProp("points", pts); return this; }

    @Override public NeoPolygonNode id(String v)       { super.id(v); return this; }
    @Override public NeoPolygonNode width(float v)     { super.width(v); return this; }
    @Override public NeoPolygonNode height(float v)    { super.height(v); return this; }
    @Override public NeoPolygonNode fill()             { super.fill(); return this; }
    @Override public NeoPolygonNode color(float r,float g,float b,float a) { super.color(r,g,b,a); return this; }
    @Override public NeoPolygonNode opacity(float v)   { super.opacity(v); return this; }
    @Override public NeoPolygonNode flexGrow(float v)  { super.flexGrow(v); return this; }
}
