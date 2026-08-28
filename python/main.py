import numpy as np
import soundfile as sf
import matplotlib.pyplot as plt

def compute_erle(mic, output):
    """ERLE in dB: how much echo power was removed."""
    mic_power = np.mean(mic.astype(np.float64) ** 2)
    output_power = np.mean(output.astype(np.float64) ** 2)
    return 10 * np.log10(mic_power / output_power)

def plot_spectrograms(mic, output, sr, out_path="spectrograms.png"):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    
    # Compute both spectrograms first, without plotting, so we can find a shared scale
    Pxx_mic, freqs, bins = plt.mlab.specgram(mic, Fs=sr)
    Pxx_out, _, _= plt.mlab.specgram(output, Fs=sr)
    
    # Convert to dB and find shared min/max across BOTH signals
    Pxx_mic_db = 10 * np.log10(Pxx_mic + 1e-10)
    Pxx_out_db = 10 * np.log10(Pxx_out + 1e-10)
    vmin = min(Pxx_mic_db.min(), Pxx_out_db.min())
    vmax = max(Pxx_mic_db.max(), Pxx_out_db.max())
    
    axes[0].specgram(mic, Fs=sr, vmin=vmin, vmax=vmax)
    axes[0].set_title("mic.wav (before)")
    axes[1].specgram(output, Fs=sr, vmin=vmin, vmax=vmax)
    axes[1].set_title("output.wav (after)")
    plt.tight_layout()
    plt.savefig(out_path)


def compute_pesq_stoi(clean_ref, output, sr):
    from pesq import pesq
    from pystoi import stoi
    mode = "wb" if sr == 16000 else "nb"  # wideband needs 16kHz
    pesq_score = pesq(sr, clean_ref, output, mode)
    stoi_score = stoi(clean_ref, output, sr, extended=False)
    return pesq_score, stoi_score

if __name__ == "__main__":
    import sys

    mic_path = sys.argv[1] if len(sys.argv) > 1 else "mic.wav"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "output.wav"
    clean_ref_path = sys.argv[3] if len(sys.argv) > 3 else None

    mic, sr_mic = sf.read(mic_path)
    output, sr_out = sf.read(output_path)

    assert sr_mic == sr_out, "Sample rates don't match"
    min_len = min(len(mic), len(output))
    mic, output = mic[:min_len], output[:min_len]

    erle = compute_erle(mic, output)
    print(f"ERLE: {erle:.2f} dB")

    plot_spectrograms(mic, output, sr_mic)

    if clean_ref_path:
        clean_ref, sr_ref = sf.read(clean_ref_path)
        clean_ref = clean_ref[:min_len]
        pesq_score, stoi_score = compute_pesq_stoi(clean_ref, output, sr_mic)
        print(f"PESQ: {pesq_score:.2f}")
        print(f"STOI: {stoi_score:.2f}")
    else:
        print("No clean reference provided — skipping PESQ/STOI (pass a third arg for those)")
