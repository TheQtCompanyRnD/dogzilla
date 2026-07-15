SUMMARY = "whisper.cpp ggml model: tiny.en q5_1 quantized (English, ~32 MB)"
DESCRIPTION = "Pre-converted, q5_1-quantized ggml weights for OpenAI Whisper \
'tiny.en', for use with whisper.cpp. ~2x faster encode than base.en on the Pi 5 \
(model size drives speed here, not quantization -- q5_1 just shrinks the \
footprint). The fast default for short voice commands; base.en is kept as the \
higher-accuracy fallback (both install side by side, selected at runtime with \
whisper-cli -m)."
HOMEPAGE = "https://huggingface.co/ggerganov/whisper.cpp"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en-q5_1.bin;downloadfilename=ggml-tiny.en-q5_1.bin"
SRC_URI[sha256sum] = "c77c5766f1cef09b6b7d47f21b546cbddd4157886b3b5d6d4f709e91e66c7c2b"

inherit allarch

S = "${UNPACKDIR}"
do_configure[noexec] = "1"
do_compile[noexec] = "1"

WHISPER_MODELDIR ?= "${datadir}/whisper.cpp/models"

do_install() {
    install -d ${D}${WHISPER_MODELDIR}
    install -m 0644 ${UNPACKDIR}/ggml-tiny.en-q5_1.bin ${D}${WHISPER_MODELDIR}/ggml-tiny.en-q5_1.bin
}

FILES:${PN} = "${WHISPER_MODELDIR}/ggml-tiny.en-q5_1.bin"
