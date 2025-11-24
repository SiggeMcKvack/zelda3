# stb - Single-file public domain libraries

**Source:** https://github.com/nothings/stb
**Version:** master branch (latest as of 2025-11-23)
**License:** Public Domain (or MIT where applicable)
**Date Added:** 2025-11-23

## Purpose
Provides PNG image reading and writing support for the asset extraction tool (`zelda3_restool`). Used to extract sprite sheets and graphics from the SNES ROM and save them as PNG files for intermediate storage and inspection.

## Files
- `stb_image.h` - Image loading library (PNG, JPG, BMP, TGA, etc.)
- `stb_image_write.h` - Image writing library (PNG, BMP, TGA, HDR)

## Usage
Both files are single-header libraries. Define the implementation macro in exactly one .c file:

```c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

## Modifications
None. Files used as-is from upstream.

## Updating
Download latest versions from https://github.com/nothings/stb:
```bash
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o stb_image.h
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -o stb_image_write.h
```

## Why Vendored?
stb libraries are explicitly designed to be vendored. They are:
- Single-header (no build system needed)
- Public domain (no licensing concerns)
- Extremely stable (rarely breaking changes)
- Widely used (~50k+ projects on GitHub)
- Not typically provided as system libraries
