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
