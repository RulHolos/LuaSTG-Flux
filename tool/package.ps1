$ProjectRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($PSScriptRoot, ".."))
$ReleasesRoot = [System.IO.Path]::Combine($ProjectRoot, "build", "releases")
$BinaryRootAMD64 = [System.IO.Path]::Combine($ProjectRoot, "build", "amd64", "bin")
$ExampleRoot = [System.IO.Path]::Combine($ProjectRoot, "data", "example")

Write-Output "Project Root       : $ProjectRoot"
Write-Output "Releases Root      : $ReleasesRoot"
Write-Output "Binary Root (amd64): $BinaryRootAMD64"
Write-Output "Example Root       : $ExampleRoot"

# build

Set-Location $ProjectRoot

cmake --workflow --preset windows-amd64-release

# read version info

$ConfigFilePath = [System.IO.Path]::Combine($ProjectRoot, "LuaSTG", "LuaSTG", "LConfig.h")
$ConfigFile = [System.IO.File]::ReadAllText($ConfigFilePath, [System.Text.Encoding]::UTF8)
$VersionMajor = "0"
$VersionMinor = "2"
$VersionPatch = "2"
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

# copy engine binary file

$BinaryFilesAMD64 = @(
    @{
        Source = [System.IO.Path]::Combine($BinaryRootAMD64, "LuaSTGFlux.exe")
        Destination = [System.IO.Path]::Combine($ReleaseRoot, "LuaSTGFlux.exe")
    },
    @{
        Source = [System.IO.Path]::Combine($BinaryRootAMD64, "d3dcompiler_47.dll")
        Destination = [System.IO.Path]::Combine($ReleaseRoot, "d3dcompiler_47.dll")
    },
    @{
        Source = [System.IO.Path]::Combine($BinaryRootAMD64, "xaudio2_9redist.dll")
        Destination = [System.IO.Path]::Combine($ReleaseRoot, "xaudio2_9redist.dll")
    }
)

foreach ($BinaryFile in $BinaryFilesAMD64) {
    if (Test-Path -Path $BinaryFile.Destination) {
        Remove-Item -Path $BinaryFile.Destination
    }
    Copy-Item -Path $BinaryFile.Source -Destination $BinaryFile.Destination
}

# copy example file

$ExampleAssets = [System.IO.Path]::Combine($ExampleRoot, "assets")
$ReleaseAssets = [System.IO.Path]::Combine($ReleaseRoot, "assets")
$ExampleScripts = [System.IO.Path]::Combine($ExampleRoot, "scripts")
$ReleaseScripts = [System.IO.Path]::Combine($ReleaseRoot, "scripts")
$DocRoot = [System.IO.Path]::Combine($ProjectRoot, "doc")
$ReleaseDocRoot = [System.IO.Path]::Combine($ReleaseRoot, "doc")
$LicenseRoot = [System.IO.Path]::Combine($ProjectRoot, "data", "license")
$ReleaseLicenseRoot = [System.IO.Path]::Combine($ReleaseRoot, "license")

if (Test-Path -Path $ReleaseAssets) {
    Remove-Item -Path $ReleaseAssets -Recurse
}
if (Test-Path -Path $ReleaseScripts) {
    Remove-Item -Path $ReleaseScripts -Recurse
}
if (Test-Path -Path $ReleaseDocRoot) {
    Remove-Item -Path $ReleaseDocRoot -Recurse
}
if (Test-Path -Path $ReleaseLicenseRoot) {
    Remove-Item -Path $ReleaseLicenseRoot -Recurse
}
Copy-Item -Path $ExampleAssets -Destination $ReleaseAssets -Recurse
Copy-Item -Path $ExampleScripts -Destination $ReleaseScripts -Recurse
Copy-Item -Path $DocRoot -Destination $ReleaseDocRoot -Recurse -Exclude ".git"
Copy-Item -Path $LicenseRoot -Destination $ReleaseLicenseRoot -Recurse
[System.IO.File]::Copy([System.IO.Path]::Combine($ExampleRoot, "config.json"), [System.IO.Path]::Combine($ReleaseRoot, "config.json"), $true)

# archive

$ArchivePath = [System.IO.Path]::Combine($ReleasesRoot, "LuaSTG-Flux-v$VersionFull.zip")
Compress-Archive -Path $ReleaseRoot -DestinationPath $ArchivePath -CompressionLevel Optimal -Force
