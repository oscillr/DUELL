# Notice

**DUELL**<br>
**MultiMorphing Bitimbral Synthesizer**<br>
**by OSCILL/R**

DUELL is free, open-source firmware for the M-VAVE FM-1. Source: <https://github.com/oscillr/DUELL>.

- **Based on Felucca** by Leo Kuroshita (@kurogedelic), Hügelton Instruments (<https://github.com/hugelton/Felucca>). DUELL is a modified version of Felucca. The upstream commit it was taken from is in [UPSTREAM_COMMIT.txt](UPSTREAM_COMMIT.txt).
- **JIANT** (juanjiant-bit, <https://github.com/juanjiant-bit/JIANT>) is credited for the SHIM and RESO reverbs only.
- The PLATE reverb is based on the reverb of the CHOMPI firmware (<https://github.com/CHOMPI-Club/CHOMPI>, MIT, Copyright (c) 2026 CHOMPI Club), which is Emilie Gillet's plate reverb of Mutable Instruments Rings / Clouds (MIT, Copyright 2014 Emilie Gillet), rewritten as integer code. The MIT notices are in [felucca-base/LICENSES/MIT-CHOMPI.txt](felucca-base/LICENSES/MIT-CHOMPI.txt). The name and logo of CHOMPI are trademarks of CHOMPI Club and are not used.
- The WAVE engine's **user wavetables** (DUELL 0.2.14, the editor's WAV converter) use two published ideas, and no code of anyone else: the fast Fourier transform of **J. W. Cooley and J. W. Tukey** (Math. Comp. 19, 1965, <https://doi.org/10.1090/S0025-5718-1965-0178586-1>), written here from the paper's idea, and the wavetable WAV file convention of **Xfer Serum** (frames of 2048 samples one after the other, the `clm ` chunk), also used by **Vital** and **Surge**, which the converter reads. They are credited on the device too (MENU > ABOUT > CREDITS) and in [felucca-base/LICENSING.md](felucca-base/LICENSING.md).
- The DELAY MODES RAND and OCT+ are based on the idea of the granular delay in the TEMPO firmware of the CHOMPI sampler (<https://github.com/CHOMPI-Club/CHOMPI>, MIT, Copyright (c) 2026 CHOMPI Club), written again as integer code for DUELL; no code is copied. The MIT notice is in [felucca-base/LICENSES/MIT-CHOMPI.txt](felucca-base/LICENSES/MIT-CHOMPI.txt). The name and logo of CHOMPI are trademarks of CHOMPI Club and are not used.
- One FM voice: aquanodemusic (Aquanode Soundscapes), CC0 1.0 Universal, https://github.com/aquanodemusic/aquanode-soundscapes. The FM6 factory sound FM BASS 2 (factory patch F9) is that author's bass voice, imported from a DX7 voice file (DX7 voice import) and adapted (the name, the carriers' sustain), then played through DUELL's own filter and effects. CC0 asks for no credit; it is given. The other FM6 factory patches are DUELL's or Felucca's own.
- Licence: GPL-3.0-only, see [LICENSE](LICENSE). Felucca's own notices and the list of third-party parts are in [felucca-base/LICENSING.md](felucca-base/LICENSING.md).
- **No warranty.** DUELL is provided as is, as the GPL-3.0 says. You install it at your own risk.
- DUELL is an independent project. It is **not affiliated with, endorsed by or supported by M-VAVE**. "M-VAVE" and "FM-1" are trademarks of their respective owners.
- The code and the splash graphic as shipped in the firmware are GPL-3.0-only. The names DUELL and OSCILL/R and the logo artwork are the property of OSCILL/R; builds that are not the official release must not present themselves as official DUELL releases.
