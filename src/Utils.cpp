#include "Utils.h"

#include <libcd.h>
#include <stdio.h>

#define CD_SECTOR_SIZE 2048
#define bytesToSectors(len) ((len + CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE)

namespace util
{

eastl::vector<uint8_t> readFile(eastl::string_view filename)
{
    CdlFILE filepos;
    if (!CdSearchFile(&filepos, const_cast<char*>(filename.data()))) {
        printf("File %s was not found\n", filename);
        return {};
    }

    CdControl(CdlSetloc, (u_char*)&filepos.pos, nullptr);

    eastl::vector<uint8_t> buffer(sizeof(u_long) * bytesToSectors(filepos.size) * CD_SECTOR_SIZE);
    CdRead(bytesToSectors(filepos.size), (u_long*)buffer.data(), CdlModeSpeed);

    CdReadSync(0, 0);

    buffer.resize(filepos.size);

    return buffer;
}

}
