# 🎙️ EDITH — Edge Voice Activator

**Smart India Hackathon 2026 | SIH26172 | ISRO**
**Team EDITH**

## 🚀 About

EDITH is a lightweight **Edge–Cloud voice activation system** for low-power devices.

A TinyML model detects a **custom keyword locally**, and only after detection is the subsequent speech streamed to a remote **ASR server** for speech-to-text.

> **The edge listens → decides → the cloud processes.**

## 💡 Key Features

* 🎯 Custom keyword spotting
* ⚡ Low-latency event-triggered streaming
* 🧠 TinyML-based on-device detection
* 📦 INT8 quantized lightweight model
* 🎤 Noise & multi-speaker robustness
* 🔄 Circular audio buffering
* 🌐 Multilingual/custom keyword support
* 🔐 Reduced unnecessary audio transmission

## 🏗️ Architecture

```text
🎤 Microphone
      ↓
Audio Pre-processing
      ↓
Log-Mel Features
      ↓
🧠 TinyML KWS
      ↓
Keyword Detected?
   ↙          ↘
 NO            YES
 ↓              ↓
Listen      Audio Buffer
               ↓
        📡 Audio Streaming
               ↓
          ☁️ Remote ASR
               ↓
          📝 Speech-to-Text
```

## 🛠️ Tech Stack

* **Hardware:** ESP32 / Low-power MCU + MEMS Microphone
* **Languages:** C/C++, Python
* **ML:** TensorFlow Lite Micro, TinyML
* **Features:** Log-Mel Spectrogram
* **Optimization:** INT8 Quantization
* **Communication:** Wi-Fi Audio Streaming
* **Server:** Remote ASR

## 📊 Target Constraints

| Metric            | Target     |
| ----------------- | ---------- |
| RAM               | `< 256 KB` |
| Idle CPU          | `< 10%`    |
| False Activations | Near-zero  |
| Detection         | High TPR   |
| Latency           | Low        |

## 📁 Project Structure

```text
EDITH-Edge-Voice-Activator/
│
├── Encryption_analysis/
│   ├── Server_audio_receiver.py   # Remote audio receiver / server
│   ├── Streaming.c                # Audio streaming implementation
│   └── dummy_code_tester.c        # Testing code
│
├── marvin_kws_int8.tflite         # INT8-quantized KWS model
├── model_data.h                   # Embedded model data for deployment

│
├── .gitignore                     # Ignored files and secrets
├── README.md                      # Project documentation
└── SIH_PPT_2026.pdf               # SIH presentation/reference

## 🤝 Contributing

```bash
git checkout -b feature/<feature-name>
git add .
git commit -m "Add <feature>"
git push origin feature/<feature-name>
```

Create a Pull Request after pushing your branch.

**Do not commit:** `.venv/`, API keys, temporary files, or unnecessary large files.

## 📚 References

* [Hello Edge: Keyword Spotting on Microcontrollers](https://arxiv.org/abs/1711.07128)
* [Speech Commands Dataset](https://arxiv.org/abs/1804.03209)
* [TensorFlow Lite Micro](https://arxiv.org/abs/2010.08678)

## 🔬 Status

🚧 **Currently in development**

**Dataset → Model → Quantization → Edge Deployment → Streaming → ASR → Testing**

---

### 🎙️ EDITH

**Lightweight AI at the Edge. Speech Recognition in the Cloud.**
