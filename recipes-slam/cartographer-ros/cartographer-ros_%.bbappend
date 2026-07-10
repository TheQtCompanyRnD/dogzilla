# Like cartographer, cartographer-ros still uses the old unprefixed Abseil
# thread-safety annotation macros (GUARDED_BY, LOCKS_EXCLUDED,
# EXCLUSIVE_LOCKS_REQUIRED) in its headers, which the newer abseil-cpp in this
# release no longer provides. meta-ros has no abseil patch for cartographer-ros
# (any distro), so map the three macros it uses to their ABSL_ equivalents via
# compiler defines. Object-like -> function-like macro chaining expands e.g.
# GUARDED_BY(mutex_) -> ABSL_GUARDED_BY(mutex_).
CXXFLAGS:append = " -DGUARDED_BY=ABSL_GUARDED_BY -DLOCKS_EXCLUDED=ABSL_LOCKS_EXCLUDED -DEXCLUSIVE_LOCKS_REQUIRED=ABSL_EXCLUSIVE_LOCKS_REQUIRED"
