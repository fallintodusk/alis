<#
.SYNOPSIS
    Exposes .claude/skills to Codex as .agents/skills via a local junction.

.DESCRIPTION
    Claude Code discovers project skills at .claude/skills/<name>/SKILL.md.
    Codex discovers repo skills at .agents/skills/<name>/SKILL.md. ALIS keeps
    ONE authored body, in .claude/skills, and exposes it to Codex through a
    local junction.

    The junction is local and gitignored. It is not committed because
    core.symlinks is false here: git would materialize a tracked link as a
    plain text file containing the target path, and Codex would silently
    discover zero skills rather than failing loudly.

    Junction, not symlink: Windows PowerShell 5.1 cannot create a symlink
    without elevation (New-Item does not pass the Developer Mode
    unprivileged-create flag). A junction needs no privilege and is the correct
    primitive for a local, same-volume directory link.

    There is deliberately NO copy fallback. A copy would be a second authored
    body that silently drifts from .claude/skills, which is the exact failure
    this script exists to prevent. If the junction cannot be created, this
    fails loudly so the operator fixes the cause.

    Idempotent. Safe to re-run.

.EXAMPLE
    scripts/agents/link_codex_skills.ps1
    scripts/agents/link_codex_skills.ps1 -Verify
#>
[CmdletBinding()]
param(
    # Prove the exposure resolves and the skill sets match. Creates nothing.
    [switch]$Verify
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$canonical = Join-Path $repoRoot '.claude\skills'
$linkParent = Join-Path $repoRoot '.agents'
$link = Join-Path $linkParent 'skills'

if (-not (Test-Path $canonical)) {
    throw "[LinkCodexSkills] Canonical skills directory missing: $canonical"
}

function Test-ReparsePoint {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $false }
    $attr = (Get-Item $Path -Force).Attributes
    return [bool]($attr -band [System.IO.FileAttributes]::ReparsePoint)
}

function Remove-LinkOnly {
    param([string]$Path)
    # Delete the reparse point itself and never recurse into it. Directory.Delete
    # with recursive=$false removes the junction entry and cannot touch the
    # target's contents on any PowerShell version. Remove-Item -Recurse happens
    # to leave the target intact on PS 5.1.19041, but that is version-specific
    # reparse-point handling and not a guarantee worth depending on.
    [System.IO.Directory]::Delete($Path, $false)
}

function Get-SkillNames {
    param([string]$Path)
    return @(Get-ChildItem $Path -Directory -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Name | Sort-Object)
}

if ($Verify) {
    if (-not (Test-ReparsePoint $link)) {
        Write-Host '[LinkCodexSkills] FAIL - .agents/skills is not a junction. Run scripts/agents/link_codex_skills.ps1'
        exit 1
    }

    $canonicalNames = Get-SkillNames $canonical
    $exposedNames = Get-SkillNames $link

    if ($canonicalNames.Count -eq 0) {
        Write-Host '[LinkCodexSkills] FAIL - canonical .claude/skills is empty'
        exit 1
    }

    # Set equality is the acceptance criterion, never a hardcoded count.
    if (Compare-Object $canonicalNames $exposedNames) {
        Write-Host "[LinkCodexSkills] FAIL - set mismatch"
        Write-Host "  canonical: $($canonicalNames -join ', ')"
        Write-Host "  exposed  : $($exposedNames -join ', ')"
        exit 1
    }

    Write-Host "[LinkCodexSkills] OK - .agents/skills -> .claude/skills ($($exposedNames -join ', '))"
    exit 0
}

if (Test-Path $link) {
    if (Test-ReparsePoint $link) {
        Remove-LinkOnly $link
    }
    else {
        # A real directory here means a second authored body of the skills,
        # which is the drift this junction exists to prevent. Refuse rather
        # than delete content the operator may not have another copy of.
        throw "[LinkCodexSkills] Refusing to replace real directory $link - it is a duplicate of .claude/skills. Inspect and remove it manually."
    }
}

if (-not (Test-Path $linkParent)) {
    New-Item -ItemType Directory -Path $linkParent -Force | Out-Null
}

# Absolute target: junctions store an absolute path, and New-Item resolves a
# relative target against the caller's working directory, not the link location.
New-Item -ItemType Junction -Path $link -Target $canonical -Force | Out-Null

# Creation can report success while the link fails to resolve, so prove it reads.
$exposed = Get-SkillNames $link
if ($exposed.Count -eq 0) {
    throw "[LinkCodexSkills] Junction created at $link but resolves to nothing. Codex would see zero skills."
}

Write-Host "[LinkCodexSkills] Created junction .agents/skills -> .claude/skills ($($exposed -join ', '))"
