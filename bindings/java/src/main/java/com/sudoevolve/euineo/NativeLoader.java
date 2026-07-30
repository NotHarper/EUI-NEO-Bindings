package com.sudoevolve.euineo;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

final class NativeLoader {
    private static final String LIBRARY = "eui_neo_jni";
    private static volatile boolean loaded;

    private NativeLoader() {}

    static synchronized void load() {
        if (loaded) return;
        String override = System.getProperty("eui.neo.native");
        try {
            if (override != null && !override.isBlank()) {
                System.load(Path.of(override).toAbsolutePath().toString());
            } else {
                String resource = "/natives/" + platformName() + "/" + System.mapLibraryName(LIBRARY);
                try (InputStream input = NativeLoader.class.getResourceAsStream(resource)) {
                    if (input == null) {
                        System.loadLibrary(LIBRARY);
                    } else {
                        Path extracted = Files.createTempFile("eui-neo-", extension());
                        extracted.toFile().deleteOnExit();
                        Files.copy(input, extracted, StandardCopyOption.REPLACE_EXISTING);
                        System.load(extracted.toAbsolutePath().toString());
                    }
                }
            }
            loaded = true;
        } catch (IOException | UnsatisfiedLinkError error) {
            throw new NeoException("Unable to load " + LIBRARY + ": " + error.getMessage(), -1);
        }
    }

    private static String platformName() {
        String os = System.getProperty("os.name", "").toLowerCase();
        String arch = System.getProperty("os.arch", "").toLowerCase();
        String normalizedArch = arch.contains("aarch64") || arch.contains("arm64") ? "aarch64" : "x86_64";
        if (os.contains("win")) return "windows-" + normalizedArch;
        if (os.contains("mac") || os.contains("darwin")) return "macos-" + normalizedArch;
        if (os.contains("linux")) return "linux-" + normalizedArch;
        throw new NeoException("Unsupported Java platform: " + os + "/" + arch, -1);
    }

    private static String extension() {
        String os = System.getProperty("os.name", "").toLowerCase();
        if (os.contains("win")) return ".dll";
        if (os.contains("mac") || os.contains("darwin")) return ".dylib";
        return ".so";
    }
}
