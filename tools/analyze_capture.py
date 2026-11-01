#!/usr/bin/env python3
"""Compare downloaded boot WAV tracks. Requires numpy, scipy and matplotlib."""
import argparse
import hashlib
import json
from pathlib import Path
import wave

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from scipy.signal import correlate, correlation_lags


def db(value):
    return float(20 * np.log10(max(float(value), 1e-12)))


def metrics(samples, rate):
    values = samples.astype(np.float64) / 32768
    blocks = values[:len(values) // 320 * 320].reshape(-1, 320)
    rms = np.sqrt(np.mean(blocks * blocks, axis=1))
    zeros = np.all(blocks == 0, axis=1)
    return {
        'samples': len(samples), 'seconds': len(samples) / rate,
        'peak_dbfs': db(np.max(np.abs(values))),
        'rms_dbfs': db(np.sqrt(np.mean(values * values))),
        'dc_offset': float(np.mean(values)),
        'clipped_samples': int(np.count_nonzero((samples == -32768) | (samples == 32767))),
        'zero_samples_percent': float(np.mean(samples == 0) * 100),
        'zero_20ms_frames': int(np.count_nonzero(zeros)),
        'zero_20ms_frame_times': (np.flatnonzero(zeros) * .02).round(3).tolist(),
        'per_second_rms_dbfs': [db(np.sqrt(np.mean(values[i:i + rate] ** 2)))
                               for i in range(0, len(values), rate)],
    }, rms


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    tracks, envelopes, report = {}, {}, {}
    for name in ('mic', 'afe'):
        path = args.directory / f'{name}-boot.wav'
        with wave.open(str(path), 'rb') as wav:
            rate = wav.getframerate()
            if (rate, wav.getnchannels(), wav.getsampwidth()) != (16000, 1, 2):
                raise ValueError(f'Unexpected WAV format: {path}')
            tracks[name] = np.frombuffer(wav.readframes(wav.getnframes()), dtype='<i2')
        report[name], envelopes[name] = metrics(tracks[name], rate)
        report[name]['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    mic = tracks['mic'].astype(np.float64)
    afe = tracks['afe'].astype(np.float64)
    centered_mic, centered_afe = mic - mic.mean(), afe - afe.mean()
    scores = correlate(centered_afe, centered_mic, method='fft')
    lags = correlation_lags(len(afe), len(mic))
    allowed = np.abs(lags) <= rate
    lag = int(lags[allowed][np.argmax(scores[allowed])])
    if lag >= 0:
        x, y = centered_mic[:len(mic) - lag], centered_afe[lag:]
    else:
        x, y = centered_mic[-lag:], centered_afe[:len(afe) + lag]
    energy = float(np.dot(x, x))
    report['comparison'] = {
        'afe_delay_samples': lag, 'afe_delay_ms': lag * 1000 / rate,
        'correlation_after_alignment': float(np.corrcoef(x, y)[0, 1]) if energy and np.any(y) else None,
        'afe_gain_after_alignment': float(np.dot(x, y) / energy) if energy else None,
    }
    (args.directory / 'analysis.json').write_text(json.dumps(report, indent=2) + '\n')
    fig, axes = plt.subplots(4, 1, figsize=(13, 10), constrained_layout=True)
    for axis, name in zip(axes[:2], ('mic', 'afe')):
        data = tracks[name].astype(np.float64) / 32768
        axis.plot(np.arange(0, len(data), 8) / rate, data[::8], lw=.45)
        axis.set(title='Mic before AFE (gain 4)' if name == 'mic' else 'AFE output',
                 ylabel='Amplitude', xlim=(0, len(data) / rate), ylim=(-1, 1))
        axis.grid(alpha=.2)
    for name, envelope in envelopes.items():
        axes[2].plot(np.arange(len(envelope)) * .02,
                     20 * np.log10(np.maximum(envelope, 1e-6)), label=name, lw=.7)
    axes[2].set(ylabel='RMS / 20 ms (dBFS)', ylim=(-100, 0), xlim=(0, 30))
    axes[2].legend()
    axes[2].grid(alpha=.2)
    axes[3].specgram(mic / 32768, Fs=rate, NFFT=512, noverlap=384,
                    cmap='magma', vmin=-110, vmax=-30)
    axes[3].set(xlabel='Time (seconds)', ylabel='Mic frequency (Hz)', ylim=(0, 8000), xlim=(0, 30))
    fig.savefig(args.directory / 'waveform.png', dpi=150)
    print(json.dumps({key: {k: v for k, v in value.items() if k not in
                           ('zero_20ms_frame_times', 'per_second_rms_dbfs')}
                      for key, value in report.items()}, indent=2))


if __name__ == '__main__':
    main()
