#include "components/ScrollableContainer.h"

#include "math/Vector2i.h"
#include "renderers/Renderer.h"

#define AUTO_SCROLL_RESET_DELAY 3000 // ms to reset to top after we reach the bottom
#define AUTO_SCROLL_DELAY 1000 // ms to wait before we start to scroll
#define AUTO_SCROLL_SPEED 50 // ms between scrolls

ScrollableContainer::ScrollableContainer(Window* window, int scrollDelay) : GuiComponent(window),
	mAutoScrollDelay(scrollDelay), mAutoScrollSpeed(0), mAutoScrollAccumulator(0), mScrollPos(0, 0), mScrollDir(0, 0), mAutoScrollResetAccumulator(0)
{
}

bool ScrollableContainer::input(InputConfig* config, Input input)
{
	if (!isVisible())
		return false;

	bool downBtn = config->isMappedTo("down", input);
	bool upBtn = config->isMappedTo("up", input);

	if (mHotkeyHeld && (downBtn || upBtn))
	{
		if (input.value)
		{
			// manual scroll
			mManualScroll = true;
			mAutoScrollAccumulator = mAutoScrollSpeed;
			mAutoScrollDelay = 0;
			mScrollDir = Vector2f(0, downBtn ? 1 : -1);
		}
		else
			restoreAutoscroll(0);
	}
	else if (!mHotkeyHeld && input.value == 0)
	{
		if (mManualScroll && (upBtn || downBtn))
		{
			// user has released hotkey before up/down
			// continue autoscroll without delay now as up/down has been released
			restoreAutoscroll(0);
		}
		else if (config->isMappedTo("pageup", input) || config->isMappedTo("pagedown", input))
		{
			// to catch e.g. up still held and TL/TR released
			restoreAutoscroll(mPristineAutoScrollDelay);
			reset();
		}
	}
	return GuiComponent::input(config, input);
}

void ScrollableContainer::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	Vector2i clipPos((int)trans.translation().x(), (int)trans.translation().y());

	Vector3f dimScaled = trans * Vector3f(mSize.x(), mSize.y(), 0);
	Vector2i clipDim((int)(dimScaled.x() - trans.translation().x()), (int)(dimScaled.y() - trans.translation().y()));

	Renderer::pushClipRect(clipPos, clipDim);

	trans.translate(-Vector3f(mScrollPos.x(), mScrollPos.y(), 0));
	Renderer::setMatrix(trans);

	GuiComponent::renderChildren(trans);

	Renderer::popClipRect();
}

void ScrollableContainer::setAutoScroll(bool autoScroll)
{
	if(autoScroll)
	{
		mScrollDir = Vector2f(0, 1);
		if (mAutoScrollDelay == 0)
		{
			mAutoScrollDelay = AUTO_SCROLL_DELAY;
		}
		mAutoScrollSpeed = AUTO_SCROLL_SPEED;
	}else{
		mScrollDir = Vector2f(0, 0);
		mAutoScrollDelay = 0;
		mAutoScrollSpeed = 0;
		mAutoScrollAccumulator = 0;
	}
	mPristineAutoScrollDelay = mAutoScrollDelay;
}

Vector2f ScrollableContainer::getScrollPos() const
{
	return mScrollPos;
}

void ScrollableContainer::setScrollPos(const Vector2f& pos)
{
	mScrollPos = pos;
}

void ScrollableContainer::update(int deltaTime)
{
	if((mHotkeyHeld && !mManualScroll) || (!mHotkeyHeld && mManualScroll))
	{
		// do nothing if hotkey is held alone (=stop scrolling) or
		// if hotkey has been released and up/down still held
		GuiComponent::update(deltaTime);
		return;
	}

	if(mAutoScrollSpeed != 0)
	{
		mAutoScrollAccumulator += deltaTime + (mManualScroll ? deltaTime : 0);

		//scale speed by our width! more text per line = slower scrolling
		// const float widthMod = (680.0f / getSize().x());
		// widthMod currently unused. Gone since this commit:
		// https://github.com/RetroPie/EmulationStation/blame/75e31d915c6c80888860fb5631636f99c5f6a52c/src/components/ScrollableContainer.cpp#L63
		while(mAutoScrollAccumulator >= mAutoScrollSpeed)
		{
			if (mManualScroll)
				// speed up while up/down held
				mScrollDir *= Vector2f(0, 1.025);
			mScrollPos += mScrollDir;
			mAutoScrollAccumulator -= mAutoScrollSpeed;
		}
	}

	//clip scrolling within bounds
	if(mScrollPos.x() < 0)
		mScrollPos[0] = 0;
	if(mScrollPos.y() < 0)
		mScrollPos[1] = 0;

	const Vector2f contentSize = getContentSize();
	if(mScrollPos.x() + getSize().x() > contentSize.x())
	{
		mScrollPos[0] = contentSize.x() - getSize().x();
		mAtEnd = !mHotkeyHeld && !mManualScroll;
	}

	if(contentSize.y() < getSize().y())
	{
		mScrollPos[1] = 0;
	}else if(mScrollPos.y() + getSize().y() > contentSize.y())
	{
		mScrollPos[1] = contentSize.y() - getSize().y();
		mAtEnd = !mHotkeyHeld && !mManualScroll;
	}

	if(mAtEnd)
	{
		mAutoScrollResetAccumulator += deltaTime;
		if(mAutoScrollResetAccumulator >= AUTO_SCROLL_RESET_DELAY)
			reset();
	}

	GuiComponent::update(deltaTime);
}

//this should probably return a box to allow for when controls don't start at 0,0
Vector2f ScrollableContainer::getContentSize()
{
	Vector2f max(0, 0);
	for(unsigned int i = 0; i < mChildren.size(); i++)
	{
		Vector2f pos(mChildren.at(i)->getPosition()[0], mChildren.at(i)->getPosition()[1]);
		Vector2f bottomRight = mChildren.at(i)->getSize() + pos;
		if(bottomRight.x() > max.x())
			max.x() = bottomRight.x();
		if(bottomRight.y() > max.y())
			max.y() = bottomRight.y();
	}

	return max;
}

void ScrollableContainer::reset()
{
	if (!mHotkeyHeld && !mManualScroll)
	{
		//LOG(LogDebug) << "ScrollableContainer::reset() called";
		mScrollPos = Vector2f(0, 0);
		mAutoScrollResetAccumulator = 0;
		mAutoScrollAccumulator = -mAutoScrollDelay + mAutoScrollSpeed;
		mAtEnd = false;
	}
}

void ScrollableContainer::restoreAutoscroll(const int delay) {
	mManualScroll = false;
	mAutoScrollAccumulator = mAutoScrollSpeed - delay;
	mAutoScrollDelay = mPristineAutoScrollDelay;
	mAutoScrollResetAccumulator = 0;
	mScrollDir = Vector2f(0, 1);
}
