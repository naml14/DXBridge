using DXBridgeExamples.Examples;

namespace DXBridgeExamples;

internal static class Program
{
    private static readonly Dictionary<string, Func<string[], int>> Examples =
        new(StringComparer.OrdinalIgnoreCase)
        {
            ["Example01LoadVersionLogs"] = Example01LoadVersionLogs.Run,
            ["Example02EnumerateAdapters"] = Example02EnumerateAdapters.Run,
            ["Example03CreateDeviceErrors"] = Example03CreateDeviceErrors.Run,
            ["Example04Dx11ClearWindow"] = Example04Dx11ClearWindow.Run,
            ["Example05Dx11MovingTriangle"] = Example05Dx11MovingTriangle.Run,
        };

    public static int Main(string[] args)
    {
        if (args.Length == 0 || IsHelp(args[0]))
        {
            PrintUsage();
            return 0;
        }

        if (!Examples.TryGetValue(args[0], out var runner))
        {
            Console.Error.WriteLine($"Unknown example: {args[0]}");
            PrintUsage();
            return 1;
        }

        try
        {
            return runner(args[1..]);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }

    private static bool IsHelp(string value)
    {
        return value is "--help" or "-h" or "/?";
    }

    private static void PrintUsage()
    {
        Console.WriteLine("Usage: dotnet run --project examples/csharp/DXBridgeExamples.csproj -- <ExampleName> [args...]");
        Console.WriteLine("Available examples:");
        Console.WriteLine("  Example01LoadVersionLogs");
        Console.WriteLine("  Example02EnumerateAdapters");
        Console.WriteLine("  Example03CreateDeviceErrors");
        Console.WriteLine("  Example04Dx11ClearWindow");
        Console.WriteLine("  Example05Dx11MovingTriangle");
    }
}
