# PowerShell script to help create an icon for FrancoDB
# This script provides instructions and can download a placeholder icon

Write-Host "FrancoDB Icon Creation Helper" -ForegroundColor Cyan
Write-Host "==============================" -ForegroundColor Cyan
Write-Host ""

# Check if ImageMagick is available
$magickAvailable = $false
try {
    $null = Get-Command magick -ErrorAction Stop
    $magickAvailable = $true
    Write-Host "[OK] ImageMagick found" -ForegroundColor Green
} catch {
    Write-Host "[INFO] ImageMagick not found (optional)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "To create an icon file:" -ForegroundColor Yellow
Write-Host "1. Create or find a PNG image (256x256 or 512x512 pixels recommended)"
Write-Host "2. Use one of these methods:"
Write-Host ""

if ($magickAvailable) {
    Write-Host "   Option A: Using ImageMagick (if you have a PNG file):" -ForegroundColor Cyan
    Write-Host "   magick convert your_image.png -define icon:auto-resize=256,128,64,48,32,16 resources\francodb.ico" -ForegroundColor White
    Write-Host ""
}

Write-Host "   Option B: Online converter:" -ForegroundColor Cyan
Write-Host "   - Visit https://www.icoconverter.com/" -ForegroundColor White
Write-Host "   - Upload your PNG image" -ForegroundColor White
Write-Host "   - Download and save as resources\francodb.ico" -ForegroundColor White
Write-Host ""

Write-Host "   Option C: Use GIMP or Photoshop:" -ForegroundColor Cyan
Write-Host "   - Open your image" -ForegroundColor White
Write-Host "   - Export as ICO format" -ForegroundColor White
Write-Host "   - Save to resources\francodb.ico" -ForegroundColor White
Write-Host ""

Write-Host "   Option D: Use a default icon:" -ForegroundColor Cyan
Write-Host "   - The installer will work without a custom icon" -ForegroundColor White
Write-Host "   - Windows will use a default application icon" -ForegroundColor White
Write-Host ""

Write-Host "Note: The icon file is optional. The installer will work without it." -ForegroundColor Gray
Write-Host ""

# Check if icon already exists
if (Test-Path "resources\francodb.ico") {
    Write-Host "[OK] Icon file found: resources\francodb.ico" -ForegroundColor Green
} else {
    Write-Host "[INFO] Icon file not found: resources\francodb.ico" -ForegroundColor Yellow
    Write-Host "       You can create one using the methods above." -ForegroundColor Yellow
}
