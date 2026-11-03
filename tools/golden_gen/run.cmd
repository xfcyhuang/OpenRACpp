@echo off
setlocal
REM golden_gen oracle host single-command entry (ticket #3 / T02)
REM Usage: run.cmd gen ^| verify ^| check
REM   gen     regenerate golden artifacts into tests/golden/** (review then commit)
REM   verify  re-produce and byte-compare against committed goldens
REM   check   acceptance loop: produce twice per artifact, assert byte-identical
REM           (SHA256 equal), then compare against committed goldens
REM Prereq: dotnet 10 + sibling ../openra checkout pinned per UPSTREAM.baseline.
REM Exit codes: 0 ok / 1 verify fail / 2 baseline drift / 3 host error / 77 missing prereq (ctest SKIP).
REM
REM Build order matters: the host exe and the upstream DLLs share openra/bin, and a
REM running host locks the assemblies it has loaded. The host therefore never builds
REM the upstream itself (self-lock, MSB3026); we build upstream slnx first, then the
REM host, then run -- no dotnet build while the host is alive.

set "HERE=%~dp0"
if "%~1"=="" goto :usage
if /i not "%~1"=="gen" if /i not "%~1"=="verify" if /i not "%~1"=="check" goto :usage

where dotnet >nul 2>nul
if errorlevel 1 (
  echo [golden_gen] dotnet not found, skip with exit code 77 1>&2
  exit /b 77
)

REM Upstream root: main layout ^<repo^>/../openra, worktree layout ^<repo^>/../../openra
set "OPENRA="
if exist "%HERE%..\..\..\openra\OpenRA.slnx" set "OPENRA=%HERE%..\..\..\openra"
if not defined OPENRA if exist "%HERE%..\..\..\..\openra\OpenRA.slnx" set "OPENRA=%HERE%..\..\..\..\openra"
if not defined OPENRA (
  echo [golden_gen] ../openra upstream checkout not found, skip with exit code 77 1>&2
  exit /b 77
)
for %%i in ("%OPENRA%") do set "OPENRA=%%~fi"

REM 1) upstream slnx: mods assemblies (OpenRA.Mods.*.dll) must exist in openra/bin
dotnet build "%OPENRA%\OpenRA.slnx" -c Release --nologo
if errorlevel 1 (
  echo [golden_gen] upstream OpenRA.slnx build failed 1>&2
  exit /b 3
)

REM 2) oracle host (incremental; lands in openra/bin next to the mod assemblies)
dotnet build "%HERE%golden-gen.csproj" -c Release --nologo
if errorlevel 1 (
  echo [golden_gen] oracle host build failed 1>&2
  exit /b 3
)

REM 3) run the host (it must not trigger any further build itself)
"%OPENRA%\bin\golden-gen.exe" --repo-root "%HERE%..\.." --openra "%OPENRA%" %*
exit /b %errorlevel%

:usage
echo Usage: run.cmd gen ^| verify ^| check 1>&2
exit /b 3
