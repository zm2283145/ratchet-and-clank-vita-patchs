using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Replanetizer.MemoryHook
{
    internal sealed class MacOSProcessMemory : IProcessMemory
    {
        private readonly Process? process;
        private readonly uint task;

        public bool IsAvailable { get; }
        public string ErrorMessage { get; }

        public MacOSProcessMemory()
        {
            Process[] processList = Process.GetProcessesByName("rpcs3");
            if (processList.Length == 0)
            {
                ErrorMessage = "Failed to find a running RPCS3 process.";
                return;
            }

            process = processList[0];
            int result = task_for_pid(mach_task_self(), process.Id, out task);
            if (result != KERN_SUCCESS)
            {
                ErrorMessage = "Failed to access the RPCS3 process. macOS may require additional process permissions.";
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

            GCHandle pinnedBuffer = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                int result = mach_vm_read_overwrite(
                    task,
                    unchecked((ulong) address),
                    (ulong) buffer.Length,
                    pinnedBuffer.AddrOfPinnedObject(),
                    out ulong bytesRead);
                return result == KERN_SUCCESS && bytesRead == (ulong) buffer.Length;
            }
            finally
            {
                pinnedBuffer.Free();
            }
        }

        public bool Suspend()
        {
            return IsAvailable && task_suspend(task) == KERN_SUCCESS;
        }

        public bool Resume()
        {
            return IsAvailable && task_resume(task) == KERN_SUCCESS;
        }

        public void Dispose()
        {
            if (task != 0)
            {
                mach_port_deallocate(mach_task_self(), task);
            }

            process?.Dispose();
        }

        private const int KERN_SUCCESS = 0;

        [DllImport("/usr/lib/libSystem.B.dylib")]
        private static extern uint mach_task_self();

        [DllImport("/usr/lib/libSystem.B.dylib")]
        private static extern int task_for_pid(uint targetTask, int processId, out uint task);

        [DllImport("/usr/lib/libSystem.B.dylib")]
        private static extern int mach_vm_read_overwrite(
            uint targetTask,
            ulong address,
            ulong size,
            IntPtr data,
            out ulong outSize);

        [DllImport("/usr/lib/libSystem.B.dylib")]
        private static extern int task_suspend(uint task);

        [DllImport("/usr/lib/libSystem.B.dylib")]
        private static extern int task_resume(uint task);

        [DllImport("/usr/lib/libSystem.B.dylib")]
        private static extern int mach_port_deallocate(uint task, uint name);
    }
}
