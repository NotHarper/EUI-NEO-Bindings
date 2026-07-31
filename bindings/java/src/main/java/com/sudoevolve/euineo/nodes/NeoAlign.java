package com.sudoevolve.euineo.nodes;

public enum NeoAlign {
    START("start"), CENTER("center"), END("end");

    public final String jsonValue;

    NeoAlign(String jsonValue) { this.jsonValue = jsonValue; }
}
