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

    KNOWN LIMITATION - Codex-managed worktrees. A managed worktree starts from
    Git content, so a gitignored local junction is not inherited. Run this
    script inside such a worktree before using ALIS skills there. Automating
    that is deliberately deferred until repeated usage justifies it.

    Idempotent. Safe to re-run.

.EXAMPLE
    scripts/agents/link_codex_skills.ps1
    scripts/agents/link_codex_skills.ps1 -Verify
#>
[CmdletBinding()]
param(
    # Prove the exposure satisfies the whole contract and exit. Creates nothing.
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

function Get-SkillExposureProblems {
    # THE single definition of "correctly exposed". Used by -Verify AND
    # immediately after creation, so the creation path can never report success
    # against a weaker standard than the one -Verify enforces.
    $problems = @()

    if (-not (Test-ReparsePoint $link)) {
        return @(".agents/skills is not a junction")
    }

    $canonicalNames = Get-SkillNames $canonical
    $exposedNames = Get-SkillNames $link

    if ($canonicalNames.Count -eq 0) {
        return @("canonical .claude/skills is empty")
    }

    # Set equality is the acceptance criterion, never a hardcoded count.
    if (Compare-Object $canonicalNames $exposedNames) {
        $problems += ("set mismatch - canonical: $($canonicalNames -join ', ') " +
            "| exposed: $($exposedNames -join ', ')")
    }

    # Both tools require the entrypoint to be exactly SKILL.md. Windows is
    # case-insensitive, so a lowercase skill.md resolves here and still fails a
    # case-sensitive consumer - check the stored name, not just existence.
    foreach ($name in $exposedNames) {
        $dir = Join-Path $link $name
        $exact = @(Get-ChildItem $dir -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ceq 'SKILL.md' })
        if ($exact.Count -ne 1) {
            $actual = @(Get-ChildItem $dir -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -ieq 'SKILL.md' } |
                Select-Object -ExpandProperty Name)
            $problems += ("$name entrypoint must be exactly SKILL.md (found: " +
                "$(if ($actual) { $actual -join ', ' } else { 'none' }))")
        }
    }

    # Disk casing is NOT enough. core.ignorecase is true on Windows because
    # NTFS really is case-insensitive, so Get-ChildItem above reports SKILL.md
    # even when git has skill.md recorded. Whoever clones onto a case-sensitive
    # filesystem then gets the lowercase name and discovers no skill. That is
    # exactly how a lowercase entry survived several commits unnoticed, with
    # `git status` reporting clean the whole time.
    #
    # Do NOT "fix" this by setting core.ignorecase=false: that flag describes
    # the filesystem, and lying about it causes phantom duplicate entries and
    # checkout collisions. Check the index instead, and repair with
    # `git mv -f <lower> <UPPER>`.
    $tracked = @(& git ls-files --cached -- $canonical 2>$null)
    if ($LASTEXITCODE -eq 0 -and $tracked.Count -gt 0) {
        foreach ($rel in $tracked) {
            $leaf = Split-Path $rel -Leaf
            if ($leaf -ieq 'SKILL.md' -and $leaf -cne 'SKILL.md') {
                $parent = (Split-Path $rel -Parent) -replace '\\', '/'
                $problems += ("git records '$rel' - the index entry must be " +
                    "exactly SKILL.md. Repair: git mv -f '$rel' " +
                    "'$parent/SKILL.md'")
            }
        }
    }

    return $problems
}

if ($Verify) {
    $problems = @(Get-SkillExposureProblems)
    if ($problems.Count -gt 0) {
        Write-Host "[LinkCodexSkills] FAIL"
        $problems | ForEach-Object { Write-Host "  $_" }
        Write-Host "  Fix with: scripts/agents/link_codex_skills.ps1"
        exit 1
    }
    Write-Host "[LinkCodexSkills] OK - .agents/skills -> .claude/skills ($((Get-SkillNames $link) -join ', '))"
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

# Creating the junction is not the contract - satisfying the whole exposure
# contract is. Prove it with the SAME checks -Verify runs.
$problems = @(Get-SkillExposureProblems)
if ($problems.Count -gt 0) {
    Write-Host "[LinkCodexSkills] Junction created but the exposure is INVALID:"
    $problems | ForEach-Object { Write-Host "  $_" }
    throw "[LinkCodexSkills] Codex skill exposure failed verification."
}

Write-Host "[LinkCodexSkills] Created junction .agents/skills -> .claude/skills ($((Get-SkillNames $link) -join ', '))"
