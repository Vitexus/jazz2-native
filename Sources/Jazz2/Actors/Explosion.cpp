﻿#include "Explosion.h"
#include "../ILevelHandler.h"

#include "../../nCine/Base/Random.h"

namespace Jazz2::Actors
{
	Explosion::Explosion()
		:
		_lightBrightness(0.0f),
		_lightIntensity(0.0f),
		_lightRadiusNear(0.0f),
		_lightRadiusFar(0.0f)
	{
	}

	void Explosion::Create(ILevelHandler* levelHandler, const Vector3i& pos, Type type)
	{
		std::shared_ptr<Explosion> explosion = std::make_shared<Explosion>();
		uint8_t explosionParams[2];
		*(uint16_t*)&explosionParams[0] = (uint16_t)type;
		explosion->OnActivated({
			.LevelHandler = levelHandler,
			.Pos = pos,
			.Params = explosionParams
		});
		levelHandler->AddActor(explosion);
	}

	Task<bool> Explosion::OnActivatedAsync(const ActorActivationDetails& details)
	{
		_type = (Type)*(uint16_t*)&details.Params[0];

		SetState(ActorState::ForceDisableCollisions, true);
		SetState(ActorState::CanBeFrozen | ActorState::CollideWithTileset | ActorState::CollideWithOtherActors | ActorState::ApplyGravitation, false);

		async_await RequestMetadataAsync("Common/Explosions"_s);

		switch (_type) {
			default:
			case Type::Tiny: SetAnimation("Tiny"_s); break;
			case Type::TinyBlue: SetAnimation("TinyBlue"_s); break;
			case Type::TinyDark: SetAnimation("TinyDark"_s); break;
			case Type::Small: SetAnimation("Small"_s); break;
			case Type::SmallDark: SetAnimation("SmallDark"_s); break;
			case Type::Large: {
				SetAnimation("Large"_s);

				_lightIntensity = 0.8f;
				_lightBrightness = 0.9f;
				_lightRadiusFar = 55.0f;
				break;
			}

			case Type::SmokeBrown: SetAnimation("SmokeBrown"_s); break;
			case Type::SmokeGray: SetAnimation("SmokeGray"_s); break;
			case Type::SmokeWhite: SetAnimation("SmokeWhite"_s); break;
			case Type::SmokePoof: SetAnimation("SmokePoof"_s); break;

			case Type::WaterSplash: SetAnimation("WaterSplash"_s); break;

			case Type::Pepper: {
				SetAnimation("Pepper"_s);

				_lightIntensity = 0.5f;
				_lightBrightness = 0.2f;
				_lightRadiusNear = 7.0f;
				_lightRadiusFar = 14.0f;
				break;
			}
			case Type::RF: {
				SetAnimation("RF"_s);

				_lightIntensity = 0.8f;
				_lightBrightness = 0.9f;
				_lightRadiusFar = 50.0f;
				break;
			}

			case Type::Generator: {
				SetAnimation("Generator"_s);

				// Apply random orientation
				_renderer.setRotation(Random().NextFloat(0.0f, 4.0f * fPiOver2));
				SetFacingLeft(Random().NextFloat() < 0.5f);
				break;
			}
		}

		async_return true;
	}

	void Explosion::OnUpdate(float timeMult)
	{
		switch (_type) {
			case Type::Large: {
				_lightRadiusFar -= timeMult * 5.0f;
				break;
			}
			case Type::Pepper: {
				_lightIntensity -= timeMult * 0.05f;
				break;
			}
			case Type::RF: {
				_lightRadiusFar -= timeMult * 0.8f;
				_lightIntensity -= timeMult * 0.02f;
				break;
			}
		}
	}

	void Explosion::OnEmitLights(SmallVectorImpl<LightEmitter>& lights)
	{
		if (_lightRadiusFar > 0.0f) {
			auto& light1 = lights.emplace_back();
			light1.Pos = _pos;
			light1.Intensity = _lightIntensity;
			light1.Brightness = _lightBrightness;
			light1.RadiusNear = _lightRadiusNear;
			light1.RadiusFar = _lightRadiusFar;
		}

		if (_type == Type::Pepper) {
			auto& light2 = lights.emplace_back();
			light2.Pos = _pos;
			light2.Intensity = 0.1f;
			light2.RadiusNear = 0.0f;
			light2.RadiusFar = 160.0f;
		}
	}

	void Explosion::OnAnimationFinished()
	{
		DecreaseHealth(INT32_MAX);
	}
}