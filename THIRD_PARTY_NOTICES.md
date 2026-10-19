# Third-party notices

## DspTap PSOLA reference

The EFFECTS TD-PSOLA implementation in `Source/PsolaHarmonyBank.h` is adapted
from the algorithmic structure of `tap::dsp::basic_psola` in DspTap,
Copyright (c) 2025-2026 Timothy Place and the DspTap contributors.

DspTap source: https://github.com/tap/DspTap

MIT License

Copyright (c) 2025-2026 Timothy Place and the DspTap contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE.


## Mutable Instruments Clouds

The EFFECTS Clouds integration uses the original STM32F Clouds DSP source from
the pichenettes/eurorack repository, pinned to commit
`08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`, together with stmlib pinned to
`e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.

Clouds DSP code is Copyright (c) 2014 Emilie Gillet and is distributed under
the MIT license. Original copyright/license headers are retained in the
materialized source files. Mutable Instruments is a registered trademark; this
integration is not presented as an official Mutable Instruments product.

Upstream source: https://github.com/pichenettes/eurorack


## Vongon-inspired EFFECTS modes

ULTRASHEER, ROSETONES, SOLARFAZE, and POLYPHRASE are used here as user-requested
reference names for independently implemented EFFECTS modes inspired by the
published signal-flow/features of Vongon products. No Vongon firmware, source
code, impulse response, or other proprietary DSP asset is included. Vongon and
the referenced product names are trademarks of their respective owner.


## Pure Data / libpd (PATCH mode)

EFFECTS PATCH embeds libpd 0.16.1 (commit
`ba0dc63262901d658af8bbda5e619a60fa975e78`) and Pure Data
Vanilla (submodule commit
`f009fd8d7b537e209e09898d487fdf1bf547da2b`).
The libpd engine is linked into EFFECTS, not a separately distributed Android
audio service. The sample .pd file is authored for EFFECTS.

Upstream source: https://github.com/libpd/libpd
Pure Data source: https://github.com/pure-data/pure-data

Libpd and Pure Data are subject to their project licenses and file-specific
copyright notices. Main BSD notice (preserved verbatim):

```
This software is copyrighted by Miller Puckette and others.  The following
terms (the "Standard Improved BSD License") apply to all files associated with
the software unless explicitly disclaimed in individual files:

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above  
   copyright notice, this list of conditions and the following 
   disclaimer in the documentation and/or other materials provided
   with the distribution.
3. The name of the author may not be used to endorse or promote
   products derived from this software without specific prior 
   written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,   
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.

```
