#pragma once

#include <ActionList/Action.h>

#include <ActionList/ActionList.h>

class ActionListFinishAction : public Action {
public:
    ActionListFinishAction(ActionList actionList);

    bool enter() override;
    void update(std::uint32_t dt) override;
    bool isFinished() const override;

    const ActionList& getActionList() const { return actionList; }

private:
    ActionList actionList;
};
