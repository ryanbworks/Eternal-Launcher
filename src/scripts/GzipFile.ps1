param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Compress', 'Decompress')]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$Source,

    [Parameter(Mandatory = $true)]
    [string]$Destination
)

# Usa o GZipStream fornecido pelo Windows PowerShell para ler/gravar o formato
# comprimido usado pelo arquivo de lista de servidores do Minecraft.
$ErrorActionPreference = 'Stop'
$sourceStream = $null
$destinationStream = $null
$gzipStream = $null

try {
    $sourceStream = [System.IO.File]::OpenRead($Source)
    $destinationStream = [System.IO.File]::Create($Destination)

    if ($Mode -eq 'Decompress') {
        $gzipStream = [System.IO.Compression.GZipStream]::new(
            $sourceStream,
            [System.IO.Compression.CompressionMode]::Decompress
        )
        $gzipStream.CopyTo($destinationStream)
    }
    else {
        $gzipStream = [System.IO.Compression.GZipStream]::new(
            $destinationStream,
            [System.IO.Compression.CompressionMode]::Compress
        )
        $sourceStream.CopyTo($gzipStream)
    }
}
finally {
    if ($null -ne $gzipStream) { $gzipStream.Dispose() }
    if ($null -ne $destinationStream) { $destinationStream.Dispose() }
    if ($null -ne $sourceStream) { $sourceStream.Dispose() }
}
