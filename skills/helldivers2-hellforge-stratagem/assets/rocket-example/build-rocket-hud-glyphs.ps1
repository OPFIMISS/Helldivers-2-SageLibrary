param(
    [string]$Source = (Join-Path $PSScriptRoot 'rocket_view.lua'),
    [Parameter(Mandatory=$true)][string]$FontFamily
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$content = [IO.File]::ReadAllText($Source)
$characters = @($content.ToCharArray() | Where-Object { [int]$_ -gt 127 } | Sort-Object -Unique)
$bitmap = [Drawing.Bitmap]::new(16, 16)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
$family = [Drawing.FontFamily]::new($FontFamily)
$font = [Drawing.Font]::new($family, 16, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
$format = [Drawing.StringFormat]::GenericTypographic.Clone()
$rows = [Collections.Generic.List[string]]::new()
$rows.Add('local chinese={')
foreach ($character in $characters) {
    $graphics.Clear([Drawing.Color]::Black)
    $graphics.DrawString([string]$character, $font, [Drawing.Brushes]::White, 0, 0, $format)
    $hex = ''
    for ($y=0; $y -lt 16; $y++) {
        $mask = 0
        for ($x=0; $x -lt 16; $x++) {
            if ($bitmap.GetPixel($x,$y).R -ge 96) { $mask = $mask -bor (1 -shl (15-$x)) }
        }
        $hex += $mask.ToString('x4')
    }
    if ($hex -notmatch '[1-9a-f]') { throw "Empty glyph: $character" }
    $rows.Add(" ['$character']='$hex',")
}
$rows.Add('}')
$block = "-- BEGIN GENERATED CJK GLYPHS`n" + ($rows -join "`n") + "`n-- END GENERATED CJK GLYPHS"
$content = [regex]::Replace($content, '(?s)-- BEGIN GENERATED CJK GLYPHS.*?-- END GENERATED CJK GLYPHS', $block)
[IO.File]::WriteAllText($Source, $content, [Text.UTF8Encoding]::new($false))
$format.Dispose()
$font.Dispose()
$family.Dispose()
$graphics.Dispose()
$bitmap.Dispose()
Write-Output "Generated $($characters.Count) CJK bitmap glyphs; no runtime font dependency."
