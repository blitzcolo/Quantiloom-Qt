# Minimize/restore smoke test for Quantiloom Studio.
#
# Studio's window is the only place two whole classes of bug show up, and
# neither is reachable from a unit test: destroying the render context frees
# every GPU resource, and rebuilding it has to re-apply the settings the
# renderer is holding. Minimizing does both. The first cost six months of a
# crash on every minimize (Quantiloom-dev 6f90649); the second silently
# dropped the NN atmosphere.
#
# Runs unattended -- launches Studio on a scene, minimizes it through
# ShowWindow, checks the process is still alive, restores, checks again.
#
#   powershell.exe -ExecutionPolicy Bypass -File scripts\minimize_smoke_test.ps1
#
# Read the two logs it leaves next to itself afterwards: the scene should
# reload on restore, and an atmospheric scene should bake its LUT a second
# time. A process that exits is a failure; so is a restore with no reload.

# Paths are derived from where this script lives, not hard-coded to a drive:
# the checkout moves (D:\ -> H:\ ...) and Quantiloom-dev is its sibling.
$RepoRoot = Split-Path -Parent $PSScriptRoot
$DevRoot  = Join-Path (Split-Path -Parent $RepoRoot) "Quantiloom-dev"

$exe = Join-Path $RepoRoot "build-msvc\Release\QuantiloomQt.exe"
$cfg = Join-Path $DevRoot  "assets\configs\cornell_box_lwir_atmos.toml"

if (-not (Test-Path $exe)) { throw "Studio not built: $exe" }
if (-not (Test-Path $cfg)) { throw "Scene config not found: $cfg" }

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
}
"@

$p = Start-Process -FilePath $exe -ArgumentList $cfg -PassThru `
     -RedirectStandardOutput (Join-Path $RepoRoot "_mini_out.log") `
     -RedirectStandardError  (Join-Path $RepoRoot "_mini_err.log")

Start-Sleep -Seconds 15
$p.Refresh()
if ($p.HasExited) { "RESULT: exited before minimize, code $($p.ExitCode)"; exit }

$h = $p.MainWindowHandle
"HWND: $h"
[W]::ShowWindow($h, 6) | Out-Null   # SW_MINIMIZE
"action: minimized"

Start-Sleep -Seconds 6
$p.Refresh()
if ($p.HasExited) {
  $p.WaitForExit()
  $c = $p.ExitCode
  "RESULT: EXITED after minimize, code $c (0x{0:X8})" -f $c
  exit
}
"state: alive while minimized"

[W]::ShowWindow($h, 9) | Out-Null   # SW_RESTORE
"action: restored"
Start-Sleep -Seconds 8
$p.Refresh()
if ($p.HasExited) { "RESULT: EXITED after restore, code $($p.ExitCode)"; exit }
"RESULT: alive after restore"
$p.Kill()
