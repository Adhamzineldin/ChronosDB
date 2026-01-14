# Icon File for FrancoDB Installer

The installer expects an icon file at `resources\francodb.ico`.

## Creating an Icon File

You can create an ICO file using one of these methods:

### Option 1: Online Icon Generator
1. Visit https://www.icoconverter.com/ or https://convertio.co/png-ico/
2. Upload a PNG image (recommended size: 256x256 or 512x512)
3. Download the generated ICO file
4. Save it as `resources\francodb.ico`

### Option 2: Using ImageMagick
```bash
magick convert francodb.png -define icon:auto-resize=256,128,64,48,32,16 francodb.ico
```

### Option 3: Using GIMP or Photoshop
1. Create or open a square image (256x256 or 512x512)
2. Export as ICO format
3. Save to `resources\francodb.ico`

### Option 4: Use a Default Windows Icon
If you don't have a custom icon, you can:
- Remove the `SetupIconFile` line from installer.iss, or
- Use a generic database/application icon from icon libraries

## Recommended Icon Design
- Size: 256x256 pixels minimum
- Format: ICO (supports multiple sizes: 16, 32, 48, 64, 128, 256)
- Style: Simple, recognizable database/server icon
- Colors: Match your application branding
