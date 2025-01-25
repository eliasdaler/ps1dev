#pragma once

#include <EASTL/string_view.h>

#include <ActionList/Action.h>

#include <Camera.h>

class DialogueBox;

class SayAction : public Action {
public:
    SayAction(DialogueBox& dialogueBox, eastl::string_view text);
    SayAction(
        DialogueBox& dialogueBox,
        eastl::string_view text,
        Camera& camera,
        const CameraTransform& transform);

    bool enter() override;
    bool update(std::uint32_t dt) override;

private:
    DialogueBox& dialogueBox;
    eastl::string_view text;

    Camera* cameraPtr{nullptr};
    CameraTransform cameraTransform;
    bool hasCameraTransform{false};
};
