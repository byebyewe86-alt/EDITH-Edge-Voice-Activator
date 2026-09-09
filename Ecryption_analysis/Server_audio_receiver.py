"""
server_audio_receiver.py
---------------------------------------------------------
Laptop-side counterpart to esp32_c6_audio_streamer.c.

Listens on UDP, decrypts each AES-256-GCM packet, verifies the
auth tag, and reassembles chunks in sequence order. Drops any
packet that fails authentication (tampered / wrong key / corrupt).

Packet format expected (little-endian):
  [seq_num : 2 bytes][nonce : 12 bytes][tag : 16 bytes][ciphertext : N bytes]

Install:
    pip install cryptography
"""

import socket
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# Must match aes_key[] on the ESP32 exactly (32 bytes = 256-bit key)
AES_KEY = bytes([
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
])

LISTEN_IP = "0.0.0.0"
LISTEN_PORT = 5005

NONCE_LEN = 12
TAG_LEN = 16
SEQ_LEN = 2

aesgcm = AESGCM(AES_KEY)


def parse_packet(data: bytes):
    """Split a raw UDP payload into (seq, nonce, ciphertext_with_tag)."""
    seq = int.from_bytes(data[0:SEQ_LEN], "little")
    nonce = data[SEQ_LEN:SEQ_LEN + NONCE_LEN]
    tag = data[SEQ_LEN + NONCE_LEN:SEQ_LEN + NONCE_LEN + TAG_LEN]
    ciphertext = data[SEQ_LEN + NONCE_LEN + TAG_LEN:]
    # cryptography's AESGCM expects ciphertext + tag concatenated
    return seq, nonce, ciphertext + tag


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LISTEN_IP, LISTEN_PORT))
    print(f"Listening for encrypted audio on UDP {LISTEN_IP}:{LISTEN_PORT}")

    expected_seq = None
    audio_buffer = {}  # seq -> plaintext, in case packets arrive out of order

    while True:
        data, addr = sock.recvfrom(4096)
        seq, nonce, ct_and_tag = parse_packet(data)

        try:
            plaintext = aesgcm.decrypt(nonce, ct_and_tag, None)
        except Exception:
            print(f"[seq {seq}] auth/decryption FAILED — dropping packet")
            continue

        if expected_seq is not None and seq != expected_seq:
            print(f"[seq {seq}] out of order (expected {expected_seq})")

        audio_buffer[seq] = plaintext
        expected_seq = seq + 1

        # Hand off decrypted PCM bytes to the ASR stage here.
        # e.g. asr_queue.put(plaintext)
        print(f"[seq {seq}] OK — {len(plaintext)} bytes decrypted from {addr}")


if __name__ == "__main__":
    main()