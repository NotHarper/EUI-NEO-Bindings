package com.sudoevolve.euineo;

public enum NeoEase {
    LINEAR("Linear"),
    IN_QUAD("InQuad"),
    OUT_QUAD("OutQuad"),
    IN_OUT_QUAD("InOutQuad"),
    OUT_CUBIC("OutCubic"),
    IN_OUT_CUBIC("InOutCubic"),
    OUT_EXPO("OutExpo"),
    OUT_BACK("OutBack");

    public final String jsonValue;

    NeoEase(String jsonValue) { this.jsonValue = jsonValue; }
}
