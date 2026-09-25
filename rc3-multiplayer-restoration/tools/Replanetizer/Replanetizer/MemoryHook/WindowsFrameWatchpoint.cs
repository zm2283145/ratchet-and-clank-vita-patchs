using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Replanetizer.MemoryHook
{
    internal sealed class WindowsFrameWatchpoint : IFrameWatchpoint
    {
        private const uint CREATE_PROCESS_DEBUG_EVENT = 3;
        private const uint CREATE_THREAD_DEBUG_EVENT = 2;
        private const uint EXIT_THREAD_DEBUG_EVENT = 4;
        private const uint EXIT_PROCESS_DEBUG_EVENT = 5;
        private const uint EXCEPTION_DEBUG_EVENT = 1;
        private const uint LOAD_DLL_DEBUG_EVENT = 6;
        private const uint EXCEPTION_SINGLE_STEP = 0x80000004;
        private const uint DBG_CONTINUE = 0x00010002;
        private const uint DBG_EXCEPTION_NOT_HANDLED = 0x80010001;
        private const uint CONTEXT_DEBUG_REGISTERS = 0x00100010;
        private const uint THREAD_GET_CONTEXT = 0x0008;
        private const uint THREAD_SET_CONTEXT = 0x0010;
        private const uint THREAD_QUERY_INFORMATION = 0x0040;
        private const uint PROCESS_SUSPEND_RESUME = 0x0800;
        private const uint DEBUG_CONTROL_MASK = 0x000D0001;
        private const int WAIT_TIMEOUT = 100;
        private const int DEBUG_EVENT_DATA_SIZE = 160;
        private const int CONTEXT_SIZE = 1232;
        private const int CONTEXT_FLAGS_OFFSET = 48;
        private const int DR0_OFFSET = 72;
        private const int DR6_OFFSET = 104;
        private const int DR7_OFFSET = 112;
        private static readonly byte[] EMPTY_CONTEXT = new byte[CONTEXT_SIZE];

        private readonly int PROCESS_ID;
        private readonly long FRAME_ADDRESS;
        private IntPtr processHandle;
        private readonly Dictionary<uint, DebugRegisterState> THREAD_STATES =
            new Dictionary<uint, DebugRegisterState>();
        private readonly object STATE_LOCK = new object();
        private bool attached;
        private bool attachAttempted;
        private bool stoppedAtWrite;
        private uint stoppedThreadId;
        private uint pendingEventProcessId;
        private uint pendingEventThreadId;
        private bool hasPendingEvent;
        private volatile bool stopRequested;
        private bool setupFailed;

        public bool IsAvailable { get; }
        public string ErrorMessage { get; private set; }

        public WindowsFrameWatchpoint(int processId, long frameAddress)
        {
            PROCESS_ID = processId;
            FRAME_ADDRESS = frameAddress;

            if (IntPtr.Size != 8)
            {
                ErrorMessage = "Hardware frame watchpoints require a 64-bit Replanetizer process.";
                return;
            }

            IsAvailable = true;
            ErrorMessage = "Hardware frame watchpoint ready.";
        }

        public bool WaitForWrite()
        {
            if (!IsAvailable || stopRequested || stoppedAtWrite) return false;
            if (!EnsureAttached()) return false;

            while (!stopRequested)
            {
                DebugEvent debugEvent = new DebugEvent();
                SetLastError(0);
                if (!WaitForDebugEvent(debugEvent, WAIT_TIMEOUT))
                {
                    if (stopRequested) return false;

                    int error = Marshal.GetLastWin32Error();
                    if (error == 0 || error == 121 || error == 258) continue;

                    ErrorMessage = $"Waiting for the frame watchpoint failed (Win32 error {error}).";
                    return false;
                }

                if (debugEvent.ProcessId != PROCESS_ID)
                {
                    ContinueDebugEvent(debugEvent.ProcessId, debugEvent.ThreadId, GetContinueStatus(debugEvent));
                    continue;
                }

                if (debugEvent.Code == CREATE_PROCESS_DEBUG_EVENT)
                {
                    setupFailed = !InstallBreakpoint(
                        debugEvent.ThreadId);
                    CloseHandleIfValid(ReadHandle(debugEvent.Data, 0));
                }
                else if (debugEvent.Code == CREATE_THREAD_DEBUG_EVENT)
                {
                    InstallBreakpoint(
                        debugEvent.ThreadId);
                }
                else if (debugEvent.Code == LOAD_DLL_DEBUG_EVENT)
                {
                    CloseHandleIfValid(ReadHandle(debugEvent.Data, 0));
                }
                else if (debugEvent.Code == EXIT_THREAD_DEBUG_EVENT)
                {
                    lock (STATE_LOCK)
                    {
                        THREAD_STATES.Remove(debugEvent.ThreadId);
                    }
                }
                else if (debugEvent.Code == EXIT_PROCESS_DEBUG_EVENT)
                {
                    ErrorMessage = "RPCS3 exited while waiting for the frame watchpoint.";
                    ContinueDebugEvent(debugEvent.ProcessId, debugEvent.ThreadId, DBG_CONTINUE);
                    attached = false;
                    return false;
                }
                else if (debugEvent.Code == EXCEPTION_DEBUG_EVENT &&
                         ReadUInt32(debugEvent.Data, 0) == EXCEPTION_SINGLE_STEP &&
                         IsOurBreakpoint(debugEvent))
                {
                    stoppedAtWrite = true;
                    stoppedThreadId = debugEvent.ThreadId;
                    if (!ClearDebugStatus(debugEvent.ThreadId))
                    {
                        ContinueDebugEvent(debugEvent.ProcessId, debugEvent.ThreadId, DBG_CONTINUE);
                        stoppedAtWrite = false;
                        stoppedThreadId = 0;
                        return false;
                    }

                    pendingEventProcessId = debugEvent.ProcessId;
                    pendingEventThreadId = debugEvent.ThreadId;
                    hasPendingEvent = true;
                    return true;
                }

                if (!ContinueDebugEvent(debugEvent.ProcessId, debugEvent.ThreadId, GetContinueStatus(debugEvent)))
                {
                    ErrorMessage = $"Continuing RPCS3 debugger event failed (Win32 error {Marshal.GetLastWin32Error()}).";
                    return false;
                }

                if (setupFailed)
                {
                    return false;
                }
            }

            return false;
        }

        private static uint GetContinueStatus(DebugEvent debugEvent)
        {
            // We only handle single step exceptions. For all other exceptions we must tell Windows
            // that we didn't handle the exception so the RPCS3 handlers can take over.
            // For example, access violations are somehow quite frequent within in-level movies.
            return debugEvent.Code == EXCEPTION_DEBUG_EVENT ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE;
        }

        private bool EnsureAttached()
        {
            if (attached) return true;
            if (attachAttempted)
            {
                return false;
            }

            attachAttempted = true;
            if (!DebugActiveProcess(PROCESS_ID))
            {
                ErrorMessage = $"Failed to attach a debugger to RPCS3 (Win32 error {Marshal.GetLastWin32Error()}).";
                return false;
            }

            attached = true;
            processHandle = OpenProcess(PROCESS_SUSPEND_RESUME, false, PROCESS_ID);
            DebugSetProcessKillOnExit(false);
            ErrorMessage = "Hardware frame watchpoint attached.";
            return true;
        }

        public bool RearmAfterWrite()
        {
            if (hasPendingEvent && !ContinuePendingEvent())
            {
                stoppedAtWrite = false;
                stoppedThreadId = 0;
                return false;
            }

            stoppedAtWrite = false;
            stoppedThreadId = 0;
            return true;
        }

        public void RequestStop()
        {
            stopRequested = true;
        }

        public void Dispose()
        {
            stopRequested = true;
            if (hasPendingEvent)
            {
                ContinuePendingEvent();
            }

            bool processSuspended = processHandle != IntPtr.Zero &&
                                    NtSuspendProcess(processHandle) >= 0;
            try
            {
                RestoreBreakpoints();
                if (attached)
                {
                    DebugActiveProcessStop(PROCESS_ID);
                    attached = false;
                }
            }
            finally
            {
                if (processSuspended)
                {
                    NtResumeProcess(processHandle);
                }

                if (processHandle != IntPtr.Zero)
                {
                    CloseHandle(processHandle);
                    processHandle = IntPtr.Zero;
                }
            }
        }

        private bool ContinuePendingEvent()
        {
            if (!ContinueDebugEvent(
                pendingEventProcessId,
                pendingEventThreadId,
                DBG_CONTINUE))
            {
                ErrorMessage = $"Continuing after the frame snapshot failed (Win32 error {Marshal.GetLastWin32Error()}).";
                return false;
            }

            hasPendingEvent = false;
            pendingEventProcessId = 0;
            pendingEventThreadId = 0;
            return true;
        }

        private bool InstallBreakpoint(uint threadId)
        {
            IntPtr threadHandle = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                false,
                threadId);

            if (threadHandle == IntPtr.Zero)
            {
                lock (STATE_LOCK)
                {
                    THREAD_STATES.Remove(threadId);
                }
                return true;
            }

            try
            {
                if (!TryGetDebugRegisters(
                    threadHandle,
                    out ulong originalDr0,
                    out ulong originalDr6,
                    out ulong originalDr7,
                    out int contextError))
                {
                    ErrorMessage = $"Failed to read RPCS3 thread {threadId} debug registers (Win32 error {contextError}).";
                    return false;
                }

                if ((originalDr7 & 3) != 0)
                {
                    ErrorMessage = $"RPCS3 thread {threadId} already uses hardware breakpoint slot 0.";
                    return false;
                }

                lock (STATE_LOCK)
                {
                    if (THREAD_STATES.ContainsKey(threadId)) return true;

                    THREAD_STATES[threadId] = new DebugRegisterState
                    {
                        Dr0 = originalDr0,
                        Dr6 = originalDr6,
                        Dr7 = originalDr7
                    };
                }

                ulong watchpointDr7 = (originalDr7 & ~DEBUG_CONTROL_MASK) | DEBUG_CONTROL_MASK;
                if (!TrySetDebugRegisters(
                    threadHandle,
                    unchecked((ulong) FRAME_ADDRESS),
                    0,
                    watchpointDr7,
                    out contextError))
                {
                    lock (STATE_LOCK)
                    {
                        THREAD_STATES.Remove(threadId);
                    }

                    ErrorMessage = $"Failed to install the frame watchpoint on RPCS3 thread {threadId} (Win32 error {contextError}).";
                    return false;
                }

                return true;
            }
            finally
            {
                CloseHandle(threadHandle);
            }
        }

        private bool IsOurBreakpoint(DebugEvent debugEvent)
        {
            IntPtr threadHandle = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                false,
                debugEvent.ThreadId);
            if (threadHandle == IntPtr.Zero) return false;

            try
            {
                if (!TryGetDebugRegisters(
                    threadHandle,
                    out ulong dr0,
                    out ulong dr6,
                    out _,
                    out _))
                {
                    return false;
                }

                return (dr6 & 1) != 0 &&
                       dr0 == unchecked((ulong) FRAME_ADDRESS);
            }
            finally
            {
                CloseHandle(threadHandle);
            }
        }

        private bool ClearDebugStatus(uint threadId)
        {
            IntPtr threadHandle = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                false,
                threadId);
            if (threadHandle == IntPtr.Zero)
            {
                lock (STATE_LOCK)
                {
                    THREAD_STATES.Remove(threadId);
                }
                ErrorMessage = $"Failed to open RPCS3 thread {threadId} to clear the frame watchpoint status (Win32 error {Marshal.GetLastWin32Error()}).";
                return false;
            }

            try
            {
                if (!TryGetDebugRegisters(
                    threadHandle,
                    out ulong dr0,
                    out _,
                    out ulong dr7,
                    out int contextError))
                {
                    ErrorMessage = $"Failed to read RPCS3 thread {threadId} debug registers (Win32 error {contextError}).";
                    return false;
                }

                if (!TrySetDebugRegisters(threadHandle, dr0, 0, dr7, out contextError))
                {
                    ErrorMessage = $"Failed to clear RPCS3 thread {threadId} debug status (Win32 error {contextError}).";
                    return false;
                }

                return true;
            }
            finally
            {
                CloseHandle(threadHandle);
            }
        }

        private void RestoreBreakpoints()
        {
            KeyValuePair<uint, DebugRegisterState>[] states;
            lock (STATE_LOCK)
            {
                states = new List<KeyValuePair<uint, DebugRegisterState>>(THREAD_STATES).ToArray();
                THREAD_STATES.Clear();
            }

            foreach (KeyValuePair<uint, DebugRegisterState> state in states)
            {
                IntPtr threadHandle = OpenThread(
                    THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                    false,
                    state.Key);
                if (threadHandle == IntPtr.Zero) continue;

                try
                {
                    TrySetDebugRegisters(
                        threadHandle,
                        state.Value.Dr0,
                        state.Value.Dr6,
                        state.Value.Dr7,
                        out _);
                }
                finally
                {
                    CloseHandle(threadHandle);
                }
            }
        }

        private static bool TryGetDebugRegisters(
            IntPtr threadHandle,
            out ulong dr0,
            out ulong dr6,
            out ulong dr7,
            out int error)
        {
            IntPtr allocation = Marshal.AllocHGlobal(CONTEXT_SIZE + 15);
            try
            {
                IntPtr context = AlignContext(allocation);
                Marshal.Copy(EMPTY_CONTEXT, 0, context, EMPTY_CONTEXT.Length);
                Marshal.WriteInt32(context, CONTEXT_FLAGS_OFFSET, unchecked((int) CONTEXT_DEBUG_REGISTERS));

                if (!GetThreadContext(threadHandle, context))
                {
                    dr0 = 0;
                    dr6 = 0;
                    dr7 = 0;
                    error = Marshal.GetLastWin32Error();
                    return false;
                }

                dr0 = unchecked((ulong) Marshal.ReadInt64(context, DR0_OFFSET));
                dr6 = unchecked((ulong) Marshal.ReadInt64(context, DR6_OFFSET));
                dr7 = unchecked((ulong) Marshal.ReadInt64(context, DR7_OFFSET));
                error = 0;
                return true;
            }
            finally
            {
                Marshal.FreeHGlobal(allocation);
            }
        }

        private static bool TrySetDebugRegisters(
            IntPtr threadHandle,
            ulong dr0,
            ulong dr6,
            ulong dr7,
            out int error)
        {
            IntPtr allocation = Marshal.AllocHGlobal(CONTEXT_SIZE + 15);
            try
            {
                IntPtr context = AlignContext(allocation);
                Marshal.Copy(EMPTY_CONTEXT, 0, context, EMPTY_CONTEXT.Length);
                Marshal.WriteInt32(context, CONTEXT_FLAGS_OFFSET, unchecked((int) CONTEXT_DEBUG_REGISTERS));
                Marshal.WriteInt64(context, DR0_OFFSET, unchecked((long) dr0));
                Marshal.WriteInt64(context, DR6_OFFSET, unchecked((long) dr6));
                Marshal.WriteInt64(context, DR7_OFFSET, unchecked((long) dr7));

                if (!SetThreadContext(threadHandle, context))
                {
                    error = Marshal.GetLastWin32Error();
                    return false;
                }

                error = 0;
                return true;
            }
            finally
            {
                Marshal.FreeHGlobal(allocation);
            }
        }

        private static IntPtr AlignContext(IntPtr allocation)
        {
            long address = allocation.ToInt64();
            return new IntPtr((address + 15) & ~15L);
        }

        private static IntPtr ReadHandle(byte[] data, int offset)
        {
            return new IntPtr(unchecked((long) BitConverter.ToUInt64(data, offset)));
        }

        private static uint ReadUInt32(byte[] data, int offset)
        {
            return BitConverter.ToUInt32(data, offset);
        }

        private static void CloseHandleIfValid(IntPtr handle)
        {
            if (handle != IntPtr.Zero && handle != new IntPtr(-1))
            {
                CloseHandle(handle);
            }
        }

        private sealed class DebugRegisterState
        {
            public ulong Dr0;
            public ulong Dr6;
            public ulong Dr7;
        }

        [StructLayout(LayoutKind.Sequential)]
        private sealed class DebugEvent
        {
            public uint Code;
            public uint ProcessId;
            public uint ThreadId;
            private uint padding;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = DEBUG_EVENT_DATA_SIZE)]
            public byte[] Data = new byte[DEBUG_EVENT_DATA_SIZE];
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DebugActiveProcess(int processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DebugActiveProcessStop(int processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DebugSetProcessKillOnExit(bool killOnExit);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool WaitForDebugEvent(
            [In, Out] DebugEvent debugEvent,
            uint milliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool ContinueDebugEvent(
            uint processId,
            uint threadId,
            uint continueStatus);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenThread(
            uint desiredAccess,
            bool inheritHandle,
            uint threadId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(
            uint desiredAccess,
            bool inheritHandle,
            int processId);

        [DllImport("ntdll.dll")]
        private static extern int NtSuspendProcess(IntPtr processHandle);

        [DllImport("ntdll.dll")]
        private static extern int NtResumeProcess(IntPtr processHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetThreadContext(
            IntPtr threadHandle,
            IntPtr context);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetThreadContext(
            IntPtr threadHandle,
            IntPtr context);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern void SetLastError(uint errorCode);
    }
}
