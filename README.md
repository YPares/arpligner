# Arpligner

Arpligner is a polyphonic arpeggiator MIDI plugin that will use your own arp
patterns (like Xfer Cthulhu, Reason PolyStep Sequencer, FL VFX Sequencer, 2Rule
TugMidiSeq...). Each step can thus play several notes of a chord at the same
time. "Steps" can also hold over an arbitrary length of time and overlap.  So
besides arpeggiation _per se_, you can do strumming or really any kind of
"turning a plain block chord into something more interesting".

The big difference is that Arpligner does (very intentionally) **not** come with
its own GUI for editing patterns. Instead it will rely on arp patterns being fed
to it as regular (and possibly live) MIDI data. Therefore, you can make use of
your DAW piano rool and MIDI sequencing capabilities[^1], play those patterns
live, or use an external MIDI sequencer (software or hardware).

Therefore, you can use it as a regular arp, playing your chords against
pre-written patterns, or the opposite. Or both can be live data! That would be
having two keyboard players: one in charge of the chords and the other one in
charge of how to layout those chords[^2].

See https://youtu.be/IQ9GFEaS4Ag for an overview of the tool and the basic
features (this uses the original Lua script but the video remains valid).

Still experimental, please post issues here in case of bugs or questions! :)

## How to use it

Arpligner has two modes: **Multi-channel** and **Multi-instance**, which affect
how Arpligner will receive MIDI data and set what it considers to be a
"**chord** track" and what it considers to be a "**pattern** track".

### General behaviour

Whatever the mode, Arpligner will interpret "notes" playing on **pattern**
tracks as the degrees of the chord which is currently playing on the **chord**
track. Starting from C3 and going up:

- C3 means "first chord degree"
- C#3 means "second chord degree"
- D3 means "third chord degree", etc.

C3 is therefore treated as the _reference_ pattern note, the one from which
everything will be derived.

However, you may wonder then what happens if you go below C3. Or if you go
"above" the last degree of your chord. Well then, Arpligner will loop through
the chord, but transposing the notes up or down as many octaves as needed, so it
always has a sensible chord note to play.

You can also select a different mapping method. For instance, you can choose
that only the white keys will be used here, which can be more convenient when
playing patterns live on a MIDI keyboard. See the [pattern settings
section](#pattern-parameters) for more info.

### Multi-channel mode

In this mode, you can use as little as one single Arpligner plugin in your DAW
session. It expects to be fed MIDI data on at least 2 channels:

- Channel 16 (by default), which will be treated as the **chord** channel
  ("track")
- At least one _other_ channel (say, Channel 1), which will be treated as a
  **pattern** channel ("track"). Any channel other than the **chord** channel
  will do, they will all be treated equally.

You can perfectly use several Arpligner instances set to that mode at the same
time, but they will just be completely independent of one another. They will
each receive their own chord track and pattern tracks.

**Multi-channel** has the advantage of requiring a bit less CPU and RAM, and
allowing for several chord "tracks" that can each one affect up to 15 patterns
"tracks".

### Multi-instance mode

In this mode, the different instances of the Arpligner plugin in your DAW
session can communicate with one another. You would then have one Arpligner
instance per relevant track, one being configured as the **Global chord instance**,
and the other ones as **Pattern instances**. MIDI channels no longer matter to
Arpligner in that mode.

**Multi-instance** has the advantage of alleviating MIDI routing configuration
in your DAW, and is not limited by the number of MIDI channels. So you have just
one chord track, but it can affect any number of pattern tracks.

Note that it is perfectly possible to have in your DAW session both
**Multi-instance** instances and **Multi-channel** instances. Only those set to
**Multi-instance** will communicate, the other ones will keep depending solely
on the MIDI data you directly feed into them.


## Installation

Please go to the [releases](https://github.com/YPares/arpligner/releases)
(unfold the "Assets" section) to download the latest release. Alternatively,
this repository comes with all the needed code to build the plugin and
standalone application.

Then install the VST3 plugin in your regular VST3 folder, depending on your system
and DAW settings.

**Important:** Due to [a VST3
limitation](https://forum.juce.com/t/arpeggiatorplugin-vst3-recognized-as-aufio-fx-instead-of-midi-fx/43563),
Arpligner will be recognized by your DAW as an Audio Fx plugin, whereas it is
only a MIDI Fx plugin. So if your DAW sorts or filters plugins by categories,
you will have to look for Arpligner under this category.

Then, I recommend to have one track dedicated to **chords**. After that, if you
use the **Multi-channel** mode, you'll need some way to configure MIDI routing
in your DAW so Arpligner receives what it expects.

### Notes about MIDI routing in Multi-channel mode

When using **Multi-channel** mode, you have two options:

- Dedicate one MIDI channel to each of your instruments. Then use only one
Arpligner instance on your **chord** track, and route **all** your MIDI data to
it, making sure that this data is properly tagged by channel. Then, split back
the MIDI data that Arpligner outputs by MIDI channel, and route each channel to
its corresponding instrument. Depending on your DAW, you may have to use 2
tracks per instrument in that fashion: one for the pattern clips for this
instrument, and one for the actual instrument.
- Place one instance of Arpligner on each of your **pattern** tracks, and route
the same **chord track** to all of them. Each instance will then receive a
pattern track on only one MIDI channel, and they will all process their patterns
according to the same chord progression. Each instance can then process pattern
data on several channels (this can be useful for multi-timbral instruments like
Kontakt or Omnisphere, or if your DAW supports layering several instruments on
the same track).

For example in Bitwig, you can put the `Note Receiver` device in a track to
route MIDI events from another track, and you can use the `Channel Map` device
in the `Source FX` section of this `Note Receiver` to make every incoming note
go to the right MIDI channel. Do not forget to deactivate the `Inputs` button in
the "Mutes" so that MIDI events from the track pass through too.

## Implementation & supported plugin formats

Arpligner is implemented in C++ with [JUCE 7](https://juce.com/), and therefore
should support a variety of systems and plugin formats. For now, I'm providing
VST3 builds for Linux and Windows x64, but you can have a look under the
`Builds` folder for supported platforms. The [project file](Arpligner.jucer) for
[Projucer](https://juce.com/discover/projucer) is also provided if you want to
generate build files for other platforms or other plugin formats. Arpligner has
no plugin-format-specific or OS-specific code so it should be pretty
straightforward.

I originally implemented Arpligner as a Lua script for
[Protoplug](https://www.osar.fr/protoplug/), but switched to direct use of JUCE
7 for maintainability and VST3 support. The original script can be found in
[`ProtoplugArpligner.lua`](ProtoplugArpligner.lua) but I should no longer be
maintaining it.

## Tips

- Arpligner should give more controllable results if you write/play your chords
  in their most canonical form: block chords, no inversions, without any
  octaving of notes. It will really use your chord note data as they come, it
  won't do anything fancy to try and detect which chord you are actually
  playing, what is its root, etc. But note that "more controllable" does not
  necessarily mean better ;)
- In pattern channels, only note pitches are affected. So your "pattern" notes
  will stay on the same channel, keep their velocity, etc. That means your
  patterns may contain velocity variations, pitch bends, CCs or that kind of
  things :)
- As it is working with potentially live MIDI data, Arpligner has no notion of
  things like "start point", "end point" or "loop", and thus cannot do things
  like "reset the pattern when the chord changes". Therefore, it's up to you to
  keep your chord and pattern clips in sync (or interestingly
  out-of-sync). E.g. for a regular arp "emulation", if you work with looping
  clips, just make your chord clip have a length that is a multiple of that of
  your pattern clip, so you'll repeat the same pattern several times over
  different chords.
- Besides pre-writing chord progressions or playing them live, you can use
  plugins like [Scaler 2](https://www.pluginboutique.com/products/6439) which
  speed up the process of writing chord progressions, and which can output their
  chords as MIDI and sync with the host's tempo & transport.
- Arpligner offers different behaviours for when it is receiving no notes or
  just one note on the chord channel. Default behaviour is "latch & transpose":
  it will keep using the last chord if it receives no more notes, and it will
  transpose it if it receives just one note. See [the settings
  below](#available-settings) for more info.

## Available settings

Arpligner's GUI shows a few parameters, and exposes them to the host.

**IMPORTANT**: Unless stated otherwise, these parameters are **not** meant to be
changed while Arpligner is processing notes. If you really want to automate some
of these (for any reason), I strongly advise you to change their values **only**
during a moment in your song when no MIDI notes are being sent to Arpligner, or
you will end up with stuck notes.

| Parameter name | Default value | Possible values | Documentation |
|--------------------------------|---------------|-----------------|---------------|
|**Instance behaviour**|`[Multi-chan] Chords on chan 16`|Choose from:|The main behaviour of this Arpligner instance|
| | |`Bypass`|Do nothing and just pass all MIDI events through|
| | |`[Multi-chan] Chords on chan XX`|Sets to Multi-channel mode, and use channel `XX` as the chord track (and any other channel as a pattern track)|
| | |`[Multi-instance] Global chord instance`|Sets this instance as the one that receives chord notes (any MIDI input, whatever its channel) and sets the current chord for all other connected instances|
| | |`[Multi-instance] Pattern instance`|Sets this instance as a "follower" of the one set to `Global instance`. Any MIDI input, whatever its channel, is considered a pattern|
| | |`[Multi-instance] 1-buffer delay pattern instance`|Like the above, but will always introduce a delay of 1 midi buffer in the pattern track. See the [current limitations](#current-limitations) for when to use this|

### Chord parameters

These parameters are used *only* in Multi-chan mode or by *Chord* instances.

| Parameter name | Default value | Possible values | Documentation |
|--------------------------------|---------------|-----------------|---------------|
|**Chord notes passthrough**|`Off`|`On` or `Off`|Whether note events on the chord track should pass through instead of being consumed. Non-note events (such as CCs) on the chord track will **always** pass through|
|**When no chord note**|`Latch last chord`|Choose from:|What to do when **no** note is playing on the chord track|
| | |`Latch last chord`|Keep using the previous chord that played (`Silence` if no previous chord is known)|
| | |`Silence`| Ignore the pattern notes|
| | |`Use pattern notes as final notes`|Consider the pattern "notes" as real notes, and pass them through without any change|
|**When single chord note**|`Transpose last chord`|Choose from:|What to do when a **single** note _n_ is playing on the chord track|
| | |`Transpose last chord`|Transpose last chord so that its lowest note becomes _n_ (`Silence` if no previous chord is known)|
| | |`Powerchord`|Turn _n_ into a "2-note chord": _n_ and the note a fifth above|
| | |`Use as is`|Use _n_ as just a "one-note chord". Tread carefully, the end result may go up in octaves pretty fast|
| | |`Silence`|Same as for "no chord note"|
| | |`Use pattern notes as final notes`|Same as for "no chord note"|

### Pattern parameters

These parameters are used *only* in Multi-chan mode or by *Pattern*
instances. When using the **Multi-instance** mode, you can therefore select a
different behaviour for each Pattern instance. This can be useful for instance
if some instruments are to be played live and others via MIDI sequencing, or if
different live players have different preferences.

| Parameter name | Default value | Possible values | Documentation |
|--------------------------------|---------------|-----------------|---------------|
|**Pattern notes mapping**|`Semitone to degree`|Choose from:|How to map midi note codes on pattern tracks to actual notes|
| | |`Map nothing`|No pattern note is mapped|
| | |`Semitone to degree`|Going up/down one _semitone_ in the pattern track means going up/down one degree in the chord|
| | |`White note to degree`|Going up/down one _white key_ in the pattern track means going up/down one degree in the chord. Black keys are not mapped|
| | |`Transpose from 1st degree`|Use Arpligner as a "dynamic" transposer: ignore all chord degrees besides the first (lowest) one. Pattern notes are just transposed accordingly. This allows you to play notes that are outside the current chord, but keeping your patterns centered around the reference note|
|**Reference pattern note**|`60 (C3)`|A MIDI note|On pattern tracks, the note that will always be mapped to the first (lowest) degree of the currently playing chord|
|**Unmapped pattern notes passthrough**|`Off`|`On` or `Off`|When the above mapping method leaves some notes unmapped, whether to pass them through or just silence them|

## Current limitations

- In Multi-instance mode, when chord notes and pattern notes `note on` MIDI
  events are perfectly aligned (which is a ubiquitous case when using MIDI clips
  or sequencers), there is really no guarantee that the chord instance will have
  the time to update the current chord before the pattern instances processes
  the notes, and therefore in these cases it can happen that your patterns will
  play a note that comes from the "previous" chord. In such cases, if your DAW
  has a feature to delay MIDI notes by a few milliseconds on pattern tracks, I
  recommend using it (in Bitwig, that's the `Note Delay` device, which can go as
  low as 10ms delay, which in my tests was enough). If not, you can try setting
  the `Instance behaviour` to `[Multi-instance] 1-buffer delay pattern
  instance`, but this feature may still contain bugs, that's why I recommend not
  using it if your DAW has a proper MIDI delay. Multi-channel mode does not have
  this problem because given all events are processed by the same instance, I
  can make sure to update the current chord prior to processing pattern notes.
- Arpligner is quite strict for now regarding the timing of notes on the chord
  channel. When your chords are played from already quantized MIDI clips or by a
  sequencer it's not a problem, but for "classical" live arp usage, i.e. when
  playing chords live against a preset pattern, it can be. So in this scenario,
  I recommend to use some sort of beat quantizer on your chord channel before
  Arpligner, and to play around with the various settings until you get a
  satisfying result.
- Note-off events are tricky to handle right, and the plugin's internal state
  will be fully refreshed every time it's reloaded, so if that happens it may
  lose track of some notes. If you ever get stuck notes, stop all MIDI data
  coming to Arpligner and disable/re-enable the plugin.


[^1]: I use Bitwig and the piano roll editor and MIDI operators to manipulate
    notes are just so much better and so much more flexible than everything I've
    seen in plugins

[^2]: And if someone ever makes a jazz jam session out of that, definitely
    record it and send me the video ^^
