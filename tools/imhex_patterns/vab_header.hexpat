/* Sources:

- https://problemkaputt.de/psx-spx.htm#cdromfileaudiosamplesetsvabandvhvbsony
- https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/PS1/Vab.h
*/

/*
  000h 1      tones        Number of Tones in the Program (Yaroze: 4) (uh?)
  001h 1      mvol         Master Volume   (Yaroze: 0..127)
  002h 1      prior                        (Yaroze: N/A)
  003h 1      mode                         (Yaroze: N/A)
  004h 1      mpan         Master Panning  (Yaroze: 0..127)
  005h 1      reserved0
  006h 2      attr                         (Yaroze: N/A)
  008h 4      reserved1
  00Ch 4      reserved2
*/
struct ProgramAttribute {
    u8  tones;
    u8  mvol;
    u8  prior;
    u8  mode;
    u8  mpan;
    u8  reserved0;      
    u16 attr;
    u32 reserved1;
    u32 reserved2;
};

/*
Tone Attributes (20h-byte per Tone, max 10h tones per Program)

  000h 1      prior        Tone Priority   (Yaroze: 0..127, 127=highest)
  001h 1      mode         Mode            (Yaroze: 0=Normal, 4=Reverberation)
  002h 1      vol          Tone Volume     (Yaroze: 0..127)
  003h 1      pan          Tone Panning    (Yaroze: 0..127)
  004h 1      center       Centre note (in semitone units) (Yaroze: 0..127)
  005h 1      shift        Centre note fine tuning         (Yaroze: 0..127)
  006h 1      min          Note limit minimum value     (Yaroze: 0..127)
  007h 1      max          Note limit maximum value     (Yaroze: 0..127)
  008h 1      vibW                                      (Yaroze: N/A)
  009h 1      vibT                                      (Yaroze: N/A)
  00Ah 1      porW                                      (Yaroze: N/A)
  00Bh 1      porT                                      (Yaroze: N/A)
  00Ch 1      pbmin        Max? value for downwards pitchbend  (Yaroze: 0..127)
  00Dh 1      pbmax        Max value for upwards pitchbend     (Yaroze: 0..127)
  00Eh 1      reserved1
  00Fh 1      reserved2
  010h 2      ADSR1        Attack,Decay    (Yaroze: 0..127,0..15)
  012h 2      ADSR2        Release,Sustain (Yaroze: 0..127,0..31)
  014h 2      prog         Program number that tone belongs to (Yaroze: 0..127)
  016h 2      vag          VAG number                          (Yaroze: 0..254)
  018h 8      reserved
*/
struct ToneAttribute {
    u8  prior;          // priority, used for controlling allocation when more voices than can be keyed on are requested
    u8  mode;           // mode (0 - normal, 4 - reverb applied)
    u8  vol;            // volume
    u8  pan;            // pan
    u8  center;         // center note
    u8  shift;          // pitch correction (0~127,cent units)
    u8  min;            // minimum note limit
    u8  max;            // maximum note limit
    u8  vibW;           // vibrato width
    u8  vibT;           // 1 cycle time of vibrato (tick units)
    u8  porW;           // portamento width
    u8  porT;           // portamento holding time (tick units)
    u8  pbmin;          // pitch bend min
    u8  pbmax;          // pitch bend max
    u8  reserved1; 
    u8  reserved2;
    u8  attack, decay;  // ADSR1 (attack/decay)
    u8  release, sust;  // ADSR2 (release/sustain)
    u16 prog;           // parent program
    u16 vag;            // waveform (VAG) used
    u16 reserved[4]; 
};

/*
  0000h 4       File ID ("pBAV")
  0004h 4       Version (usually 7) (reportedly 6 exists, too) (5, 20h exists)
  0008h 4       VAB ID (usually 0)
  000Ch 4       Total .VAB filesize in bytes (or sum of .VH and .VB filesizes)
  0010h 2       Reserved (EEEEh)
  0012h 2       Number of Programs, minus 1 (0000h..007Fh = 1..128 programs)
  0014h 2       Number of Tones, minus? (max 0800h?) (aka max 10h per program)
  0016h 2       Number of VAGs, minus? (max 00FEh)
  0018h 1       Master Volume (usually 7Fh)
  0019h 1       Master Pan    (usually 40h)
  001Ah 1       Bank Attribute 1 (user defined) (usually 00h)
  001Bh 1       Bank Attribute 2 (user defined) (usually 00h)
  001Ch 4       Reserved (FFFFFFFFh)
  0020h 800h    Program Attributes 10h-byte per Program 00h..7Fh   (fixed size)
  0820h P*200h  Tone Attributes 200h-byte per Program 00h..P-1  (variable size)
  xx20h 200h    16bit VAG Sizes (div8) for VAG 00h..FFh            (fixed size)
  xx20h (...)   ADPCM data (only in .VAB files, otherwise in separate .VB file)
*/
struct VabHeader {
    char fileID[4];
    u32 version;
    u32 vabId;
    u32 size;
    u16 reserved;
    u16 numPrograms;
    u16 numTones;
    u16 numVAGs;
    u8 masterVolume;
    u8 masterPan;
    u8 bankAttribute1;
    u8 bankAttribute2;
    u32 reserved2;
    ProgramAttribute programAttributes[128];
    ToneAttribute toneAttributes[16*numPrograms];
    u16 vagSizes[256];
};

VabHeader vab @ 0x0;

u32 numTonesTotal out;

fn main() {
    for (u32 i = 0, i < vab.numPrograms, i = i + 1) {
        numTonesTotal += vab.programAttributes[i].tones;
    }
};