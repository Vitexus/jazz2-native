﻿#if defined(WITH_ANGELSCRIPT)

#include "LevelScripts.h"
#include "RegisterArray.h"
#include "RegisterString.h"
#include "ScriptActorWrapper.h"

#include "../LevelHandler.h"
#include "../PreferencesCache.h"
#include "../Actors/ActorBase.h"

#include "../../nCine/Base/Random.h"

#if defined(DEATH_TARGET_WINDOWS) && !defined(CMAKE_BUILD)
#   if defined(_M_X64)
#		if defined(_DEBUG)
#			pragma comment(lib, "../Libs/x64/angelscriptd.lib")
#		else
#			pragma comment(lib, "../Libs/x64/angelscript.lib")
#		endif
#   elif defined(_M_IX86)
#		if defined(_DEBUG)
#			pragma comment(lib, "../Libs/x86/angelscriptd.lib")
#		else
#			pragma comment(lib, "../Libs/x86/angelscript.lib")
#		endif
#   else
#       error Unsupported architecture
#   endif
#endif

#if !defined(DEATH_TARGET_ANDROID) && !defined(_WIN32_WCE) && !defined(__psp2__)
#	include <locale.h>		// setlocale()
#endif

// Without namespace for shorter log messages
static void asScript(String& msg)
{
	LOGI_X("%s", msg.data());
}

static float asFractionf(float v)
{
	float intPart;
	return modff(v, &intPart);
}

static int asRandom()
{
	return Random().Next();
}

static int asRandom(int max)
{
	return Random().Fast(0, max);
}

static float asRandom(float min, float max)
{
	return Random().FastFloat(min, max);
}

namespace Jazz2::Scripting
{
	LevelScripts::LevelScripts(LevelHandler* levelHandler, const StringView& scriptPath)
		:
		_levelHandler(levelHandler),
		_module(nullptr)
	{
		_engine = asCreateScriptEngine();
		_engine->SetUserData(this, EngineToOwner);
		_engine->SetContextCallbacks(RequestContextCallback, ReturnContextCallback, this);

		int r;
		r = _engine->SetMessageCallback(asMETHOD(LevelScripts, Message), this, asCALL_THISCALL); RETURN_ASSERT(r >= 0);

		_module = _engine->GetModule("Main", asGM_ALWAYS_CREATE); RETURN_ASSERT(_module != nullptr);

		// Built-in types
		RegisterArray(_engine);
		RegisterString(_engine);
		
		// Math functions
		r = _engine->RegisterGlobalFunction("float cos(float)", asFUNCTIONPR(cosf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float sin(float)", asFUNCTIONPR(sinf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float tan(float)", asFUNCTIONPR(tanf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("float acos(float)", asFUNCTIONPR(acosf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float asin(float)", asFUNCTIONPR(asinf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float atan(float)", asFUNCTIONPR(atanf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float atan2(float,float)", asFUNCTIONPR(atan2f, (float, float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("float cosh(float)", asFUNCTIONPR(coshf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float sinh(float)", asFUNCTIONPR(sinhf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float tanh(float)", asFUNCTIONPR(tanhf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("float log(float)", asFUNCTIONPR(logf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float log10(float)", asFUNCTIONPR(log10f, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("float pow(float, float)", asFUNCTIONPR(powf, (float, float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float sqrt(float)", asFUNCTIONPR(sqrtf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("float ceil(float)", asFUNCTIONPR(ceilf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float abs(float)", asFUNCTIONPR(fabsf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float floor(float)", asFUNCTIONPR(floorf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float fraction(float)", asFUNCTIONPR(asFractionf, (float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("int Random()", asFUNCTIONPR(asRandom, (), int), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("int Random(int)", asFUNCTIONPR(asRandom, (int), int), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float Random(float, float)", asFUNCTIONPR(asRandom, (float, float), float), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		// Game-specific functions
		r = _engine->RegisterGlobalFunction("void Print(const string &in)", asFUNCTION(asScript), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("uint8 get_Difficulty() property", asFUNCTION(asGetDifficulty), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("int get_LevelWidth() property", asFUNCTION(asGetLevelWidth), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("int get_LevelHeight() property", asFUNCTION(asGetLevelHeight), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float get_ElapsedFrames() property", asFUNCTION(asGetElapsedFrames), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float get_AmbientLight() property", asFUNCTION(asGetAmbientLight), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void set_AmbientLight(float) property", asFUNCTION(asSetAmbientLight), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("float get_WaterLevel() property", asFUNCTION(asGetWaterLevel), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void set_WaterLevel(float) property", asFUNCTION(asSetWaterLevel), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("void PreloadMetadata(const string &in)", asFUNCTION(asPreloadMetadata), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void RegisterSpawnable(int, const string &in)", asFUNCTION(asRegisterSpawnable), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void Spawn(int, int, int)", asFUNCTION(asSpawnEvent), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void Spawn(int, int, int, const array<uint8> &in)", asFUNCTION(asSpawnEventParams), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void Spawn(const string &in, int, int)", asFUNCTION(asSpawnType), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void Spawn(const string &in, int, int, const array<uint8> &in)", asFUNCTION(asSpawnTypeParams), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("void MusicPlay(const string &in)", asFUNCTION(asMusicPlay), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void ShowLevelText(const string &in)", asFUNCTION(asShowLevelText), asCALL_CDECL); RETURN_ASSERT(r >= 0);
		r = _engine->RegisterGlobalFunction("void SetWeather(uint8, uint8)", asFUNCTION(asSetWeather), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		// Game-specific definitions
		constexpr char AsLibrary[] = R"(
enum GameDifficulty {
	Default,
	Easy,
	Normal,
	Hard
}

enum WeatherType {
	None,

	Snow,
	Flowers,
	Rain,
	Leaf,

	OutdoorsOnly = 0x80
};
)";
		r = _module->AddScriptSection("__Definitions", AsLibrary, _countof(AsLibrary) - 1, 0); RETURN_ASSERT(r >= 0);

		// Game-specific classes
		ScriptActorWrapper::RegisterFactory(_engine, _module);

		if (!AddScriptFromFile(scriptPath)) {
			LOGE("Cannot compile level script");
			return;
		}

		r = _module->Build(); RETURN_ASSERT_MSG(r >= 0, "Cannot compile the script. Please correct the code and try again.");

		asIScriptFunction* func = _module->GetFunctionByDecl("void OnLevelLoad()");
		if (func != nullptr) {
			asIScriptContext* ctx = _engine->RequestContext();

			ctx->Prepare(func);
			r = ctx->Execute();
			if (r == asEXECUTION_EXCEPTION) {
				LOGE_X("An exception \"%s\" occurred in \"%s\". Please correct the code and try again.", ctx->GetExceptionString(), ctx->GetExceptionFunction()->GetDeclaration());
			}

			_engine->ReturnContext(ctx);
		}
	}

	LevelScripts::~LevelScripts()
	{
		for (auto ctx : _contextPool) {
			ctx->Release();
		}

		if (_engine != nullptr) {
			_engine->ShutDownAndRelease();
			_engine = nullptr;
		}
	}

	bool LevelScripts::AddScriptFromFile(const StringView& path)
	{
		auto s = fs::Open(path, FileAccessMode::Read);
		if (s->GetSize() <= 0) {
			return false;
		}

		String scriptContent(NoInit, s->GetSize());
		s->Read(scriptContent.data(), s->GetSize());

		SmallVector<String> includes;
		int scriptSize = (int)scriptContent.size();

		// First perform the checks for #if directives to exclude code that shouldn't be compiled
		int pos = 0;
		int nested = 0;
		while (pos < scriptSize) {
			asUINT len = 0;
			asETokenClass t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			if (t == asTC_UNKNOWN && scriptContent[pos] == '#' && (pos + 1 < scriptSize)) {
				int start = pos++;

				// Is this an #if directive?
				t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);

				StringView token = scriptContent.slice(pos, pos + len);
				pos += len;

				if (token == "if"_s) {
					t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
					if (t == asTC_WHITESPACE) {
						pos += len;
						t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
					}

					if (t == asTC_IDENTIFIER) {
						StringView word = scriptContent.slice(pos, pos + len);

						// Overwrite the #if directive with space characters to avoid compiler error
						pos += len;

						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}

						// Has this identifier been defined by the application or not?
						if (_definedWords.find(String::nullTerminatedView(word)) == _definedWords.end()) {
							// Exclude all the code until and including the #endif
							pos = ExcludeCode(scriptContent, pos);
						} else {
							nested++;
						}
					}
				} else if (token == "endif"_s) {
					// Only remove the #endif if there was a matching #if
					if (nested > 0) {
						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}
						nested--;
					}
				}
			} else
				pos += len;
		}

		pos = 0;
		while (pos < scriptSize) {
			asUINT len = 0;
			asETokenClass t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			if (t == asTC_COMMENT || t == asTC_WHITESPACE) {
				pos += len;
				continue;
			}

			StringView token = scriptContent.slice(pos, pos + len);

			// Is this a preprocessor directive?
			if (token == "#"_s && (pos + 1 < scriptSize)) {
				int start = pos++;

				t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
				if (t == asTC_IDENTIFIER) {
					token = scriptContent.slice(pos, pos + len);
					if (token == "include"_s) {
						pos += len;
						t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
						if (t == asTC_WHITESPACE) {
							pos += len;
							t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
						}

						if (t == asTC_VALUE && len > 2 && (scriptContent[pos] == '"' || scriptContent[pos] == '\'')) {
							// Get the include file
							includes.push_back(String(&scriptContent[pos + 1], len - 2));
							pos += len;

							// Overwrite the include directive with space characters to avoid compiler error
							for (int i = start; i < pos; i++) {
								if (scriptContent[i] != '\n') {
									scriptContent[i] = ' ';
								}
							}
						}
					} else if (token == "pragma"_s) {
						// Read until the end of the line
						pos += len;
						for (; pos < scriptSize && scriptContent[pos] != '\n'; pos++);

						// TODO: Call the pragma callback
						/*string pragmaText(&scriptContent[start + 7], pos - start - 7);
						int r = pragmaCallback ? pragmaCallback(pragmaText, *this, pragmaParam) : -1;
						if (r < 0) {
							// TODO: Report the correct line number
							_engine->WriteMessage(sectionname, 0, 0, asMSGTYPE_ERROR, "Invalid #pragma directive");
							return r;
						}*/

						// Overwrite the pragma directive with space characters to avoid compiler error
						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}
					}
				} else {
					// Check for lines starting with #!, e.g. shebang interpreter directive. These will be treated as comments and removed by the preprocessor
					if (scriptContent[pos] == '!') {
						// Read until the end of the line
						pos += len;
						for (; pos < scriptSize && scriptContent[pos] != '\n'; pos++);

						// Overwrite the directive with space characters to avoid compiler error
						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}
					}
				}
			} else {
				// Don't search for metadata/includes within statement blocks or between tokens in statements
				pos = SkipStatement(scriptContent, pos);
			}
		}

		// Build the actual script
		_engine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true);
		_module->AddScriptSection(path.data(), scriptContent.data(), scriptSize, 0);

		if (includes.size() > 0) {
			// Try to load the included file from the relative directory of the current file
			String currentDir = fs::GetDirectoryName(path);

			// Load the included scripts
			for (auto& include : includes) {
				if (!AddScriptFromFile(fs::JoinPath(currentDir, include[0] == '/' || include[0] == '\\' ? include.exceptPrefix(1) : StringView(include)))) {
					return false;
				}
			}
		}

		return true;
	}

	int LevelScripts::ExcludeCode(String& scriptContent, int pos)
	{
		int scriptSize = (int)scriptContent.size();
		asUINT len = 0;
		int nested = 0;

		while (pos < scriptSize) {
			_engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			if (scriptContent[pos] == '#') {
				scriptContent[pos] = ' ';
				pos++;

				// Is it an #if or #endif directive?
				_engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);

				StringView token = scriptContent.slice(pos, pos + len);

				if (token == "if"_s) {
					nested++;
				} else if (token == "endif"_s) {
					if (nested-- == 0) {
						pos += len;
						break;
					}
				}

				for (uint32_t i = pos; i < pos + len; i++) {
					if (scriptContent[i] != '\n') {
						scriptContent[i] = ' ';
					}
				}
			} else if (scriptContent[pos] != '\n') {
				for (uint32_t i = pos; i < pos + len; i++) {
					if (scriptContent[i] != '\n') {
						scriptContent[i] = ' ';
					}
				}
			}
			pos += len;
		}

		return pos;
	}

	int LevelScripts::SkipStatement(String& scriptContent, int pos)
	{
		int scriptSize = (int)scriptContent.size();
		asUINT len = 0;

		// Skip until ; or { whichever comes first
		while (pos < scriptSize && scriptContent[pos] != ';' && scriptContent[pos] != '{') {
			_engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			pos += len;
		}

		// Skip entire statement block
		if (pos < scriptSize && scriptContent[pos] == '{') {
			pos += 1;

			// Find the end of the statement block
			int level = 1;
			while (level > 0 && pos < scriptSize) {
				asETokenClass t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
				if (t == asTC_KEYWORD) {
					if (scriptContent[pos] == '{') {
						level++;
					} else if (scriptContent[pos] == '}') {
						level--;
					}
				}

				pos += len;
			}
		} else {
			pos += 1;
		}
		return pos;
	}

	asIScriptContext* LevelScripts::RequestContextCallback(asIScriptEngine* engine, void* param)
	{
		asIScriptContext* ctx = nullptr;

		// Check if there is a free context available in the pool
		auto _this = reinterpret_cast<LevelScripts*>(param);
		if (!_this->_contextPool.empty()) {
			ctx = _this->_contextPool.pop_back_val();
		} else {
			// No free context was available so we'll have to create a new one
			ctx = engine->CreateContext();
		}

		return ctx;
	}

	void LevelScripts::ReturnContextCallback(asIScriptEngine* engine, asIScriptContext* ctx, void* param)
	{
		// Unprepare the context to free any objects it may still hold (e.g. return value)
		// This must be done before making the context available for re-use, as the clean
		// up may trigger other script executions, e.g. if a destructor needs to call a function.
		ctx->Unprepare();

		// Place the context into the pool for when it will be needed again
		auto _this = reinterpret_cast<LevelScripts*>(param);
		_this->_contextPool.push_back(ctx);
	}

	void LevelScripts::Message(const asSMessageInfo& msg)
	{
		switch (msg.type) {
			case asMSGTYPE_ERROR: LOGE_X("%s (%i, %i): %s", msg.section, msg.row, msg.col, msg.message); break;
			case asMSGTYPE_WARNING: LOGW_X("%s (%i, %i): %s", msg.section, msg.row, msg.col, msg.message); break;
			default: LOGI_X("%s (%i, %i): %s", msg.section, msg.row, msg.col, msg.message); break;
		}
	}

	Actors::ActorBase* LevelScripts::CreateActorInstance(const StringView& typeName)
	{
		auto nullTerminatedTypeName = String::nullTerminatedView(typeName);

		// Create an instance of the ActorBase script class that inherits from the ScriptActorWrapper C++ class
		asITypeInfo* typeInfo = _module->GetTypeInfoByName(nullTerminatedTypeName.data());
		if (typeInfo == nullptr) {
			return nullptr;
		}

		asIScriptObject* obj = reinterpret_cast<asIScriptObject*>(_engine->CreateScriptObject(typeInfo));

		// Get the pointer to the C++ side of the ActorBase class
		ScriptActorWrapper* obj2 = *reinterpret_cast<ScriptActorWrapper**>(obj->GetAddressOfProperty(0));

		// Increase the reference count to the C++ object, as this is what will be used to control the life time of the object from the application side 
		obj2->AddRef();

		// Release the reference to the script side
		obj->Release();

		return obj2;
	}

	void LevelScripts::OnLevelBegin()
	{
		asIScriptFunction* func = _module->GetFunctionByDecl("void OnLevelBegin()");
		if (func == nullptr) {
			return;
		}
			
		asIScriptContext* ctx = _engine->RequestContext();

		ctx->Prepare(func);
		int r = ctx->Execute();
		if (r == asEXECUTION_EXCEPTION) {
			LOGE_X("An exception \"%s\" occurred in \"%s\". Please correct the code and try again.", ctx->GetExceptionString(), ctx->GetExceptionFunction()->GetDeclaration());
		}

		_engine->ReturnContext(ctx);
	}

	void LevelScripts::OnLevelCallback(uint8_t* eventParams)
	{
		// TODO: Call also other variants
		// void onFunction…(Player@ player)
		// void onFunction…(Player@ player, bool paramName)
		// void onFunction…(Player@ player, uint8 paramName)
		// void onFunction…(Player@ player, int8 paramName)

		char funcName[64];
		sprintf_s(funcName, "void onFunction%i()", eventParams[0]);

		asIScriptFunction* func = _module->GetFunctionByDecl(funcName);
		if (func != nullptr) {
			asIScriptContext* ctx = _engine->RequestContext();

			ctx->Prepare(func);
			int r = ctx->Execute();
			if (r == asEXECUTION_EXCEPTION) {
				LOGE_X("An exception \"%s\" occurred in \"%s\". Please correct the code and try again.", ctx->GetExceptionString(), ctx->GetExceptionFunction()->GetDeclaration());
			}

			_engine->ReturnContext(ctx);
		} else {
			LOGW_X("Callback function \"%s\" was not found in the script. Please correct the code and try again.", funcName);
		}
	}

	uint8_t LevelScripts::asGetDifficulty()
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		return (uint8_t)_this->_levelHandler->_difficulty;
	}

	int LevelScripts::asGetLevelWidth()
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		return _this->_levelHandler->_tileMap->LevelBounds().X;
	}

	int LevelScripts::asGetLevelHeight()
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		return _this->_levelHandler->_tileMap->LevelBounds().Y;
	}

	float LevelScripts::asGetElapsedFrames()
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		return _this->_levelHandler->_elapsedFrames;
	}

	float LevelScripts::asGetAmbientLight()
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		return _this->_levelHandler->_ambientLightTarget;
	}

	void LevelScripts::asSetAmbientLight(float value)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		_this->_levelHandler->_ambientLightTarget = value;
	}

	float LevelScripts::asGetWaterLevel()
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		return _this->_levelHandler->_waterLevel;
	}

	void LevelScripts::asSetWaterLevel(float value)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		_this->_levelHandler->_waterLevel = value;
	}

	void LevelScripts::asPreloadMetadata(const String& path)
	{
		ContentResolver::Current().PreloadMetadataAsync(path);
	}

	void LevelScripts::asRegisterSpawnable(int eventType, const String& typeName)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));

		_this->_eventTypeToTypeName.emplace(eventType, typeName);

		_this->_levelHandler->EventSpawner()->RegisterSpawnable((EventType)eventType, [](const Actors::ActorActivationDetails& details) -> std::shared_ptr<Actors::ActorBase> {
			if (auto levelHandler = dynamic_cast<LevelHandler*>(details.LevelHandler)) {
				auto _this = levelHandler->_scripts.get();
				auto it = _this->_eventTypeToTypeName.find((int)details.Type);
				if (it != _this->_eventTypeToTypeName.end()) {
					auto actor = _this->CreateActorInstance(it->second);
					actor->OnActivated(details);
					return std::shared_ptr<Actors::ActorBase>(actor);
				}
			}
			return nullptr;
		});
	}

	void LevelScripts::asSpawnEvent(int eventType, int x, int y)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));

		uint8_t spawnParams[Events::EventSpawner::SpawnParamsSize] { };
		auto actor = _this->_levelHandler->EventSpawner()->SpawnEvent((EventType)eventType, spawnParams, Actors::ActorState::None, Vector3i(x, y, ILevelHandler::MainPlaneZ));
		if (actor != nullptr) {
			_this->_levelHandler->AddActor(actor);
		}
	}

	void LevelScripts::asSpawnEventParams(int eventType, int x, int y, const CScriptArray& eventParams)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));

		uint8_t spawnParams[Events::EventSpawner::SpawnParamsSize] { };
		int size = eventParams.GetSize();
		std::memcpy(spawnParams, eventParams.At(0), size);

		auto actor = _this->_levelHandler->EventSpawner()->SpawnEvent((EventType)eventType, spawnParams, Actors::ActorState::None, Vector3i(x, y, ILevelHandler::MainPlaneZ));
		if (actor != nullptr) {
			_this->_levelHandler->AddActor(actor);
		}
	}

	void LevelScripts::asSpawnType(const String& typeName, int x, int y)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));

		auto actor = _this->CreateActorInstance(typeName);
		if (actor == nullptr) {
			return;
		}

		uint8_t spawnParams[Events::EventSpawner::SpawnParamsSize] { };
		actor->OnActivated({
			.LevelHandler = _this->_levelHandler,
			.Pos = Vector3i(x, y, ILevelHandler::MainPlaneZ),
			.Params = spawnParams
		});
		_this->_levelHandler->AddActor(std::shared_ptr<Actors::ActorBase>(actor));
	}

	void LevelScripts::asSpawnTypeParams(const String& typeName, int x, int y, const CScriptArray& eventParams)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));

		auto actor = _this->CreateActorInstance(typeName);
		if (actor == nullptr) {
			return;
		}

		uint8_t spawnParams[Events::EventSpawner::SpawnParamsSize] { };
		int size = eventParams.GetSize();
		std::memcpy(spawnParams, eventParams.At(0), size);

		actor->OnActivated({
			.LevelHandler = _this->_levelHandler,
			.Pos = Vector3i(x, y, ILevelHandler::MainPlaneZ),
			.Params = spawnParams
		});
		_this->_levelHandler->AddActor(std::shared_ptr<Actors::ActorBase>(actor));
	}

	void LevelScripts::asMusicPlay(const String& path)
	{
		if (path.empty()) {
			return;
		}

		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		auto _levelHandler = _this->_levelHandler;

		if (_levelHandler->_musicPath != path) {
			_levelHandler->_music = ContentResolver::Current().GetMusic(path);
			if (_levelHandler->_music != nullptr) {
				_levelHandler->_musicPath = path;
				_levelHandler->_music->setLooping(true);
				_levelHandler->_music->setGain(PreferencesCache::MasterVolume * PreferencesCache::MusicVolume);
				_levelHandler->_music->setSourceRelative(true);
				_levelHandler->_music->play();
			}
		}
	}

	void LevelScripts::asShowLevelText(const String& text)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		_this->_levelHandler->ShowLevelText(text);
	}

	void LevelScripts::asSetWeather(uint8_t weatherType, uint8_t intensity)
	{
		auto ctx = asGetActiveContext();
		auto _this = reinterpret_cast<LevelScripts*>(ctx->GetEngine()->GetUserData(EngineToOwner));
		_this->_levelHandler->SetWeather((WeatherType)weatherType, intensity);
	}
}

#endif