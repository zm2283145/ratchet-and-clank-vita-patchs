using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Replanetizer.MemoryHook
{
    internal sealed class WindowsProcessMemory : IProcessMemory
    {
        private const int PROCESS_WM_READ = 0x38;
        private const int PROCESS_SUSPEND_RESUME = 0x0800;

        private readonly Process? process;
        private readonly IntPtr processHandle;

        public bool IsAvailable { get; }
        public string ErrorMessage { get; }
        internal int ProcessId => process?.Id ?? 0;

        public WindowsProcessMemory()
        {
            Process[] processList = Process.GetProcessesByName("rpcs3");
            if (processList.Length == 0)
            {
                ErrorMessage = "Failed to find a running RPCS3 process.";
                return;
            }

            process = processList[0];
            processHandle = OpenProcess(PROCESS_WM_READ | PROCESS_SUSPEND_RESUME, false, process.Id);
            if (processHandle == IntPtr.Zero)
            {
                ErrorMessage = "Failed to open the RPCS3 process.";
                process.Dispose();
                process = null;
                return;
            }

            IsAvailable = true;
            ErrorMessage = "Success!";
        }

        public bool Read(long address, byte[] buffer)
        {
            if (!IsAvailable)
            {
                return false;
            }

            int bytesRead = 0;
            bool readSucceeded = ReadProcessMemory(processHandle, address, buffer, buffer.Length, ref bytesRead);
            return readSucceeded && bytesRead == buffer.Length;
        }

        public bool Suspend()
        {
            return IsAvailable && NtSuspendProcess(processHandle) >= 0;
        }

        public bool Resume()
        {
            return IsAvailable && NtResumeProcess(processHandle) >= 0;
        }

        public void Dispose()
        {
            if (processHandle != IntPtr.Zero)
            {
                CloseHandle(processHandle);
            }

            process?.Dispose();
        }

        [DllImport("kernel32.dll")]
        private static extern IntPtr OpenProcess(int desiredAccess, bool inheritHandle, int processId);

        [DllImport("kernel32.dll")]
        private static extern bool ReadProcessMemory(
            IntPtr processHandle,
            long baseAddress,
            byte[] buffer,
            int size,
            ref int numberOfBytesRead);

        [DllImport("ntdll.dll")]
        private static extern int NtSuspendProcess(IntPtr processHandle);

        [DllImport("ntdll.dll")]
        private static extern int NtResumeProcess(IntPtr processHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);
    }
}
