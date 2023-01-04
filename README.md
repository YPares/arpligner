# MIDI scripts

This repo contains some Lua scripts for [Protoplug](https://www.osar.fr/protoplug/), for live MIDI manipulation.

## ARPLIGNER

This script is a replacement for arpeggiator plugins like Cthulhu (albeit with a different set of features for now),
but it will make use of your DAW piano rool and MIDI sequencing capabilities instead of coming with its own pattern editor.
(I use Bitwig and the piano roll editor and MIDI operators to manipulate notes are just so much better and so much more flexible than everything I've seen in plugins)

ARPLIGNER expects to be fed MIDI data on at least 2 channels:

- Chan 16, which will be treated as the "chord" channel
- At least one chan from 1 to 15, which will be treated as "pattern" channels

This script will consider notes on pattern channels to be degrees of a chord (the chord currently being played on the chord channel), starting from C3 and going up:

- C3 is "first chord degree"
- C#3 is "second chord degree"
- D3 is "third chord degree", etc.

But what happens if you go below C3? Or if you go "above" the last degree of your chord? Well then, ARPLIGNER will loop through the chord, but transposing the notes up or down as many octaves as needed, so it always has a sensible chord note to play.

In pattern channels, only note pitches are affected. So your "pattern" notes will stay on the same channel, keep their velocity, etc. That means your patterns may contain velocity variations, pitch bends, CCs or that kind of things :)

Non-note events incoming on the chord channel (CCs, etc) will just pass through (and stay on channel 16).

For now, when no note is playing on the chord channel, notes on pattern channels will stay as they are (if you like some chromatic experiments that can be fun...)

Video demo/tutorials to come...
