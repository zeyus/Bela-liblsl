#!/usr/bin/env python3
"""Stream audio from a WAV file to Lab Streaming Layer.

This script reads audio data from a WAV file and streams it to LSL in real-time
at the WAV file's native sample rate.
"""

import argparse
import time
import wave
import numpy as np
from pylsl import StreamInfo, StreamOutlet, local_clock


def main():
    parser = argparse.ArgumentParser(description="Stream WAV file to LSL")
    parser.add_argument("filename", help="WAV file to stream")
    parser.add_argument("--name", default="audio", help="Stream name (default: audio)")
    parser.add_argument("--type", default="audio", help="Stream type (default: audio)")
    parser.add_argument("--chunk", type=int, default=256, help="Chunk size (default: 256 samples)")
    parser.add_argument("--loop", action="store_true", help="Loop the WAV file")
    args = parser.parse_args()

    # Open the WAV file
    print(f"Opening WAV file: {args.filename}")
    try:
        wav_file = wave.open(args.filename, 'rb')
    except Exception as e:
        print(f"Error opening WAV file: {e}")
        return

    # Get WAV file properties
    n_channels = wav_file.getnchannels()
    sample_width = wav_file.getsampwidth()
    sample_rate = wav_file.getframerate()
    n_frames = wav_file.getnframes()
    duration = n_frames / sample_rate

    print(f"WAV file properties:")
    print(f"  Channels: {n_channels}")
    print(f"  Sample rate: {sample_rate} Hz")
    print(f"  Bit depth: {sample_width * 8} bits")
    print(f"  Duration: {duration:.2f} seconds")

    # Create LSL stream info
    # We always use float32 for the LSL stream regardless of WAV bit depth
    info = StreamInfo(
        name=args.name,
        type=args.type,
        channel_count=n_channels,
        nominal_srate=sample_rate,
        channel_format='float32',
        source_id=f'wav_file_{args.filename}'
    )

    # Add some metadata about the stream
    info.desc().append_child_value("manufacturer", "WAV File Streamer")
    channels = info.desc().append_child("channels")
    for c in range(n_channels):
        if n_channels == 1:
            channels.append_child("channel").append_child_value("label", "mono")
        elif n_channels == 2:
            channels.append_child("channel").append_child_value("label", "left" if c == 0 else "right")
        else:
            channels.append_child("channel").append_child_value("label", f"channel{c+1}")

    # Create outlet
    outlet = StreamOutlet(info, chunk_size=args.chunk)
    print(f"Created LSL outlet: {args.name}")

    # Function to convert WAV samples to float32 in range [-1.0, 1.0]
    def wav_to_float(wav_data, sample_width):
        if sample_width == 1:  # 8-bit (unsigned)
            samples = np.frombuffer(wav_data, dtype=np.uint8)
            samples = samples.astype(np.float32) / 128.0 - 1.0
        elif sample_width == 2:  # 16-bit (signed)
            samples = np.frombuffer(wav_data, dtype=np.int16)
            samples = samples.astype(np.float32) / 32768.0
        elif sample_width == 3:  # 24-bit (signed)
            # Convert 24-bit to 32-bit signed int
            samples = np.zeros(len(wav_data) // 3, dtype=np.int32)
            for i in range(len(samples)):
                samples[i] = int.from_bytes(wav_data[i*3:i*3+3], byteorder='little', signed=True)
            samples = samples.astype(np.float32) / 8388608.0
        elif sample_width == 4:  # 32-bit (signed)
            samples = np.frombuffer(wav_data, dtype=np.int32)
            samples = samples.astype(np.float32) / 2147483648.0
        else:
            raise ValueError(f"Unsupported sample width: {sample_width}")
        return samples

    # Stream the WAV file
    print("Streaming WAV file to LSL...")
    chunk_samples = args.chunk * n_channels
    chunk_bytes = chunk_samples * sample_width
    
    try:
        # Main streaming loop
        while True:
            wav_file.setpos(0)  # Reset to beginning of file
            
            # Track timing to ensure real-time playback
            start_time = local_clock()
            samples_sent = 0
            
            while True:
                # Calculate elapsed time and required samples
                elapsed_time = local_clock() - start_time
                required_samples = int(sample_rate * elapsed_time * n_channels) - samples_sent
                
                # If we need to send more samples
                if required_samples >= chunk_samples:
                    # Read chunk from WAV file
                    wav_data = wav_file.readframes(args.chunk)
                    
                    # If we've reached the end of the file
                    if len(wav_data) < chunk_bytes:
                        if args.loop:
                            break  # Break the inner loop to restart file
                        else:
                            print("End of WAV file reached. Exiting.")
                            return  # Exit the program
                    
                    # Convert to float and reshape to [samples, channels]
                    samples = wav_to_float(wav_data, sample_width)
                    
                    # If stereo or more channels, reshape to have one sample per row
                    if n_channels > 1:
                        samples = samples.reshape(-1, n_channels)
                    else:
                        # For mono, we need to reshape differently
                        samples = samples.reshape(-1, 1)
                    
                    # Push samples to LSL
                    for sample in samples:
                        outlet.push_sample(sample)
                    
                    samples_sent += len(samples) * n_channels
                else:
                    # Sleep a bit before checking again
                    time.sleep(0.001)
            
            if not args.loop:
                break
            
            print("Restarting WAV file playback...")
    
    except KeyboardInterrupt:
        print("\nStreaming interrupted by user.")
    finally:
        wav_file.close()
        print("Streaming stopped, WAV file closed.")


if __name__ == "__main__":
    main()
