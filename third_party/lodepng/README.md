# lodepng

PNG encoder and decoder in C and C++.

**Version:** master branch (latest)
**Homepage:** https://lodev.org/lodepng/
**GitHub:** https://github.com/lvandeve/lodepng

## License

Copyright (c) 2005-2024 Lode Vandevenne

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

## Why lodepng?

Used in zelda3 for indexed PNG support. stb_image auto-converts indexed PNGs
to RGB/grayscale, but we need raw palette indices for SNES 4bpp sprite encoding.

lodepng provides direct access to palette indices via `LodePNGColorMode`.
