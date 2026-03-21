namespace DXBridgeExamples.DxBridge;

internal sealed class CommandLine
{
    private readonly Dictionary<string, string> _values = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _flags = new(StringComparer.OrdinalIgnoreCase);

    public CommandLine(string[] args, IEnumerable<string> valueOptions, IEnumerable<string> flagOptions)
    {
        var valueSet = new HashSet<string>(valueOptions, StringComparer.OrdinalIgnoreCase);
        var flagSet = new HashSet<string>(flagOptions, StringComparer.OrdinalIgnoreCase);

        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            if (arg is "--help" or "-h" or "/?")
            {
                HelpRequested = true;
                continue;
            }

            if (flagSet.Contains(arg))
            {
                _flags.Add(arg);
                continue;
            }

            if (valueSet.Contains(arg))
            {
                if (i + 1 >= args.Length)
                {
                    throw new ArgumentException($"Missing value for {arg}");
                }

                _values[arg] = args[++i];
                continue;
            }

            throw new ArgumentException($"Unknown argument: {arg}");
        }
    }

    public bool HelpRequested { get; }

    public bool HasFlag(string name)
    {
        return _flags.Contains(name);
    }

    public string? GetString(string name, string? defaultValue = null)
    {
        return _values.TryGetValue(name, out var value) ? value : defaultValue;
    }

    public int GetInt(string name, int defaultValue)
    {
        var value = GetString(name, null);
        if (value is null)
        {
            return defaultValue;
        }

        if (!int.TryParse(value, out var parsed))
        {
            throw new ArgumentException($"Invalid integer for {name}: {value}");
        }

        return parsed;
    }

    public float GetFloat(string name, float defaultValue)
    {
        var value = GetString(name, null);
        if (value is null)
        {
            return defaultValue;
        }

        if (!float.TryParse(value, out var parsed))
        {
            throw new ArgumentException($"Invalid float for {name}: {value}");
        }

        return parsed;
    }

    public float[] GetFloatList(string name, int expectedCount, float[] defaultValue)
    {
        var value = GetString(name, null);
        if (value is null)
        {
            return defaultValue;
        }

        var parts = value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (parts.Length != expectedCount)
        {
            throw new ArgumentException($"Expected {expectedCount} comma-separated floats for {name}, got: {value}");
        }

        var result = new float[expectedCount];
        for (var i = 0; i < parts.Length; i++)
        {
            if (!float.TryParse(parts[i], out result[i]))
            {
                throw new ArgumentException($"Invalid float in {name}: {parts[i]}");
            }
        }

        return result;
    }
}
