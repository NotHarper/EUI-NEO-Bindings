package com.sudoevolve.euineo.nodes;

import com.sudoevolve.euineo.NeoUi;

public class NeoRectNode extends NeoNode {
    public NeoRectNode(NeoUi ui) { super("rect", ui); }

    @Override public NeoRectNode id(String v)         { super.id(v); return this; }
    @Override public NeoRectNode width(float v)       { super.width(v); return this; }
    @Override public NeoRectNode width(String v)      { super.width(v); return this; }
    @Override public NeoRectNode height(float v)      { super.height(v); return this; }
    @Override public NeoRectNode height(String v)     { super.height(v); return this; }
    @Override public NeoRectNode fill()               { super.fill(); return this; }
    @Override public NeoRectNode color(float r,float g,float b,float a) { super.color(r,g,b,a); return this; }
    @Override public NeoRectNode radius(float v)      { super.radius(v); return this; }
    @Override public NeoRectNode opacity(float v)     { super.opacity(v); return this; }
    @Override public NeoRectNode flexGrow(float v)    { super.flexGrow(v); return this; }
    @Override public NeoRectNode onClick(Runnable h)  { super.onClick(h); return this; }
    @Override public NeoRectNode hoverColor(float r,float g,float b,float a) { super.hoverColor(r,g,b,a); return this; }
    @Override public NeoRectNode pressedColor(float r,float g,float b,float a) { super.pressedColor(r,g,b,a); return this; }
    @Override public NeoRectNode add(NeoNode child)   { super.add(child); return this; }
    @Override public NeoRectNode add(NeoNode... cs)   { super.add(cs); return this; }
}
