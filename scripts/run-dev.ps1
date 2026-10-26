<#
.SYNOPSIS
  Runs the Kinetiq server or client with automatic cleanup.

.DESCRIPTION
  Starts the given target (server or client) and binds its lifetime to this
  script's own process using a Windows Job Object with the
  JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flag. This is the same mechanism a normal
  terminal window already uses to kill everything it started when you close
  it -- this script makes that guarantee hold even for detached/background
  launches, where the child process would otherwise be orphaned and keep
  running (and holding its port) after the launcher is gone.

  Stop it with Ctrl+C, or by killing this script's process -- either way, the
  server/client (and anything it spawned, e.g. npm's own child processes) is
  killed with it.

.EXAMPLE
  ./scripts/run-dev.ps1 -Target server
.EXAMPLE
  ./scripts/run-dev.ps1 -Target client
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("server", "client")]
    [string]$Target
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

switch ($Target) {
    "server" {
        $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
        $workDir = Join-Path $repoRoot "Server"
        $fileName = Join-Path $workDir "build\kinetiq_server.exe"
        $argumentList = ""
    }
    "client" {
        # Route through cmd.exe /c, same as a normal terminal does implicitly
        # for .cmd files -- invoking npm.cmd directly via Process.Start breaks
        # npm's own %~dp0-based path resolution.
        $workDir = Join-Path $repoRoot "Client"
        $fileName = "cmd.exe"
        $argumentList = "/c npm run dev"
    }
}

if ($Target -eq "server" -and -not (Test-Path $fileName)) {
    throw "Server binary not found at $fileName -- build it first: cmake --build --preset default (from Server/)"
}

# --- Windows Job Object plumbing (P/Invoke) -------------------------------
$jobSource = @"
using System;
using System.Runtime.InteropServices;

public static class DevJob
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateJobObject(IntPtr a, string lpName);

    [DllImport("kernel32.dll")]
    public static extern bool SetInformationJobObject(IntPtr hJob, int JobObjectInfoClass, IntPtr lpJobObjectInfo, uint cbJobObjectInfoLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool AssignProcessToJobObject(IntPtr hJob, IntPtr hProcess);

    [StructLayout(LayoutKind.Sequential)]
    public struct JOBOBJECT_BASIC_LIMIT_INFORMATION
    {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IO_COUNTERS
    {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
    {
        public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        public IO_COUNTERS IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    public const int JobObjectExtendedLimitInformation = 9;
    public const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x2000;

    public static IntPtr CreateKillOnCloseJob()
    {
        IntPtr job = CreateJobObject(IntPtr.Zero, null);
        var info = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        int length = Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
        IntPtr infoPtr = Marshal.AllocHGlobal(length);
        Marshal.StructureToPtr(info, infoPtr, false);

        SetInformationJobObject(job, JobObjectExtendedLimitInformation, infoPtr, (uint)length);
        Marshal.FreeHGlobal(infoPtr);
        return job;
    }
}
"@
Add-Type -TypeDefinition $jobSource -Language CSharp

$job = [DevJob]::CreateKillOnCloseJob()

Write-Host "Starting $Target (working dir: $workDir)"
Write-Host "Ctrl+C (or killing this process) stops it and everything it spawned."

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $fileName
$psi.Arguments = $argumentList
$psi.WorkingDirectory = $workDir
$psi.UseShellExecute = $false

$process = [System.Diagnostics.Process]::Start($psi)
[DevJob]::AssignProcessToJobObject($job, $process.Handle) | Out-Null

try {
    $process.WaitForExit()
    exit $process.ExitCode
} finally {
    if (-not $process.HasExited) {
        $process.Kill()
    }
}
