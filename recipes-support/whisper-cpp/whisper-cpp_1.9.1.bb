SUMMARY = "whisper.cpp - OpenAI Whisper automatic speech recognition (offline, C/C++)"
DESCRIPTION = "Port of OpenAI's Whisper ASR model in C/C++ (ggml). Runs offline \
on the CPU; tiny/base models run near real-time on the Pi 5's Cortex-A76 cores."
HOMEPAGE = "https://github.com/ggml-org/whisper.cpp"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=223b26b3c1143120c87e2b13111d3e99"

SRC_URI = "git://github.com/ggml-org/whisper.cpp.git;protocol=https;branch=master"
# v1.9.1
SRCREV = "f049fff95a089aa9969deb009cdd4892b3e74916"

inherit cmake

# GGML_NATIVE=OFF: this is a cross build, so don't probe the build host's CPU;
# let the OE tune flags (cortexa76) drive the target ISA. Build the CLI
# (whisper-cli) but skip tests/server/SDL-based streaming.
EXTRA_OECMAKE = " \
    -DGGML_NATIVE=OFF \
    -DWHISPER_BUILD_TESTS=OFF \
    -DWHISPER_BUILD_EXAMPLES=ON \
    -DWHISPER_BUILD_SERVER=OFF \
    -DBUILD_SHARED_LIBS=ON \
"

# whisper/ggml ship unversioned .so libraries; keep them in the main package
# (and silence the dev-so QA that expects unversioned .so only in -dev).
FILES:${PN} += "${libdir}/lib*.so"
INSANE_SKIP:${PN} += "dev-so"
