﻿#include "InGameMenu.h"
#include "PauseSection.h"
#include "../ControlScheme.h"
#include "../../PreferencesCache.h"
#include "../../LevelHandler.h"

#include "../../../nCine/Application.h"
#include "../../../nCine/Graphics/RenderQueue.h"
#include "../../../nCine/Graphics/Viewport.h"
#include "../../../nCine/Input/IInputManager.h"
#include "../../../nCine/Audio/AudioReaderMpt.h"
#include "../../../nCine/Base/Random.h"

namespace Jazz2::UI::Menu
{
	InGameMenu::InGameMenu(LevelHandler* root)
		:
		_root(root),
		_pressedActions(0),
		_touchButtonsTimer(0.0f)
	{
		_canvas = std::make_unique<MenuCanvas>(this);
		_canvas->setParent(root->_upscalePass.GetNode());

		auto& resolver = ContentResolver::Current();

		Metadata* metadata = resolver.RequestMetadata("UI/MainMenu"_s);
		if (metadata != nullptr) {
			_graphics = &metadata->Graphics;
			_sounds = &metadata->Sounds;
		}

		_smallFont = resolver.GetFont(FontType::Small);
		_mediumFont = resolver.GetFont(FontType::Medium);

		// Mark Menu button as already pressed to avoid some issues
		_pressedActions = (1 << (int)PlayerActions::Menu) | (1 << ((int)PlayerActions::Menu + 16));

		SwitchToSection<PauseSection>();

		PlaySfx("MenuSelect"_s, 0.5f);
	}

	InGameMenu::~InGameMenu()
	{
		_canvas->setParent(nullptr);
	}

	void InGameMenu::MenuCanvas::OnUpdate(float timeMult)
	{
		Canvas::OnUpdate(timeMult);

		_owner->UpdatePressedActions();

		// Destroy stopped players
		for (int i = (int)_owner->_playingSounds.size() - 1; i >= 0; i--) {
			if (_owner->_playingSounds[i]->state() == IAudioPlayer::PlayerState::Stopped) {
				_owner->_playingSounds.erase(&_owner->_playingSounds[i]);
			} else {
				break;
			}
		}

		if (_owner->_touchButtonsTimer > 0.0f) {
			_owner->_touchButtonsTimer -= timeMult;
		}

		if (!_owner->_sections.empty()) {
			auto& lastSection = _owner->_sections.back();
			lastSection->OnUpdate(timeMult);
		}
	}

	void InGameMenu::OnTouchEvent(const nCine::TouchEvent& event)
	{
		if (!_sections.empty()) {
			_touchButtonsTimer = 1200.0f;

			auto& lastSection = _sections.back();
			lastSection->OnTouchEvent(event, _canvas->ViewSize);
		}
	}

	bool InGameMenu::MenuCanvas::OnDraw(RenderQueue& renderQueue)
	{
		Canvas::OnDraw(renderQueue);

		ViewSize = _owner->_root->_upscalePass.GetViewSize();

		Vector2i center = ViewSize / 2;

		int charOffset = 0;
		int charOffsetShadow = 0;

		constexpr float logoScale = 1.0f;
		constexpr float logoTextScale = 1.0f;
		constexpr float logoTranslateX = 1.0f;
		constexpr float logoTranslateY = 0.0f;
		constexpr float logoTextTranslate = 0.0f;

		// Show blurred viewport behind
		DrawTexture(*_owner->_root->_blurPass4.GetTarget(), Vector2f::Zero, 500, Vector2f(static_cast<float>(ViewSize.X), static_cast<float>(ViewSize.Y)), Vector4f(1.0f, 0.0f, 1.0f, 0.0f), Colorf(0.5f, 0.5f, 0.5f, std::min(AnimTime * 8.0f, 1.0f)));
		Vector4f ambientColor = _owner->_root->_ambientColor;
		if (ambientColor.W < 1.0f) {
			DrawSolid(Vector2f::Zero, 502, Vector2f(static_cast<float>(ViewSize.X), static_cast<float>(ViewSize.Y)), Colorf(ambientColor.X, ambientColor.Y, ambientColor.Z, (1.0f - ambientColor.W) * std::min(AnimTime * 8.0f, 1.0f)));
		}

		if (_owner->_touchButtonsTimer > 0.0f && _owner->_sections.size() >= 2) {
			_owner->DrawElement("MenuLineArrow"_s, -1, static_cast<float>(center.X), 40.0f, ShadowLayer, Alignment::Center, Colorf::White);
		}

		// Title
		_owner->DrawElement("MenuCarrot"_s, -1, center.X - 76.0f * logoTranslateX, 64.0f + logoTranslateY + 2.0f, ShadowLayer + 200, Alignment::Center, Colorf(0.0f, 0.0f, 0.0f, 0.3f), 0.8f * logoScale, 0.8f * logoScale);
		_owner->DrawElement("MenuCarrot"_s, -1, center.X - 76.0f * logoTranslateX, 64.0f + logoTranslateY, MainLayer + 200, Alignment::Center, Colorf::White, 0.8f * logoScale, 0.8f * logoScale);

		_owner->_mediumFont->DrawString(this, "Jazz"_s, charOffsetShadow, center.X - 63.0f, 70.0f + logoTranslateY + 2.0f, FontShadowLayer + 200,
			Alignment::Left, Colorf(0.0f, 0.0f, 0.0f, 0.32f), 0.75f * logoTextScale, 1.65f, 3.0f, 3.0f, 0.0f, 0.92f);
		_owner->_mediumFont->DrawString(this, "2"_s, charOffsetShadow, center.X - 19.0f, 70.0f - 8.0f + logoTranslateY + 2.0f, FontShadowLayer + 200,
			Alignment::Left, Colorf(0.0f, 0.0f, 0.0f, 0.32f), 0.5f * logoTextScale, 0.0f, 0.0f, 0.0f, 0.0f);
		_owner->_mediumFont->DrawString(this, "Resurrection"_s, charOffsetShadow, center.X - 10.0f, 70.0f + 4.0f + logoTranslateY + 2.5f, FontShadowLayer + 200,
			Alignment::Left, Colorf(0.0f, 0.0f, 0.0f, 0.3f), 0.5f * logoTextScale, 0.4f, 1.2f, 1.2f, 0.46f, 0.8f);

		_owner->_mediumFont->DrawString(this, "Jazz"_s, charOffset, center.X - 63.0f * logoTranslateX + logoTextTranslate, 70.0f + logoTranslateY, FontLayer + 200,
			Alignment::Left, Colorf(0.54f, 0.44f, 0.34f, 0.5f), 0.75f * logoTextScale, 1.65f, 3.0f, 3.0f, 0.0f, 0.92f);
		_owner->_mediumFont->DrawString(this, "2"_s, charOffset, center.X - 19.0f * logoTranslateX + logoTextTranslate, 70.0f - 8.0f + logoTranslateY, FontLayer + 200,
			Alignment::Left, Colorf(0.54f, 0.44f, 0.34f, 0.5f), 0.5f * logoTextScale, 0.0f, 0.0f, 0.0f, 0.0f);
		_owner->_mediumFont->DrawString(this, "Resurrection"_s, charOffset, center.X - 10.0f * logoTranslateX + logoTextTranslate, 70.0f + 4.0f + logoTranslateY, FontLayer + 200,
			Alignment::Left, Colorf(0.6f, 0.42f, 0.42f, 0.5f), 0.5f * logoTextScale, 0.4f, 1.2f, 1.2f, 0.46f, 0.8f);

		// Version
		Vector2f bottomRight = Vector2f(static_cast<float>(ViewSize.X), static_cast<float>(ViewSize.Y));
		bottomRight.X = ViewSize.X - 24.0f;
		bottomRight.Y -= 10.0f;
		_owner->DrawStringShadow("v" NCINE_VERSION, charOffset, bottomRight.X, bottomRight.Y, IMenuContainer::FontLayer,
			Alignment::BottomRight, Colorf(0.45f, 0.45f, 0.45f, 0.5f), 0.7f, 0.4f, 1.2f, 1.2f, 0.46f, 0.8f);

		// Copyright
		Vector2f bottomLeft = bottomRight;
		bottomLeft.X = 24.0f;
		_owner->DrawStringShadow("© 2016-2022  Dan R."_s, charOffset, bottomLeft.X, bottomLeft.Y, IMenuContainer::FontLayer,
			Alignment::BottomLeft, Colorf(0.45f, 0.45f, 0.45f, 0.5f), 0.7f, 0.4f, 1.2f, 1.2f, 0.46f, 0.8f);

		if (!_owner->_sections.empty()) {
			auto& lastSection = _owner->_sections.back();
			lastSection->OnDraw(this);
		}

		return true;
	}

	void InGameMenu::SwitchToSectionPtr(std::unique_ptr<MenuSection> section)
	{
		if (!_sections.empty()) {
			auto& lastSection = _sections.back();
			lastSection->OnHide();
		}

		auto& currentSection = _sections.emplace_back(std::move(section));
		currentSection->OnShow(this);
	}

	void InGameMenu::LeaveSection()
	{
		if (_sections.empty()) {
			return;
		}

		_sections.pop_back();

		if (!_sections.empty()) {
			auto& lastSection = _sections.back();
			lastSection->OnShow(this);
		}
	}

	void InGameMenu::ChangeLevel(Jazz2::LevelInitialization&& levelInit)
	{
		_root->_root->ChangeLevel(std::move(levelInit));
	}

	void InGameMenu::ApplyPreferencesChanges(ChangedPreferencesType type)
	{
		if ((type & ChangedPreferencesType::Graphics) == ChangedPreferencesType::Graphics) {
			Viewport::chain().clear();
			Vector2i res = theApplication().resolutionInt();
			_root->OnInitializeViewport(res.X, res.Y);
		}

		if ((type & ChangedPreferencesType::Audio) == ChangedPreferencesType::Audio) {
			if (_root->_music != nullptr) {
				_root->_music->setGain(PreferencesCache::MasterVolume * PreferencesCache::MusicVolume);
			}
			if (_root->_sugarRushMusic != nullptr) {
				_root->_sugarRushMusic->setGain(PreferencesCache::MasterVolume * PreferencesCache::MusicVolume);
			}
		}
	}

	void InGameMenu::DrawElement(const StringView& name, int frame, float x, float y, uint16_t z, Alignment align, const Colorf& color, float scaleX, float scaleY, bool additiveBlending)
	{
		auto it = _graphics->find(String::nullTerminatedView(name));
		if (it == _graphics->end()) {
			return;
		}

		if (frame < 0) {
			frame = it->second.FrameOffset + ((int)(_canvas->AnimTime * it->second.FrameCount / it->second.AnimDuration) % it->second.FrameCount);
		}

		GenericGraphicResource* base = it->second.Base;
		Vector2f size = Vector2f(base->FrameDimensions.X * scaleX, base->FrameDimensions.Y * scaleY);
		Vector2f adjustedPos = Canvas::ApplyAlignment(align, Vector2f(x - _canvas->ViewSize.X * 0.5f, _canvas->ViewSize.Y * 0.5f - y), size);

		Vector2i texSize = base->TextureDiffuse->size();
		int col = frame % base->FrameConfiguration.X;
		int row = frame / base->FrameConfiguration.X;
		Vector4f texCoords = Vector4f(
			float(base->FrameDimensions.X) / float(texSize.X),
			float(base->FrameDimensions.X * col) / float(texSize.X),
			float(base->FrameDimensions.Y) / float(texSize.Y),
			float(base->FrameDimensions.Y * row) / float(texSize.Y)
		);

		texCoords.W += texCoords.Z;
		texCoords.Z *= -1;

		_canvas->DrawTexture(*base->TextureDiffuse.get(), adjustedPos, z, size, texCoords, color, additiveBlending);
	}

	void InGameMenu::DrawElement(const StringView& name, float x, float y, uint16_t z, Alignment align, const Colorf& color, const Vector2f& size, const Vector4f& texCoords)
	{
		auto it = _graphics->find(String::nullTerminatedView(name));
		if (it == _graphics->end()) {
			return;
		}

		GenericGraphicResource* base = it->second.Base;
		Vector2f adjustedPos = Canvas::ApplyAlignment(align, Vector2f(x - _canvas->ViewSize.X * 0.5f, _canvas->ViewSize.Y * 0.5f - y), size);

		_canvas->DrawTexture(*base->TextureDiffuse.get(), adjustedPos, z, size, texCoords, color, false);
	}

	void InGameMenu::DrawSolid(float x, float y, uint16_t z, Alignment align, const Vector2f& size, const Colorf& color, bool additiveBlending)
	{
		Vector2f adjustedPos = Canvas::ApplyAlignment(align, Vector2f(x - _canvas->ViewSize.X * 0.5f, _canvas->ViewSize.Y * 0.5f - y), size);
		_canvas->DrawSolid(adjustedPos, z, size, color, additiveBlending);
	}

	void InGameMenu::DrawStringShadow(const StringView& text, int& charOffset, float x, float y, uint16_t z, Alignment align, const Colorf& color, float scale,
		float angleOffset, float varianceX, float varianceY, float speed, float charSpacing, float lineSpacing)
	{
		int charOffsetShadow = charOffset;
		_smallFont->DrawString(_canvas.get(), text, charOffsetShadow, x, y + 2.8f * scale, FontShadowLayer,
			align, Colorf(0.0f, 0.0f, 0.0f, (color == Font::DefaultColor ? 0.29f : 0.29f * 2.0f * color.A())), scale, angleOffset, varianceX, varianceY, speed, charSpacing, lineSpacing);
		_smallFont->DrawString(_canvas.get(), text, charOffset, x, y, z,
			align, color, scale, angleOffset, varianceX, varianceY, speed, charSpacing, lineSpacing);
	}

	void InGameMenu::PlaySfx(const StringView& identifier, float gain)
	{
		auto it = _sounds->find(String::nullTerminatedView(identifier));
		if (it != _sounds->end()) {
			int idx = (it->second.Buffers.size() > 1 ? Random().Next(0, (int)it->second.Buffers.size()) : 0);
			auto& player = _playingSounds.emplace_back(std::make_shared<AudioBufferPlayer>(it->second.Buffers[idx].get()));
			player->setPosition(Vector3f(0.0f, 0.0f, 100.0f));
			player->setGain(gain * PreferencesCache::MasterVolume * PreferencesCache::SfxVolume);
			player->setSourceRelative(true);

			player->play();
		} else {
			LOGE_X("Sound effect \"%s\" was not found", identifier.data());
		}
	}

	void InGameMenu::ResumeGame()
	{
		_root->ResumeGame();
	}

	void InGameMenu::GoToMainMenu()
	{
		_root->_root->GoToMainMenu(false);
	}

	bool InGameMenu::ActionHit(PlayerActions action)
	{
		return ((_pressedActions & ((1 << (int)action) | (1 << (16 + (int)action)))) == (1 << (int)action));
	}

	void InGameMenu::UpdatePressedActions()
	{
		auto& input = theApplication().inputManager();
		auto& pressedKeys = _root->_pressedKeys;

		_pressedActions = ((_pressedActions & 0xffff) << 16);

		if (pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Up)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Up)]) {
			_pressedActions |= (1 << (int)PlayerActions::Up);
		}
		if (pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Down)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Down)]) {
			_pressedActions |= (1 << (int)PlayerActions::Down);
		}
		if (pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Left)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Left)]) {
			_pressedActions |= (1 << (int)PlayerActions::Left);
		}
		if (pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Right)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Right)]) {
			_pressedActions |= (1 << (int)PlayerActions::Right);
		}
		// Also allow Return (Enter) as confirm key
		if (pressedKeys[(uint32_t)KeySym::RETURN] || pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Fire)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Fire)] ||
			pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Jump)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Jump)]) {
			_pressedActions |= (1 << (int)PlayerActions::Fire);
		}
		// Allow Android Back button as menu key
		if (pressedKeys[(uint32_t)ControlScheme::Key1(0, PlayerActions::Menu)] || pressedKeys[(uint32_t)ControlScheme::Key2(0, PlayerActions::Menu)] || (PreferencesCache::UseNativeBackButton && pressedKeys[(uint32_t)KeySym::BACK])) {
			_pressedActions |= (1 << (int)PlayerActions::Menu);
		}
		// Use SwitchWeapon action as Delete key
		if (pressedKeys[(uint32_t)KeySym::DELETE]) {
			_pressedActions |= (1 << (int)PlayerActions::ChangeWeapon);
		}

		// Try to get 8 connected joysticks
		const JoyMappedState* joyStates[8];
		int jc = 0;
		for (int i = 0; i < IInputManager::MaxNumJoysticks && jc < _countof(joyStates); i++) {
			if (input.isJoyPresent(i) && input.isJoyMapped(i)) {
				joyStates[jc++] = &input.joyMappedState(i);
			}
		}

		ButtonName jb; int ji1, ji2, ji3, ji4;

		jb = ControlScheme::Gamepad(0, PlayerActions::Up, ji1);
		if (ji1 >= 0 && ji1 < jc && joyStates[ji1]->isButtonPressed(jb)) {
			_pressedActions |= (1 << (int)PlayerActions::Up);
		}
		jb = ControlScheme::Gamepad(0, PlayerActions::Down, ji2);
		if (ji2 >= 0 && ji2 < jc && joyStates[ji2]->isButtonPressed(jb)) {
			_pressedActions |= (1 << (int)PlayerActions::Down);
		}
		jb = ControlScheme::Gamepad(0, PlayerActions::Left, ji3);
		if (ji3 >= 0 && ji3 < jc && joyStates[ji3]->isButtonPressed(jb)) {
			_pressedActions |= (1 << (int)PlayerActions::Left);
		}
		jb = ControlScheme::Gamepad(0, PlayerActions::Right, ji4);
		if (ji4 >= 0 && ji4 < jc && joyStates[ji4]->isButtonPressed(jb)) {
			_pressedActions |= (1 << (int)PlayerActions::Right);
		}

		// Use analog controls only if all movement buttons are mapped to the same joystick
		if (ji1 == ji2 && ji2 == ji3 && ji3 == ji4 && ji1 >= 0 && ji1 < jc) {
			float x = joyStates[ji1]->axisValue(AxisName::LX);
			float y = joyStates[ji1]->axisValue(AxisName::LY);

			if (x < -0.6f) {
				_pressedActions |= (1 << (int)PlayerActions::Left);
			} else if (x > 0.6f) {
				_pressedActions |= (1 << (int)PlayerActions::Right);
			}
			if (y < -0.6f) {
				_pressedActions |= (1 << (int)PlayerActions::Up);
			} else if (y > 0.6f) {
				_pressedActions |= (1 << (int)PlayerActions::Down);
			}
		}

		jb = ControlScheme::Gamepad(0, PlayerActions::Jump, ji1);
		jb = ControlScheme::Gamepad(0, PlayerActions::Fire, ji2);
		if (ji1 == ji2) ji2 = -1;

		if ((ji1 >= 0 && ji1 < jc && (joyStates[ji1]->isButtonPressed(ButtonName::A) || joyStates[ji1]->isButtonPressed(ButtonName::X))) ||
			(ji2 >= 0 && ji2 < jc && (joyStates[ji2]->isButtonPressed(ButtonName::A) || joyStates[ji2]->isButtonPressed(ButtonName::X)))) {
			_pressedActions |= (1 << (int)PlayerActions::Fire);
		}

		if ((ji1 >= 0 && ji1 < jc && (joyStates[ji1]->isButtonPressed(ButtonName::B) || joyStates[ji1]->isButtonPressed(ButtonName::START))) ||
			(ji2 >= 0 && ji2 < jc && (joyStates[ji2]->isButtonPressed(ButtonName::B) || joyStates[ji2]->isButtonPressed(ButtonName::START)))) {
			_pressedActions |= (1 << (int)PlayerActions::Menu);
		}

		jb = ControlScheme::Gamepad(0, PlayerActions::ChangeWeapon, ji1);
		if (ji1 >= 0 && ji1 < jc && joyStates[ji1]->isButtonPressed(ButtonName::Y)) {
			_pressedActions |= (1 << (int)PlayerActions::ChangeWeapon);
		}
	}
}