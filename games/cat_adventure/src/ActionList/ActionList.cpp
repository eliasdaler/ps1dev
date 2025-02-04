#include "ActionList.h"

#include "Actions/ActionListFinishAction.h"
#include "Actions/DoAction.h"

#include <common/syscalls/syscalls.h>

ActionList::ActionList() : ActionList("empty"_sh)
{}

ActionList::ActionList(StringHash name) : name(name)
{}

ActionList::ActionList(StringHash name, eastl::vector<eastl::unique_ptr<Action>> actions) :
    name(eastl::move(name)), actions(eastl::move(actions))
{}

void ActionList::addAction(eastl::unique_ptr<Action> action)
{
    if (onAddSetParallel) {
        action->parallel = true;
    }
    actions.push_back(eastl::move(action));
}

void ActionList::addAction(eastl::function<void()> action)
{
    addAction(eastl::make_unique<DoAction>(eastl::move(action)));
}

void ActionList::addAction(ActionList actionList)
{
    addAction(eastl::make_unique<ActionListFinishAction>(eastl::move(actionList)));
}

bool ActionList::isFinished() const
{
    if (looping) {
        return false;
    }

    return currentIdx >= actions.size();
}

void ActionList::update(std::uint32_t dt)
{
    // assert(wasPlayCalled && "play() not called before update!");
    // TODO: add psyqo::Kernel::assert?

    if (isFinished()) {
        return;
    }

    auto& action = *actions[currentIdx];
    if (action.parallel) {
        updateParallelActions(dt);
        return;
    }

    action.update(dt);
    if (action.isFinished()) {
        goToNextAction();
    }
}

void ActionList::updateParallelActions(std::uint32_t dt)
{
    bool hasUnfinishedActions = false;
    int numParallelActions = 0;
    for (std::size_t i = currentIdx; i < actions.size(); ++i) {
        auto& action = *actions[i];
        if (!action.parallel) {
            break;
        }
        ++numParallelActions;
        if (!action.isFinished()) {
            action.update(dt);
            if (!action.isFinished()) {
                hasUnfinishedActions = true;
            }
        }
    }
    if (!hasUnfinishedActions) {
        // -1 because goToNextAction will call advance
        for (int i = 0; i < numParallelActions - 1; ++i) {
            advanceIndex();
        }
        goToNextAction();
    }
}

void ActionList::play()
{
    wasPlayCalled = true;
    currentIdx = 0;
    stopped = true;
    goToNextAction();
}

void ActionList::advanceIndex()
{
    ++currentIdx;
    if (currentIdx >= actions.size() && looping) {
        currentIdx = 0;
    }
}

void ActionList::goToNextAction()
{
    while (!isFinished()) {
        if (stopped) {
            // assert(currentIdx == 0);
            stopped = false;
        } else {
            advanceIndex();
        }

        if (isFinished()) {
            return;
        }

        auto& action = *actions[currentIdx];
        if (!action.parallel) {
            const auto finished = action.enter();
            if (!finished) {
                return;
            }
        } else {
            // enter N parallel actions
            bool hasUnfinished = false;
            for (std::size_t i = currentIdx; i < actions.size(); ++i) {
                auto& action = *actions[i];
                if (!action.parallel) {
                    break;
                }
                const auto finished = action.enter();
                if (!finished) {
                    hasUnfinished = true;
                }
            }
            if (hasUnfinished) {
                return;
            }
        }
    }
}
