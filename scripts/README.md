# pylsl audio streaming

- install [uv](https://docs.astral.sh/uv/getting-started/installation/)
- cd into the `scripts` folder
- run `uv run stream_to_bela.py chant_44.1kHz.wav --loop` for 44.1kHz audio stream or `uv run stream_to_bela.py chant_22.05kHz.wav --loop` for 22.05kHz audio stream
- on the bela, run the `render_lsl_audio.cpp` example
- you should see / hear the audio stream being played on the bela
