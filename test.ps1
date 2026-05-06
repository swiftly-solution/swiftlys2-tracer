.\build.ps1
$env:CORECLR_ENABLE_PROFILING=1
$env:CORECLR_PROFILER="{a2648b53-a560-486c-9e56-c3922a330182}"
$env:CORECLR_PROFILER_PATH="./build/windows/x64/debug/sw2tracer.dll"
Write-Host "`nEnvironment Variables:" -ForegroundColor Cyan
Get-ChildItem Env: | Where-Object { $_.Name -like "*CORECLR*" -or $_.Name -like "*COMPlus*" } | Format-Table
dotnet run --project ./DotnetTest/DotnetTest.csproj 
