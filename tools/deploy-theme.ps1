<#
.SYNOPSIS
Likhi WordPress Theme cPanel Direct Deployment Shortcut
.DESCRIPTION
Executes tools/deploy_theme.py to securely sync local theme files to cPanel over FTPS.
#>
param(
    [switch]$DryRun,
    [switch]$All,
    [string]$File,
    [switch]$Rollback
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$pyScript = Join-Path $scriptDir "deploy_theme.py"

$argsList = @()
if ($DryRun) { $argsList += "--dry-run" }
if ($All) { $argsList += "--all" }
if ($File) { $argsList += @("--file", $File) }
if ($Rollback) { $argsList += "--rollback" }

python $pyScript @argsList
