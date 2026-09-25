using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Replanetizer.MemoryHook
{
    internal sealed class LinuxProcessMemory : IProcessMemory
    {
        private const int SIGSTOP = 19;
        private const int SIGCONT = 18;

        private readonly Process? process;
        private readonly int processId;

        public bool IsAvailable { get; }
        public string ErrorMessage { get; }

        public LinuxProcessMemory()
        {
            Process[] processList = Process.GetProcessesByName("rpcs3");
            if (processList.Length == 0)
            {
                ErrorMessage = "Failed to find a running RPCS3 process.";
                return;
            }

            process = processList[0];
            processId = process.Id;
            IsAvailable = true;
            ErrorMessage = "Success!";
        }

        public bool Read(long address, byte[] buffer)
        {
            if (!IsAvailable)
            {
                return false;
            }

            GCHandle pinnedBuffer = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                Iovec local = new Iovec
                {
                    Base = pinnedBuffer.AddrOfPinnedObject(),
                    Length = new UIntPtr((uint) buffer.Length)
                };
                Iovec remote = new Iovec
                {
                    Base = new IntPtr(address),
                    Length = new UIntPtr((uint) buffer.Length)
                };

                IntPtr bytesRead = process_vm_readv(
                    processId,
                    ref local,
                    new UIntPtr(1),
                    ref remote,
                    new UIntPtr(1),
                    UIntPtr.Zero);
                return bytesRead.ToInt64() == buffer.Length;
            }
            finally
            {
                pinnedBuffer.Free();
            }
        }

        public bool Suspend()
        {
            return IsAvailable && kill(processId, SIGSTOP) == 0;
        }

        public bool Resume()
        {
            return IsAvailable && kill(processId, SIGCONT) == 0;
        }

        public void Dispose()
        {
            process?.Dispose();
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Iovec
        {
            public IntPtr Base;
            public UIntPtr Length;
        }

        [DllImport("libc", SetLastError = true)]
        private static extern IntPtr process_vm_readv(
            int processId,
            ref Iovec local,
            UIntPtr localCount,
            ref Iovec remote,
            UIntPtr remoteCount,
            UIntPtr flags);

        [DllImport("libc", SetLastError = true)]
        private static extern int kill(int processId, int signal);
    }
}
