$ProjectRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($PSScriptRoot, ".."))
$ReleasesRoot = [System.IO.Path]::Combine($ProjectRoot, "build", "releases")
$BinaryRootAMD64 = [System.IO.Path]::Combine($ProjectRoot, "build", "amd64", "bin")
$DataRoot = [System.IO.Path]::Combine($ProjectRoot, "data")

$Packages = @(
    @{
        Name = "Example"
        Source = [System.IO.Path]::Combine($DataRoot, "example")
    }
    @{
        Name = "THlib Ryannlib bundle"
        Source = [System.IO.Path]::Combine($DataRoot, "thlib-ryannlib")
    }
)

$BinaryFileNames = @(
    "LuaSTGFlux.exe",
    "d3dcompiler_47.dll",
    "xaudio2_9redist.dll"
)

Write-Output "Project Root        : $ProjectRoot"
Write-Output "Releases Root       : $ReleasesRoot"
Write-Output "Binary Root (amd64) : $BinaryRootAMD64"
Write-Output "Packages            : $($Packages.Count)"

# build

Set-Location $ProjectRoot

cmake --workflow --preset windows-amd64-release

# read version info

$ConfigFilePath = [System.IO.Path]::Combine($ProjectRoot, "LuaSTG", "LuaSTG", "LConfig.h")
$ConfigFile = [System.IO.File]::ReadAllText($ConfigFilePath, [System.Text.Encoding]::UTF8)
$VersionMajor = "0"
$VersionMinor = "2"
$VersionPatch = "3"
foreach ($Line in $ConfigFile.Split("`n")) {
    if ($Line.Contains("LUASTG_VERSION_MAJOR")) {
        $VersionMajor = $Line.Replace("#define", "").Replace("LUASTG_VERSION_MAJOR", "").Trim()
    }
    if ($Line.Contains("LUASTG_VERSION_MINOR")) {
        $VersionMinor = $Line.Replace("#define", "").Replace("LUASTG_VERSION_MINOR", "").Trim()
    }
    if ($Line.Contains("LUASTG_VERSION_PATCH")) {
        $VersionPatch = $Line.Replace("#define", "").Replace("LUASTG_VERSION_PATCH", "").Trim()
    }
}
$VersionFull = "$VersionMajor.$VersionMinor.$VersionPatch"
$ReleaseRoot = [System.IO.Path]::Combine($ReleasesRoot, "LuaSTG-Flux-v$VersionFull")

Write-Output "Version            : $VersionFull"
Write-Output "Release Root       : $ReleaseRoot"

if (-not [System.IO.Directory]::Exists($ReleaseRoot)) {
    [System.IO.Directory]::CreateDirectory($ReleaseRoot)
}

# copy packages (each gets its own subfolder with engine binaries + package files)

foreach ($Package in $Packages) {
    $PackageName = $Package.Name
    $PackageSource = $Package.Source
    $PackageRoot = [System.IO.Path]::Combine($ReleaseRoot, $PackageName)

    Write-Output ""
    Write-Output "--- Package: $PackageName ---"
    Write-Output "Source : $PackageSource"
    Write-Output "Dest   : $PackageRoot"

    if (-not (Test-Path -Path $PackageSource)) {
        Write-Warning "Package source not found, skipping: $PackageSource"
        continue
    }

    if (Test-Path -Path $PackageRoot) {
        Remove-Item -Path $PackageRoot -Recurse -Force
    }
    New-Item -Path $PackageRoot -ItemType Directory | Out-Null

    foreach ($BinaryFileName in $BinaryFileNames) {
        $BinarySrc = [System.IO.Path]::Combine($BinaryRootAMD64, $BinaryFileName)
        $BinaryDst = [System.IO.Path]::Combine($PackageRoot, $BinaryFileName)
        Copy-Item -Path $BinarySrc -Destination $BinaryDst
    }

    $PackageItems = Get-ChildItem -Path $PackageSource -Exclude ".gitignore", "*.code-workspace"
    foreach ($Item in $PackageItems) {
        $Destination = [System.IO.Path]::Combine($PackageRoot, $Item.Name)
        if ($Item.PSIsContainer) {
            Copy-Item -Path $Item.FullName -Destination $Destination -Recurse
        } else {
            Copy-Item -Path $Item.FullName -Destination $Destination
        }
    }

    $PackageArchivePath = [System.IO.Path]::Combine($ReleaseRoot, "LuaSTG-Flux-v$VersionFull - $PackageName.zip")
    Compress-Archive -Path $PackageRoot -DestinationPath $PackageArchivePath -CompressionLevel Optimal -Force

    Write-Output "Done."
}

# archive full release

$ArchivePath = [System.IO.Path]::Combine($ReleasesRoot, "LuaSTG-Flux-v$VersionFull.zip")
Compress-Archive -Path $ReleaseRoot -DestinationPath $ArchivePath -CompressionLevel Optimal -Force
