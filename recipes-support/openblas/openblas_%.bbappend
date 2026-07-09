# openblas 0.3.31's bfloat16 sbgemm path passes an incompatible function
# pointer type, which GCC 14+ (host GCC 15 here) treats as a hard error
# (-Werror=incompatible-pointer-types). The recipe interpolates
# TOOLCHAIN_OPTIONS into CC and doesn't honor CFLAGS, so append the
# warning-downgrade there. Recipe-scoped, so it affects only openblas.
TOOLCHAIN_OPTIONS:append = " -Wno-error=incompatible-pointer-types"
