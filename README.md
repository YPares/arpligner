# Arpligner

This is a Lua script for [Protoplug](https://www.osar.fr/protoplug/), for live MIDI chord manipulation.

Still experimental, please post issues here if you have a problem with the script :)

### The gist

Arpligner is a polyphonic arpeggiator that will use your own arp patterns (like Xfer Cthulhu, Reason PolyStep Sequencer, FL VFX Sequencer, 2Rule TugMidiSeq...).
Each step can thus play several notes of a chord at the same time. "Steps" can also hold over an arbitrary length of time and overlap.
So besides arpeggiation _per se_, you can do strumming or really any kind of "turning a plain block chord into something more interesting".

The big difference is that Arpligner does (very intentionally) **not** come with its own GUI for editing patterns. Instead it will rely on arp patterns being fed to it as regular (and possibly live) MIDI data.
Therefore, you can make use of your DAW piano rool and MIDI sequencing capabilities[^1], play those patterns live, or use an external MIDI sequencer (software or hardware).

Therefore, you can use it as a regular arp, playing your chords against pre-written patterns, or the opposite.
Or both can be live data! That would be having two keyboard players: one in charge of the chords and the other one in charge of how to layout those chords[^2].

### Installation

You first need to [download Protoplug](https://github.com/pac-dev/protoplug/releases) and install it.

Then if you are using Bitwig, you can use the `Arpligner.bwpreset` directly[^3].
If you are using another DAW, load the `Lua Protoplug Gen` plugin[^4] as any VST2 plugin in your DAW project. Once opened, the GUI shows a code page.
Empty it and copy/paste the contents of `Arpligner.lua` into it. You can [copy the code straight from Github](Arpligner.lua), no need to download the script first. Then click on "Compile" in the bottom right-hand corner. Then Protoplug's status (loaded script and everything) should be saved along with your DAW project.

After that, you'll need some way to feed MIDI into Protoplug. If you are using the Bitwig preset, place it on the track(s) where the **pattern** MIDI clips play.
The preset comes with a Note Receiver device on which you can set where it should get the chords from (the preset automatically moves each incoming chord note to Channel 16).

### How to use it

Arpligner expects to be fed MIDI data on at least 2 channels:

- Chan 16, which will be treated as the "chord" channel
- At least one chan from 1 to 15, which will be treated as "pattern" channels

This script will interpret "notes" playing on **pattern** channels as the degrees of the chord which is currently playing on the **chord** channel. Starting from C3 and going up:

- C3 means "first chord degree"
- C#3 means "second chord degree"
- D3 means "third chord degree", etc.

But what happens if you go below C3? Or if you go "above" the last degree of your chord? Well then, Arpligner will loop through the chord, but transposing the notes up or down as many octaves as needed, so it always has a sensible chord note to play.

### Tips

- Arpligner should give more controllable results if you write/play your chords in their most canonical form: block chords, no inversions, without any octaving of notes. It will really use your chord note data as they come, it won't do anything fancy to try and detect which chord you are actually playing, what is its root, etc.
- In pattern channels, only note pitches are affected. So your "pattern" notes will stay on the same channel, keep their velocity, etc. That means your patterns may contain velocity variations, pitch bends, CCs or that kind of things :)
- Non-note events incoming on the chord channel (CCs, etc) will just pass through (and stay on channel 16).
- As it is working with potentially live MIDI data, Arpligner cannot do things like "resetting the pattern when the chord changes", so it's up to you to keep your chord and pattern clips in sync (or interestingly out-of-sync). E.g. for a regular arp "emulation", just make your chord clip have a length that is a multiple of that of your pattern clip, so you'll repeat the same pattern several times over different chords.
- Besides pre-writing chord progressions or playing them live, you can use plugins like [Scaler 2](https://www.pluginboutique.com/products/6439) which speed up the process of writing chord progressions, and which can output their chords as MIDI and sync with the host's tempo & transport.
- When **no note** is playing on the chord channel, you have a few options (controllable via a "When no chord notes" param exposed to the host, and also settable in the "params" tab of Protoplug's GUI). **DO NOT** change this parameter while notes are being played, else it will result in stuck notes. Stop the transport and stop playing first. Options are:
    - "Use default chord": uses a C7 chord (though you can change that default chord at the top of the script). This is the default behaviour
    - "Silence": play nothing
    - "Passthrough": just use the pattern "notes" directly, as if they were real notes

### Current limitations

- Note-off events are tricky to handle right, and the plugin's internal state will be fully refreshed every time it's reloaded, so if that happens it may lose track of some notes. If you ever get stuck notes, stop all MIDI data coming to Arpligner, open Protoplug's GUI and re-click on "Compile" to reset the script (or just disabling/re-enabling the Protoplug plugin might work, depending on your DAW).

Video demo/tutorials to come...


[^1]: I use Bitwig and the piano roll editor and MIDI operators to manipulate notes are just so much better and so much more flexible than everything I've seen in plugins

[^2]: And if someone ever makes a jazz jam session out of that, definitely record it and send me the video ^^

[^3]: See for instance [this video by Mattias Holmgren](https://www.youtube.com/watch?v=siY4ZpNOeCY) for how to use third-party presets with Bitwig. If you are not using Bitwig, then ~~go download it~~ your DAW should have a similar way to save and share presets, and if you ever build one don't hesitate to submit a PR :)

[^4]: Protoplug actually comes with 2 plugins, "Fx" (for audio FXs) and "Gen" (for synths). I believe in the case of Arpliner both should work but I really only tested with "Gen"
