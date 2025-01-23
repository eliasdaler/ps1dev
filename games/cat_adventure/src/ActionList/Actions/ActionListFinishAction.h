#pragma once

#include <ActionList/Action.h>

#include <ActionList/ActionList.h>

class ActionListFinishAction : public Action {
public:
    ActionListFinishAction(ActionList actionList);

    bool enter() override;
    bool update(std::uint32_t dt) override;

    const ActionList& getActionList() const { return actionList; }

private:
    ActionList actionList;
};
