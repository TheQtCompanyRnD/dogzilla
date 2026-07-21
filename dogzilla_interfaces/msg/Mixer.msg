# Audio mixer state, published latched (transient_local) on
# /dogzilla/audio/mixer. One field per channel; multi-field on purpose: the
# QtRos2 publisher exposes one bindable property per channel, and the
# subscriber exposes matching read-only properties.
#
# Channels are set individually via the SetVolume service (channel-addressed,
# so independent setters can't overwrite each other's channels). Extend the
# mixer by adding fields here and handling the new channel name there.
float32 master  # default audio sink (speaker), linear 0.0 .. 1.0
float32 mic     # default audio source (capture gain), linear 0.0 .. 1.0
