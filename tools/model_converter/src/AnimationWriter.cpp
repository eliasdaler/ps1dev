#include "AnimationWriter.h"

#include <fstream>

#include <FsUtil.h>

#include "ConversionParams.h"
#include "FixedPoint.h"
#include "ModelJsonFile.h"

#include "DJBHash.h"

void writeAnimationsToFile(
    std::filesystem::path& path,
    const std::vector<Animation>& animations,
    const ConversionParams& params)
{
    std::ofstream file(path, std::ios::binary);
    fsutil::binaryWrite(file, static_cast<std::uint32_t>(animations.size()));
    for (const auto& anim : animations) {
        fsutil::binaryWrite(file, DJBHash::hash(anim.name));

        // This field will be used for all kinds of flags, but for now we only use
        // it for setting animations to either be looped or not
        fsutil::binaryWrite(file, static_cast<std::uint32_t>(anim.looped));

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
