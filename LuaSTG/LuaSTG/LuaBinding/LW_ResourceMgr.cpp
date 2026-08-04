#include "LuaBinding/LuaWrapper.hpp"
#include "lua/plus.hpp"
#include "AppFrame.h"

void luastg::binding::ResourceManager::Register(lua_State* L) noexcept
{
	struct Wrapper
	{
		static int SetResLoadInfo(lua_State* L) noexcept {
			ResourceMgr::SetResourceLoadingLog((bool)lua_toboolean(L, 1));
			return 0;
		}
		static int CreateResourcePool(lua_State* L) noexcept {
			const char* s = luaL_checkstring(L, 1);
			LRES.CreatePool(s);
			return 0;
		}
		static int RemoveResourcePool(lua_State* L) noexcept {
			const char* s = luaL_checkstring(L, 1);
			LRES.RemovePool(s);
			return 0;
		}
		static int SetResourceStatus(lua_State* L) noexcept
		{
			const char* s = luaL_checkstring(L, 1);
			if (!LRES.SetActivedPoolByName(s))
				return luaL_error(L, "invalid argument #1 for 'SetResourceStatus', pool '%s' not found", s);
			return 0;
		}
		static int GetResourceStatus(lua_State* L) noexcept
		{
			lua_pushstring(L, LRES.GetActivedPoolName().c_str());
			return 1;
		}
		static int LoadTexture(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			if (!pActivedPool->LoadTexture(name, path, lua_toboolean(L, 3) == 0 ? false : true))
				return luaL_error(L, "can't load texture from file '%s'.", path);
			return 0;
		}
		static int LoadTextureBin(lua_State* L)
		{
			const char* name = luaL_checkstring(L, 1);

			size_t bin_size = 0;
			const char* bin_data = luaL_checklstring(L, 2, &bin_size);
			std::vector<uint8_t> bin(bin_data, bin_data + bin_size);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			if (!pActivedPool->LoadTextureBin(name, bin, lua_toboolean(L, 3) == 0 ? false : true))
				return luaL_error(L, "can't load texture '%s' from binary data.", name);
			return 0;
		}
		static int LoadSprite(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* texname = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (!pActivedPool->CreateSprite(
				name,
				texname,
				luaL_checknumber(L, 3),
				luaL_checknumber(L, 4),
				luaL_checknumber(L, 5),
				luaL_checknumber(L, 6),
				luaL_optnumber(L, 7, 0.),
				luaL_optnumber(L, 8, 0.),
				lua_toboolean(L, 9) == 0 ? false : true
			))
			{
				return luaL_error(L, "load image failed (name='%s', tex='%s').", name, texname);
			}
			return 0;
		}
		static int LoadAnimation(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			
			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (lua_istable(L, 2)) {
				std::vector<core::SmartReference<IResourceSprite>> sprites;
				sprites.reserve(lua_objlen(L, 2));
				for (int i = 1; i <= static_cast<int>(lua_objlen(L, 2)); i += 1) {
					lua_pushinteger(L, i);
					lua_gettable(L, 2);
					char const* sprite_name = luaL_checkstring(L, -1);
					auto sprite = LRES.FindSprite(sprite_name);
					if (!sprite)
						return luaL_error(L, "load animation failed (name='%s'), sprite '%s' not found", name, sprite_name);
					sprites.push_back(sprite);
					lua_pop(L, 1);
				}
				if (!pActivedPool->CreateAnimation(
					name,
					sprites,
					luaL_checkinteger(L, 3),
					luaL_optnumber(L, 4, 0.0f),
					luaL_optnumber(L, 5, 0.0f),
					lua_toboolean(L, 6) != 0
				)) {
					return luaL_error(L, "load animation failed (name='%s').", name);
				}
			}
			else {
				const char* texname = luaL_checkstring(L, 2);
				if (!pActivedPool->CreateAnimation(
					name,
					texname,
					luaL_checknumber(L, 3),
					luaL_checknumber(L, 4),
					luaL_checknumber(L, 5),
					luaL_checknumber(L, 6),
					(int)luaL_checkinteger(L, 7),
					(int)luaL_checkinteger(L, 8),
					(int)luaL_checkinteger(L, 9),
					luaL_optnumber(L, 10, 0.0f),
					luaL_optnumber(L, 11, 0.0f),
					lua_toboolean(L, 12) != 0
				)) {
					return luaL_error(L, "load animation failed (name='%s', tex='%s').", name, texname);
				}
			}
			
			return 0;
		}
		static int LoadPS(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			
			const char* name = luaL_checkstring(L, 1);
			const char* img_name = luaL_checkstring(L, 3);
			if (lua_type(L, 2) == LUA_TTABLE) {
				hgeParticleSystemInfo info;
				bool ret = TranslateTableToParticleInfo(L, 2, info);
				if (!ret) return luaL_error(L, "load particle failed (name='%s', define=?, img='%s').", name, img_name);
				if (!pActivedPool->LoadParticle(
					name,
					info,
					img_name,
					luaL_optnumber(L, 4, 0.0f),
					luaL_optnumber(L, 5, 0.0f),
					lua_toboolean(L, 6) == 0 ? false : true
				))
				{
					return luaL_error(L, "load particle failed (name='%s', define=table, img='%s').", name, img_name);
				}
				return 0;
			}
			else {
				const char* path = luaL_checkstring(L, 2);

				if (!pActivedPool->LoadParticle(
					name,
					path,
					img_name,
					luaL_optnumber(L, 4, 0.0f),
					luaL_optnumber(L, 5, 0.0f),
					lua_toboolean(L, 6) == 0 ? false : true
				))
				{
					return luaL_error(L, "load particle failed (name='%s', file='%s', img='%s').", name, path, img_name);
				}
				return 0;
			}
		}
		static int LoadSound(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (!pActivedPool->LoadSoundEffect(name, path))
				return luaL_error(L, "load sound failed (name=%s, path=%s)", name, path);
			return 0;
		}
		static int LoadMusic(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			double loop_end = luaL_checknumber(L, 3);
			double loop_duration = luaL_checknumber(L, 4);
			double loop_start = std::max(0., loop_end - loop_duration);

			if (!pActivedPool->LoadMusic(
				name,
				path,
				loop_start,
				loop_end,
				(lua_gettop(L) >= 5) ? lua_toboolean(L, 5) : false
				))
			{
				return luaL_error(L, "load music failed (name=%s, path=%s, loop=%f~%f)", name, path, loop_start, loop_end);
			}
			return 0;
		}
		static int LoadFont(lua_State* L) noexcept
		{
			bool bSucceed = false;
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (lua_gettop(L) == 2)
			{
				// HGE字体 mipmap=true
				bSucceed = pActivedPool->LoadSpriteFont(name, path);
			}
			else
			{
				if (lua_isboolean(L, 3))
				{
					// HGE字体 mipmap=user_defined
					bSucceed = pActivedPool->LoadSpriteFont(name, path, lua_toboolean(L, 3) == 0 ? false : true);
				}
				else
				{
					// fancy2d字体
					const char* texpath = luaL_checkstring(L, 3);
					if (lua_gettop(L) == 4)
						bSucceed = pActivedPool->LoadSpriteFont(name, path, texpath, lua_toboolean(L, 4) == 0 ? false : true);
					else
						bSucceed = pActivedPool->LoadSpriteFont(name, path, texpath);
				}
			}

			if (!bSucceed)
				return luaL_error(L, "can't load font from file '%s'.", path);
			return 0;
		}
		static int LoadTTF(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool) {
				return luaL_error(L, "can't load resource at this time.");
			}
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);
			bool result = pActivedPool->LoadTTFFont(name, path, (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
			lua_pushboolean(L, result);
			return 1;
		}
		static int LoadTrueTypeFont(lua_State* L) noexcept
		{
			lua::stack_t S(L);

			// 先检查有没有资源池
			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
			{
				return luaL_error(L, "can't load resource at this time.");
			}
			
			// 第一个参数，资源名
			std::string_view const name = S.get_value<std::string_view>(1);
			
			// 第二个参数，字体组
			if (!lua_istable(L, 2))
			{
				return luaL_error(L, "invalid parameter #2, required table");
			}
			int const cnt = (int)lua_objlen(L, 2);
			std::vector<core::Graphics::TrueTypeFontInfo> fonts(cnt);
			for (int i = 1; i <= cnt; i += 1)
			{
				auto& font = fonts[i - 1];
				font.source = "";
				font.font_face = 0;
				font.font_size = core::Vector2F(0.0f, 0.0f);
				font.is_force_to_file = false;
				font.is_buffer = false;

				lua_pushinteger(L, i);		// name param fonts i
				lua_gettable(L, 2);			// name param fonts font
				if (!lua_istable(L, -1))
				{
					return luaL_error(L, "invalid value #%d in parameter #2, required table", i);
				}

				lua_getfield(L, -1, "source"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TSTRING) // name param fonts font v
				{
					font.source = S.get_value<std::string_view>(-1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_getfield(L, -1, "font_face"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TNUMBER) // name param fonts font v
				{
					font.font_face = (uint32_t)luaL_checkinteger(L, -1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_getfield(L, -1, "width"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TNUMBER) // name param fonts font v
				{
					font.font_size.x = (float)luaL_checknumber(L, -1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_getfield(L, -1, "height"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TNUMBER) // name param fonts font v
				{
					font.font_size.y = (float)luaL_checknumber(L, -1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_pop(L, 1);				// name param fonts
			}

			bool result = pActivedPool->LoadTrueTypeFont(name.data(), fonts.data(), fonts.size());
			lua_pushboolean(L, result);
			
			return 1;
		}
		static int LoadFX(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (!pActivedPool->LoadFX(name, path))
				return luaL_error(L, "load fx failed (name=%s, path=%s)", name, path);

			return 0;
		}
		static int LoadModel(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* model_path = luaL_checkstring(L, 2);
			
			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			if (!pActivedPool->LoadModel(
				name,
				model_path))
			{
				return luaL_error(L, "load model failed (name='%s', model='%s').", name, model_path);
			}
			return 0;
		}
		static int LoadVideo(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			const char* video_path = luaL_checkstring(L, 2);

			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			if (!pActivedPool->LoadVideo(name, video_path))
				return luaL_error(L, "load video failed (name='%s', path='%s').", name, video_path);
			return 0;
		}
		static int SetVideoState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceVideo> p = LRES.FindVideo(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "video '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
				p->GetSprite()->setColor(*Color::Cast(L, 3));
			else if (lua_gettop(L) == 6)
			{
				core::Color4B tColors[] = {
					*Color::Cast(L, 3),
					*Color::Cast(L, 4),
					*Color::Cast(L, 5),
					*Color::Cast(L, 6)
				};
				p->GetSprite()->setColor(tColors);
			}
			return 0;
		}
		static int CreateRenderTarget(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			
			ResourcePool* pActivedPool = LRES.GetActivedPool();
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			
			if (lua_gettop(L) >= 3)
			{
				const int width = (int)luaL_checkinteger(L, 2);
				const int height = (int)luaL_checkinteger(L, 3);
				if (width < 1 || height < 1)
					return luaL_error(L, "invalid render target size (%dx%d).", width, height);
				bool depth_buffer = true;
				if (lua_gettop(L) >= 4)
					depth_buffer = lua_toboolean(L, 4);
				if (!pActivedPool->CreateRenderTarget(name, width, height, depth_buffer))
					return luaL_error(L, "can't create render target with name '%s'.", name);
			}
			else
			{
				if (!pActivedPool->CreateRenderTarget(name, 0, 0, true))
					return luaL_error(L, "can't create render target with name '%s'.", name);
			}
			
			return 0;
		}
		static int IsRenderTarget(lua_State* L) noexcept
		{
			core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "render target '%s' not found.", luaL_checkstring(L, 1));
			lua_pushboolean(L, p->IsRenderTarget());
			return 1;
		}
		static int SetTexturePreMulAlphaState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
			if (p)
			{
				p->GetTexture()->setPremultipliedAlpha(lua_toboolean(L, 2));
				return 0;
			}
			return luaL_error(L, "texture '%s' not found.", luaL_checkstring(L, 1));
		}
		static int SetTextureSamplerState(lua_State* L)noexcept
		{
			lua::stack_t S(L);
			std::string_view const sampler_name = S.get_value<std::string_view>(2);
			if (sampler_name == "" || sampler_name == "point+wrap" || sampler_name == "point+clamp" || sampler_name == "linear+wrap" || sampler_name == "linear+clamp")
			{
				std::string_view const tex_name = S.get_value<std::string_view>(1);
				core::SmartReference<IResourceTexture> p = LRES.FindTexture(tex_name.data());
				if (!p)
				{
					spdlog::error("[luastg] lstg.SetTextureSamplerState failed: can't find texture '{}'", tex_name);
					return luaL_error(L, "can't find texture '%s'", tex_name.data());
				}

				// 映射
				core::Graphics::IRenderer::SamplerState state = core::Graphics::IRenderer::SamplerState::LinearClamp;
				if (sampler_name == "point+wrap") state = core::Graphics::IRenderer::SamplerState::PointWrap;
				else if (sampler_name == "point+clamp") state = core::Graphics::IRenderer::SamplerState::PointClamp;
				else if (sampler_name == "linear+wrap") state = core::Graphics::IRenderer::SamplerState::LinearWrap;
				else if (sampler_name == "" || sampler_name == "linear+clamp") state = core::Graphics::IRenderer::SamplerState::LinearClamp;
				else return luaL_error(L, "unknown sampler state '%s'", sampler_name.data());

				// 设置
				core::Graphics::ISamplerState* p_sampler = LAPP.GetRenderer2D()->getKnownSamplerState(state);
				p->GetTexture()->setSamplerState(p_sampler);

				return 0;
			}
			else
			{
				return luaL_error(L, "unsupported deprecated usage");
			}
		}
		static int GetTextureSize(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			core::Vector2U size;
			if (!LRES.GetTextureSize(name, size))
				return luaL_error(L, "texture '%s' not found.", name);
			lua_pushinteger(L, (lua_Integer)size.x);
			lua_pushinteger(L, (lua_Integer)size.y);
			return 2;
		}
		static int GetTextureHandle(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			size_t handle;
			if (!LRES.GetTextureHandle(name, handle))
				return luaL_error(L, "texture '%s' not found.", name);
			lua_pushinteger(L, (lua_Integer)handle);
			return 1;
		}
		static int RemoveResource(lua_State* L) noexcept
		{
			const char* s = luaL_checkstring(L, 1);
			if (strcmp(s, "none") == 0)
				return 0; // Not a real pool but just in case.
			
			ResourcePool* pool = LRES.GetPool(s);
			if (!pool)
				return luaL_error(L, "invalid argument # for 'RemoveResource', pool '%s' not found", s);
			
			if (lua_gettop(L) == 1)
			{
				pool->Clear();
			}
			else
			{
				ResourceType tResourceType = static_cast<ResourceType>(luaL_checkint(L, 2));
				const char* tResourceName = luaL_checkstring(L, 3);
				pool->RemoveResource(tResourceType, tResourceName);
			}

			return 0;
		}
		static int CheckRes(lua_State* L) noexcept
		{
			ResourceType tResourceType = static_cast<ResourceType>(luaL_checkint(L, 1));
			const char* tResourceName = luaL_checkstring(L, 2);

			auto pools = LRES.EnumPools();
			for (auto const& pname : pools)
			{
				ResourcePool* p = LRES.GetPool(pname);
				if (p && p->CheckResourceExists(tResourceType, tResourceName))
				{
					lua_pushstring(L, pname.c_str());
					return 1;
				}
			}
			lua_pushnil(L);
			return 1;
		}
		static int EnumRes(lua_State* L) noexcept
		{
			ResourceType tResourceType = static_cast<ResourceType>(luaL_checkint(L, 1));
			auto pools = LRES.EnumPools();
			int pushed = 0;
			for (auto const& pname : pools)
			{
				ResourcePool* p = LRES.GetPool(pname);
				if (p)
				{
					p->ExportResourceList(L, tResourceType);
					++pushed;
				}
			}
			return pushed;
		}

		static int SetImageScale(lua_State* L) noexcept
		{
			if (lua_gettop(L) <= 1)
			{
				float x = static_cast<float>(luaL_checknumber(L, 1));
				if (x == 0.f)
					return luaL_error(L, "invalid argument #1 for 'SetImageScale'.");
				LRES.SetGlobalImageScaleFactor(x);
			}
			else
			{
				core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
				if (!p)
					return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
				float x = (float)luaL_checknumber(L, 2);
				p->GetSprite()->setUnitsPerPixel(x);
			}
			return 0;
		}
		static int GetImageScale(lua_State* L) noexcept
		{
			if (lua_gettop(L) <= 0)
			{
				lua_Number ret = LRES.GetGlobalImageScaleFactor();
				lua_pushnumber(L, ret);
				return 1;
			}
			else
			{
				core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
				if (!p)
					return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
				lua_pushnumber(L, p->GetSprite()->getUnitsPerPixel());
				return 1;
			}
		}
		static int SetImageState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
				p->GetSprite()->setColor(*Color::Cast(L, 3));
			else if (lua_gettop(L) == 6)
			{
				core::Color4B tColors[] = {
					*Color::Cast(L, 3),
					*Color::Cast(L, 4),
					*Color::Cast(L, 5),
					*Color::Cast(L, 6)
				};
				p->GetSprite()->setColor(tColors);
			}
			return 0;
		}
		static int SetImageCenter(lua_State* L) noexcept
		{
			core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
			p->GetSprite()->setTextureCenter(core::Vector2F(
				static_cast<float>(luaL_checknumber(L, 2) + p->GetSprite()->getTextureRect().a.x),
				static_cast<float>(luaL_checknumber(L, 3) + p->GetSprite()->getTextureRect().a.y))
			);
			return 0;
		}

		static int SetAnimationScale(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsSpriteCloned())
				return luaL_error(L, "SetAnimationScale on animation '%s' is invalid, please set each sprite separately.");
			float x = (float)luaL_checknumber(L, 2);
			for (size_t i = 0; i < p->GetCount(); ++i)
				p->GetSprite((uint32_t)i)->GetSprite()->setUnitsPerPixel(x);
			return 0;
		}
		static int GetAnimationScale(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsSpriteCloned())
				return luaL_error(L, "GetAnimationScale on animation '%s' is invalid, please get from each sprite separately.");
			lua_pushnumber(L, p->GetSprite(0)->GetSprite()->getUnitsPerPixel());
			return 1;
		}
		static int SetAnimationState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
			{
				p->SetVertexColor(*Color::Cast(L, 3));
			}
			else if (lua_gettop(L) == 6)
			{
				core::Color4B tColors[] = {
					*Color::Cast(L, 3),
					*Color::Cast(L, 4),
					*Color::Cast(L, 5),
					*Color::Cast(L, 6)
				};
				p->SetVertexColor(tColors);
			}
			return 0;
		}
		static int SetAnimationCenter(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsSpriteCloned())
				return luaL_error(L, "SetAnimationCenter on animation '%s' is invalid, please set each sprite separately.");
			for (size_t i = 0; i < p->GetCount(); ++i)
			{
				p->GetSprite((uint32_t)i)->GetSprite()->setTextureCenter(core::Vector2F(
					static_cast<float>(luaL_checknumber(L, 2) + p->GetSprite((uint32_t)i)->GetSprite()->getTextureRect().a.x),
					static_cast<float>(luaL_checknumber(L, 3) + p->GetSprite((uint32_t)i)->GetSprite()->getTextureRect().a.y)
				));
			}
			return 0;
		}

		static int SetFontState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceFont> p = LRES.FindSpriteFont(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "sprite font '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
			{
				p->SetBlendColor(*Color::Cast(L, 3));
			}
			return 0;
		}

		static int CacheTTFString(lua_State* L) {
			size_t len = 0;
			const char* str = luaL_checklstring(L, 2, &len);
			LRES.CacheTTFFontString(luaL_checkstring(L, 1), str, len);
			return 0;
		}

		static int TransferResource(lua_State* L) noexcept
		{
			const char* src_pool_name = luaL_checkstring(L, 1);
			const char* dst_pool_name = luaL_checkstring(L, 2);
			ResourceType type = static_cast<ResourceType>(luaL_checkinteger(L, 3));
			const char* res_name = luaL_checkstring(L, 4);

			ResourcePool* src = LRES.GetPool(src_pool_name);
			if (!src)
				return luaL_error(L, "TransferResource: source pool '%s' not found", src_pool_name);
			ResourcePool* dst = LRES.GetPool(dst_pool_name);
			if (!dst)
				return luaL_error(L, "TransferResource: destination pool '%s' not found", dst_pool_name);

			if (!src->TransferResourceTo(type, res_name, dst))
				return luaL_error(L, "TransferResource: failed to transfer '%s' from '%s' to '%s'", res_name, src_pool_name, dst_pool_name);

			return 0;
		}
	};

	luaL_Reg const lib[] = {
		{ "SetResLoadInfo", &Wrapper::SetResLoadInfo },
		{ "CreateResourcePool", &Wrapper::CreateResourcePool },
		{ "RemoveResourcePool", &Wrapper::RemoveResourcePool },
		{ "SetResourceStatus", &Wrapper::SetResourceStatus },
		{ "GetResourceStatus", &Wrapper::GetResourceStatus },
		{ "LoadTexture", &Wrapper::LoadTexture },
		{ "LoadTextureBin", &Wrapper::LoadTextureBin },
		{ "LoadImage", &Wrapper::LoadSprite },
		{ "LoadAnimation", &Wrapper::LoadAnimation },
		{ "LoadPS", &Wrapper::LoadPS },
		{ "LoadSound", &Wrapper::LoadSound },
		{ "LoadMusic", &Wrapper::LoadMusic },
		{ "LoadFont", &Wrapper::LoadFont },
		{ "LoadTTF", &Wrapper::LoadTTF },
		{ "LoadTrueTypeFont", &Wrapper::LoadTrueTypeFont },
		{ "LoadFX", &Wrapper::LoadFX },
		{ "LoadModel", &Wrapper::LoadModel },
		{ "LoadVideo", &Wrapper::LoadVideo },
		{ "SetVideoState", &Wrapper::SetVideoState },
		{ "CreateRenderTarget", &Wrapper::CreateRenderTarget },
		{ "IsRenderTarget", &Wrapper::IsRenderTarget },
		{ "SetTexturePreMulAlphaState", &Wrapper::SetTexturePreMulAlphaState },
		{ "SetTextureSamplerState", &Wrapper::SetTextureSamplerState },
		{ "GetTextureSize", &Wrapper::GetTextureSize },
		{ "GetTextureHandle", &Wrapper::GetTextureHandle },
		{ "RemoveResource", &Wrapper::RemoveResource },
		{ "CheckRes", &Wrapper::CheckRes },
		{ "EnumRes", &Wrapper::EnumRes },

		{ "SetImageScale", &Wrapper::SetImageScale },
		{ "GetImageScale", &Wrapper::GetImageScale },
		{ "SetImageState", &Wrapper::SetImageState },
		{ "SetImageCenter", &Wrapper::SetImageCenter },

		{ "SetAnimationScale", &Wrapper::SetAnimationScale },
		{ "GetAnimationScale", &Wrapper::GetAnimationScale },
		{ "SetAnimationState", &Wrapper::SetAnimationState },
		{ "SetAnimationCenter", &Wrapper::SetAnimationCenter },

		{ "SetFontState", &Wrapper::SetFontState },

		{ "CacheTTFString", &Wrapper::CacheTTFString },
		{ "TransferResource", &Wrapper::TransferResource },
		{ NULL, NULL },
	};

	luaL_Reg const lib_empty[] = {
		{ NULL, NULL },
	};

	luaL_register(L, LUASTG_LUA_LIBNAME, lib);                    // ??? lstg
	luaL_register(L, LUASTG_LUA_LIBNAME ".ResourceManager", lib); // ??? lstg lstg.ResourceManager
	lua_setfield(L, -1, "ResourceManager");                       // ??? lstg
	lua_pop(L, 1);                                                // ???
}
