#if defined(WITH_SDL)

#include "SdlInputManager.h"
#include "../Input/IInputEventHandler.h"
#include "../Input/JoyMapping.h"
#include "../Application.h"

#ifdef __HAS_LOCAL_SDL
#	include "../../../Libs/SDL2/SDL.h"
#else
#	include <SDL.h>
#endif

#include <cstring>

#ifdef WITH_IMGUI
#	include "SdlGfxDevice.h"
#	include "ImGuiSdlInput.h"
#endif

#ifdef WITH_NUKLEAR
#	include "SdlGfxDevice.h"
#	include "NuklearSdlInput.h"
#endif

namespace nCine
{
	///////////////////////////////////////////////////////////
	// STATIC DEFINITIONS
	///////////////////////////////////////////////////////////

	const int IInputManager::MaxNumJoysticks = 16;

	SDL_Window* SdlInputManager::windowHandle_ = nullptr;

	TouchEvent SdlInputManager::touchEvent_;
	SdlMouseState SdlInputManager::mouseState_;
	SdlMouseEvent SdlInputManager::mouseEvent_;
	SdlScrollEvent SdlInputManager::scrollEvent_;
	SdlKeyboardState SdlInputManager::keyboardState_;
	KeyboardEvent SdlInputManager::keyboardEvent_;
	TextInputEvent SdlInputManager::textInputEvent_;

	SDL_Joystick* SdlInputManager::sdlJoysticks_[SdlInputManager::MaxNumJoysticks];
	SmallVector<SdlJoystickState, SdlInputManager::MaxNumJoysticks> SdlInputManager::joystickStates_(SdlInputManager::MaxNumJoysticks);
	JoyButtonEvent SdlInputManager::joyButtonEvent_;
	JoyHatEvent SdlInputManager::joyHatEvent_;
	JoyAxisEvent SdlInputManager::joyAxisEvent_;
	JoyConnectionEvent SdlInputManager::joyConnectionEvent_;

	char SdlInputManager::joyGuidString_[33];

	///////////////////////////////////////////////////////////
	// CONSTRUCTORS and DESTRUCTOR
	///////////////////////////////////////////////////////////

	SdlInputManager::SdlInputManager()
	{
		const unsigned int ret = SDL_WasInit(SDL_INIT_VIDEO);
		FATAL_ASSERT_MSG(ret != 0, "SDL video subsystem is not initialized");

		// Initializing the joystick subsystem
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		// Enabling joystick event processing
		SDL_JoystickEventState(SDL_ENABLE);

		memset(sdlJoysticks_, 0, sizeof(SDL_Joystick*) * MaxNumJoysticks);

		// Opening attached joysticks
		const int numJoysticks = SDL_NumJoysticks();
		for (int i = 0; i < numJoysticks; i++) {
			sdlJoysticks_[i] = SDL_JoystickOpen(i);
			if (sdlJoysticks_[i]) {
				SDL_Joystick* sdlJoy = sdlJoysticks_[i];
				LOGI_X("Joystick %d \"%s\" - %d hats, %d axes, %d buttons, %d balls",
					   i, SDL_JoystickName(sdlJoy), SDL_JoystickNumHats(sdlJoy), SDL_JoystickNumAxes(sdlJoy), SDL_JoystickNumButtons(sdlJoy), SDL_JoystickNumBalls(sdlJoy));
			}
		}

		joyMapping_.init(this);

#ifdef WITH_IMGUI
		ImGuiSdlInput::init(SdlGfxDevice::windowHandle());
#endif

#ifdef WITH_NUKLEAR
		NuklearSdlInput::init(SdlGfxDevice::windowHandle());
#endif
	}

	SdlInputManager::~SdlInputManager()
	{
#ifdef WITH_NUKLEAR
		NuklearSdlInput::shutdown();
#endif

#ifdef WITH_IMGUI
		ImGuiSdlInput::shutdown();
#endif

		// Close a joystick if opened
		for (unsigned int i = 0; i < MaxNumJoysticks; i++) {
			if (isJoyPresent(i)) {
				SDL_JoystickClose(sdlJoysticks_[i]);
				sdlJoysticks_[i] = nullptr;
			}
		}
	}

	///////////////////////////////////////////////////////////
	// PUBLIC FUNCTIONS
	///////////////////////////////////////////////////////////

	bool SdlJoystickState::isButtonPressed(int buttonId) const
	{
		bool isPressed = false;
		if (sdlJoystick_ != nullptr)
			isPressed = SDL_JoystickGetButton(sdlJoystick_, buttonId) != 0;
		return isPressed;
	}

	unsigned char SdlJoystickState::hatState(int hatId) const
	{
		unsigned char hatState = 0;
		if (sdlJoystick_ != nullptr)
			hatState = SDL_JoystickGetHat(sdlJoystick_, hatId);

		return hatState;
	}

	short int SdlJoystickState::axisValue(int axisId) const
	{
		short int axisValue = 0;
		if (sdlJoystick_ != nullptr)
			axisValue = SDL_JoystickGetAxis(sdlJoystick_, axisId);
		return axisValue;
	}

	float SdlJoystickState::axisNormValue(int axisId) const
	{
		// If the joystick is not present the returned value is zero
		const float value = axisValue(axisId) / float(IInputManager::MaxAxisValue);

		return value;
	}

	bool SdlInputManager::shouldQuitOnRequest()
	{
		bool shouldQuit = true;

		if (inputEventHandler_)
			shouldQuit = inputEventHandler_->onQuitRequest();

		return shouldQuit;
	}

	void SdlInputManager::parseEvent(const SDL_Event& event)
	{
#ifdef WITH_IMGUI
		ImGuiSdlInput::processEvent(&event);
#endif

#ifdef WITH_NUKLEAR
		NuklearSdlInput::processEvent(&event);
#endif

		if (inputEventHandler_ == nullptr)
			return;

		if (event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) {
			handleJoyDeviceEvent(event);
			return;
		}

		// Filling static event structures
		switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keyboardEvent_.scancode = event.key.keysym.scancode;
				keyboardEvent_.sym = SdlKeys::keySymValueToEnum(event.key.keysym.sym);
				keyboardEvent_.mod = SdlKeys::keyModMaskToEnumMask(event.key.keysym.mod);
				break;
			case SDL_TEXTINPUT: {
#if defined(DEATH_TARGET_WINDOWS)
				strncpy_s(textInputEvent_.text, event.text.text, 4);
#else
				size_t textLength = std::min(sizeof(textInputEvent_.text) - 1, strlen(event.text.text));
				strncpy(textInputEvent_.text, event.text.text, textLength);
				textInputEvent_.text[textLength] = '\0';
#endif
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				mouseEvent_.x = event.button.x;
				mouseEvent_.y = theApplication().heightInt() - event.button.y;
				mouseEvent_.button_ = event.button.button;
				break;
			case SDL_MOUSEMOTION:
				if (cursor_ != Cursor::HiddenLocked) {
					mouseState_.x = event.motion.x;
					mouseState_.y = theApplication().heightInt() - event.motion.y;
				} else {
					mouseState_.x += event.motion.xrel;
					mouseState_.y -= event.motion.yrel;
				}
				mouseState_.buttons_ = event.motion.state;
				break;
			case SDL_MOUSEWHEEL:
				scrollEvent_.x = static_cast<float>(event.wheel.x);
				scrollEvent_.y = static_cast<float>(event.wheel.y);
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				joyButtonEvent_.joyId = joyInstanceIdToDeviceIndex(event.jbutton.which);
				joyButtonEvent_.buttonId = event.jbutton.button;
				break;
			case SDL_JOYAXISMOTION:
				joyAxisEvent_.joyId = joyInstanceIdToDeviceIndex(event.jaxis.which);
				joyAxisEvent_.axisId = event.jaxis.axis;
				joyAxisEvent_.value = event.jaxis.value;
				joyAxisEvent_.normValue = joyAxisEvent_.value / float(MaxAxisValue);
				break;
			case SDL_JOYHATMOTION:
				joyHatEvent_.joyId = joyInstanceIdToDeviceIndex(event.jhat.which);
				joyHatEvent_.hatId = event.jhat.hat;
				joyHatEvent_.hatState = event.jhat.value;
				break;
			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
			case SDL_FINGERUP:
				touchEvent_.count = SDL_GetNumTouchFingers(event.tfinger.touchId);
				touchEvent_.actionIndex = event.tfinger.fingerId;

				switch (event.type) {
					case SDL_FINGERDOWN: touchEvent_.type = (touchEvent_.count == 1 ? TouchEventType::Down : TouchEventType::PointerDown); break;
					case SDL_FINGERMOTION: touchEvent_.type = TouchEventType::Move; break;
					case SDL_FINGERUP: touchEvent_.type = (touchEvent_.count == 0 ? TouchEventType::Up : TouchEventType::PointerUp); break;
				}

				for (unsigned int i = 0; i < touchEvent_.count; i++) {
					SDL_Finger* finger = SDL_GetTouchFinger(event.tfinger.touchId, i);
					TouchEvent::Pointer& pointer = touchEvent_.pointers[i];
					pointer.id = static_cast<int>(finger->id);
					pointer.x = finger->x;
					pointer.y = finger->y;
					pointer.pressure = finger->pressure;
				}
				break;
			default:
				break;
		}

		// Calling the event handler method
		switch (event.type) {
			case SDL_KEYDOWN:
				inputEventHandler_->onKeyPressed(keyboardEvent_);
				break;
			case SDL_KEYUP:
				inputEventHandler_->onKeyReleased(keyboardEvent_);
				break;
			case SDL_TEXTINPUT:
				inputEventHandler_->onTextInput(textInputEvent_);
				break;
			case SDL_MOUSEBUTTONDOWN:
				inputEventHandler_->onMouseButtonPressed(mouseEvent_);
				break;
			case SDL_MOUSEBUTTONUP:
				inputEventHandler_->onMouseButtonReleased(mouseEvent_);
				break;
			case SDL_MOUSEMOTION:
				inputEventHandler_->onMouseMoved(mouseState_);
				break;
			case SDL_MOUSEWHEEL:
				inputEventHandler_->onScrollInput(scrollEvent_);
				break;
			case SDL_JOYBUTTONDOWN:
				joyMapping_.onJoyButtonPressed(joyButtonEvent_);
				inputEventHandler_->onJoyButtonPressed(joyButtonEvent_);
				break;
			case SDL_JOYBUTTONUP:
				joyMapping_.onJoyButtonReleased(joyButtonEvent_);
				inputEventHandler_->onJoyButtonReleased(joyButtonEvent_);
				break;
			case SDL_JOYAXISMOTION:
				joyMapping_.onJoyAxisMoved(joyAxisEvent_);
				inputEventHandler_->onJoyAxisMoved(joyAxisEvent_);
				break;
			case SDL_JOYHATMOTION:
				joyMapping_.onJoyHatMoved(joyHatEvent_);
				inputEventHandler_->onJoyHatMoved(joyHatEvent_);
				break;
			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
			case SDL_FINGERUP:
				inputEventHandler_->onTouchEvent(touchEvent_);
				break;
			default:
				break;
		}
	}

	bool SdlInputManager::isJoyPresent(int joyId) const
	{
		ASSERT(joyId >= 0);
		ASSERT_MSG_X(joyId < int(MaxNumJoysticks), "joyId is %d and the maximum is %u", joyId, MaxNumJoysticks - 1);

		return (sdlJoysticks_[joyId] && SDL_JoystickGetAttached(sdlJoysticks_[joyId]));
	}

	const char* SdlInputManager::joyName(int joyId) const
	{
		if (isJoyPresent(joyId)) {
			return SDL_JoystickName(sdlJoysticks_[joyId]);
		} else {
			return nullptr;
		}
	}

	const JoystickGuid SdlInputManager::joyGuid(int joyId) const
	{
		if (isJoyPresent(joyId)) {
#if defined(DEATH_TARGET_EMSCRIPTEN)
			return JoystickGuidType::Default;
#else
			const SDL_JoystickGUID joystickGuid = SDL_JoystickGetGUID(sdlJoysticks_[joyId]);
			SDL_JoystickGetGUIDString(joystickGuid, joyGuidString_, 33);
			return StringView(joyGuidString_);
#endif
		} else {
			return JoystickGuidType::Unknown;
		}
	}

	int SdlInputManager::joyNumButtons(int joyId) const
	{
		int numButtons = -1;

		if (isJoyPresent(joyId))
			numButtons = SDL_JoystickNumButtons(sdlJoysticks_[joyId]);

		return numButtons;
	}

	int SdlInputManager::joyNumHats(int joyId) const
	{
		int numHats = -1;

		if (isJoyPresent(joyId))
			numHats = SDL_JoystickNumHats(sdlJoysticks_[joyId]);

		return numHats;
	}

	int SdlInputManager::joyNumAxes(int joyId) const
	{
		int numAxes = -1;

		if (isJoyPresent(joyId))
			numAxes = SDL_JoystickNumAxes(sdlJoysticks_[joyId]);

		return numAxes;
	}

	const JoystickState& SdlInputManager::joystickState(int joyId) const
	{
		joystickStates_[joyId].sdlJoystick_ = nullptr;

		if (isJoyPresent(joyId))
			joystickStates_[joyId].sdlJoystick_ = sdlJoysticks_[joyId];

		return joystickStates_[joyId];
	}

	void SdlInputManager::setCursor(Cursor cursor)
	{
		if (cursor != cursor_) {
			bool changeMode = true;
			switch (cursor) {
				case Cursor::Arrow:
					SDL_ShowCursor(SDL_ENABLE);
					SDL_SetRelativeMouseMode(SDL_FALSE);
					break;
				case Cursor::Hidden:
					SDL_ShowCursor(SDL_DISABLE);
					SDL_SetRelativeMouseMode(SDL_FALSE);
					break;
				case Cursor::HiddenLocked:
					const int supported = SDL_SetRelativeMouseMode(SDL_TRUE);
					changeMode = (supported == 0);
					break;
			}

			if (changeMode) {
				// Handling ImGui cursor changes
				IInputManager::setCursor(cursor);

				cursor_ = cursor;
			}
		}
	}

	///////////////////////////////////////////////////////////
	// PRIVATE FUNCTIONS
	//////////////////////////////////////////////////////////

	void SdlInputManager::handleJoyDeviceEvent(const SDL_Event& event)
	{
		if (event.type == SDL_JOYDEVICEADDED) {
			const int deviceIndex = event.jdevice.which;
			joyConnectionEvent_.joyId = deviceIndex;
			sdlJoysticks_[deviceIndex] = SDL_JoystickOpen(deviceIndex);

			SDL_Joystick* joy = sdlJoysticks_[deviceIndex];

#if defined(ENABLE_LOG) && !defined(DEATH_TARGET_EMSCRIPTEN)
			const SDL_JoystickGUID joystickGuid = SDL_JoystickGetGUID(joy);
			SDL_JoystickGetGUIDString(joystickGuid, joyGuidString_, 33);
			LOGI_X("Joystick %d \"%s\" (%s) has been connected - %d hats, %d axes, %d buttons, %d balls",
				   deviceIndex, SDL_JoystickName(joy), joyGuidString_, SDL_JoystickNumHats(joy), SDL_JoystickNumAxes(joy), SDL_JoystickNumButtons(joy), SDL_JoystickNumBalls(joy));
#endif
			joyMapping_.onJoyConnected(joyConnectionEvent_);
			inputEventHandler_->onJoyConnected(joyConnectionEvent_);
		} else if (event.type == SDL_JOYDEVICEREMOVED) {
			const int deviceIndex = joyInstanceIdToDeviceIndex(event.jdevice.which);
			if (deviceIndex == -1)
				return;

			joyConnectionEvent_.joyId = deviceIndex;
			SDL_JoystickClose(sdlJoysticks_[deviceIndex]);
			sdlJoysticks_[deviceIndex] = nullptr;

			// Compacting the array of SDL joystick pointers
			for (int i = deviceIndex; i < MaxNumJoysticks - 1; i++)
				sdlJoysticks_[i] = sdlJoysticks_[i + 1];
			sdlJoysticks_[MaxNumJoysticks - 1] = nullptr;

			LOGI_X("Joystick %d has been disconnected", deviceIndex);
			inputEventHandler_->onJoyDisconnected(joyConnectionEvent_);
			joyMapping_.onJoyDisconnected(joyConnectionEvent_);
		}
	}

	int SdlInputManager::joyInstanceIdToDeviceIndex(SDL_JoystickID instanceId)
	{
		int deviceIndex = -1;
		for (int i = 0; i < MaxNumJoysticks; i++) {
			SDL_JoystickID id = SDL_JoystickInstanceID(sdlJoysticks_[i]);
			if (instanceId == id) {
				deviceIndex = i;
				break;
			}
		}
		return deviceIndex;
	}
}

#endif