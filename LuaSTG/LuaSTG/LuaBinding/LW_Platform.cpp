#include "LuaBinding/LuaWrapper.hpp"
#include "Platform/KnownDirectory.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include "utf8.hpp"

static const char* getString(lua_State* L, int index, const char* name, const char* defaultValue = nullptr) {
	lua_getfield(L, index, name);
	const char* value = lua_isnil(L, -1) ? defaultValue : luaL_checkstring(L, -1);
	lua_pop(L, 1);
	return value;
}

void luastg::binding::Platform::Register(lua_State* L) noexcept
{
	struct Wrapper
	{
		static int GetLocalAppDataPath(lua_State* L) noexcept
		{
			try
			{
				std::string path;
				if (::Platform::KnownDirectory::getLocalAppData(path))
				{
					lua_pushstring(L, path.c_str());
				}
				else
				{
					lua_pushstring(L, "");
				}
			}
			catch (const std::bad_alloc&)
			{
				lua_pushstring(L, "");
			}
			return 1;
		}
		static int GetRoamingAppDataPath(lua_State* L) noexcept
		{
			try
			{
				std::string path;
				if (::Platform::KnownDirectory::getRoamingAppData(path))
				{
					lua_pushstring(L, path.c_str());
				}
				else
				{
					lua_pushstring(L, "");
				}
			}
			catch (const std::bad_alloc&)
			{
				lua_pushstring(L, "");
			}
			return 1;
		}
		static int Execute(lua_State* L) noexcept
		{
#ifdef DEFINE_EXECUTE_API_FUNCTION
			struct Detail_
			{
				static bool Execute(const char* path, const char* args, const char* directory, bool bWait, bool bShow) noexcept
				{
					std::wstring tPath, tArgs, tDirectory;

					try
					{
						tPath = utf8::to_wstring(path);
						tArgs = utf8::to_wstring(args);
						if (directory)
							tDirectory = utf8::to_wstring(directory);

						SHELLEXECUTEINFO tShellExecuteInfo;
						memset(&tShellExecuteInfo, 0, sizeof(SHELLEXECUTEINFO));

						tShellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFO);
						tShellExecuteInfo.fMask = bWait ? SEE_MASK_NOCLOSEPROCESS : 0;
						tShellExecuteInfo.lpVerb = L"open";
						tShellExecuteInfo.lpFile = tPath.c_str();
						tShellExecuteInfo.lpParameters = tArgs.c_str();
						tShellExecuteInfo.lpDirectory = directory ? tDirectory.c_str() : nullptr;
						tShellExecuteInfo.nShow = bShow ? SW_SHOWDEFAULT : SW_HIDE;
						
						if (FALSE == ShellExecuteEx(&tShellExecuteInfo))
							return false;

						if (bWait)
						{
							WaitForSingleObject(tShellExecuteInfo.hProcess, INFINITE);
							CloseHandle(tShellExecuteInfo.hProcess);
						}
						return true;
					}
					catch (const std::bad_alloc&)
					{
						return false;
					}
				}
			};

			const char* path = luaL_checkstring(L, 1);
			const char* args = luaL_optstring(L, 2, "");
			const char* directory = luaL_optstring(L, 3, NULL);
			bool bWait = true;
			bool bShow = true;
			if (lua_gettop(L) >= 4)
				bWait = lua_toboolean(L, 4) == 0 ? false : true;
			if (lua_gettop(L) >= 5)
				bShow = lua_toboolean(L, 5) == 0 ? false : true;
			
			lua_pushboolean(L, Detail_::Execute(path, args, directory, bWait, bShow));
			return 1;
#else
			luaL_error(L, "Execute() is disabled in your LuaSTG-Flux build for security reasons.\nIf you have to use it, please build the engine yourself with the flag turned on.");
			return 0;
#endif
		}

		static int OpenFolder(lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);

			std::string fixedPath(path);
			std::replace(fixedPath.begin(), fixedPath.end(), '/', '\\');

			HINSTANCE result = ShellExecuteW(
				nullptr,
				L"open",
				utf8::to_wstring(fixedPath).c_str(),
				nullptr,
				nullptr,
				SW_SHOWNORMAL
			);

			lua_pushboolean(L, (intptr_t)result > 32);
			return 1;
		}

		static int SaveFileDialog(lua_State* L)
		{
			luaL_checktype(L, 1, LUA_TTABLE);

			// 1. Create save dialog
			IFileSaveDialog* dialog = nullptr;
			HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));

			if (FAILED(hr))
				return luaL_error(L, "Failed to create file save dialog");

			// 2. Set title & labels
			const char* title = getString(L, 1, "title");
			const char* okButtonLabel = getString(L, 1, "saveButtonLabel");
			const char* fileNameLabel = getString(L, 1, "fileNameLabel");

			if (title) dialog->SetTitle(utf8::to_wstring(title).c_str());
			if (okButtonLabel) dialog->SetOkButtonLabel(utf8::to_wstring(okButtonLabel).c_str());
			if (fileNameLabel) dialog->SetFileNameLabel(utf8::to_wstring(fileNameLabel).c_str());

			// 3. Set filename & default extension
			const char* fileName = getString(L, 1, "fileName", "default_name");
			const char* defaultExtension = getString(L, 1, "defaultExtension");

			dialog->SetFileName(utf8::to_wstring(fileName).c_str());
			if (defaultExtension) dialog->SetDefaultExtension(utf8::to_wstring(defaultExtension).c_str());

			// 4. Specify folder to open dialog in
			const char* defaultFolder = getString(L, 1, "defaultFolder", ".");
			const char* forcedFolder = getString(L, 1, "folder");

			std::string fixedPath(forcedFolder ? forcedFolder : defaultFolder);
			std::replace(fixedPath.begin(), fixedPath.end(), '/', '\\');

			auto widePath = utf8::to_wstring(fixedPath);
			DWORD length = GetFullPathNameW(widePath.c_str(), 0, nullptr, nullptr);
			std::wstring absolutePath(length, L'\0');
			GetFullPathNameW(widePath.c_str(), length, absolutePath.data(), nullptr);

			IShellItem* folder = nullptr;
			hr = SHCreateItemFromParsingName(absolutePath.c_str(), nullptr, IID_PPV_ARGS(&folder));

			if (SUCCEEDED(hr)) {
				if (forcedFolder) dialog->SetFolder(folder);
				else dialog->SetDefaultFolder(folder);

				folder->Release();
			}

			// 5. Setup file type filters
			size_t fileTypeIndex = 1;
			std::vector<std::wstring> filterNames;
			std::vector<std::wstring> filterSpecs;
			std::vector<COMDLG_FILTERSPEC> filters;

			lua_getfield(L, 1, "fileTypes");

			if (!lua_isnil(L, -1)) {
				luaL_checktype(L, -1, LUA_TTABLE);
				const size_t count = lua_objlen(L, -1);

				filterNames.reserve(count);
				filterSpecs.reserve(count);
				filters.reserve(count);

				for (size_t i = 1; i <= count; ++i) {
					lua_rawgeti(L, -1, (int)i);
					luaL_checktype(L, -1, LUA_TTABLE);

					lua_getfield(L, -1, "name");
					const char* name = luaL_checkstring(L, -1);
					filterNames.push_back(utf8::to_wstring(name));
					lua_pop(L, 1);

					lua_getfield(L, -1, "extension");
					const char* extension = luaL_checkstring(L, -1);
					filterSpecs.push_back(utf8::to_wstring(extension));
					lua_pop(L, 1);

					lua_getfield(L, -1, "selected");
					if (!lua_isnil(L, -1) && lua_toboolean(L, -1))
						fileTypeIndex = i;
					lua_pop(L, 1);

					filters.push_back({
						filterNames.back().c_str(),
						filterSpecs.back().c_str()
					});
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);

			filters.push_back({ L"All Files", L"*.*" });
			dialog->SetFileTypes((UINT)filters.size(), filters.data());

			if (fileTypeIndex > 1 && fileTypeIndex <= filters.size())
				dialog->SetFileTypeIndex((UINT)fileTypeIndex);

			// 6. Show dialog & return chosen path
			hr = dialog->Show(nullptr);

			if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
				dialog->Release();
				lua_pushnil(L);
				return 1;
			}

			if (FAILED(hr)) {
				dialog->Release();
				return luaL_error(L, "Failed to show file save dialog");
			}

			IShellItem* result = nullptr;
			hr = dialog->GetResult(&result);

			if (FAILED(hr)) {
				dialog->Release();
				return luaL_error(L, "Failed to get file save dialog result");
			}

			PWSTR path = nullptr;
			hr = result->GetDisplayName(SIGDN_FILESYSPATH, &path);

			if (FAILED(hr)) {
				result->Release();
				dialog->Release();
				return luaL_error(L, "Failed to get selected file path");
			}

			lua_pushstring(L, utf8::to_string(path).c_str());

			CoTaskMemFree(path);
			result->Release();
			dialog->Release();
			return 1;
		}

		static int OpenFileDialog(lua_State* L)
		{
			luaL_checktype(L, 1, LUA_TTABLE);

			// 1. Create open dialog
			IFileOpenDialog* dialog = nullptr;
			HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));

			if (FAILED(hr))
				return luaL_error(L, "Failed to create file open dialog");

			// 2. Set title & labels
			const char* title = getString(L, 1, "title");
			const char* okButtonLabel = getString(L, 1, "openButtonLabel");
			const char* fileNameLabel = getString(L, 1, "fileNameLabel");

			if (title) dialog->SetTitle(utf8::to_wstring(title).c_str());
			if (okButtonLabel) dialog->SetOkButtonLabel(utf8::to_wstring(okButtonLabel).c_str());
			if (fileNameLabel) dialog->SetFileNameLabel(utf8::to_wstring(fileNameLabel).c_str());

			// 3. Specify folder to open dialog in
			const char* defaultFolder = getString(L, 1, "defaultFolder", ".");
			const char* forcedFolder = getString(L, 1, "folder");

			std::string fixedPath(forcedFolder ? forcedFolder : defaultFolder);
			std::replace(fixedPath.begin(), fixedPath.end(), '/', '\\');

			auto widePath = utf8::to_wstring(fixedPath);
			DWORD length = GetFullPathNameW(widePath.c_str(), 0, nullptr, nullptr);
			std::wstring absolutePath(length, L'\0');
			GetFullPathNameW(widePath.c_str(), length, absolutePath.data(), nullptr);

			IShellItem* folder = nullptr;
			hr = SHCreateItemFromParsingName(absolutePath.c_str(), nullptr, IID_PPV_ARGS(&folder));

			if (SUCCEEDED(hr)) {
				if (forcedFolder) dialog->SetFolder(folder);
				else dialog->SetDefaultFolder(folder);

				folder->Release();
			}

			// 4. Setup file type filters
			size_t fileTypeIndex = 1;
			std::vector<std::wstring> filterNames;
			std::vector<std::wstring> filterSpecs;
			std::vector<COMDLG_FILTERSPEC> filters;

			lua_getfield(L, 1, "fileTypes");

			if (!lua_isnil(L, -1)) {
				luaL_checktype(L, -1, LUA_TTABLE);
				const size_t count = lua_objlen(L, -1);

				filterNames.reserve(count);
				filterSpecs.reserve(count);
				filters.reserve(count + 1);

				for (size_t i = 1; i <= count; ++i) {
					lua_rawgeti(L, -1, (int)i);
					luaL_checktype(L, -1, LUA_TTABLE);

					lua_getfield(L, -1, "name");
					const char* name = luaL_checkstring(L, -1);
					filterNames.push_back(utf8::to_wstring(name));
					lua_pop(L, 1);

					lua_getfield(L, -1, "extension");
					const char* extension = luaL_checkstring(L, -1);
					filterSpecs.push_back(utf8::to_wstring(extension));
					lua_pop(L, 1);

					lua_getfield(L, -1, "selected");
					if (!lua_isnil(L, -1) && lua_toboolean(L, -1))
						fileTypeIndex = i;
					lua_pop(L, 1);

					filters.push_back({
						filterNames.back().c_str(),
						filterSpecs.back().c_str()
					});
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);

			filters.push_back({ L"All Files", L"*.*" });
			dialog->SetFileTypes((UINT)filters.size(), filters.data());

			if (fileTypeIndex > 1 && fileTypeIndex <= filters.size())
				dialog->SetFileTypeIndex((UINT)fileTypeIndex);

			// 5. Show dialog & return chosen file
			hr = dialog->Show(nullptr);

			if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
				dialog->Release();
				lua_pushnil(L);
				return 1;
			}

			if (FAILED(hr)) {
				dialog->Release();
				return luaL_error(L, "Failed to show file open dialog");
			}

			IShellItem* result = nullptr;
			hr = dialog->GetResult(&result);

			if (FAILED(hr)) {
				dialog->Release();
				return luaL_error(L, "Failed to get selected file");
			}

			PWSTR path = nullptr;
			hr = result->GetDisplayName(SIGDN_FILESYSPATH, &path);

			if (FAILED(hr)) {
				result->Release();
				dialog->Release();
				return luaL_error(L, "Failed to get selected file path");
			}

			lua_pushstring(L, utf8::to_string(path).c_str());

			CoTaskMemFree(path);
			result->Release();
			dialog->Release();
			return 1;
		}

		static int api_MessageBox(lua_State* L)
		{
			char const* title = luaL_checkstring(L, 1);
			char const* text = luaL_checkstring(L, 2);
			UINT flags = (UINT)luaL_checkinteger(L, 3);
			int result = MessageBoxW(
				(LAPP.GetAppModel() && LAPP.GetAppModel()->getWindow()) ? (HWND)LAPP.GetAppModel()->getWindow()->getNativeHandle() : NULL,
				utf8::to_wstring(text).c_str(),
				utf8::to_wstring(title).c_str(),
				flags);
			lua_pushinteger(L, result);
			return 1;
		}
	};

	luaL_Reg const lib[] = {
		{ "GetLocalAppDataPath", &Wrapper::GetLocalAppDataPath },
		{ "GetRoamingAppDataPath", &Wrapper::GetRoamingAppDataPath },
		{ "Execute", &Wrapper::Execute },
		{ "OpenFolder", &Wrapper::OpenFolder },
		{ "SaveFileDialog", &Wrapper::SaveFileDialog },
		{ "OpenFileDialog", &Wrapper::OpenFileDialog },
		{ "MessageBox", &Wrapper::api_MessageBox },
		{ NULL, NULL },
	};

	luaL_register(L, LUASTG_LUA_LIBNAME, lib);             // ??? lstg
	luaL_register(L, LUASTG_LUA_LIBNAME ".Platform", lib); // ??? lstg lstg.Platform
	lua_setfield(L, -1, "Platform");                       // ??? lstg
	lua_pop(L, 1);                                         // ???
}
