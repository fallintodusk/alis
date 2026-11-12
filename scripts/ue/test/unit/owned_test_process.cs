// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Microsoft.Win32.SafeHandles;

namespace Project.Testing
{
    public sealed class ProjectOwnedTestProcess : IDisposable
    {
        private SafeFileHandle job;
        private IntPtr process;
        public StreamReader StandardOutput { get; private set; }
        public StreamReader StandardError { get; private set; }
        public int Id { get; private set; }

        public static ProjectOwnedTestProcess Start(ProcessStartInfo info)
        {
            if (info.UseShellExecute || !info.RedirectStandardOutput || !info.RedirectStandardError)
                throw new ArgumentException("Owned test execution requires redirected, shell-free startup.");
            var owner = new ProjectOwnedTestProcess();
            var native = new ProcessInformation();
            AnonymousPipeServerStream output = null, error = null;
            SafeFileHandle input = null;
            IntPtr attributes = IntPtr.Zero, handles = IntPtr.Zero;
            bool initialized = false, started = false;
            try
            {
                owner.job = CreateJobObject(IntPtr.Zero, null);
                Check(!owner.job.IsInvalid);
                var limits = new ExtendedLimits();
                limits.Basic.LimitFlags = 0x2000; // KILL_ON_JOB_CLOSE; descendants cannot break away.
                Check(SetInformationJobObject(owner.job, 9, ref limits, Marshal.SizeOf(limits)));
                output = new AnonymousPipeServerStream(PipeDirection.In, HandleInheritability.Inheritable);
                error = new AnonymousPipeServerStream(PipeDirection.In, HandleInheritability.Inheritable);
                var security = new SecurityAttributes { Length = Marshal.SizeOf(typeof(SecurityAttributes)), Inherit = true };
                input = CreateFile("NUL", 0x80000000, 3, ref security, 3, 0, IntPtr.Zero);
                Check(!input.IsInvalid);
                IntPtr bytes = IntPtr.Zero;
                InitializeProcThreadAttributeList(IntPtr.Zero, 1, 0, ref bytes);
                if (bytes == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
                attributes = Marshal.AllocHGlobal(bytes);
                Check(InitializeProcThreadAttributeList(attributes, 1, 0, ref bytes));
                initialized = true;
                handles = Marshal.AllocHGlobal(IntPtr.Size * 3);
                Marshal.WriteIntPtr(handles, 0, input.DangerousGetHandle());
                Marshal.WriteIntPtr(handles, IntPtr.Size, output.ClientSafePipeHandle.DangerousGetHandle());
                Marshal.WriteIntPtr(handles, IntPtr.Size * 2, error.ClientSafePipeHandle.DangerousGetHandle());
                Check(UpdateProcThreadAttribute(attributes, 0, new IntPtr(0x20002), handles,
                    new IntPtr(IntPtr.Size * 3), IntPtr.Zero, IntPtr.Zero));
                var startup = new StartupInfoEx();
                startup.Info.Size = Marshal.SizeOf(startup);
                startup.Info.Flags = 0x101; // USESTDHANDLES | USESHOWWINDOW(SW_HIDE).
                startup.Info.Input = input.DangerousGetHandle();
                startup.Info.Output = output.ClientSafePipeHandle.DangerousGetHandle();
                startup.Info.Error = error.ClientSafePipeHandle.DangerousGetHandle();
                startup.Attributes = attributes;
                var command = new StringBuilder("\"" + info.FileName + "\" " + info.Arguments);
                // No child code executes before containment; a fast parent cannot escape assignment.
                Check(CreateProcess(info.FileName, command, IntPtr.Zero, IntPtr.Zero, true,
                    0x08080004, IntPtr.Zero, String.IsNullOrEmpty(info.WorkingDirectory) ? null : info.WorkingDirectory,
                    ref startup, out native));
                owner.process = native.Process;
                owner.Id = native.Id;
                Check(AssignProcessToJobObject(owner.job, owner.process));
                output.DisposeLocalCopyOfClientHandle();
                error.DisposeLocalCopyOfClientHandle();
                owner.StandardOutput = new StreamReader(output, Console.OutputEncoding);
                owner.StandardError = new StreamReader(error, Console.OutputEncoding);
                Check(ResumeThread(native.Thread) != UInt32.MaxValue);
                started = true;
                return owner;
            }
            catch
            {
                // Assignment can fail under an incompatible outer Job. Never resume uncontained.
                if (native.Process != IntPtr.Zero)
                {
                    if (!TerminateProcess(native.Process, 1))
                        throw new Win32Exception(Marshal.GetLastWin32Error(), "Suspended test process termination failed.");
                    if (WaitForSingleObject(native.Process, 5000) != 0)
                        throw new InvalidOperationException("Suspended test process termination is unverified.");
                }
                throw;
            }
            finally
            {
                if (native.Thread != IntPtr.Zero) CloseHandle(native.Thread);
                if (initialized) DeleteProcThreadAttributeList(attributes);
                if (attributes != IntPtr.Zero) Marshal.FreeHGlobal(attributes);
                if (handles != IntPtr.Zero) Marshal.FreeHGlobal(handles);
                if (input != null) input.Dispose();
                if (!started)
                {
                    if (output != null) output.Dispose();
                    if (error != null) error.Dispose();
                    owner.Dispose();
                }
            }
        }

        public bool WaitForExit(int milliseconds)
        {
            uint result = WaitForSingleObject(process, checked((uint)milliseconds));
            if (result == UInt32.MaxValue) throw new Win32Exception(Marshal.GetLastWin32Error());
            return result == 0;
        }

        public int ExitCode
        {
            get
            {
                if (!WaitForExit(0)) throw new InvalidOperationException("Owned process has not exited.");
                uint code;
                Check(GetExitCodeProcess(process, out code));
                return unchecked((int)code);
            }
        }

        public bool Stop(int milliseconds)
        {
            Check(TerminateJobObject(job, 124));
            var clock = Stopwatch.StartNew();
            do
            {
                Accounting state;
                Check(QueryInformationJobObject(job, 1, out state, Marshal.SizeOf(typeof(Accounting)), IntPtr.Zero));
                if (state.ActiveProcesses == 0) return true;
                Thread.Sleep(20);
            } while (clock.ElapsedMilliseconds < milliseconds);
            return false;
        }

        public void Dispose()
        {
            if (job != null) { job.Dispose(); job = null; }
            if (process != IntPtr.Zero) { CloseHandle(process); process = IntPtr.Zero; }
            if (StandardOutput != null) { StandardOutput.Dispose(); StandardOutput = null; }
            if (StandardError != null) { StandardError.Dispose(); StandardError = null; }
        }

        private static void Check(bool success) { if (!success) throw new Win32Exception(Marshal.GetLastWin32Error()); }
        [StructLayout(LayoutKind.Sequential)] private struct SecurityAttributes { public int Length; public IntPtr Descriptor; [MarshalAs(UnmanagedType.Bool)] public bool Inherit; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] private struct StartupInfo
        {
            public int Size; public string Reserved, Desktop, Title;
            public int X, Y, XSize, YSize, XCount, YCount, Fill, Flags;
            public short Show, ReservedSize; public IntPtr ReservedBytes, Input, Output, Error;
        }
        [StructLayout(LayoutKind.Sequential)] private struct StartupInfoEx { public StartupInfo Info; public IntPtr Attributes; }
        [StructLayout(LayoutKind.Sequential)] private struct ProcessInformation { public IntPtr Process, Thread; public int Id, ThreadId; }
        [StructLayout(LayoutKind.Sequential)] private struct BasicLimits
        {
            public long ProcessTime, JobTime; public uint LimitFlags;
            public UIntPtr MinimumWorkingSet, MaximumWorkingSet; public uint ActiveProcessLimit;
            public UIntPtr Affinity; public uint Priority, Scheduling;
        }
        [StructLayout(LayoutKind.Sequential)] private struct ExtendedLimits
        {
            public BasicLimits Basic; public ulong ReadCount, WriteCount, OtherCount, ReadBytes, WriteBytes, OtherBytes;
            public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
        }
        [StructLayout(LayoutKind.Sequential)] private struct Accounting
        {
            public long User, Kernel, PeriodUser, PeriodKernel;
            public uint Faults, TotalProcesses, ActiveProcesses, TerminatedProcesses;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern SafeFileHandle CreateJobObject(IntPtr security, string name);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool SetInformationJobObject(SafeFileHandle job, int kind, ref ExtendedLimits info, int size);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool AssignProcessToJobObject(SafeFileHandle job, IntPtr process);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool TerminateJobObject(SafeFileHandle job, uint code);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool QueryInformationJobObject(SafeFileHandle job, int kind, out Accounting info, int size, IntPtr returned);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern SafeFileHandle CreateFile(string name, uint access, uint share, ref SecurityAttributes security, uint disposition, uint flags, IntPtr template);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool InitializeProcThreadAttributeList(IntPtr list, int count, int flags, ref IntPtr size);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool UpdateProcThreadAttribute(IntPtr list, uint flags, IntPtr attribute, IntPtr value, IntPtr size, IntPtr previous, IntPtr returned);
        [DllImport("kernel32.dll")] private static extern void DeleteProcThreadAttributeList(IntPtr list);
        [DllImport("kernel32.dll", EntryPoint = "CreateProcessW", CharSet = CharSet.Unicode, SetLastError = true)] private static extern bool CreateProcess(string application, StringBuilder command, IntPtr processSecurity, IntPtr threadSecurity, bool inherit, uint flags, IntPtr environment, string directory, ref StartupInfoEx startup, out ProcessInformation info);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool TerminateProcess(IntPtr process, uint code);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool GetExitCodeProcess(IntPtr process, out uint code);
        [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
    }
}
