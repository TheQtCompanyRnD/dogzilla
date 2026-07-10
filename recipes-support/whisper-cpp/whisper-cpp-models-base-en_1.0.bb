SUMMARY = "whisper.cpp ggml model: base.en (English, ~142 MB)"
DESCRIPTION = "Pre-converted ggml weights for OpenAI Whisper 'base.en', for use \
with whisper.cpp. Good accuracy/speed balance for English on the Pi 5."
HOMEPAGE = "https://huggingface.co/ggerganov/whisper.cpp"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin;downloadfilename=ggml-base.en.bin"
SRC_URI[sha256sum] = "a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002"

inherit allarch

S = "${UNPACKDIR}"
do_configure[noexec] = "1"
do_compile[noexec] = "1"

WHISPER_MODELDIR ?= "${datadir}/whisper.cpp/models"

do_install() {
    install -d ${D}${WHISPER_MODELDIR}
    install -m 0644 ${UNPACKDIR}/ggml-base.en.bin ${D}${WHISPER_MODELDIR}/ggml-base.en.bin
}

FILES:${PN} = "${WHISPER_MODELDIR}/ggml-base.en.bin"
