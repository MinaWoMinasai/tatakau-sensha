"""Original Tank Expedition audio. Python + NumPy; no recordings or sample bank.

Run this file to regenerate PCM16 assets and numerical validation measurements.
The first three SE adapt the project's earlier expedition_review/audio_demo
oscillator recipes. Music is a modest original 8-bar, 128 BPM prototype.
"""
from pathlib import Path
import json
import wave

import numpy as np

SR = 48_000
BPM = 128
BEAT = 60.0 / BPM
LOOP_SECONDS = 32 * BEAT
OUT = Path(__file__).resolve().parent


def time(seconds):
    return np.arange(round(seconds * SR), dtype=np.float64) / SR


def amp(db):
    return 10.0 ** (db / 20.0)


def chirp(frequency):
    return np.sin(2 * np.pi * np.cumsum(frequency) / SR)


def noise(length, seed, low=0, high=10_000):
    raw = np.random.default_rng(seed).standard_normal(length)
    frequencies = np.fft.rfftfreq(length, 1.0 / SR)
    filt = np.exp(-((frequencies / high) ** 4))
    if low:
        filt *= 1 - np.exp(-((frequencies / low) ** 4))
    shaped = np.fft.irfft(np.fft.rfft(raw) * filt, n=length)
    return shaped / max(np.std(shaped), 1e-12)


def envelope(sound, attack_ms=2, release_ms=20):
    sound = np.asarray(sound, dtype=np.float64).copy()
    first = min(round(SR * attack_ms / 1000), len(sound) // 2)
    last = min(round(SR * release_ms / 1000), len(sound) // 2)
    sound[:first] *= np.sin(np.linspace(0, np.pi / 2, first)) ** 2
    sound[-last:] *= np.cos(np.linspace(0, np.pi / 2, last)) ** 2
    return sound


def normalize(sound, peak_db):
    return sound * amp(peak_db) / max(np.max(np.abs(sound)), 1e-12)


def finish(sound, peak_db=-6, attack_ms=2, release_ms=20):
    sound = sound - np.mean(sound)
    return normalize(envelope(sound, attack_ms, release_ms), peak_db)


def shot():
    t = time(0.12)
    body = chirp(210 + 720 * np.exp(-t / 0.008)) * np.exp(-t / 0.021)
    glint = np.sin(2 * np.pi * 2100 * t) * np.exp(-t / 0.0045)
    air = noise(len(t), 19001, 1400, 7000) * np.exp(-t / 0.006)
    return finish(0.88 * body + 0.13 * glint + 0.055 * air, -6, 1.5, 12)


def hit():
    t = time(0.175)
    partials = [(1550, 1.0, 0.022), (2337, 0.53, 0.036),
                (3620, 0.30, 0.021), (4871, 0.12, 0.014)]
    ring = sum(a * np.sin(2 * np.pi * f * t) * np.exp(-t / decay)
               for f, a, decay in partials)
    grit = noise(len(t), 19002, 1700, 6500) * np.exp(-t / 0.007)
    return finish(0.34 * ring + 0.07 * grit, -6, 1.2, 12)


def kill():
    t = time(0.53)
    kick = chirp(43 + 118 * np.exp(-t / 0.025)) * np.exp(-t / 0.092)
    dust = noise(len(t), 19003, 70, 1900) * np.exp(-t / 0.062)
    shards = np.zeros_like(t)
    for i, start in enumerate([0.017, 0.048, 0.087, 0.138, 0.203]):
        u = np.maximum(0, t - start)
        env = (1 - np.exp(-u / 0.0015)) * np.exp(-u / 0.025) * (t >= start)
        shards += 0.11 * 0.78 ** i * chirp(3100 - i * 390 + 340 * np.exp(-u / 0.01)) * env
    return finish(0.84 * kick + 0.12 * dust + shards, -5, 2, 35)


def warning():
    t = time(0.29)
    sound = np.zeros_like(t)
    for start, frequency in [(0, 880), (0.135, 1046.5)]:
        u = np.maximum(t - start, 0)
        env = (1 - np.exp(-u / 0.004)) * np.exp(-u / 0.033) * (t >= start)
        sound += (np.sin(2 * np.pi * frequency * u)
                  + 0.16 * np.sin(2 * np.pi * frequency * 2 * u)) * env
    return finish(sound, -7, 2, 25)


def dash():
    t = time(0.235)
    env = (1 - np.exp(-t / 0.012)) * np.exp(-t / 0.045)
    air = noise(len(t), 19105, 260, 4200)
    tone = chirp(220 + 1150 * np.exp(-t / 0.05))
    return finish((0.5 * air + 0.5 * tone) * env, -7, 3, 30)


def upgrade():
    t = time(0.68)
    sound = np.zeros_like(t)
    for start, frequency in [(0, 587.33), (0.075, 739.99), (0.150, 880), (0.225, 1174.66)]:
        u = np.maximum(t - start, 0)
        env = (1 - np.exp(-u / 0.003)) * np.exp(-u / 0.075) * (t >= start)
        sound += (np.sin(2 * np.pi * frequency * u)
                  + 0.2 * np.sin(2 * np.pi * frequency * 2 * u)) * env
    return finish(sound, -6, 3, 40)


def armor_break():
    t = time(0.38)
    low = chirp(110 + 210 * np.exp(-t / 0.012)) * np.exp(-t / 0.07)
    grit = noise(len(t), 19107, 800, 6200) * np.exp(-t / 0.04)
    glass = (np.sin(2 * np.pi * 1829 * t) + 0.45 * np.sin(2 * np.pi * 2711 * t)) * np.exp(-t / 0.07)
    return finish(0.7 * low + 0.15 * grit + 0.3 * glass, -6, 1.5, 25)


def hz(midi):
    return 440 * 2 ** ((midi - 69) / 12)


def bass(midi):
    t = time(BEAT * 0.43)
    f = hz(midi)
    osc = np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * f * 2 * t)
    osc += 0.1 * np.sin(2 * np.pi * f * 3 * t)
    return envelope(osc * np.exp(-t / 0.09), 4, 25)


def chord(notes):
    t = time(BEAT * 2.2)
    sound = np.zeros_like(t)
    for midi in notes:
        f = hz(midi)
        sound += np.sin(2 * np.pi * f * t) + 0.14 * np.sin(2 * np.pi * f * 2 * t)
    return envelope(sound / len(notes) * np.exp(-t / 0.50), 28, 100)


def pluck(midi):
    t = time(BEAT * 0.62)
    f = hz(midi)
    sound = np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * f * 2 * t)
    return envelope(sound * np.exp(-t / 0.063), 4, 30)


def music():
    length = round(LOOP_SECONDS * SR)
    base = np.zeros((length, 2))
    intense = np.zeros_like(base)

    def put(target, beat, sound, gain, pan=0):
        # Circular overlap-add preserves ringing tails across the file seam.
        indices = (round(beat * BEAT * SR) + np.arange(len(sound))) % length
        stereo = sound[:, None] * np.array([np.sqrt((1 - pan) / 2), np.sqrt((1 + pan) / 2)])
        target[indices] += stereo * gain

    t = time(0.20)
    kick = envelope(chirp(49 + 88 * np.exp(-t / 0.016)) * np.exp(-t / 0.045), 2, 30)
    t = time(0.14)
    snare = envelope((0.4 * noise(len(t), 19120, 650, 5500)
                      + 0.65 * np.sin(2 * np.pi * 188 * t)) * np.exp(-t / 0.029), 1.5, 20)
    t = time(0.055)
    hat = envelope(noise(len(t), 19121, 4200, 10_000) * np.exp(-t / 0.011), 1, 12)
    # Dm - Bb - F - C, two bars per chord; original sparse riff, no source song.
    progression = [(38, (62, 65, 69)), (34, (58, 62, 65)),
                   (41, (60, 65, 69)), (36, (60, 64, 67))]
    bass_steps = [(0, 0), (0.75, 0), (1.5, 12), (2.5, 0), (3.25, 7)]
    for bar in range(8):
        root, notes = progression[bar // 2]
        for beat in range(4):
            put(base, bar * 4 + beat, kick, 0.40 if beat % 2 == 0 else 0.32)
        for offset, interval in bass_steps:
            put(base, bar * 4 + offset, bass(root + interval), 0.23)
        put(base, bar * 4 + 0.25, chord(notes), 0.11, -0.28)
        put(base, bar * 4 + 2.5, chord(notes), 0.085, 0.28)
        for eighth in range(8):
            put(intense, bar * 4 + eighth * 0.5, hat,
                0.095 if eighth % 2 else 0.065, 0.18 if eighth % 2 else -0.18)
        for beat in (1, 3):
            put(intense, bar * 4 + beat, snare, 0.18)
        arp = [notes[0] + 12, notes[2], notes[1] + 12, notes[2] + 12]
        for step, note in enumerate(arp):
            put(intense, bar * 4 + 0.5 + step * 0.75, pluck(note), 0.09,
                -0.34 if step % 2 else 0.34)
        if bar in (3, 7):
            for offset in (3.25, 3.5, 3.75):
                put(intense, bar * 4 + offset, hat, 0.06)
    # One shared scale preserves stem balance; even both at unity leave headroom.
    combined_peak = max(np.max(np.abs(base + intense)), np.max(np.abs(base)), np.max(np.abs(intense)))
    gain = amp(-8) / combined_peak
    return base * gain, intense * gain


def write(name, data):
    assert np.all(np.isfinite(data)) and np.max(np.abs(data)) < 1.0
    pcm = np.round(data * 32767).astype('<i2')
    with wave.open(str(OUT / name), 'wb') as handle:
        handle.setnchannels(1 if data.ndim == 1 else data.shape[1])
        handle.setsampwidth(2)
        handle.setframerate(SR)
        handle.writeframes(pcm.tobytes())


def measure(name, loop=False):
    with wave.open(str(OUT / name), 'rb') as handle:
        channels, width, rate, count, compression, _ = handle.getparams()
        raw = np.frombuffer(handle.readframes(count), dtype='<i2').reshape(count, channels)
    x = raw.astype(np.float64) / 32768
    assert width == 2 and rate == SR and compression == 'NONE'
    assert np.max(np.abs(raw.astype(np.int32))) < 32767
    result = {'file': name, 'seconds': count / SR, 'channels': channels,
              'sample_rate': rate, 'bits': 16,
              'peak_dbfs': round(20 * np.log10(max(np.max(np.abs(x)), 1e-12)), 2),
              'rms_dbfs': round(20 * np.log10(max(np.sqrt(np.mean(x * x)), 1e-12)), 2),
              'clipped_samples': 0}
    if loop:
        assert count == round(LOOP_SECONDS * SR)
        seam = float(np.max(np.abs(x[0] - x[-1])))
        delta = np.abs(np.diff(x, axis=0))
        # No discontinuity larger than normal intersample motion at the seam.
        assert seam <= max(float(np.percentile(delta, 99)), 2 / 32768)
        result.update(loop_beats=32, bpm=BPM, seam_jump=seam,
                      sample_delta_p99=float(np.percentile(delta, 99)),
                      first_samples=raw[0].tolist(), last_samples=raw[-1].tolist())
    else:
        assert np.all(raw[0] == 0) and np.all(raw[-1] == 0)
        result.update(first_samples=raw[0].tolist(), last_samples=raw[-1].tolist())
    return result


def main():
    sounds = {'shot.wav': shot(), 'hit.wav': hit(), 'kill.wav': kill(),
              'warning.wav': warning(), 'dash.wav': dash(), 'upgrade.wav': upgrade(),
              'armor_break.wav': armor_break()}
    base, intense = music()
    sounds.update({'music_base.wav': base, 'music_intensity.wav': intense})
    for name, sound in sounds.items():
        write(name, sound)
    measurements = [measure(name, name.startswith('music_')) for name in sounds]
    combined_pcm = np.round((base + intense) * 32767).astype(np.int32)
    assert np.max(np.abs(combined_pcm)) < 32767
    # Preview only: base, combat, boss; game mixes these stems independently.
    demo = base.copy() * 0.40
    intensity_gain = np.clip((np.arange(len(base)) / SR - 4) / 4, 0, 1) * 0.40
    demo += intense * intensity_gain[:, None]
    for start, name in [(1, 'shot.wav'), (2, 'hit.wav'), (3, 'kill.wav'),
                        (4, 'warning.wav'), (5, 'dash.wav'), (6, 'upgrade.wav'),
                        (7, 'armor_break.wav'), (10.0, 'shot.wav'), (10.12, 'shot.wav'),
                        (10.24, 'shot.wav'), (10.29, 'hit.wav'), (10.36, 'shot.wav'),
                        (10.48, 'shot.wav'), (10.55, 'kill.wav')]:
        sample = sounds[name]
        first = round(start * SR)
        demo[first:first + len(sample)] += sample[:, None] * 0.45
    demo[:2400] *= np.linspace(0, 1, 2400)[:, None]
    demo[-9600:] *= np.linspace(1, 0, 9600)[:, None]
    write('preview.wav', demo)
    measurements.append(measure('preview.wav'))
    report = {'source': 'original procedural synthesis; no external recordings or samples',
              'auditioned': False, 'music': 'original 8-bar electro prototype; D minor; 128 BPM',
              'combined_stems_peak_dbfs': round(20 * np.log10(np.max(np.abs(base + intense))), 2),
              'files': measurements}
    (OUT / 'measurements.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for item in measurements:
        print(f"{item['file']}: {item['seconds']:.3f}s / peak {item['peak_dbfs']} dBFS / clips=0")


if __name__ == '__main__':
    main()
