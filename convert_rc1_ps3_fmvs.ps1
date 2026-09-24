param(
    [Parameter(Mandatory = $true)]
    [string] $SourceRoot,

    [Parameter(Mandatory = $true)]
    [string] $OutputRoot,

    [string] $Encoder = 'C:\Program Files (x86)\RADVideo\binkc.exe',

    [ValidateRange(100000, 2000000)]
    [int] $DataRate = 300000,

    [string] $Only,

    [switch] $Force
)

$ErrorActionPreference = 'Stop'
$SourceRoot = [IO.Path]::GetFullPath($SourceRoot)
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$Encoder = [IO.Path]::GetFullPath($Encoder)

if (-not (Test-Path -LiteralPath $Encoder)) {
    throw "Bink 1 encoder not found: $Encoder"
}
if (-not (Test-Path -LiteralPath $SourceRoot)) {
    throw "PS3 RC1 movie folder not found: $SourceRoot"
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$files = @(Get-ChildItem -LiteralPath $SourceRoot -File -Filter '*.bik' |
    Where-Object Name -NotMatch '_j\.bik$' | Sort-Object Name)

if ($Only) {
    $files = @($files | Where-Object Name -EQ $Only)
    if ($files.Count -ne 1) {
        throw "Requested movie not found: $Only"
    }
} elseif ($files.Count -ne 42) {
    throw "Expected the 42 non-Japanese RC1 movies used by the Vita release; found $($files.Count)."
}

function Get-BinkInfo([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -le 44) {
        return $null
    }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open,
        [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $header = New-Object byte[] 44
        if ($stream.Read($header, 0, 44) -ne 44) {
            return $null
        }
    } finally {
        $stream.Dispose()
    }
    [PSCustomObject]@{
        Magic = [Text.Encoding]::ASCII.GetString($header, 0, 4)
        HeaderBytes = [BitConverter]::ToUInt32($header, 4) + 8L
        ActualBytes = $item.Length
        Frames = [BitConverter]::ToUInt32($header, 8)
        LargestFrame = [BitConverter]::ToUInt32($header, 12)
        Width = [BitConverter]::ToUInt32($header, 20)
        Height = [BitConverter]::ToUInt32($header, 24)
        FpsNumerator = [BitConverter]::ToUInt32($header, 28)
        FpsDenominator = [BitConverter]::ToUInt32($header, 32)
        AudioTracks = [BitConverter]::ToUInt32($header, 40)
    }
}

function Test-CompleteBink([string] $Path) {
    $info = Get-BinkInfo $Path
    return $null -ne $info -and $info.Magic -eq 'BIKi' -and
        $info.ActualBytes -eq $info.HeaderBytes
}

for ($index = 0; $index -lt $files.Count; ++$index) {
    $source = $files[$index]
    $output = Join-Path $OutputRoot $source.Name
    $label = '[{0}/{1}] {2}' -f ($index + 1), $files.Count, $source.Name

    if (-not $Force -and (Test-CompleteBink $output)) {
        Write-Output "$label already complete"
        continue
    }
    if (Test-Path -LiteralPath $output) {
        Remove-Item -LiteralPath $output
    }

    Write-Output "$label encoding"
    $arguments = @(
        $source.FullName,
        $output,
        "/D$DataRate",
        '/L4',
        '/R48000',
        '/B16',
        '/C2',
        '/(720',
        '/)408',
        '/O'
    )
    $process = Start-Process -FilePath $Encoder -ArgumentList $arguments `
        -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddMinutes(30)
    while (-not $process.HasExited -and -not (Test-CompleteBink $output)) {
        if ((Get-Date) -gt $deadline) {
            $process.Kill()
            $process.WaitForExit()
            throw "$label exceeded the 30-minute safety limit."
        }
        Start-Sleep -Milliseconds 500
    }

    if (Test-CompleteBink $output) {
        if (-not $process.HasExited) {
            # This encoder can remain open in its preview window after writing
            # a complete file. Stop it only after the Bink header and size agree.
            $process.Kill()
            $process.WaitForExit()
        }
    } elseif ($process.HasExited -and $process.ExitCode -ne 0) {
        throw "$label failed with exit code $($process.ExitCode)."
    } else {
        throw "$label produced no complete Bink 1 file."
    }

    $info = Get-BinkInfo $output
    if ($info.Width -ne 720 -or $info.Height -ne 408 -or
        $info.FpsNumerator -ne 30 -or $info.FpsDenominator -ne 1 -or
        $info.AudioTracks -ne 1) {
        throw "$label has unexpected output properties."
    }
    if ($info.LargestFrame -ge 65536) {
        throw "$label has a $($info.LargestFrame)-byte frame. Re-encode it at a lower data rate before using it on Vita."
    }
    Write-Output "$label complete (largest frame: $($info.LargestFrame) bytes)"
}

Write-Output "Converted $($files.Count) RC1 movie file(s) to $OutputRoot"
