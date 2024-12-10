#include "AnimationWriter.h"

#include <fstream>

#include <FsUtil.h>

#include "ModelJsonFile.h"

#include "ConversionParams.h"
#include "FixedPoint.h"

namespace
{
struct DJBHash {
public:
    static std::uint32_t hash(const std::string& str)
    {
        return static_cast<std::uint32_t>(djbProcess(seed, str.c_str(), str.size()));
    }

    static std::uint64_t djbProcess(std::uint64_t hash, const char str[], std::size_t n)
    {
        std::uint64_t res = hash;
        for (std::size_t i = 0; i < n; ++i) {
            res = res * 33 + str[i];
        }
        return res;
    }

    static constexpr std::uint64_t seed{5381};
};
}

void writeAnimationsToFile(
    std::filesystem::path& path,
    const std::vector<Animation>& animations,
    const ConversionParams& params)
{
    std::ofstream file(path, std::ios::binary);
    fsutil::binaryWrite(file, static_cast<std::uint32_t>(animations.size()));
    for (const auto& anim : animations) {
        fsutil::binaryWrite(file, DJBHash::hash(anim.name));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(anim.length));
        fsutil::binaryWrite(file, static_cast<std::uint16_t>(anim.tracks.size()));
        for (const auto& track : anim.tracks) {
            fsutil::binaryWrite(file, static_cast<std::uint8_t>(track.trackType));
            fsutil::binaryWrite(file, static_cast<std::uint8_t>(track.jointId));
            fsutil::binaryWrite(file, static_cast<std::uint16_t>(track.keys.size()));
            for (const auto& key : track.keys) {
                fsutil::binaryWrite(file, floatToFixed<std::int32_t>((float)key.frame));
                if (track.trackType == 0) {
                    fsutil::binaryWrite(file, floatToFixed<std::int16_t>(key.data.rotation.w));
                    fsutil::binaryWrite(file, floatToFixed<std::int16_t>(key.data.rotation.x));
                    fsutil::binaryWrite(file, floatToFixed<std::int16_t>(key.data.rotation.y));
                    fsutil::binaryWrite(file, floatToFixed<std::int16_t>(key.data.rotation.z));
                } else if (track.trackType == 1) {
                    fsutil::binaryWrite(
                        file, floatToFixed<std::int16_t>(key.data.translation.x, params.scale));
                    fsutil::binaryWrite(
                        file, floatToFixed<std::int16_t>(key.data.translation.y, params.scale));
                    fsutil::binaryWrite(
                        file, floatToFixed<std::int16_t>(key.data.translation.z, params.scale));
                    fsutil::binaryWrite(file, std::uint16_t{}); // pad
                }
            }
        }
    }
}
