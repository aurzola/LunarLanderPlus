import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, filtfilt

def butter_bandpass(lowcut, highcut, fs, order=4):
    """Crea los coeficientes para un filtro paso banda Butterworth."""
    nyq = 0.5 * fs
    low = lowcut / nyq
    high = highcut / nyq
    b, a = butter(order, [low, high], btype='band')
    return b, a

def apply_bandpass_filter(data, lowcut=300.0, highcut=3000.0, fs=44100, order=4):
    """Aplica el filtro de radio (elimina graves y agudos extemos)."""
    b, a = butter_bandpass(lowcut, highcut, fs, order=order)
    return filtfilt(b, a, data)

def apply_distortion(data, gain=2.5):
    """Simula la saturación analógica del micrófono con una función tangente hiperbólica."""
    saturated = np.tanh(data * gain)
    return saturated / np.max(np.abs(saturated))

def add_radio_noise(data, noise_level=0.03):
    """Añade estática de radio de fondo (ruido blanco)."""
    noise = np.random.normal(0, noise_level, len(data))
    return data + noise

def generate_quindar_tone(freq=2525, duration=0.25, fs=44100):
    """Genera el tono de inicio/cierre de transmisión (Quindar Tone / Roger Beep)."""
    t = np.linspace(0, duration, int(fs * duration), False)
    tone = 0.3 * np.sin(2 * np.pi * freq * t)
    # Ventana de suavizado al inicio y final del tono para evitar clicks
    window = np.hanning(len(tone))
    return tone * window

def process_nasa_voice(input_wav_path, output_wav_path):
    # 1. Cargar archivo de audio
    fs, data = wavfile.read(input_wav_path)
    
    # Convertir a mono si es estéreo
    if len(data.shape) > 1:
        data = data.mean(axis=1)
        
    # Normalizar audio de entrada (-1.0 a 1.0)
    data = data.astype(np.float32)
    if np.max(np.abs(data)) > 0:
        data = data / np.max(np.abs(data))

    # 2. Aplicar filtro de banda de radio (300 Hz - 2800 Hz)
    filtered = apply_bandpass_filter(data, lowcut=350.0, highcut=2800.0, fs=fs, order=4)

    # 3. Aplicar distorsión suave
    distorted = apply_distortion(filtered, gain=3.0)

    # 4. Añadir estática de fondo
    noisy = add_radio_noise(distorted, noise_level=0.02)

    # 5. Generar y concatenar el Tono Quindar al inicio y al final
    intro_tone = generate_quindar_tone(freq=2525, duration=0.25, fs=fs) # Tono de entrada
    outro_tone = generate_quindar_tone(freq=2475, duration=0.25, fs=fs) # Tono de salida
    silence = np.zeros(int(fs * 0.1)) # 100ms de silencio

    final_audio = np.concatenate([intro_tone, silence, noisy, silence, outro_tone])

    # Normalizar resultado final y convertir a 16-bit PCM WAV
    final_audio = final_audio / np.max(np.abs(final_audio))
    final_audio_int16 = (final_audio * 32767).astype(np.int16)

    # 6. Guardar el archivo de salida
    wavfile.write(output_wav_path, fs, final_audio_int16)
    print(f"Procesamiento completado. Audio guardado en: {output_wav_path}")

# --- Ejemplo de uso ---
# Asegúrate de usar un archivo WAV como entrada
process_nasa_voice("mi_voz.wav", "voz_nasa_mission_control.wav")