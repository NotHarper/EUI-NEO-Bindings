package com.sudoevolve.euineo.nodes;

public enum NeoImageFit {
    COVER("cover"), CONTAIN("contain"), STRETCH("stretch");

    public final String jsonValue;

    NeoImageFit(String jsonValue) { this.jsonValue = jsonValue; }
}
