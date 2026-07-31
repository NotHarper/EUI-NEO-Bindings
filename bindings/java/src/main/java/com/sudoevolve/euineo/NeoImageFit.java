package com.sudoevolve.euineo;

public enum NeoImageFit {
    COVER("cover"), CONTAIN("contain"), STRETCH("stretch");

    public final String jsonValue;

    NeoImageFit(String jsonValue) { this.jsonValue = jsonValue; }
}
