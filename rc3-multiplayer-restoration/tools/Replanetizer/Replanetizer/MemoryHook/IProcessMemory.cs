using System;

namespace Replanetizer.MemoryHook
{
    internal interface IProcessMemory : IDisposable
    {
        bool IsAvailable { get; }
        string ErrorMessage { get; }

        bool Read(long address, byte[] buffer);
        bool Suspend();
        bool Resume();
    }

    internal interface IFrameWatchpoint : IDisposable
    {
        bool IsAvailable { get; }
        string ErrorMessage { get; }

        bool WaitForWrite();
        bool RearmAfterWrite();
        void RequestStop();
    }
}
