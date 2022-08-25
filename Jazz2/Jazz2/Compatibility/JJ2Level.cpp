﻿#include "JJ2Level.h"
#include "EventConverter.h"
#include "../EventType.h"

#include "../../nCine/Base/Algorithms.h"
#include "../../nCine/IO/FileSystem.h"

namespace Jazz2::Compatibility
{
	void JJ2Level::Open(const StringView& path, bool strictParser)
	{
		auto s = fs::Open(path, FileAccessMode::Read);
		ASSERT_MSG(s->IsOpened(), "Cannot open file for reading");

		// Skip copyright notice
		s->Seek(180, SeekOrigin::Current);

		LevelName = fs::GetFileNameWithoutExtension(path);
		lowercaseInPlace(LevelName);

		JJ2Block headerBlock(s, 262 - 180);

		uint32_t magic = headerBlock.ReadUInt32();
		ASSERT_MSG(magic == 0x4C56454C /*LEVL*/, "Invalid magic string");

		uint32_t passwordHash = headerBlock.ReadUInt32();

		_name = headerBlock.ReadString(32, true);

		uint16_t version = headerBlock.ReadUInt16();
		_version = (version <= 514 ? JJ2Version::BaseGame : JJ2Version::TSF);

		int recordedSize = headerBlock.ReadInt32();
		ASSERT_MSG(!strictParser || s->GetSize() == recordedSize, "Unexpected file size");

		// Get the CRC; would check here if it matches if we knew what variant it is AND what it applies to
		// Test file across all CRC32 variants + Adler had no matches to the value obtained from the file
		// so either the variant is something else or the CRC is not applied to the whole file but on a part
		int recordedCRC = headerBlock.ReadInt32();

		// Read the lengths, uncompress the blocks and bail if any block could not be uncompressed
		// This could look better without all the copy-paste, but meh.
		int infoBlockPackedSize = headerBlock.ReadInt32();
		int infoBlockUnpackedSize = headerBlock.ReadInt32();
		int eventBlockPackedSize = headerBlock.ReadInt32();
		int eventBlockUnpackedSize = headerBlock.ReadInt32();
		int dictBlockPackedSize = headerBlock.ReadInt32();
		int dictBlockUnpackedSize = headerBlock.ReadInt32();
		int layoutBlockPackedSize = headerBlock.ReadInt32();
		int layoutBlockUnpackedSize = headerBlock.ReadInt32();

		JJ2Block infoBlock(s, infoBlockPackedSize, infoBlockUnpackedSize);
		JJ2Block eventBlock(s, eventBlockPackedSize, eventBlockUnpackedSize);
		JJ2Block dictBlock(s, dictBlockPackedSize, dictBlockUnpackedSize);
		JJ2Block layoutBlock(s, layoutBlockPackedSize, layoutBlockUnpackedSize);

		LoadMetadata(infoBlock, strictParser);
		LoadEvents(eventBlock, strictParser);
		LoadLayers(dictBlock, dictBlockUnpackedSize / 8, layoutBlock, strictParser);

		// Try to read MLLE data stream
		uint32_t mlleMagic = s->ReadValue<uint32_t>();
		if (mlleMagic == 0x454C4C4D /*MLLE*/) {
			uint32_t mlleVersion = s->ReadValue<uint32_t>();
			int mlleBlockPackedSize = s->ReadValue<int>();
			int mlleBlockUnpackedSize = s->ReadValue<int>();

			JJ2Block mlleBlock(s, mlleBlockPackedSize, mlleBlockUnpackedSize);
			LoadMlleData(mlleBlock, mlleVersion, strictParser);
		}
	}

	void JJ2Level::LoadMetadata(JJ2Block& block, bool strictParser)
	{
		// First 9 bytes are JCS coordinates on last save.
		block.DiscardBytes(9);

		_lightingMin = block.ReadByte();
		_lightingStart = block.ReadByte();

		_animCount = block.ReadUInt16();

		_verticalMPSplitscreen = block.ReadBool();
		_isMpLevel = block.ReadBool();

		// This should be the same as size of block in the start?
		int headerSize = block.ReadInt32();

		String secondLevelName = block.ReadString(32, true);
		ASSERT_MSG(!strictParser || _name == secondLevelName, "Level name mismatch");

		_tileset = block.ReadString(32, true);
		_bonusLevel = block.ReadString(32, true);
		_nextLevel = block.ReadString(32, true);
		_secretLevel = block.ReadString(32, true);
		_music = block.ReadString(32, true);

		for (int i = 0; i < TextEventStringsCount; ++i) {
			_textEventStrings[i] = block.ReadString(512, true);
		}

		LoadLayerMetadata(block, strictParser);

		uint16_t staticTilesCount = block.ReadUInt16();
		ASSERT_MSG(!strictParser || GetMaxSupportedTiles() - _animCount == staticTilesCount, "Tile count mismatch");

		LoadStaticTileData(block, strictParser);

		// The unused XMask field
		block.DiscardBytes(GetMaxSupportedTiles());

		LoadAnimatedTiles(block, strictParser);
	}

	void JJ2Level::LoadStaticTileData(JJ2Block& block, bool strictParser)
	{
		int tileCount = GetMaxSupportedTiles();
		_staticTiles = std::make_unique<TilePropertiesSection[]>(tileCount);

		for (int i = 0; i < tileCount; ++i) {
			int tileEvent = block.ReadInt32();

			auto& tile = _staticTiles[i];
			tile.Event.EventType = (JJ2Event)(uint8_t)(tileEvent & 0x000000FF);
			tile.Event.Difficulty = (uint8_t)((tileEvent & 0x0000C000) >> 14);
			tile.Event.Illuminate = ((tileEvent & 0x00002000) >> 13 == 1);
			tile.Event.TileParams = (uint32_t)(((tileEvent >> 12) & 0x000FFFF0) | ((tileEvent >> 8) & 0x0000000F));
		}
		for (int i = 0; i < tileCount; ++i) {
			_staticTiles[i].Flipped = block.ReadBool();
		}

		for (int i = 0; i < tileCount; ++i) {
			_staticTiles[i].Type = block.ReadByte();
		}
	}

	void JJ2Level::LoadAnimatedTiles(JJ2Block& block, bool strictParser)
	{
		_animatedTiles = std::make_unique<AnimatedTileSection[]>(_animCount);

		for (int i = 0; i < _animCount; i++) {
			auto& tile = _animatedTiles[i];
			tile.Delay = block.ReadUInt16();
			tile.DelayJitter = block.ReadUInt16();
			tile.ReverseDelay = block.ReadUInt16();
			tile.IsReverse = block.ReadBool();
			tile.Speed = block.ReadByte(); // 0-70
			tile.FrameCount = block.ReadByte();

			for (int j = 0; j < 64; j++) {
				tile.Frames[j] = block.ReadUInt16();
			}
		}
	}

	void JJ2Level::LoadLayerMetadata(JJ2Block& block, bool strictParser)
	{
		_layers = std::make_unique<LayerSection[]>(JJ2LayerCount);

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].Flags = block.ReadUInt32();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].Type = block.ReadByte();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].Used = block.ReadBool();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].Width = block.ReadInt32();
		}

		// This is related to how data is presented in the file; the above is a WYSIWYG version, solely shown on the UI
		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].InternalWidth = block.ReadInt32();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].Height = block.ReadInt32();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].Depth = block.ReadInt32();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].DetailLevel = block.ReadByte();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].WaveX = block.ReadFloatEncoded();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].WaveY = block.ReadFloatEncoded();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].SpeedX = block.ReadFloatEncoded();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].SpeedY = block.ReadFloatEncoded();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].AutoSpeedX = block.ReadFloatEncoded();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].AutoSpeedY = block.ReadFloatEncoded();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].TexturedBackgroundType = block.ReadByte();
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			_layers[i].TexturedParams1 = block.ReadByte();
			_layers[i].TexturedParams2 = block.ReadByte();
			_layers[i].TexturedParams3 = block.ReadByte();
		}
	}

	void JJ2Level::LoadEvents(JJ2Block& block, bool strictParser)
	{
		int width = _layers[3].Width;
		int height = _layers[3].Height;
		if (width <= 0 && height <= 0) {
			return;
		}

		_events = std::make_unique<TileEventSection[]>(width * height);

		for (int y = 0; y < _layers[3].Height; y++) {
			for (int x = 0; x < width; x++) {
				uint32_t eventData = block.ReadUInt32();

				auto& tileEvent = _events[x + y * width];
				tileEvent.EventType = (JJ2Event)(uint8_t)(eventData & 0x000000FF);
				tileEvent.Difficulty = (uint8_t)((eventData & 0x00000300) >> 8);
				tileEvent.Illuminate = ((eventData & 0x00000400) >> 10 == 1);
				tileEvent.TileParams = ((eventData & 0xFFFFF000) >> 12);
			}
		}

		auto& lastTileEvent = _events[(width * height) - 1];
		if (lastTileEvent.EventType == JJ2Event::MCE) {
			_hasPit = true;
		}

		for (int i = 0; i < width * height; i++) {
			if (_events[i].EventType == JJ2Event::CTF_BASE) {
				_hasCTF = true;
			} else if (_events[i].EventType == JJ2Event::WARP_ORIGIN) {
				if (((_events[i].TileParams >> 16) & 1) == 1) {
					_hasLaps = true;
				}
			}
		}
	}

	void JJ2Level::LoadLayers(JJ2Block& dictBlock, int dictLength, JJ2Block& layoutBlock, bool strictParser)
	{
		struct DictionaryEntry {
			uint16_t Tiles[4];
		};

		std::unique_ptr<DictionaryEntry[]> dictionary = std::make_unique<DictionaryEntry[]>(dictLength);
		for (int i = 0; i < dictLength; i++) {
			auto& entry = dictionary[i];
			for (int j = 0; j < 4; j++) {
				entry.Tiles[j] = dictBlock.ReadUInt16();
			}
		}

		for (int i = 0; i < JJ2LayerCount; ++i) {
			auto& layer = _layers[i];

			if (layer.Used) {
				layer.Tiles = std::make_unique<uint16_t[]>(layer.InternalWidth * layer.Height);

				for (int y = 0; y < layer.Height; y++) {
					for (int x = 0; x < layer.InternalWidth; x += 4) {
						uint16_t dictIdx = layoutBlock.ReadUInt16();

						for (int j = 0; j < 4; j++) {
							if (j + x >= layer.Width) {
								break;
							}

							layer.Tiles[j + x + y * layer.InternalWidth] = dictionary[dictIdx].Tiles[j];
						}
					}
				}
			} else {
				// Array will be initialized with zeros
				layer.Tiles = std::make_unique<uint16_t[]>(layer.Width * layer.Height);
			}
		}
	}

	void JJ2Level::LoadMlleData(JJ2Block& block, uint32_t version, bool strictParser)
	{
		// TODO
	}

	void JJ2Level::Convert(const String& targetPath, const EventConverter& eventConverter, const std::function<LevelToken(const StringView&)>& levelTokenConversion)
	{
		auto so = fs::Open(targetPath, FileAccessMode::Write);
		ASSERT_MSG(so->IsOpened(), "Cannot open file for writing");

		so->WriteValue<uint64_t>(0x2095A59FF0BFBBEF);
		so->WriteValue<uint8_t>(1); // Version

		// Flags
		uint16_t flags = 0;
		if (_hasPit) {
			flags |= 0x01;
		}
		if (_verticalMPSplitscreen) {
			flags |= 0x02;
		}
		if (_isMpLevel) {
			flags |= 0x10;
			if (_hasLaps) {
				flags |= 0x20;
			}
			if (_hasCTF) {
				flags |= 0x40;
			}
		}
		so->WriteValue<uint16_t>(flags);

		so->WriteValue<uint8_t>((uint8_t)_name.size());
		so->Write(_name.data(), _name.size());

		lowercaseInPlace(_nextLevel);
		lowercaseInPlace(_secretLevel);
		lowercaseInPlace(_bonusLevel);

		WriteLevelName(so, _nextLevel, levelTokenConversion);
		WriteLevelName(so, _secretLevel, levelTokenConversion);
		WriteLevelName(so, _bonusLevel, levelTokenConversion);

		// Default Tileset
		lowercaseInPlace(_tileset);
		StringView tileset = _tileset;
		if (StringHasSuffixIgnoreCase(tileset, ".j2t"_s)) {
			tileset = tileset.exceptSuffix(4);
		}
		so->WriteValue<uint8_t>((uint8_t)tileset.size());
		so->Write(tileset.data(), tileset.size());

		// Default Music
		lowercaseInPlace(_music);
		if (_music.findOr('.', _music.end()) == _music.end()) {
			String music = _music + ".j2b"_s;
			so->WriteValue<uint8_t>((uint8_t)music.size());
			so->Write(music.data(), music.size());
		} else {
			so->WriteValue<uint8_t>((uint8_t)_music.size());
			so->Write(_music.data(), _music.size());
		}
		
		so->WriteValue<uint8_t>(_lightingStart * 255 / 64);

		// TODO: Darkness color
		so->WriteValue<uint8_t>(0);
		so->WriteValue<uint8_t>(0);
		so->WriteValue<uint8_t>(0);
		so->WriteValue<uint8_t>(255);

		// TODO: Weather
		so->WriteValue<uint8_t>(0);

		// Text Event Strings
		so->WriteValue<uint8_t>(TextEventStringsCount);
		for (int i = 0; i < TextEventStringsCount; i++) {
			size_t textLength = _textEventStrings[i].size();
			for (int j = 0; j < textLength; j++) {
				if (_textEventStrings[i][j] == '@') {
					_textEventStrings[i][j] = '\n';
				}
			}

			so->WriteValue<uint16_t>((uint8_t)textLength);
			so->Write(_textEventStrings[i].data(), textLength);
		}

		// TODO: Additional Tilesets

		uint16_t maxTiles = (uint16_t)GetMaxSupportedTiles();
		uint16_t lastTilesetTileIndex = (uint16_t)(maxTiles - _animCount);

		// Animated Tiles
		so->WriteValue<uint16_t>(_animCount);

		for (int i = 0; i < _animCount; i++) {
			auto& tile = _animatedTiles[i];
			so->WriteValue<uint8_t>(tile.FrameCount);
			so->WriteValue<uint8_t>(tile.Speed);
			so->WriteValue<uint16_t>(tile.Delay);
			so->WriteValue<uint16_t>(tile.DelayJitter);
			so->WriteValue<uint8_t>(tile.IsReverse ? 1 : 0);
			so->WriteValue<uint16_t>(tile.ReverseDelay);

			for (int j = 0; j < tile.FrameCount; j++) {
				// Max. tiles is either 0x0400 or 0x1000 and doubles as a mask to separate flipped tiles.
				// In J2L, each flipped tile had a separate entry in the tile list, probably to make
				// the dictionary concept easier to handle.
				bool flipX = false, flipY = false;
				uint16_t tileIdx = tile.Frames[j];
				if ((tileIdx & maxTiles) > 0) {
					flipX = true;
					tileIdx -= maxTiles;
				}

				if (tileIdx >= lastTilesetTileIndex) {
					//Log.Write(LogType.Warning, "Level \"" + levelToken + "\" has animated tile in animated tile (" + (tileIdx - lastTilesetTileIndex) + " -> " + fixFrames[0] + ")! Applying quick tile redirection.");

					tileIdx = _animatedTiles[tileIdx - lastTilesetTileIndex].Frames[0];
				}

				uint8_t tileFlags = 0x00;
				if (flipX) {
					tileFlags |= 0x01; // Flip X
				}
				if (flipY) {
					tileFlags |= 0x02; // Flip Y
				}

				if (_staticTiles[tile.Frames[j]].Type == 1) {
					tileFlags |= 0x10; // Legacy Translucent
				} else if (_staticTiles[tile.Frames[j]].Type == 3) {
					tileFlags |= 0x20; // Invisible
				}

				so->WriteValue<uint8_t>(tileFlags);
				so->WriteValue<uint16_t>(tileIdx);
			}
		}

		// Layers
		int layerCount = 0;
		for (int i = 0; i < JJ2LayerCount; i++) {
			if (_layers[i].Used) {
				layerCount++;
			}
		}

		// TODO: Compress layer data
		so->WriteValue<uint8_t>(layerCount);
		for (int i = 0; i < JJ2LayerCount; i++) {
			auto& layer = _layers[i];
			if (layer.Used) {
				bool isSky = (i == 7);
				bool isSprite = (i == 3);
				so->WriteValue<uint8_t>(isSprite ? 2 : (isSky ? 1 : 0));	// Layer type
				so->WriteValue<uint8_t>(layer.Flags & 0xff);				// Layer flags

				so->WriteValue<int32_t>(layer.Width);
				so->WriteValue<int32_t>(layer.Height);

				if (!isSprite) {
					bool hasTexturedBackground = ((layer.Flags & 0x08) == 0x08);
					if (isSky && !hasTexturedBackground) {
						so->WriteValue<float>(180.0f);
						so->WriteValue<float>(-300.0f);
					} else {
						so->WriteValue<float>(0.0f);
						so->WriteValue<float>(0.0f);
					}

					so->WriteValue<float>(layer.SpeedX);
					so->WriteValue<float>(layer.SpeedY);
					so->WriteValue<float>(layer.AutoSpeedX);
					so->WriteValue<float>(layer.AutoSpeedY);
					so->WriteValue<int16_t>((int16_t)layer.Depth);

					if (isSky && hasTexturedBackground) {
						so->WriteValue<uint8_t>(layer.TexturedBackgroundType + 1);
						so->WriteValue<uint8_t>(layer.TexturedParams1);
						so->WriteValue<uint8_t>(layer.TexturedParams2);
						so->WriteValue<uint8_t>(layer.TexturedParams3);
					}
				}

				for (int y = 0; y < layer.Height; y++) {
					for (int x = 0; x < layer.Width; x++) {
						uint16_t tileIdx = layer.Tiles[y * layer.InternalWidth + x];

						bool flipX = false, flipY = false;
						if ((tileIdx & 0x2000) != 0) {
							flipY = true;
							tileIdx -= 0x2000;
						}

						if ((tileIdx & ~(maxTiles | (maxTiles - 1))) != 0) {
							// Fix of bug in updated Psych2.j2l
							tileIdx = (uint16_t)((tileIdx & (maxTiles | (maxTiles - 1))) | maxTiles);
						}

						// Max. tiles is either 0x0400 or 0x1000 and doubles as a mask to separate flipped tiles.
						// In J2L, each flipped tile had a separate entry in the tile list, probably to make
						// the dictionary concept easier to handle.

						if ((tileIdx & maxTiles) > 0) {
							flipX = true;
							tileIdx -= maxTiles;
						}

						bool animated = false;
						if (tileIdx >= lastTilesetTileIndex) {
							animated = true;
							tileIdx -= lastTilesetTileIndex;
						}

						bool legacyTranslucent = false;
						bool invisible = false;
						if (!animated && tileIdx < lastTilesetTileIndex) {
							legacyTranslucent = (_staticTiles[tileIdx].Type == 1);
							invisible = (_staticTiles[tileIdx].Type == 3);
						}

						uint8_t tileFlags = 0;
						if (flipX) {
							tileFlags |= 0x01;
						}
						if (flipY) {
							tileFlags |= 0x02;
						}
						if (animated) {
							tileFlags |= 0x04;
						}

						if (legacyTranslucent) {
							tileFlags |= 0x10;
						} else if (invisible) {
							tileFlags |= 0x20;
						}

						so->WriteValue<uint8_t>(tileFlags);
						so->WriteValue<uint16_t>(tileIdx);
					}
				}
			}
		}

		// Events
		for (int y = 0; y < _layers[3].Height; y++) {
			for (int x = 0; x < _layers[3].Width; x++) {
				auto& tileEvent = _events[x + y * _layers[3].Width];

				int flags = 0;
				if (tileEvent.Illuminate) flags |= 0x04; // Illuminated
				if (tileEvent.Difficulty != 2 /*DIFFICULTY_HARD*/) {
					flags |= 0x10; // Difficulty: Easy
				}
				if (tileEvent.Difficulty == 0 /*DIFFICULTY_ALL*/) {
					flags |= 0x20; // Difficulty: Normal
				}
				if (tileEvent.Difficulty != 1 /*DIFFICULTY_EASY*/) {
					flags |= 0x40; // Difficulty: Hard
				}
				if (tileEvent.Difficulty == 3 /*DIFFICULTY_MULTIPLAYER*/) {
					flags |= 0x80; // Multiplayer Only
				}

				// ToDo: Flag 0x08 not used

				JJ2Event eventType;
				int generatorDelay;
				uint8_t generatorFlags;
				if (tileEvent.EventType == JJ2Event::MODIFIER_GENERATOR) {
					// Generators are converted differently
					uint8_t eventParams[8];
					EventConverter::ConvertParamInt(tileEvent.TileParams, {
						{ JJ2ParamUInt, 8 },	// Event
						{ JJ2ParamUInt, 8 },	// Delay
						{ JJ2ParamBool, 1 }	// Initial Delay
					}, eventParams);

					eventType = (JJ2Event)eventParams[0];
					generatorDelay = eventParams[1];
					generatorFlags = (uint8_t)eventParams[2];
				} else {
					eventType = tileEvent.EventType;
					generatorDelay = -1;
					generatorFlags = 0;
				}

				ConversionResult converted = eventConverter.TryConvert(this, eventType, tileEvent.TileParams);

				// If the event is unsupported or can't be converted, add it to warning list
				if (eventType != JJ2Event::EMPTY && converted.Type == EventType::Empty) {
					//int count;
					//_unsupportedEvents.TryGetValue(eventType, out count);
					//_unsupportedEvents[eventType] = (count + 1);
				}

				so->WriteValue<uint16_t>((uint16_t)converted.Type);

				bool allZeroes = true;
				if (converted.Type != EventType::Empty) {
					for (int i = 0; i < _countof(converted.Params); i++) {
						if (converted.Params[i] != 0) {
							allZeroes = false;
							break;
						}
					}
				}

				if (allZeroes) {
					if (generatorDelay == -1) {
						so->WriteValue<uint8_t>(flags | 0x01 /*NoParams*/);
					} else {
						so->WriteValue<uint8_t>(flags | 0x01 /*NoParams*/ | 0x02 /*Generator*/);
						so->WriteValue<uint8_t>(generatorFlags);
						so->WriteValue<uint8_t>(generatorDelay);
					}
				} else {
					if (generatorDelay == -1) {
						so->WriteValue<uint8_t>(flags);
					} else {
						so->WriteValue<uint8_t>(flags | 0x02 /*Generator*/);
						so->WriteValue<uint8_t>(generatorFlags);
						so->WriteValue<uint8_t>(generatorDelay);
					}

					so->Write(converted.Params, sizeof(converted.Params));
				}
			}
		}
	}

	void JJ2Level::WriteLevelName(const std::unique_ptr<IFileStream>& so, const StringView& value, const std::function<LevelToken(const StringView&)>& levelTokenConversion)
	{
		if (!value.empty()) {
			StringView adjustedValue = value;
			if (StringHasSuffixIgnoreCase(adjustedValue, ".j2l"_s) ||
				StringHasSuffixIgnoreCase(adjustedValue, ".lev"_s)) {
				adjustedValue = adjustedValue.exceptSuffix(4);
			}

			if (levelTokenConversion != nullptr) {
				LevelToken token = levelTokenConversion(adjustedValue);
				if (!token.Episode.empty()) {
					String fullName = token.Episode + "/"_s + token.Level;
					so->WriteValue<uint8_t>(fullName.size());
					so->Write(fullName.data(), fullName.size());
				} else {
					so->WriteValue<uint8_t>(token.Level.size());
					so->Write(token.Level.data(), token.Level.size());
				}
			} else {
				so->WriteValue<uint8_t>(adjustedValue.size());
				so->Write(adjustedValue.data(), adjustedValue.size());
			}
		} else {
			so->WriteValue<uint8_t>(0);
		}
	}

	bool JJ2Level::StringHasSuffixIgnoreCase(const StringView& value, const StringView& suffix)
	{
		const std::size_t size = value.size();
		const std::size_t suffixSize = suffix.size();
		if (size < suffixSize) return false;

		for (std::size_t i = 0; i < suffixSize; i++) {
			if (tolower(value[size - suffixSize + i]) != suffix[i]) {
				return false;
			}
		}

		return true;
	}
}