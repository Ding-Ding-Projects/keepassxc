function Assert-KpxcExecutableVersion {
    param(
        [Parameter(Mandatory)][string]$FileVersion,
        [Parameter(Mandatory)][string]$ProductVersion,
        [Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+$')][string]$ExpectedVersion
    )

    $accepted = @($ExpectedVersion, "$ExpectedVersion.0")
    if ($FileVersion -notin $accepted) {
        throw "Packaged executable FileVersion '$FileVersion' does not match '$ExpectedVersion'."
    }
    if ($ProductVersion -notin $accepted) {
        throw "Packaged executable ProductVersion '$ProductVersion' does not match '$ExpectedVersion'."
    }
}
