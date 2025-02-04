#include "ActionListManager.h"

#include <psyqo/kernel.hh>

void ActionListManager::update(std::uint32_t dt, bool gamePaused)
{
    for (auto& [name, al] : actionLists) {
        if (gamePaused && !al.shouldRunWhenGameIsPaused()) {
            continue;
        }
        al.update(dt);
    }
    // remove finished lists
    eastl::erase_if(actionLists, [](const auto& p) { return p.second.isFinished(); });
}

void ActionListManager::addActionList(ActionList actionList)
{
    actionList.play();
    if (!actionList.isFinished()) {
        if (isActionListPlaying(actionList.getName())) {
            psyqo::Kernel::assert(false, "Action list with this name is already playing");
            /* throw eastl::runtime_error(fmt::format(
                "action list with name '{}' is already playing, call stopActionList first",
                actionList.getName())); */
        }
        actionLists.emplace(actionList.getName(), eastl::move(actionList));
    }
}

void ActionListManager::stopActionList(StringHash actionListName)
{
    auto it = actionLists.find(actionListName);
    if (it != actionLists.end()) {
        psyqo::Kernel::assert(false, "Action list with this name is not playing");
    }
    actionLists.erase(it);
};

bool ActionListManager::isActionListPlaying(StringHash actionListName) const
{
    return actionLists.count(actionListName) != 0; // eastl hash map has no contains?
};

const ActionList& ActionListManager::getActionList(const StringHash actionListName) const
{
    return actionLists.at(actionListName);
}
