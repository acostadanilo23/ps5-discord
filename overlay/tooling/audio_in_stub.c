// Link metadata only. The PS5 provides libSceAudioIn at runtime.
int sceAudioInInit(void) { return 0; }
int sceAudioInOpen(int user, unsigned type, unsigned index, unsigned frames,
                   unsigned rate, unsigned format) {
    (void)user; (void)type; (void)index; (void)frames; (void)rate; (void)format;
    return 0;
}
int sceAudioInInput(int handle, void* samples) {
    (void)handle; (void)samples;
    return 0;
}
int sceAudioInClose(int handle) {
    (void)handle;
    return 0;
}
