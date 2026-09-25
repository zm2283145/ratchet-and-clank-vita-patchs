using System.Runtime.InteropServices;

namespace Replanetizer.MemoryHook
{
    internal static class ProcessMemoryFactory
    {
        public static IProcessMemory Create()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return new WindowsProcessMemory();
            }

            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                return new LinuxProcessMemory();
            }

            if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return new MacOSProcessMemory();
            }

            return new UnsupportedProcessMemory("Memory hooks are not supported on this operating system.");
        }
    }

    internal sealed class UnsupportedProcessMemory : IProcessMemory
    {
        public bool IsAvailable => false;
        public string ErrorMessage { get; }

        public UnsupportedProcessMemory(string errorMessage)
        {
            ErrorMessage = errorMessage;
        }

        public bool Read(long address, byte[] buffer)
        {
            return false;
        }

        public bool Suspend()
        {
            return false;
        }

        public bool Resume()
        {
            return false;
        }

        public void Dispose()
        {
        }
    }
}
