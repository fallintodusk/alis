# ffmpeg (Post-process)

Encode, compress, and trim trailer output. ffmpeg runs **after** MRQ produces an image sequence (PNG/EXR) or ProRes file.

Router: [README.md](README.md). UE side: [render_setup.md](render_setup.md).

## ffmpeg / ffprobe path on this machine

The user's gyan.dev ffmpeg is on PATH only in interactive PowerShell. For scripts and non-interactive shells use the Kdenlive-bundled fallback:

```
C:\Program Files\Kdenlive\bin\ffmpeg.exe
C:\Program Files\Kdenlive\bin\ffprobe.exe
```

Both fallback paths are always resolvable.

## Image sequence -> mp4 (h264, web-friendly)

The default trailer encode. Input PNGs are named `{sequence_name}.{frame_number}.png` (MRQ default).

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -framerate 60 `
  -i "Saved\MovieRenders\MyShot\MyShot.%04d.png" `
  -c:v libx264 -preset slow -crf 18 `
  -pix_fmt yuv420p `
  -movflags +faststart `
  "Saved\MovieRenders\MyShot\MyShot.mp4"
```

Key flags:
- `-framerate 60` (input rate) **must come before -i**. Output rate matches by default.
- `-crf 18` for trailers (visually lossless). `-crf 23` for quick shares. Lower = bigger.
- `-preset slow` trades encode time for compression efficiency. Use `medium` if iterating.
- `-pix_fmt yuv420p` for max compatibility (browsers, Discord, YouTube).
- `-movflags +faststart` puts the index at the front so the file streams while downloading.

## Smaller file, same quality (h265)

Roughly 30-50% smaller than h264 at the same visual quality. Supported by modern browsers and players; not universal in older tooling.

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -framerate 60 `
  -i "Saved\MovieRenders\MyShot\MyShot.%04d.png" `
  -c:v libx265 -preset slow -crf 22 `
  -tag:v hvc1 `
  -pix_fmt yuv420p `
  -movflags +faststart `
  "Saved\MovieRenders\MyShot\MyShot_h265.mp4"
```

`-tag:v hvc1` makes the file playable in QuickTime / Apple ecosystems. Without it, only ffplay/VLC will recognise the codec.

CRF mapping vs h264:
- h264 crf 18 -> h265 crf 22
- h264 crf 23 -> h265 crf 26

## ProRes .mov re-encode (NVENC HEVC, hardware-accelerated)

The production MRQ preset outputs **Apple ProRes 422 HQ 10-bit** directly as a self-contained `.mov` (see [render_setup.md](render_setup.md)). ProRes is a mastering codec - ~432 Mbps for 1080p60 - so even short clips run hundreds of MB. Re-encode with NVIDIA's HEVC hardware encoder for ~30x smaller files with no visible loss.

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" -hide_banner -y `
  -i "Saved\MovieRenders\MyShot.mov" `
  -c:v hevc_nvenc -preset p7 -tune hq -rc vbr -cq 19 -b:v 0 `
  -spatial_aq 1 -temporal_aq 1 `
  -pix_fmt p010le -tag:v hvc1 -an `
  "Saved\MovieRenders\MyShot_enc.mp4"
```

Flag notes:
- `-preset p7 -tune hq` - slowest NVENC preset, quality-tuned. Still encodes faster than realtime on a modern NVIDIA GPU.
- `-rc vbr -cq 19 -b:v 0` - constant-quality VBR. CQ 19 is the NVENC analog of libx265 CRF 18 (visually lossless from a clean master). Bump to CQ 22-24 for smaller files.
- `-spatial_aq 1 -temporal_aq 1` - adaptive quantization. Protects gradients (skies, fog) against banding.
- `-pix_fmt p010le` - keeps 10-bit colour. UE ProRes is 10-bit 4:2:2; this preserves bit depth and accepts 4:2:0 chroma subsampling (invisible for delivery, lossy if you re-grade downstream).
- `-tag:v hvc1` - required for QuickTime and Google Drive web preview compatibility.
- `-an` - strips audio. UE Take Recorder shots don't carry an audio track.

Verified results (1920x1080 @ 60 fps ProRes 422 HQ inputs, NVIDIA hardware, 2026-05-17/18):

| Shot | Duration | Source | Encoded | Reduction | Encode time |
|---|---:|---:|---:|---:|---:|
| BalconyStatueLocker | 22.15 s | 1142.8 MB | 33.3 MB | 34.3x | 17 s |
| Helicopter | 6.17 s | 317.3 MB | 12.2 MB | 26.0x | 6 s |
| StreetTrash | 12.40 s | 646.9 MB | 23.4 MB | 27.6x | 10 s |
| Kitchen | 29.53 s | 1437.2 MB | 41.4 MB | 34.7x | 10 s |
| **Totals** | **70.25 s** | **3544.2 MB** | **110.3 MB** | **32.1x** | **43 s** |

Output bitrate is ~12-17 Mbps depending on motion content. Comfortably visually lossless for review, Drive upload, or YouTube (which will re-encode regardless).

### Batch via Makefile

Routine batch encoding goes through `make cinematics-convert` (driver: [scripts/ue/cinematic/convert.ps1](../../scripts/ue/cinematic/convert.ps1)). It scans `Saved/MovieRenders/`, skips any `.mov` whose `_enc.mp4` already exists, runs a 5-second size-stability check on each candidate, and only encodes the stable ones:

```bash
make cinematics-convert
```

Output: per-file `[ENC]`/`[SKIP]`/`[FAIL]` lines and a final summary table (source MB, encoded MB, ratio, seconds). Mid-render files are skipped with `still being written` instead of being corrupted.

Glob-and-encode patterns are forbidden. A half-written `.mov` from a mid-render looks identical on disk to a finished one - touching it corrupts the output and wastes the render. See [troubleshooting.md](troubleshooting.md#movie-renders-folder-safety) for the standing safety rule.

## Two-pass (target a file size)

When uploading to a service with a hard size cap (e.g. 25 MiB Discord limit, 8 MiB free Discord):

```powershell
# Target bitrate = (target_size_in_bits) / duration_seconds
# Example: 25 MiB / 60s = 25*8*1024*1024 / 60 = ~3500 kbps total

& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" -y `
  -framerate 60 -i "MyShot.%04d.png" `
  -c:v libx264 -b:v 3300k -pass 1 -an -f null NUL

& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -framerate 60 -i "MyShot.%04d.png" `
  -c:v libx264 -b:v 3300k -pass 2 `
  -pix_fmt yuv420p -movflags +faststart `
  "MyShot_25MiB.mp4"
```

Two-pass is the right answer for hard size caps. CRF is the right answer when quality matters more than size.

## Quick probe (frame count, duration, codec)

```powershell
& "C:\Program Files\Kdenlive\bin\ffprobe.exe" -v error `
  -select_streams v:0 `
  -show_entries stream=codec_name,r_frame_rate,duration,nb_frames,width,height `
  -of default=noprint_wrappers=1 `
  "Saved\MovieRenders\MyShot\MyShot.mp4"
```

## Trim without re-encoding

Fast, lossless, but cuts to nearest keyframe (~1-2s precision):

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -ss 00:00:05 -to 00:00:15 `
  -i "MyShot.mp4" -c copy "MyShot_trimmed.mp4"
```

For frame-accurate trim, re-encode (slower, exact):

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -i "MyShot.mp4" -ss 00:00:05 -to 00:00:15 `
  -c:v libx264 -crf 18 -preset slow "MyShot_trimmed.mp4"
```

## Concat two clips

Input file `concat.txt`:
```
file 'shot_01.mp4'
file 'shot_02.mp4'
```

Concat (same codec/resolution/fps - no re-encode):
```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -f concat -safe 0 -i concat.txt -c copy "trailer.mp4"
```

If clips differ (resolution, fps, codec), use the concat filter and re-encode:
```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -i shot_01.mp4 -i shot_02.mp4 `
  -filter_complex "[0:v][1:v]concat=n=2:v=1:a=0[v]" `
  -map "[v]" -c:v libx264 -crf 18 "trailer.mp4"
```

## EXR sequence -> mp4 (with tone-map)

EXRs are linear; encoding them as-is gives crushed blacks. Apply a Rec.709 tone map:

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -framerate 60 -i "MyShot.%04d.exr" `
  -vf "zscale=transfer=linear:primaries=bt709:matrix=bt709,tonemap=hable,zscale=transfer=bt709,format=yuv420p" `
  -c:v libx264 -crf 18 -preset slow `
  "MyShot_from_exr.mp4"
```

For colour-managed work, grade in DaVinci/AE and export from there - do not tone-map twice.

## Encoding tradeoff cheat sheet

| Knob | Faster | Smaller | Better quality |
|---|---|---|---|
| `-preset` | `ultrafast` | `slow`/`veryslow` | `slow` |
| `-crf` | higher (28+) | higher | lower (18-20) |
| codec | `libx264` | `libx265` | `libx265` or ProRes |
| `-pix_fmt` | `yuv420p` | `yuv420p` | `yuv444p` (compat suffers) |

With an NVIDIA GPU, `hevc_nvenc -preset p7 -cq 19` matches the size/quality of `libx265 -slow -crf 18` at 5-10x the speed. See the [ProRes re-encode section](#prores-mov-re-encode-nvenc-hevc-hardware-accelerated) for the production-tested command.

## Audio (when needed)

Trailers usually get audio in the NLE, not in ffmpeg. If you must mux a finished wav:

```powershell
& "C:\Program Files\Kdenlive\bin\ffmpeg.exe" `
  -i MyShot.mp4 -i track.wav `
  -c:v copy -c:a aac -b:a 192k -shortest `
  "MyShot_with_audio.mp4"
```

## Common ffmpeg pitfalls

- `-framerate` before `-i` (input rate) vs `-r` after `-i` (output rate). Mixing them silently drops or duplicates frames.
- PNG sequence frame numbering must be zero-padded and contiguous - `%04d` assumes `0001..NNNN`. MRQ produces this by default.
- On Windows PowerShell, backtick (`` ` ``) is the line continuation char. Caret (`^`) is for cmd.exe.
- `-movflags +faststart` is cheap and almost always worth setting for web upload.
