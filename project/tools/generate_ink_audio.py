"""Generate original Ink Lab SFX from math and seeded noise, never source audio.

Requires NumPy only. Usage: python generate_ink_audio.py --output-dir PATH
All synthesis uses 48 kHz mono PCM16. The reference videos informed event lengths
and wet/plucked character only; no recorded waveform is loaded or mixed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import wave
import zlib
import numpy as np

SAMPLE_RATE = 48000
VERSION = "ink-original-synthesis-1.0"


def timeline(seconds):
    return np.arange(round(seconds * SAMPLE_RATE), dtype=np.float64) / SAMPLE_RATE


def band_noise(rng, length, low, high, slope=0.0):
    """Frequency-shaped fresh noise; periodic transform gets an event envelope."""
    n = max(2, length)
    noise = rng.standard_normal(n)
    f = np.fft.rfftfreq(n, 1 / SAMPLE_RATE)
    shape = (1 - np.exp(-(f / max(1, low)) ** 4)) * np.exp(-(f / high) ** 6)
    shape *= np.maximum(f, low) ** (-slope)
    result = np.fft.irfft(np.fft.rfft(noise) * shape, n)
    return result / max(np.std(result), 1e-9)


def falling_tone(t, start, end, glide, decay):
    phase = 2 * np.pi * (end * t + (start - end) * glide * (1 - np.exp(-t / glide)))
    return np.sin(phase) * (1 - np.exp(-t / .0012)) * np.exp(-t / decay)


def resonances(t, base, decay, rng):
    result = np.zeros_like(t)
    # Inharmonic modes evoke a stretched string/object, without sample playback.
    for ratio, level in [(1, .55), (1.437, .30), (2.193, .19), (2.713, .10), (3.871, .045)]:
        frequency = base * ratio * rng.uniform(.991, 1.009)
        bend = .007 * np.sin(2 * np.pi * 39 * t) * np.exp(-t / .06)
        result += level * np.sin(2 * np.pi * frequency * t + bend) * np.exp(-t / (decay / ratio ** .4))
    return result * (1 - np.exp(-t / .0008))


def water_grains(t, rng, count, spread):
    result = np.zeros_like(t)
    for _ in range(count):
        start = rng.uniform(.003, spread)
        local = np.maximum(t - start, 0)
        low = rng.uniform(130, 750)
        grain = falling_tone(local, low * rng.uniform(2.2, 5.0), low, rng.uniform(.0015, .0045), rng.uniform(.004, .013))
        result += grain * (t >= start) * rng.uniform(.05, .19)
    return result


def finish(y, peak, fade=True):
    y = y - np.mean(y)
    y = np.tanh(y * .85)
    if fade:
        attack = min(round(.001 * SAMPLE_RATE), len(y) // 8)
        release = min(round(.009 * SAMPLE_RATE), len(y) // 8)
        y[:attack] *= np.linspace(0, 1, attack)
        y[-release:] *= np.linspace(1, 0, release)
    y *= peak / max(np.max(np.abs(y)), 1e-9)
    return y


def shooter(rng):
    t = timeline(.12)
    pump = falling_tone(t, 260, 80, .010, .032)
    liquid = band_noise(rng, len(t), 230, 3200, .35) * np.exp(-t / .027)
    snap = band_noise(rng, len(t), 1800, 11000) * np.exp(-t / .0038)
    shell = resonances(t, 480, .023, rng)
    return finish(.48 * pump + .35 * liquid + .12 * snap + .12 * shell + water_grains(t, rng, 13, .065), .65)


def stringer(rng, level):
    duration, base, decay = [(.20, 390, .062), (.26, 465, .083), (.32, 550, .105)][level]
    t = timeline(duration)
    twang = resonances(t, base, decay, rng)
    elastic = falling_tone(t, base * 2.6, base * .44, .012, decay * .65)
    air = band_noise(rng, len(t), 650, 9200 + level * 700, .15) * np.exp(-t / (.019 + level * .006))
    wet = band_noise(rng, len(t), 150, 1700, .50) * np.exp(-t / (.036 + level * .010))
    chest = falling_tone(t, 240 + level * 25, 62, .020, .052 + level * .013)
    return finish(.36 * twang + .14 * elastic + .25 * air + .26 * wet + .32 * chest + water_grains(t, rng, 16 + level * 5, .09 + level * .025), .65)


def charging_loop(rng):
    n = round(.12 * SAMPLE_RATE)
    phase = np.arange(n, dtype=np.float64) / n
    # Integer cycle counts give an exactly periodic 120-ms buffer. Pitch control
    # belongs to the live voice; no recorded charge tone is reused here.
    y = np.zeros(n)
    for cycles, level in [(48, .12), (96, .09), (159, .32), (264, .15), (360, .065), (528, .025)]:
        y += level * np.sin(2 * np.pi * cycles * phase + rng.uniform(0, 2 * np.pi))
    y *= .86 + .10 * np.sin(2 * np.pi * phase) + .04 * np.sin(2 * np.pi * 3 * phase)
    y += band_noise(rng, n, 950, 6200) * .023
    return finish(y, .36, fade=False)


def charge_cue(rng, full):
    t = timeline(.18 if full else .12)
    base = 1800 if full else 1350
    bell = resonances(t, base, .063 if full else .035, rng)
    # Short upward glint followed by a damped bell, an original feedback cue.
    phase = 2 * np.pi * (base * .50 * t + base * .42 * (t - .012 * (1 - np.exp(-t / .012))))
    glint = np.sin(phase) * np.exp(-t / .031) * (1 - np.exp(-t / .001))
    return finish(.50 * bell + .20 * glint + .025 * band_noise(rng, len(t), 2000, 9500) * np.exp(-t / .008), .40)


def stick(rng):
    t = timeline(.075)
    glass = resonances(t, 2100, .013, rng)
    click = band_noise(rng, len(t), 2600, 12000) * np.exp(-t / .0023)
    wet = falling_tone(t, 940, 180, .004, .014)
    return finish(.23 * glass + .19 * click + .27 * wet + water_grains(t, rng, 4, .022), .40)


def burst(rng):
    t = timeline(.30)
    pop = falling_tone(t, 220, 45, .019, .058)
    spread = band_noise(rng, len(t), 110, 5300, .45) * np.exp(-t / .066)
    spray = band_noise(rng, len(t), 1700, 10500) * np.exp(-t / .019)
    return finish(.48 * pop + .38 * spread + .09 * spray + water_grains(t, rng, 29, .17), .40)


def write_wav(path, y):
    pcm = np.round(np.clip(y, -.999, .999) * 32767).astype('<i2')
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SAMPLE_RATE)
        w.writeframes(pcm.tobytes())


def make_preview(assets):
    y = np.zeros(round(6.8 * SAMPLE_RATE))
    events = []
    def add(name, start, volume, audio=None):
        data = assets[name] if audio is None else audio
        at = round(start * SAMPLE_RATE)
        y[at:at+len(data)] += data * volume * .55
        events.append({'asset': name + '.wav', 'start_seconds': start, 'duration_seconds': len(data) / SAMPLE_RATE, 'volume_before_master': volume})
    def charge(start, duration):
        n = round(duration * SAMPLE_RATE)
        progress = np.minimum(np.arange(n) / SAMPLE_RATE / 1.2, 1)
        ratio = .8 + .35 * progress
        phase = np.cumsum(ratio) % len(assets['charge_loop'])
        lo = phase.astype(int); hi = (lo + 1) % len(assets['charge_loop']); frac = phase - lo
        data = assets['charge_loop'][lo] * (1-frac) + assets['charge_loop'][hi] * frac
        fade = round(.012 * SAMPLE_RATE)
        data[:fade] *= np.linspace(0, 1, fade); data[-fade:] *= np.linspace(1, 0, fade)
        add('charge_loop', start, .38, data)
    add('stringer_tap', .20, .8)
    charge(1.10, .60); add('charge_first', 1.60, .38)
    add('stringer_mid', 1.76, .8); add('arrow_stick', 1.94, .55); add('arrow_burst', 2.66, .55)
    charge(3.30, 1.20); add('charge_first', 3.80, .38); add('charge_full', 4.50, .38)
    add('stringer_full', 4.66, .8); add('arrow_stick', 4.84, .55); add('arrow_burst', 5.56, .55)
    if abs(y).max() > .95: y *= .95 / abs(y).max()
    return y, events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parents[2] / 'generated/ink_phase5/synth_audio')
    parser.add_argument('--seed', type=int, default=574211)
    args = parser.parse_args(); args.output_dir.mkdir(parents=True, exist_ok=True)
    builders = {'shooter_shot': shooter, 'stringer_tap': lambda r: stringer(r, 0), 'stringer_mid': lambda r: stringer(r, 1),
        'stringer_full': lambda r: stringer(r, 2), 'charge_loop': charging_loop, 'charge_first': lambda r: charge_cue(r, False),
        'charge_full': lambda r: charge_cue(r, True), 'arrow_stick': stick, 'arrow_burst': burst}
    assets = {}; records = []
    for name, builder in builders.items():
        seed = (args.seed ^ zlib.crc32(name.encode('ascii'))) & 0xffffffff
        audio = builder(np.random.default_rng(seed)); assets[name] = audio
        assert np.isfinite(audio).all() and abs(audio).max() <= .650001
        path = args.output_dir / (name + '.wav'); write_wav(path, audio)
        records.append({'file': path.name, 'seed': seed, 'algorithm': builder.__name__ if builder.__name__ != '<lambda>' else name,
            'duration_seconds': len(audio) / SAMPLE_RATE, 'peak_linear': float(abs(audio).max()),
            'rms_dbfs': float(20*np.log10(np.sqrt((audio**2).mean()) + 1e-12)),
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'loop': name == 'charge_loop'})
    preview, events = make_preview(assets)
    write_wav(args.output_dir / 'preview_stringer_sequence.wav', preview)
    manifest = {'version': VERSION, 'original_procedural_synthesis': True, 'recorded_audio_read_or_mixed': False,
        'auditory_review_performed': False, 'sample_rate': SAMPLE_RATE, 'channels': 1, 'format': 'signed 16-bit PCM',
        'seed': args.seed, 'numpy_version': np.__version__, 'script': 'project/tools/generate_ink_audio.py',
        'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'design_reference': 'Only qualitative envelope, wet/plucked character and gameplay timing from user-provided videos. No audio waveform samples used.',
        'parameters': {'shot_peak_limit': .65, 'cue_impact_peak_limit': .40, 'charge_loop_peak': .36, 'charge_loop_period_seconds': .12,
            'modal_ratios': [1, 1.437, 2.193, 2.713, 3.871], 'charge_loop_cycles': [48, 96, 159, 264, 360, 528]},
        'assets': records, 'preview': {'file': 'preview_stringer_sequence.wav', 'duration_seconds': len(preview)/SAMPLE_RATE,
            'master': .55, 'shot_volume': .8, 'charge_volume': .38, 'impact_volume': .55, 'pitch_ratio': '.8 + .35 * charge_progress',
            'peak_linear': float(abs(preview).max()), 'events': events}}
    (args.output_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    for r in records: print(r['file'], f"{r['duration_seconds']:.3f}s peak={r['peak_linear']:.3f}")
    print('preview_stringer_sequence.wav', '6.800s', f'peak={abs(preview).max():.4f}', '| original synthesis only; not aurally reviewed')


if __name__ == '__main__':
    main()
