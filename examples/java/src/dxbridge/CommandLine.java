package dxbridge;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public final class CommandLine {
    private final Map<String, String> values = new HashMap<String, String>();
    private final Set<String> flags = new HashSet<String>();
    private boolean helpRequested;

    public CommandLine(String[] args, Set<String> valueOptions, Set<String> flagOptions) {
        parse(args, valueOptions, flagOptions);
    }

    public static Set<String> options(String... names) {
        return new HashSet<String>(Arrays.asList(names));
    }

    public boolean isHelpRequested() {
        return helpRequested;
    }

    public boolean hasFlag(String name) {
        return flags.contains(name);
    }

    public String getString(String name, String defaultValue) {
        String value = values.get(name);
        return value == null ? defaultValue : value;
    }

    public int getInt(String name, int defaultValue) {
        String value = values.get(name);
        if (value == null) {
            return defaultValue;
        }
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException ex) {
            throw new IllegalArgumentException("Invalid integer for " + name + ": " + value);
        }
    }

    public float getFloat(String name, float defaultValue) {
        String value = values.get(name);
        if (value == null) {
            return defaultValue;
        }
        try {
            return Float.parseFloat(value);
        } catch (NumberFormatException ex) {
            throw new IllegalArgumentException("Invalid float for " + name + ": " + value);
        }
    }

    public float[] getFloatList(String name, int expectedCount, float[] defaultValue) {
        String value = values.get(name);
        if (value == null) {
            return defaultValue;
        }

        String[] parts = value.split(",");
        if (parts.length != expectedCount) {
            throw new IllegalArgumentException(
                "Expected " + expectedCount + " comma-separated floats for " + name + ", got: " + value
            );
        }

        float[] parsed = new float[expectedCount];
        for (int i = 0; i < parts.length; i++) {
            try {
                parsed[i] = Float.parseFloat(parts[i].trim());
            } catch (NumberFormatException ex) {
                throw new IllegalArgumentException("Invalid float in " + name + ": " + parts[i]);
            }
        }
        return parsed;
    }

    private void parse(String[] args, Set<String> valueOptions, Set<String> flagOptions) {
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if ("--help".equals(arg) || "-h".equals(arg) || "/?".equals(arg)) {
                helpRequested = true;
                continue;
            }

            if (flagOptions.contains(arg)) {
                flags.add(arg);
                continue;
            }

            if (valueOptions.contains(arg)) {
                if (i + 1 >= args.length) {
                    throw new IllegalArgumentException("Missing value for " + arg);
                }
                values.put(arg, args[++i]);
                continue;
            }

            throw new IllegalArgumentException("Unknown argument: " + arg);
        }
    }
}
