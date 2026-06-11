param(
  [string]$Python = "python",
  [string]$Sums = "16,32,64,128,256,512,1024,2048,4096,8192",
  [string]$Lanes = "2,4,8,16,32,64,128,256,512,1024",
  [string]$HvmThreads = "1,2,4,8,16,32,64,128,256,512,1024",
  [int]$HvmRuns = 100,
  [ValidateSet("csim", "synth", "cosim")]
  [string]$HlsMode = "cosim",
  [int]$Timeout = 900
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

Push-Location $RepoRoot
try {
  & $Python "$ScriptDir\run_sum_sweep_benchmarks.py" `
    --sums $Sums `
    --lanes $Lanes `
    --hvm-threads $HvmThreads `
    --hvm-runs $HvmRuns `
    --hls-mode $HlsMode `
    --timeout $Timeout
} finally {
  Pop-Location
}
