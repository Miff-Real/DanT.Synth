# PCycl

![PCycl Front Panel](img/pcycl.png)

`PCycl` (Poly Cycle) takes a mono signal and routes it to one channel of a polyphonic output. Each trigger moves the
signal to the next channel, and after the last channel it returns to the first. The trigger is passed through on a
second polyphonic output, on the same channel as the signal.

Spreading a mono signal across channels like this lets each value carry on after the next one has arrived. Some of
the things it can be used for:

* Playing a mono sequencer polyphonically, so that the release of one note rings on while the next one starts.
* Sending successive hits of a pattern to different voices, or to different channels of a polyphonic effect.
* Collecting a series of values, such as random voltages, into the channels of a single cable.

## Controls and Ports

Each icon on the panel sits above the port it describes.

### Signal Input

* **`Signal input`**: The mono signal to be routed, for example the pitch output of a sequencer. If a polyphonic cable
  is connected only its first channel is used.

### Trigger Input

* **`Next channel trigger input`**: A rising edge here moves the signal to the next channel. This would normally be the
  gate or trigger that belongs to the signal. The voltage at this input is also passed to the trigger output.

### Reset

* **`Reset` Button** and **`Reset trigger input`**: Returns the signal to the first channel. The next trigger stays on
  the first channel, so the first note after a reset always plays there.

### Channels

* **`Channels` Knob**: Sets the number of channels to cycle through, from `1` to `16`. The number is shown above the
  knob.

* **Grid Light**: Shows which channel is currently receiving the signal.

### Outputs

* **`[Poly] Signal output`**: The routed signal. The channel that is currently selected follows the input. What the
  other channels output is set in the context menu, see `Idle channels` below.

* **`[Poly] Trigger output`**: The trigger input, on the same channel as the signal. The other channels are at `0V`.
  It can instead send a trigger or gate made by the module, see `Trigger output` below.

When the module is bypassed both inputs are passed to their outputs unchanged, as mono signals.

## Context Menu

* **Idle channels**: What the signal output sends on the channels that are not selected.
  * `Hold last value` (default): Each channel keeps the last value it received. Use this for pitch, so that a note
    which is still releasing stays in tune.
  * `Zero volts`: The channels fall to `0V`.

* **Timing correction**: Lines up the signal and its trigger when they do not arrive together. See `Timing` below.
  * `Automatic` (default on): The module measures how far apart the two arrive and delays the earlier one to match.
    The menu shows the last measurement.
  * `Signal delay`: Shown when `Automatic` is off. Delays the signal by a fixed `0` to `64` samples.

* **Trigger output**: What the trigger output sends. A note starts each time the signal moves to the next channel,
  and on the first trigger after a reset.
  * `Pass trigger input` (default): The trigger input is passed through.
  * `Off`: The output stays at `0V`.
  * `Trigger for each note`: A `1ms`, `10V` trigger is sent on the new channel each time a note starts.
  * `Gate for each note`: A `10V` gate is sent on the new channel each time a note starts. Its length is set by
    `Gate length`. Gates on different channels overlap.

  If a note starts on a channel whose gate is still high, the gate first drops to `0V` for `1ms` so that the voice is
  triggered again.

* **Gate length**: The length of the gate sent by `Gate for each note`, from `0.01` to `2` seconds. The default is
  `0.1` seconds.

* **Automatic channel increment**: When enabled, the signal also moves to the next channel whenever it jumps by the
  threshold or more. With a `V/Oct` signal and the default threshold of one semitone, each different note played on a
  keyboard is given a new channel, including notes played legato where the gate never falls.
  * **Threshold**: The size of jump that counts, from `0.01V` to `2V`. The default is `0.083V`, one semitone.
  * Only jumps count. Slow movement such as a glide, vibrato or pitch bend stays on the same channel.
  * A jump and a trigger that arrive within `1ms` of each other are treated as one note, so patching both does not
    skip a channel.
  * If there is no gate to patch to the trigger input, set `Trigger output` to send a trigger or gate for each note.

## Timing

In VCV Rack every cable delays its signal by one sample. A signal and a trigger that leave their source on the same
sample therefore arrive at `PCycl` together only if they pass through the same number of cables on the way. Each extra
cable in one route makes that one arrive a sample later. For example, a clock patched straight to the trigger input,
and also to a sequencer whose output goes through a quantizer to the signal input, puts two extra cables in the
signal's route, so the trigger arrives two samples before the signal.

`PCycl` changes channel on the sample that the trigger arrives, so the difference matters:

* **Together**: The new value only ever appears on the new channel.

* **Signal arrives first**: The new value reaches the previous channel before the trigger moves it on. With
  `Hold last value` that channel then keeps the wrong value, so a note that is still releasing would jump to the pitch
  of the next one.

* **Trigger arrives first**: The new channel outputs the old value until the new one arrives.

### Automatic correction

With `Timing correction` set to `Automatic`, the module measures the difference each time a trigger and a step in the
signal arrive within `1ms` of each other, and delays whichever arrives first by that many samples. Both outputs then
change on the same sample, and neither of the cases above occurs.

* The difference is set by the patch, so it is measured on one note and applied from the next. The first note after
  the patch is changed is not corrected.
* When the two already arrive together nothing is delayed.
* The measurement needs a signal that steps from one steady value to another, as a sequencer or keyboard does. A
  signal that is always moving, such as an LFO, cannot be measured and is not delayed.
* Differences of up to `1ms` are corrected, limited to `64` samples.
* The context menu shows the last measurement, for example `Measured: trigger 2 samples early`.

### Manual correction

With `Automatic` off, `Signal delay` delays the signal by a fixed number of samples. Set it to the number of extra
cables in the trigger's route. It can only help when the signal arrives first.

Enabling `Automatic channel increment` also protects the previous channel when a stepped signal arrives first, because
the jump moves to the new channel before the trigger arrives.

## Patch Example

To play a mono sequencer polyphonically:

1. Patch the sequencer's pitch output to the `Signal input` and its gate output to the `Next channel trigger input`.
2. Set the `Channels` knob to the number of voices wanted.
3. Patch the `[Poly] Signal output` to the `V/Oct` input of a polyphonic oscillator and the `[Poly] Trigger output` to
   the gate input of a polyphonic envelope.

Each step of the sequence now plays on the next voice, and with a long release on the envelope the notes overlap. A
voice is used again once the cycle comes back round to it, so if its previous note is still sounding it will be cut
off by the new one. Use more channels or a shorter release to avoid this.

VCV's MIDI to CV module can give each new note its own channel in the same way, once its polyphony channels are set
above one. `PCycl` does this for sources that are not MIDI.
