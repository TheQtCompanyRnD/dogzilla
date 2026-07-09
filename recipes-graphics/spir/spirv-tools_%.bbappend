# spirv-tools builds with -Werror (SPIRV_WERROR=ON). On modern host toolchains
# (e.g. Arch's GCC 15) its stricter -Wmaybe-uninitialized analysis raises a
# false positive in decoration_manager.cpp that becomes a fatal error during
# the -native build. Turn off warnings-as-errors so it builds on the host.
EXTRA_OECMAKE += "-DSPIRV_WERROR=OFF"
