# FET Compressor (native ViPERDSP)

FET Compressor is available on the Dynamics tab and is **off by default**.
Threshold: -48..0 dBFS; ratio: 1:1..10:1; soft knee: 0..12 dB;
makeup gain: 0..24 dB; attack: 1..100 ms; release: 5..500 ms.

The GUI converts dB into the native logarithmic gain representation
and ratio R into the negative compression slope -(1-1/R). No substitute
compressor is used. Native parameters update on the non-realtime worker.

All effects can affect perceived loudness. Start with low compression and
compare at matched levels. FET and other dynamics processors may interact.
