#pragma once

#include <EASTL/string_view.h>

#include <ActionList/Action.h>

#include <Camera.h>
#include <Core/StringHash.h>

class DialogueBox;
struct AnimatedModelObject;

struct SayParams {
    Camera* cameraPtr{nullptr};
    CameraTransform cameraTransform;

    AnimatedModelObject* object{nullptr};
    StringHash anim{};
    StringHash faceAnim{};
};

class SayAction : public Action {
public:
    SayAction(DialogueBox& dialogueBox, eastl::string_view text);
    SayAction(DialogueBox& dialogueBox, eastl::string_view text, const SayParams& params);

    bool enter() override;
    bool update(std::uint32_t dt) override;

private:
    DialogueBox& dialogueBox;
    eastl::string_view text;

    SayParams params;
};
