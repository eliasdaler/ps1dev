#include "ActionList.h"

#include "Actions/ActionListFinishAction.h"
#include "Actions/DoAction.h"

ActionList::ActionList() : ActionList("empty")
{}

ActionList::ActionList(eastl::string name) : name(eastl::move(name))
{}

ActionList::ActionList(eastl::string name, eastl::vector<eastl::unique_ptr<Action>> actions) :
    name(eastl::move(name)), actions(eastl::move(actions))
{}

void ActionList::addAction(eastl::unique_ptr<Action> action)
{
    actions.push_back(eastl::move(action));
}

void ActionList::addAction(eastl::function<void()> action)
{
    actions.push_back(eastl::make_unique<DoAction>(eastl::move(action)));
}

void ActionList::addAction(ActionList actionList)
{
    actions.push_back(eastl::make_unique<ActionListFinishAction>(eastl::move(actionList)));
}

void ActionList::addActionFront(eastl::unique_ptr<Action> action)
{
    actions.insert(actions.begin(), eastl::move(action));
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
    if (bool finished = action.update(dt); finished) {
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
        const auto finished = action.enter();
        if (!finished) {
            return;
        }
    }
}
