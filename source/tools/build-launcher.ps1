$ErrorActionPreference = 'Stop'
$editorRoot = Join-Path $PSScriptRoot '..\editor'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:winexe /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll /r:System.Management.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll ("/win32icon:"+(Join-Path $editorRoot 'StarForge.ico')) ("/out:"+(Join-Path $editorRoot 'StarForge.exe')) (Join-Path $editorRoot 'Host-v8.cs') (Join-Path $editorRoot 'UpdateAgent.cs')
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
& $compiler /nologo /target:winexe /r:System.Windows.Forms.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll ("/win32icon:"+(Join-Path $editorRoot 'StarForge.ico')) ("/out:"+(Join-Path $editorRoot 'StarForgeUpdater.exe')) (Join-Path $editorRoot 'StarForgeUpdater.cs')
if ($LASTEXITCODE -ne 0) { throw 'Updater compilation failed.' }
Write-Output 'Built editor/StarForge.exe and StarForgeUpdater.exe. No desktop shortcuts were added.'
