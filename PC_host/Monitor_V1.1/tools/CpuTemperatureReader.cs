using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Principal;
using LibreHardwareMonitor.Hardware;

internal static class CpuTemperatureReader
{
    private static bool IsAdministrator()
    {
        WindowsIdentity identity = WindowsIdentity.GetCurrent();
        return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
    }

    private static void Collect(IHardware hardware, List<Tuple<string, float>> values)
    {
        hardware.Update();
        foreach (ISensor sensor in hardware.Sensors)
        {
            if (sensor.SensorType == SensorType.Temperature && sensor.Value.HasValue)
                values.Add(Tuple.Create(sensor.Name ?? "CPU", sensor.Value.Value));
        }
        foreach (IHardware child in hardware.SubHardware)
            Collect(child, values);
    }

    public static int Main(string[] args)
    {
        bool diagnose = false;
        StreamWriter diagnosticWriter = null;
        foreach (string argument in args)
        {
            if (argument == "--diagnose")
                diagnose = true;
            if (argument.StartsWith("--diagnose-file=", StringComparison.OrdinalIgnoreCase))
            {
                diagnose = true;
                string path = argument.Substring("--diagnose-file=".Length).Trim('"');
                diagnosticWriter = new StreamWriter(path, false, System.Text.Encoding.UTF8);
                diagnosticWriter.AutoFlush = true;
                Console.SetError(diagnosticWriter);
            }
        }
        try
        {
            Computer computer = new Computer { IsCpuEnabled = true };
            try
            {
                computer.Open();
                List<Tuple<string, float>> values = new List<Tuple<string, float>>();
                foreach (IHardware hardware in computer.Hardware)
                {
                    if (diagnose)
                        Console.Error.WriteLine("Hardware={0}; Type={1}", hardware.Name, hardware.HardwareType);
                    if (hardware.HardwareType == HardwareType.Cpu)
                    {
                        Collect(hardware, values);
                        if (diagnose)
                        {
                            hardware.Update();
                            foreach (ISensor sensor in hardware.Sensors)
                                Console.Error.WriteLine(
                                    "Sensor={0}; Type={1}; Value={2}",
                                    sensor.Name,
                                    sensor.SensorType,
                                    sensor.Value.HasValue ? sensor.Value.Value.ToString(CultureInfo.InvariantCulture) : "null"
                                );
                        }
                    }
                }

                Tuple<string, float> selected = null;
                foreach (Tuple<string, float> value in values)
                {
                    string name = value.Item1.ToLowerInvariant();
                    bool preferred = name.Contains("package") || name.Contains("tctl") || name.Contains("tdie");
                    if (selected == null || preferred || value.Item2 > selected.Item2)
                        selected = value;
                    if (preferred)
                        break;
                }
                if (selected == null)
                {
                    Console.Error.WriteLine(
                        "NO_CPU_TEMPERATURE; Admin={0}; CpuHardware={1}; Sensors={2}",
                        IsAdministrator(),
                        computer.Hardware.Count,
                        values.Count
                    );
                    return 2;
                }

                Console.WriteLine(
                    "{0}|{1}",
                    selected.Item2.ToString("0.0", CultureInfo.InvariantCulture),
                    selected.Item1.Replace("|", "/")
                );
                return 0;
            }
            finally
            {
                computer.Close();
            }
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error.Message);
            return 1;
        }
        finally
        {
            if (diagnosticWriter != null)
                diagnosticWriter.Dispose();
        }
    }
}
