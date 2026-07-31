package com.sudoevolve.euineo;

public class SmokeTest {
    public static void main(String[] args) {
        int failures = 0;

        // Library load + version query (headless, no window created)
        try {
            String version = NeoEngine.version();
            if (version == null || version.isEmpty()) {
                System.err.println("FAIL: version() returned empty string");
                failures++;
            } else {
                System.out.println("PASS: version() = " + version);
            }
        } catch (Exception e) {
            System.err.println("FAIL: version() threw: " + e);
            failures++;
        }

        // NeoConfig defaults
        try {
            NeoConfig config = new NeoConfig();
            if (config.width <= 0 || config.height <= 0) {
                System.err.println("FAIL: NeoConfig default size is zero or negative: " + config.width + "x" + config.height);
                failures++;
            } else {
                System.out.println("PASS: NeoConfig defaults ok: " + config.width + "x" + config.height);
            }
        } catch (Exception e) {
            System.err.println("FAIL: NeoConfig() threw: " + e);
            failures++;
        }

        // NeoConfig builder chain
        try {
            NeoConfig config = new NeoConfig().title("Test").size(1024, 768).framesPerSecond(60.0);
            if (!config.title.equals("Test") || config.width != 1024 || config.height != 768) {
                System.err.println("FAIL: NeoConfig builder values not set correctly");
                failures++;
            } else {
                System.out.println("PASS: NeoConfig builder chain ok");
            }
        } catch (Exception e) {
            System.err.println("FAIL: NeoConfig builder threw: " + e);
            failures++;
        }

        System.out.println(failures == 0 ? "All smoke tests passed." : failures + " smoke test(s) FAILED.");
        System.exit(failures == 0 ? 0 : 1);
    }
}
