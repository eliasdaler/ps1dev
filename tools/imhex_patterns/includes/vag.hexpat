struct Vag {
    char header[4];
    be u32 version;
    u32 reserved; 
    be u32 datasize;
    be u32 sampleRate;
    char filename[16];
    u8 zeros[12]; // FIXME: is it always 12?
    u8 data[datasize];
};

Vag vag @ 0x0;