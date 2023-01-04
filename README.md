# Protoplug scripts

This repo contains some Lua scripts for [Protoplug](https://www.osar.fr/protoplug/), for live MIDI manipulation.

## ARPLIGNER

This script is a drop-in replacement for arpeggiator plugins like Cthulhu (albeit with less features for now),
but it will make use of your DAW piano rool and MIDI sequencing capabilities instead of coming with its own pattern editor.
(I use Bitwig and the pattern editor and MIDI operators to manipulate notes are just awesome)

ARPLIGNER expects several MIDI channels as a input:

- Chan 16, which will be treated as the "chord channel"
- Chans 1 to 15, which will be treated as "pattern" channels

This script will consider notes on pattern channels to be degrees of a chord (the chord currently being played on the chord channel), starting from C3 and going up:

- C3 is "first chord degree"
- C#3 is "second chord degree"
- D3 is "third chord degree", etc.

But what happens if you go below C3? Or if you go "above" the last degree of your chord? Well then, ARPLIGNER will loop through the chord, but transposing the notes up or down as many octaves as needed so you ever get a note to play!

Video demo to come...
