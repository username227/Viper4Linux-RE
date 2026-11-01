# Speaker Correction

- Native ViPERDSP `SpeakerCorrection` effect; no correction files needed.
- Built-in filters are high-pass 80 Hz, low-pass 13,500 Hz and band-pass around 420 Hz (Q 3.88).
- Intended for loudspeakers, not headphones. This is **not** a calibration profile.
- Off by default; independent of the Convolver.
- Changing the enable state uses the worker (not the realtime PipeWire callback).
- GUI and global preset persist enable state; no other parameters are invented.
