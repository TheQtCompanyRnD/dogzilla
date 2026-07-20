# Master (default audio sink) volume state, published latched
# (transient_local) on /dogzilla/audio/volume.
# Single-field on purpose: the QtRos2 publisher exposes it as one bindable
# `value` property, and the subscriber collapses to a plain number.
float32 value  # linear volume, 0.0 (mute) .. 1.0 (full)
