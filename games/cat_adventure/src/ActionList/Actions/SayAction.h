#pragma once

#include <EASTL/string_view.h>

#include <ActionList/Action.h>

class DialogueBox;

class SayAction : public Action {
public:
    SayAction(DialogueBox& dialogueBox, eastl::string_view text);

    bool enter() override;
    bool update(std::uint32_t dt) override;

private:
    DialogueBox& dialogueBox;
    eastl::string_view text;
};
