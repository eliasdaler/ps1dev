#include "VabFile.h"

#include "FileReader.h"

#include <common/syscalls/syscalls.h>

void VabFile::load(eastl::string_view filename, const eastl::vector<uint8_t>& data)
{
    util::FileReader fr{
        .bytes = data.data(),
    };

    const auto hdr = fr.GetUInt32();
    const auto isVab = (hdr == 0x56414270); // "pBAV"

    if (!isVab) {
        ramsyscall_printf("not a VAB file, header: %08X\n", (int)hdr);
        return;
    }

    fr.cursor = 0;

    fr.ReadObj(header);
    fr.ReadArr(progAttributes.data(), 128);

    toneAttributes.resize(16 * header.numPrograms);
    fr.ReadArr(toneAttributes.data(), toneAttributes.size());

    fr.ReadArr(vagSizes.data(), 256);

    instruments.resize(header.numPrograms);
    for (int i = 0; i < header.numPrograms; ++i) {
        auto& instInfo = instruments[i];
        const auto& progInfo = progAttributes[i];
        instInfo.numTones = progInfo.tones;

        int toneNum = 0;
        for (int j = 0; j < toneAttributes.size(); ++j) {
            if (toneAttributes[j].prog == i) {
                instInfo.tones[toneNum] = j;
                ++toneNum;
            }
        }
    }
}
