// Supported markups:
// [b]bold[/b]
// [i]italic[/i]
// [u]underline[/u]
// [s]strikethrough[/s]
// [color=#RRGGBB] or [color=#AARRGGBB] ... [/color]
// [size=N]sized[/size]
// [ruby=reading]base[/ruby]
// [wave]text[/wave]
// [wave amp=N speed=N]...[/wave]
// [shake]text[/shake]
// [shake i=N]...[/shake]
// [gradient=#RRGGBB,#RRGGBB]text[/gradient]  (also: [gradiant=...])

// If anyone looks at that file. I'm sorry. It's....it was pain.

#include "RichText.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <charconv>
#include <chrono>
#include "Platform/HResultChecker.hpp"
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include "AppFrame.h"
#include "utf8.hpp"
#include "LuaBinding/LuaWrapper.hpp"
#include "lua/plus.hpp"
#include "Core/Graphics/Sprite.hpp"
#include "LuaBinding/LuaWrapperMisc.hpp"
#include "GameResource/LegacyBlendStateHelper.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOSERVICE
#define NOMCX
#define NOIME

#include <sdkddkver.h>
#include <Windows.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#undef GetTextMetrics

#include <d2d1_3.h>
#include <d2d1effects.h>
#include <dwrite_3.h>
#include <wincodec.h>

namespace {
	struct RichTextDLLLoader {
		HRESULT(WINAPI* D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, D2D1_FACTORY_OPTIONS const*, void**){};
		HRESULT(WINAPI* DWriteCreateFactory)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**){};

		RichTextDLLLoader() {
			if (HMODULE h = LoadLibraryW(L"d2d1.dll"))
				D2D1CreateFactory = (decltype(D2D1CreateFactory))GetProcAddress(h, "D2D1CreateFactory");
			if (HMODULE h = LoadLibraryW(L"dwrite.dll"))
				DWriteCreateFactory = (decltype(DWriteCreateFactory))GetProcAddress(h, "DWriteCreateFactory");
		}
	};

	static RichTextDLLLoader g_dll;

	inline D2D1::ColorF toD2D1Color(core::Color4B c) {
		return D2D1::ColorF(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
	}

	inline float hashRandF(int seed) {
		unsigned u = (unsigned)seed;
		u = (u ^ 61u) ^ (u >> 16u);
		u *= 9u;
		u ^= u >> 4u;
		u *= 0x27d4eb2du;
		u ^= u >> 15u;
		return (float)(u & 0xFFFFu) / 32767.5f - 1.f;
	}

	inline Microsoft::WRL::ComPtr<IDWriteFactory> const& getSharedDWriteFactory() {
		static Microsoft::WRL::ComPtr<IDWriteFactory> factory = [] {
			Microsoft::WRL::ComPtr<IDWriteFactory> f;
			if (g_dll.DWriteCreateFactory)
				g_dll.DWriteCreateFactory(
					DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
					reinterpret_cast<IUnknown**>(f.GetAddressOf()));
			return f;
		}();
		return factory;
	}

	class FontFileStream;
	class FontFileLoader;
	class FontFileEnumerator;
	class FontCollectionLoader;

	class FontFileStream final : public IDWriteFontFileStream {
		std::atomic<ULONG> m_ref{ 1 };
		core::SmartReference<core::IData> m_data;
	public:
		HRESULT WINAPI QueryInterface(IID const& id, void** out) noexcept override {
			if (id == __uuidof(IUnknown) || id == __uuidof(IDWriteFontFileStream)) {
				AddRef();
				*out = static_cast<IDWriteFontFileStream*>(this);
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG WINAPI AddRef() noexcept override { return ++m_ref; }
		ULONG WINAPI Release() noexcept override {
			ULONG r = --m_ref;
			if (!r)
				delete this;
			return r;
		}

		HRESULT WINAPI ReadFileFragment(void const** start, UINT64 off, UINT64 sz, void** ctx) noexcept override {
			if (!m_data)
				return E_FAIL;
			if (off + sz > m_data->size())
				return E_INVALIDARG;
			*start = static_cast<uint8_t*>(m_data->data()) + (size_t)off;
			*ctx = static_cast<uint8_t*>(m_data->data()) + (size_t)off;
			return S_OK;
		}
		void WINAPI ReleaseFileFragment(void*) noexcept override {}
		HRESULT WINAPI GetFileSize(UINT64* sz) noexcept override {
			*sz = m_data ? m_data->size() : 0;
			return S_OK;
		}
		HRESULT WINAPI GetLastWriteTime(UINT64*) noexcept override { return E_NOTIMPL; }

		bool load(std::string_view path) {
			return core::FileSystemManager::readFile(path, m_data.put());
		}
		void setData(core::IData* data) {
			m_data = data;
		}
	};

	class FontFileLoader final : public IDWriteFontFileLoader {
		std::atomic<ULONG> m_ref{ 1 };
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<FontFileStream>> m_cache;
	public:
		HRESULT WINAPI QueryInterface(IID const& id, void** out) noexcept override {
			if (id == __uuidof(IUnknown)) {
				AddRef();
				*out = static_cast<IUnknown*>(this);
				return S_OK;
			}
			if (id == __uuidof(IDWriteFontFileLoader)) {
				AddRef();
				*out = static_cast<IDWriteFontFileLoader*>(this);
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG WINAPI AddRef() noexcept override { return ++m_ref; }
		ULONG WINAPI Release() noexcept override {
			ULONG r = --m_ref;
			if (!r)
				delete this;
			return r;
		}

		HRESULT WINAPI CreateStreamFromKey(void const* key, UINT32 keySize, IDWriteFontFileStream** out) noexcept override {
			try {
				std::string path((char const*)key, keySize);
				auto it = m_cache.find(path);
				if (it != m_cache.end()) {
					it->second->AddRef();
					*out = it->second.Get();
					return S_OK;
				}
				Microsoft::WRL::ComPtr<FontFileStream> s;
				s.Attach(new FontFileStream());
				if (!s->load(path))
					return E_FAIL;
				s->AddRef();
				*out = s.Get();
				m_cache.emplace(std::move(path), std::move(s));
				return S_OK;
			} catch (...) { return E_OUTOFMEMORY; }
		}

		void preload(std::string const& key, core::IData* data) {
			if (!data || m_cache.count(key))
				return;
			Microsoft::WRL::ComPtr<FontFileStream> s;
			s.Attach(new FontFileStream());
			s->setData(data);
			m_cache.emplace(key, std::move(s));
		}
	};

	class FontFileEnumerator final : public IDWriteFontFileEnumerator {
		std::atomic<ULONG> m_ref{ 1 };
		Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
		Microsoft::WRL::ComPtr<IDWriteFontFileLoader> m_loader;
		std::vector<std::string> m_paths;
		LONG m_idx{ -1 };
	public:
		HRESULT WINAPI QueryInterface(IID const& id, void** out) noexcept override {
			if (id == __uuidof(IUnknown)) {
				AddRef();
				*out = static_cast<IUnknown*>(this);
				return S_OK;
			}
			if (id == __uuidof(IDWriteFontFileEnumerator)) {
				AddRef();
				*out = static_cast<IDWriteFontFileEnumerator*>(this);
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG WINAPI AddRef() noexcept override { return ++m_ref; }
		ULONG WINAPI Release() noexcept override {
			ULONG r = --m_ref;
			if (!r)
				delete this;
			return r;
		}

		void init(IDWriteFactory* f, IDWriteFontFileLoader* l, std::vector<std::string> paths) {
			m_factory = f; m_loader = l; m_paths = std::move(paths);
		}
		HRESULT WINAPI MoveNext(BOOL* has) noexcept override {
			m_idx++;
			*has = (m_idx >= 0 && m_idx < (LONG)m_paths.size()) ? TRUE : FALSE;
			return S_OK;
		}
		HRESULT WINAPI GetCurrentFontFile(IDWriteFontFile** out) noexcept override {
			if (m_idx < 0 || m_idx >= (LONG)m_paths.size())
				return E_FAIL;
			auto const& p = m_paths[(size_t)m_idx];
			if (!p.empty() && p[0] == '@')
				return m_factory->CreateCustomFontFileReference(p.data(), (UINT32)p.size(), m_loader.Get(), out);
			if (core::FileSystemManager::hasFile(p))
				return m_factory->CreateCustomFontFileReference(p.data(), (UINT32)p.size(), m_loader.Get(), out);
			std::wstring wp = utf8::to_wstring(p);
			return m_factory->CreateFontFileReference(wp.c_str(), nullptr, out);
		}
	};

	class FontCollectionLoader final : public IDWriteFontCollectionLoader {
		std::atomic<ULONG> m_ref{ 1 };
		Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
		Microsoft::WRL::ComPtr<IDWriteFontFileLoader> m_fileLoader;
		std::vector<std::string> m_paths;
	public:
		HRESULT WINAPI QueryInterface(IID const& id, void** out) noexcept override {
			if (id == __uuidof(IUnknown)) {
				AddRef();
				*out = static_cast<IUnknown*>(this);
				return S_OK;
			}
			if (id == __uuidof(IDWriteFontCollectionLoader)) {
				AddRef();
				*out = static_cast<IDWriteFontCollectionLoader*>(this);
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG WINAPI AddRef() noexcept override { return ++m_ref; }
		ULONG WINAPI Release() noexcept override {
			ULONG r = --m_ref;
			if (!r)
				delete this;
			return r;
		}

		void init(IDWriteFactory* f, IDWriteFontFileLoader* l, std::vector<std::string> paths) {
			m_factory = f;
			m_fileLoader = l;
			m_paths = std::move(paths);
		}
		HRESULT WINAPI CreateEnumeratorFromKey(IDWriteFactory* f, void const*, UINT32, IDWriteFontFileEnumerator** out) noexcept override {
			try {
				auto* e = new FontFileEnumerator();
				e->init(f ? f : m_factory.Get(), m_fileLoader.Get(), m_paths);
				*out = e;
				return S_OK;
			} catch (...) { return E_OUTOFMEMORY; }
		}
	};

	struct FontCollectionCacheEntry {
		Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
		Microsoft::WRL::ComPtr<FontFileLoader> fileLoader;
		Microsoft::WRL::ComPtr<FontCollectionLoader> colLoader;
	};

	inline std::unordered_map<std::string, FontCollectionCacheEntry>& getFontCollectionCache() {
		static std::unordered_map<std::string, FontCollectionCacheEntry> cache;
		return cache;
	}

	struct GlyphGeoKey {
		IDWriteFontFace* face{};
		UINT16 glyphIndex{};
		float emSize{};
		bool sideways{};

		bool operator==(GlyphGeoKey const& o) const {
			return face == o.face && glyphIndex == o.glyphIndex && emSize == o.emSize && sideways == o.sideways;
		}
	};

	struct GlyphGeoKeyHash {
		size_t operator()(GlyphGeoKey const& k) const noexcept {
			size_t h = std::hash<void const*>{}(k.face);
			h ^= std::hash<UINT16>{}(k.glyphIndex) + 0x9e3779b9u + (h << 6) + (h >> 2);
			h ^= std::hash<float>{}(k.emSize) + 0x9e3779b9u + (h << 6) + (h >> 2);
			h ^= std::hash<bool>{}(k.sideways) + 0x9e3779b9u + (h << 6) + (h >> 2);
			return h;
		}
	};

	using GlyphGeometryCache = std::unordered_map<GlyphGeoKey, Microsoft::WRL::ComPtr<ID2D1PathGeometry>, GlyphGeoKeyHash>;

	struct DrawingEffect final : IUnknown {
		std::atomic<ULONG> m_ref{ 1 };

		core::Color4B fillColor{ 255, 255, 255, 255 };
		bool hasFillColor{ false };

		bool gradient{ false };
		bool gradientVertical{ false };
		std::vector<core::Color4B> gradientColors;
		float gradientSpanStartX{ 0.f };
		float gradientSpanWidth{ 1.f };
		float gradientSpanStartY{ 0.f };
		float gradientSpanHeight{ 1.f };

		bool wave{ false };
		float waveAmp{ 3.f };
		float waveSpeed{ 2.f };

		bool shake{ false };
		float shakeIntensity{ 1.5f };

		float time{ 0.f };

		HRESULT WINAPI QueryInterface(IID const& id, void** out) noexcept override {
			if (id == __uuidof(IUnknown)) {
				AddRef();
				*out = static_cast<IUnknown*>(this);
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG WINAPI AddRef() noexcept override { return ++m_ref; }
		ULONG WINAPI Release() noexcept override {
			ULONG r = --m_ref;
			if (!r)
				delete this;
			return r;
		}
	};

	struct RunStyle {
		bool bold{ false };
		bool italic{ false };
		bool underline{ false };
		bool strikethrough{ false };
		std::optional<core::Color4B> color;
		std::optional<float> size;
		bool wave{ false };
		float waveAmp{ 3.f };
		float waveSpeed{ 2.f };
		bool shake{ false };
		float shakeIntensity{ 1.5f };
		bool gradient{ false };
		bool gradientVertical{ false };
		std::vector<core::Color4B> gradientColors;
	};

	struct ParsedRun {
		uint32_t start{};
		uint32_t length{};
		RunStyle style;
	};

	struct RubyAnnotation {
		uint32_t start{};
		uint32_t length{};
		std::wstring rubyText;
	};

	struct ParseResult {
		std::wstring plainText;
		std::vector<ParsedRun> runs;
		std::vector<RubyAnnotation> rubies;
		bool hasAnimation{ false };
	};

	static std::string_view trimSV(std::string_view sv) {
		while (!sv.empty() && (unsigned char)sv.front() <= ' ')
			sv.remove_prefix(1);
		while (!sv.empty() && (unsigned char)sv.back() <= ' ')
			sv.remove_suffix(1);
		return sv;
	}

	static bool parseHexColor(std::string_view s, core::Color4B& out) {
		if (!s.empty() && s[0] == '#')
			s.remove_prefix(1);
		if (s.size() == 6) {
			unsigned v = 0;
			auto [p, ec] = std::from_chars(s.data(), s.data() + 6, v, 16);
			if (ec != std::errc{})
				return false;
			out.r = (uint8_t)(v >> 16);
			out.g = (uint8_t)(v >> 8);
			out.b = (uint8_t)(v);
			out.a = 255;
			return true;
		}
		if (s.size() == 8) {
			unsigned v = 0;
			auto [p, ec] = std::from_chars(s.data(), s.data() + 8, v, 16);
			if (ec != std::errc{})
				return false;
			out.a = (uint8_t)(v >> 24);
			out.r = (uint8_t)(v >> 16);
			out.g = (uint8_t)(v >> 8);
			out.b = (uint8_t)(v);
			return true;
		}
		return false;
	}

	static float parseFloat(std::string_view s, float def = 0.f) {
		s = trimSV(s);
		float v = def;
		std::from_chars(s.data(), s.data() + s.size(), v);
		return v;
	}

	// help.
	static std::unordered_map<std::string, std::string> parseAttrs(std::string_view s) {
		std::unordered_map<std::string, std::string> m;
		size_t i = 0;
		while (i < s.size()) {
			while (i < s.size() && (unsigned char)s[i] <= ' ')
				++i;
			if (i >= s.size())
				break;
			size_t k0 = i;
			while (i < s.size() && s[i] != '=' && (unsigned char)s[i] > ' ')
				++i;
			std::string key(s.substr(k0, i - k0));
			if (i >= s.size() || s[i] != '=') {
				if (!key.empty())
					m[key] = "";
				continue;
			}
			++i;
			std::string val;
			if (i < s.size() && s[i] == '"') {
				++i;
				size_t v0 = i;
				while (i < s.size() && s[i] != '"')
					++i;
				val = std::string(s.substr(v0, i - v0));
				if (i < s.size())
					++i;
			} else {
				size_t v0 = i;
				while (i < s.size() && (unsigned char)s[i] > ' ')
					++i;
				val = std::string(s.substr(v0, i - v0));
			}
			if (!key.empty()) m[std::move(key)] = std::move(val);
		}
		return m;
	}

	static std::string toLower(std::string_view sv) {
		std::string r(sv);
		for (char& c : r)
			if (c >= 'A' && c <= 'Z')
				c += ('a' - 'A');
		return r;
	}

	// help even more.
	static bool parseTag(std::string_view raw, bool& isClose, std::string& tagName,
			std::unordered_map<std::string, std::string>& attrs) {
		if (raw.size() < 2)
			return false;
		if (raw.front() != '[' || raw.back() != ']')
			return false;
		raw = raw.substr(1, raw.size() - 2);
		isClose = (!raw.empty() && raw[0] == '/');
		if (isClose)
			raw.remove_prefix(1);
		size_t i = 0;
		while (i < raw.size() && (unsigned char)raw[i] > ' ' && raw[i] != '=')
			++i;
		tagName = toLower(raw.substr(0, i));
		if (tagName.empty())
			return false;
		if (i < raw.size() && raw[i] == '=') {
			attrs[""] = std::string(trimSV(raw.substr(i + 1)));
		} else if (i < raw.size()) {
			attrs = parseAttrs(raw.substr(i));
		}
		return true;
	}

	// BIG HELP EVEN EVEN MORE
	static ParseResult parseRichText(std::string_view utf8src) {
		ParseResult result;

		std::wstring wsrc = utf8::to_wstring(utf8src);

		struct StackEntry {
			std::string name;
			uint32_t start{};
			RunStyle style;
			std::wstring rubyText;
		};
		std::vector<StackEntry> stk;

		RunStyle curStyle;

		size_t pos = 0;
		while (pos < wsrc.size()) {
			if (wsrc[pos] != L'[') {
				result.plainText += wsrc[pos++];
				continue;
			}
			size_t end = wsrc.find(L']', pos);
			if (end == std::wstring::npos) {
				result.plainText += wsrc[pos++];
				continue;
			}
			std::string tagRaw = utf8::to_string(wsrc.substr(pos, end - pos + 1));
			bool isClose = false;
			std::string tagName;
			std::unordered_map<std::string, std::string> attrs;
			if (!parseTag(tagRaw, isClose, tagName, attrs)) {
				result.plainText += wsrc[pos++];
				continue;
			}
			pos = end + 1;

			if (isClose) {
				for (int si = (int)stk.size() - 1; si >= 0; --si) {
					if (stk[(size_t)si].name != tagName)
						continue;
					StackEntry entry = std::move(stk[(size_t)si]);
					stk.erase(stk.begin() + si);
					uint32_t startPos = entry.start;
					uint32_t len = (uint32_t)result.plainText.size() - startPos;
					if (len > 0) {
						if (tagName == "ruby") {
							RubyAnnotation ann;
							ann.start = startPos;
							ann.length = len;
							ann.rubyText = entry.rubyText;
							result.rubies.push_back(std::move(ann));
						} else {
							ParsedRun run;
							run.start = startPos;
							run.length = len;
							run.style = entry.style;
							if (run.style.wave || run.style.shake)
								result.hasAnimation = true;
							result.runs.push_back(run);
						}
					}
					break;
				}
			} else {
				StackEntry entry;
				entry.name = tagName;
				entry.start = (uint32_t)result.plainText.size();
				entry.style = curStyle;

				if (tagName == "b") {
					entry.style.bold = true;
				} else if (tagName == "i") {
					entry.style.italic = true;
				} else if (tagName == "u") {
					entry.style.underline = true;
				} else if (tagName == "s") {
					entry.style.strikethrough = true;
				} else if (tagName == "color") {
					core::Color4B c;
					std::string_view val = attrs.count("") ? std::string_view(attrs.at("")) : std::string_view{};
					if (parseHexColor(val, c))
						entry.style.color = c;
				} else if (tagName == "size") {
					float sz = parseFloat(attrs.count("") ? attrs.at("") : "0", 0.f);
					if (sz > 0.f)
						entry.style.size = sz;
				} else if (tagName == "wave") {
					entry.style.wave = true;
					entry.style.waveAmp = parseFloat(attrs.count("amp") ? attrs.at("amp") : "3", 3.f);
					entry.style.waveSpeed = parseFloat(attrs.count("speed") ? attrs.at("speed") : "2", 2.f);
				} else if (tagName == "shake") {
					entry.style.shake = true;
					entry.style.shakeIntensity = parseFloat(attrs.count("i") ? attrs.at("i") : "1.5", 1.5f);
				} else if (tagName == "ruby") {
					std::string_view val = attrs.count("") ? std::string_view(attrs.at("")) : std::string_view{};
					entry.rubyText = utf8::to_wstring(val);
				} else if (tagName == "gradient" || tagName == "gradiant") {
					std::string_view val = attrs.count("") ? std::string_view(attrs.at("")) : std::string_view{};
					std::vector<core::Color4B> colors;
					std::string_view rem = val;
					while (!rem.empty()) {
						auto comma = rem.find(',');
						std::string_view token = (comma != std::string_view::npos) ? rem.substr(0, comma) : rem;
						token = trimSV(token);
						size_t sp = token.find(' ');
						if (sp != std::string_view::npos) token = token.substr(0, sp);
						core::Color4B c;
						if (parseHexColor(token, c))
							colors.push_back(c);
						if (comma == std::string_view::npos) break;
						rem = rem.substr(comma + 1);
					}
					if (colors.size() >= 2) {
						entry.style.gradient = true;
						entry.style.gradientColors = std::move(colors);
						std::string_view dir = attrs.count("dir") ? std::string_view(attrs.at("dir")) : std::string_view{};
						if (dir.empty()) {
							size_t d = val.find("dir=");
							if (d != std::string_view::npos) {
								std::string_view dv = val.substr(d + 4);
								size_t e = dv.find(' ');
								dir = (e != std::string_view::npos) ? dv.substr(0, e) : dv;
							}
						}
						entry.style.gradientVertical = (dir == "v" || dir == "vertical");
					}
				} else {
					result.plainText += L'[';
					result.plainText += utf8::to_wstring(tagRaw.substr(1));
					continue;
				}
				stk.push_back(std::move(entry));
			}
		}

		for (auto& entry : stk) {
			uint32_t len = (uint32_t)result.plainText.size() - entry.start;
			if (len > 0 && entry.name != "ruby") {
				ParsedRun run;
				run.start = entry.start;
				run.length = len;
				run.style = entry.style;
				if (run.style.wave || run.style.shake)
					result.hasAnimation = true;
				result.runs.push_back(run);
			}
		}

		return result;
	}

	struct GlyphRendererContext {
		core::Color4B defaultFillColor{ 255, 255, 255, 255 };
		float outlineWidth{ 0.f };
		core::Color4B outlineColor{ 0, 0, 0, 255 };
		bool renderOutlinePass{ false };
		float layoutOriginX{ 0.f };
		float layoutOriginY{ 0.f };

		ID2D1SolidColorBrush* reusableBrush{ nullptr };
		GlyphGeometryCache* geometryCache{ nullptr };
	};

	class RichTextGlyphRenderer final : public IDWriteTextRenderer {
		Microsoft::WRL::ComPtr<ID2D1Factory> m_factory;
		Microsoft::WRL::ComPtr<ID2D1RenderTarget> m_rt;
		Microsoft::WRL::ComPtr<ID2D1StrokeStyle> m_strokeStyle;
		GlyphRendererContext* m_ctx{ nullptr };
	public:
		void init(ID2D1Factory* f, ID2D1RenderTarget* rt, GlyphRendererContext* ctx) {
			m_factory = f;
			m_rt = rt;
			m_ctx = ctx;
			D2D1_STROKE_STYLE_PROPERTIES sp{};
			sp.startCap = sp.endCap = sp.dashCap = D2D1_CAP_STYLE_ROUND;
			sp.lineJoin = D2D1_LINE_JOIN_ROUND;
			m_factory->CreateStrokeStyle(sp, nullptr, 0, &m_strokeStyle);
		}

		HRESULT WINAPI QueryInterface(IID const& id, void** out) noexcept override {
			if (id == __uuidof(IUnknown) ||
			    id == __uuidof(IDWritePixelSnapping) ||
			    id == __uuidof(IDWriteTextRenderer)) {
				AddRef();
				*out = static_cast<IDWriteTextRenderer*>(this);
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG WINAPI AddRef() noexcept override { return 2; }
		ULONG WINAPI Release() noexcept override { return 1; }

		HRESULT WINAPI IsPixelSnappingDisabled(void*, BOOL* d) noexcept override {
			*d = FALSE;
			return S_OK;
		}
		HRESULT WINAPI GetCurrentTransform(void*, DWRITE_MATRIX* t) noexcept override {
			m_rt->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(t));
			return S_OK;
		}
		HRESULT WINAPI GetPixelsPerDip(void*, FLOAT* ppd) noexcept override {
			FLOAT x, y;
			m_rt->GetDpi(&x, &y);
			*ppd = x / 96.f;
			return S_OK;
		}

		HRESULT WINAPI DrawGlyphRun(
			void* /*ctx*/,
			FLOAT baseX,
			FLOAT baseY,
			DWRITE_MEASURING_MODE /*mode*/,
			DWRITE_GLYPH_RUN const* run,
			DWRITE_GLYPH_RUN_DESCRIPTION const* /*desc*/,
			IUnknown* effect) noexcept override
		{
			if (!m_ctx || !run || run->glyphCount == 0)
				return S_OK;

			DrawingEffect* de = effect ? static_cast<DrawingEffect*>(effect) : nullptr;

			auto resolveColor = [&](bool outline) -> core::Color4B {
				if (outline)
					return m_ctx->outlineColor;
				if (de && de->hasFillColor)
					return de->fillColor;
				return m_ctx->defaultFillColor;
			};

			auto lerpColor = [](core::Color4B a, core::Color4B b, float t) -> core::Color4B {
				auto l8 = [](uint8_t x, uint8_t y, float t) -> uint8_t {
					return (uint8_t)std::lround(x + (y - x) * t);
				};
				return core::Color4B(l8(a.r,b.r,t), l8(a.g,b.g,t), l8(a.b,b.b,t), l8(a.a,b.a,t));
			};

			bool doWave = de && de->wave;
			bool doShake = de && de->shake;
			bool doGradient = de && de->gradient;
			float time = de ? de->time : 0.f;
			float shakeFrac = 0.f;
			int shakeBucket = 0;
			if (doShake) {
				float t10 = time * 10.f;
				shakeBucket = (int)t10;
				shakeFrac = t10 - (float)shakeBucket;
			}

			if (!doWave && !doShake && !doGradient) {
				return drawSingleRun(run, baseX, baseY, resolveColor(m_ctx->renderOutlinePass));
			}

			Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> vertGradBrush;
			if (doGradient && de->gradientVertical && !m_ctx->renderOutlinePass) {
				auto const& gc = de->gradientColors;
				size_t const n = gc.size();
				if (n >= 2) {
					std::vector<D2D1_GRADIENT_STOP> stops(n);
					for (size_t i = 0; i < n; ++i) {
						stops[i].position = float(i) / float(n - 1);
						stops[i].color = toD2D1Color(gc[i]);
					}
					Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopColl;
					m_rt->CreateGradientStopCollection(stops.data(), UINT32(n), &stopColl);
					if (stopColl) {
						float spanTop = m_ctx->layoutOriginY + de->gradientSpanStartY;
						float spanBot = spanTop + de->gradientSpanHeight;
						m_rt->CreateLinearGradientBrush(
							D2D1::LinearGradientBrushProperties(
								D2D1::Point2F(0.f, spanTop),
								D2D1::Point2F(0.f, spanBot)),
							stopColl.Get(), &vertGradBrush);
					}
				}
			}

			float advX = baseX;
			for (UINT32 g = 0; g < run->glyphCount; ++g) {
				float gAdv = run->glyphAdvances ? run->glyphAdvances[g] : 0.f;
				float offX = 0.f, offY = 0.f;

				if (doWave) {
					float phase = de->waveSpeed * time + advX * 0.05f;
					offY = de->waveAmp * std::sin(phase);
				}
				if (doShake) {
					int k0 = (int)(advX) * 7 + shakeBucket;
					int k1 = k0 + 1;
					float ox = hashRandF(k0) + (hashRandF(k1) - hashRandF(k0)) * shakeFrac;
					float oy = hashRandF(k0 ^ 0x1234) + (hashRandF(k1 ^ 0x1234) - hashRandF(k0 ^ 0x1234)) * shakeFrac;
					offX = de->shakeIntensity * ox;
					offY += de->shakeIntensity * oy;
				}

				DWRITE_GLYPH_RUN sg{};
				sg.fontFace = run->fontFace;
				sg.fontEmSize = run->fontEmSize;
				sg.glyphCount = 1;
				sg.glyphIndices = run->glyphIndices ? run->glyphIndices + g : nullptr;
				sg.glyphAdvances = nullptr;
				sg.glyphOffsets = nullptr;
				sg.isSideways = run->isSideways;
				sg.bidiLevel = run->bidiLevel;

				if (vertGradBrush) {
					drawSingleRunWithBrush(&sg, advX + offX, baseY + offY, vertGradBrush.Get());
				} else {
					core::Color4B glyphColor;
					if (doGradient && !m_ctx->renderOutlinePass) {
						float t = 0.f;
						float layoutX = advX - m_ctx->layoutOriginX;
						if (de->gradientSpanWidth > 0.f)
							t = std::clamp((layoutX + gAdv * 0.5f - de->gradientSpanStartX) / de->gradientSpanWidth, 0.f, 1.f);
						auto const& gc = de->gradientColors;
						size_t const n = gc.size();
						if (n >= 2) {
							float scaled = t * float(n - 1);
							size_t i = (size_t)std::min((int)scaled, (int)(n - 2));
							glyphColor = lerpColor(gc[i], gc[i + 1], scaled - float(i));
						} else {
							glyphColor = resolveColor(false);
						}
					} else {
						glyphColor = resolveColor(m_ctx->renderOutlinePass);
					}
					drawSingleRun(&sg, advX + offX, baseY + offY, glyphColor);
				}

				if (run->glyphOffsets)
					advX += gAdv + run->glyphOffsets[g].advanceOffset;
				else
					advX += gAdv;
			}
			return S_OK;
		}

		HRESULT WINAPI DrawUnderline(void*, FLOAT bx, FLOAT by,
				DWRITE_UNDERLINE const* u, IUnknown* effect) noexcept override {
			DrawingEffect* de = effect ? static_cast<DrawingEffect*>(effect) : nullptr;
			core::Color4B c = m_ctx->renderOutlinePass ? m_ctx->outlineColor : (de && de->hasFillColor ? de->fillColor : m_ctx->defaultFillColor);
			D2D1_RECT_F r = D2D1::RectF(0, u->offset, u->width, u->offset + u->thickness);
			D2D1::Matrix3x2F mat = D2D1::Matrix3x2F::Translation(bx, by);
			Microsoft::WRL::ComPtr<ID2D1RectangleGeometry> rg;
			Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> tg;
			m_factory->CreateRectangleGeometry(r, &rg);
			m_factory->CreateTransformedGeometry(rg.Get(), mat, &tg);
			ID2D1SolidColorBrush* brush = m_ctx->reusableBrush;
			Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> localBrush;
			if (brush) {
				brush->SetColor(toD2D1Color(c));
			} else {
				m_rt->CreateSolidColorBrush(toD2D1Color(c), &localBrush);
				brush = localBrush.Get();
			}
			if (m_ctx->renderOutlinePass && m_ctx->outlineWidth > 0.f)
				m_rt->DrawGeometry(tg.Get(), brush, m_ctx->outlineWidth, m_strokeStyle.Get());
			else if (!m_ctx->renderOutlinePass)
				m_rt->FillGeometry(tg.Get(), brush);
			return S_OK;
		}

		HRESULT WINAPI DrawStrikethrough(void*, FLOAT bx, FLOAT by,
				DWRITE_STRIKETHROUGH const* s, IUnknown* effect) noexcept override {
			DrawingEffect* de = effect ? static_cast<DrawingEffect*>(effect) : nullptr;
			core::Color4B c = m_ctx->renderOutlinePass ? m_ctx->outlineColor : (de && de->hasFillColor ? de->fillColor : m_ctx->defaultFillColor);
			D2D1_RECT_F r = D2D1::RectF(0, s->offset, s->width, s->offset + s->thickness);
			D2D1::Matrix3x2F mat = D2D1::Matrix3x2F::Translation(bx, by);
			Microsoft::WRL::ComPtr<ID2D1RectangleGeometry> rg;
			Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> tg;
			m_factory->CreateRectangleGeometry(r, &rg);
			m_factory->CreateTransformedGeometry(rg.Get(), mat, &tg);
			ID2D1SolidColorBrush* brush = m_ctx->reusableBrush;
			Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> localBrush;
			if (brush) {
				brush->SetColor(toD2D1Color(c));
			} else {
				m_rt->CreateSolidColorBrush(toD2D1Color(c), &localBrush);
				brush = localBrush.Get();
			}
			if (m_ctx->renderOutlinePass && m_ctx->outlineWidth > 0.f)
				m_rt->DrawGeometry(tg.Get(), brush, m_ctx->outlineWidth, m_strokeStyle.Get());
			else if (!m_ctx->renderOutlinePass)
				m_rt->FillGeometry(tg.Get(), brush);
			return S_OK;
		}

		HRESULT WINAPI DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject* obj,
				BOOL sw, BOOL rtl, IUnknown* effect) noexcept override {
			if (obj)
				obj->Draw(nullptr, this, 0.f, 0.f, sw, rtl, effect);
			return S_OK;
		}

	private:
		HRESULT getOrBuildGlyphGeometry(DWRITE_GLYPH_RUN const* run, ID2D1PathGeometry** outGeom) {
			if (m_ctx->geometryCache && run->glyphCount == 1 && run->glyphIndices) {
				GlyphGeoKey key{ run->fontFace, run->glyphIndices[0], run->fontEmSize, run->isSideways != 0 };
				auto it = m_ctx->geometryCache->find(key);
				if (it != m_ctx->geometryCache->end()) {
					*outGeom = it->second.Get();
					return S_OK;
				}

				Microsoft::WRL::ComPtr<ID2D1PathGeometry> pg;
				HRESULT hr = m_factory->CreatePathGeometry(&pg);
				if (FAILED(hr))
					return hr;
				Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
				hr = pg->Open(&sink);
				if (FAILED(hr))
					return hr;
				hr = run->fontFace->GetGlyphRunOutline(
					run->fontEmSize, run->glyphIndices, nullptr,
					nullptr, 1,
					run->isSideways, (run->bidiLevel & 1), sink.Get());
				if (FAILED(hr))
					return hr;
				sink->Close();
				*outGeom = pg.Get();
				(*m_ctx->geometryCache)[key] = std::move(pg);
				return S_OK;
			}

			Microsoft::WRL::ComPtr<ID2D1PathGeometry> pg;
			HRESULT hr = m_factory->CreatePathGeometry(&pg);
			if (FAILED(hr))
				return hr;
			Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
			hr = pg->Open(&sink);
			if (FAILED(hr))
				return hr;
			hr = run->fontFace->GetGlyphRunOutline(
				run->fontEmSize, run->glyphIndices, run->glyphAdvances,
				run->glyphOffsets, run->glyphCount,
				run->isSideways, (run->bidiLevel & 1), sink.Get());
			if (FAILED(hr))
				return hr;
			sink->Close();
			m_scratchGeom = pg;
			*outGeom = pg.Get();
			return S_OK;
		}

		HRESULT drawSingleRunWithBrush(DWRITE_GLYPH_RUN const* run, FLOAT x, FLOAT y, ID2D1Brush* brush) {
			if (!run || !run->fontFace || !brush)
				return S_OK;

			ID2D1PathGeometry* geom = nullptr;
			HRESULT hr = getOrBuildGlyphGeometry(run, &geom);
			if (FAILED(hr))
				return hr;

			Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> tg;
			D2D1::Matrix3x2F mat = D2D1::Matrix3x2F::Translation(x, y);
			hr = m_factory->CreateTransformedGeometry(geom, mat, &tg);
			if (FAILED(hr))
				return hr;
			if (!m_ctx->renderOutlinePass)
				m_rt->FillGeometry(tg.Get(), brush);
			return S_OK;
		}

		HRESULT drawSingleRun(DWRITE_GLYPH_RUN const* run, FLOAT x, FLOAT y, core::Color4B color) {
			if (!run || !run->fontFace)
				return S_OK;

			ID2D1PathGeometry* geom = nullptr;
			HRESULT hr = getOrBuildGlyphGeometry(run, &geom);
			if (FAILED(hr))
				return hr;

			Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> tg;
			D2D1::Matrix3x2F mat = D2D1::Matrix3x2F::Translation(x, y);
			hr = m_factory->CreateTransformedGeometry(geom, mat, &tg);
			if (FAILED(hr))
				return hr;

			ID2D1SolidColorBrush* brush = m_ctx->reusableBrush;
			Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> localBrush;
			if (brush) {
				brush->SetColor(toD2D1Color(color));
			} else {
				m_rt->CreateSolidColorBrush(toD2D1Color(color), &localBrush);
				brush = localBrush.Get();
			}

			if (m_ctx->renderOutlinePass && m_ctx->outlineWidth > 0.f)
				m_rt->DrawGeometry(tg.Get(), brush, m_ctx->outlineWidth, m_strokeStyle.Get());
			else if (!m_ctx->renderOutlinePass)
				m_rt->FillGeometry(tg.Get(), brush);
			return S_OK;
		}

		Microsoft::WRL::ComPtr<ID2D1PathGeometry> m_scratchGeom;
	};

}

namespace luastg::binding {

	struct RichText::Impl {
		Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory;
		Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection;
		std::wstring fontFamily;
		Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
		Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;

		GlyphGeometryCache glyphGeometryCache;

		core::SmartReference<core::Graphics::IRenderTarget> renderTarget;
		core::SmartReference<core::Graphics::ISprite> sprite;
		core::SmartReference<core::Graphics::ISpriteRenderer> spriteRenderer;

		std::string rawText;
		ParseResult parsed;
		float fontSize{ 24.f };
		float layoutWidth{ 0.f };
		float layoutHeight{ 0.f };
		float maxFitWidth{ 0.f };
		float maxFitHeight{ 0.f };
		float effectiveSizeScale{ 1.f };

		luastg::BlendMode blendMode{ luastg::BlendMode::MulAlpha };
		core::Color4B vertexColor{ 255, 255, 255, 255 };

		core::Color4B fillColor{ 255, 255, 255, 255 };
		float outlineWidth{ 0.f };
		core::Color4B outlineColor{ 0, 0, 0, 255 };
		float shadowOffsetX{ 0.f };
		float shadowOffsetY{ 0.f };
		float shadowBlur{ 0.f };
		core::Color4B shadowColor{ 0, 0, 0, 180 };
		bool hasShadow{ false };

		DWRITE_TEXT_ALIGNMENT hAlign{ DWRITE_TEXT_ALIGNMENT_LEADING };
		DWRITE_PARAGRAPH_ALIGNMENT vAlign{ DWRITE_PARAGRAPH_ALIGNMENT_NEAR };

		float unitPerPixel{ 1.f };
		bool autoScale{ true };

		float time{ 0.f };
		std::chrono::steady_clock::time_point lastUpdateTime{ std::chrono::steady_clock::now() };

		bool layoutDirty{ true };
		bool renderDirty{ true };

		uint32_t texWidth{ 0 };
		uint32_t texHeight{ 0 };

		friend class RichTextBinding;

		bool initFromFile(std::string_view fontFilePath, float size) {
			fontSize = size;
			if (!createDWriteFactory())
				return false;

			if (!loadFileFontCollection(fontFilePath))
				return false;

			if (!detectFontFamily())
				return false;

			return createTextFormat();
		}

		bool initFromSystem(std::string_view familyNameUtf8, float size) {
			fontSize = size;
			if (!createDWriteFactory())
				return false;
			fontFamily = utf8::to_wstring(familyNameUtf8);
			return createTextFormat();
		}

		// I have no fucking clue how I managed that with DirectWrite api state...
		bool initFromPool(std::string_view resName, float size) {
			fontSize = size;

			if (!createDWriteFactory())
				return false;

			if (!loadPoolFontCollection(resName))
				return false;

			if (!detectFontFamily())
				return false;
			return createTextFormat();
		}

		std::string_view getText() const { return rawText; }

		void setText(std::string_view utf8text) {
			if (rawText == utf8text)
				return;
			rawText = std::string(utf8text);
			parsed = parseRichText(utf8text);
			layoutDirty = true;
			renderDirty = true;
		}

		void update() {
			if (parsed.hasAnimation) {
				auto now = std::chrono::steady_clock::now();
				float dt = std::chrono::duration<float>(now - lastUpdateTime).count();
				lastUpdateTime = now;
				time += dt;
				renderDirty = true;
			} else {
				lastUpdateTime = std::chrono::steady_clock::now();
			}
		}

		bool drawAt(float wx, float wy, float sx, float sy, float rot) {
			if (parsed.plainText.empty())
				return true;

			auto* renderer = LAPP.GetAppModel()->getRenderer();

			if (autoScale) {
				auto ortho = renderer->getOrtho();
				auto viewport = renderer->getViewport();
				float orthoW = ortho.b.x - ortho.a.x;
				float vpW = viewport.b.x - viewport.a.x;
				if (vpW > 0.f && orthoW > 0.f) {
					float newUPP = orthoW / vpW;
					if (std::abs(newUPP - unitPerPixel) > 1e-5f) {
						unitPerPixel = newUPP;
						layoutDirty = true;
						renderDirty = true;
					}
				}
			}

			if (layoutDirty && !rebuildLayout())
				return false;
			if (renderDirty && !renderToTarget())
				return false;
			if (!spriteRenderer || !sprite)
				return false;

			sprite->setUnitsPerPixel(unitPerPixel);

			float ax = 0.f, ay = 0.f;
			float worldW = (float)texWidth * unitPerPixel;
			float worldH = (float)texHeight * unitPerPixel;
			if (hAlign == DWRITE_TEXT_ALIGNMENT_LEADING)
				ax = worldW * 0.5f;
			else if (hAlign == DWRITE_TEXT_ALIGNMENT_TRAILING)
				ax = -worldW * 0.5f;
			if (layoutHeight == 0.f) {
				if (vAlign == DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
					ay = -worldH * 0.5f;
				else if (vAlign == DWRITE_PARAGRAPH_ALIGNMENT_FAR)
					ay = worldH * 0.5f;
			}

			spriteRenderer->setTransform(
				core::Vector2F(wx + ax, wy + ay),
				core::Vector2F(sx, sy),
				rot);
			auto const bd = luastg::translateLegacyBlendState(blendMode);
			spriteRenderer->setLegacyBlendState(bd.vertex_color_blend_state, bd.blend_state);
			spriteRenderer->setColor(vertexColor);
			spriteRenderer->draw(renderer);
			return true;
		}

		bool measure(float& w, float& h) {
			if (parsed.plainText.empty()) {
				w = 0;
				h = 0;
				return true;
			}
			if (layoutDirty && !rebuildLayout())
				return false;
			if (layoutHeight > 0.f)
				h = layoutHeight;
			DWRITE_TEXT_METRICS m{};
			if (FAILED(textLayout->GetMetrics(&m)))
				return false;
			float padL, padT, padR, padB;
			computePadding(padL, padT, padR, padB, fontSize / unitPerPixel);
			w = (m.width + padL + padR) * unitPerPixel;
			if (layoutHeight == 0.f)
				h = (m.height + padT + padB) * unitPerPixel;
			return true;
		}

		bool hasAnimation() const { return parsed.hasAnimation; }

		~Impl() = default;

	private:
		bool createDWriteFactory() {
			dwriteFactory = getSharedDWriteFactory();
			return dwriteFactory != nullptr;
		}

		bool loadFileFontCollection(std::string_view path) {
			std::string cacheKey(path);
			auto& cache = getFontCollectionCache();
			auto it = cache.find(cacheKey);
			if (it != cache.end()) {
				fontCollection = it->second.collection;
				return true;
			}

			Microsoft::WRL::ComPtr<FontFileLoader> newFileLoader;
			newFileLoader.Attach(new FontFileLoader());
			if (FAILED(dwriteFactory->RegisterFontFileLoader(newFileLoader.Get())))
				return false;

			Microsoft::WRL::ComPtr<FontCollectionLoader> newColLoader;
			newColLoader.Attach(new FontCollectionLoader());
			newColLoader->init(dwriteFactory.Get(), newFileLoader.Get(), { cacheKey });
			if (FAILED(dwriteFactory->RegisterFontCollectionLoader(newColLoader.Get())))
				return false;

			Microsoft::WRL::ComPtr<IDWriteFontCollection> newCollection;
			HRESULT hr = dwriteFactory->CreateCustomFontCollection(
				newColLoader.Get(), cacheKey.data(), (UINT32)cacheKey.size(), &newCollection);
			if (FAILED(hr))
				return false;

			FontCollectionCacheEntry entry;
			entry.collection = newCollection;
			entry.fileLoader = newFileLoader;
			entry.colLoader = newColLoader;
			cache.emplace(std::move(cacheKey), std::move(entry));

			fontCollection = newCollection;
			return true;
		}

		bool loadPoolFontCollection(std::string_view resName) {
			std::string cacheKey = "@pool:" + std::string(resName);
			auto& cache = getFontCollectionCache();
			auto it = cache.find(cacheKey);
			if (it != cache.end()) {
				fontCollection = it->second.collection;
				return true;
			}

			auto res = LRES.FindTTFFont(std::string(resName).c_str());
			if (!res)
				return false;

			auto* mgr = res->GetGlyphManager();
			if (!mgr)
				return false;

			uint32_t fontDataCount = mgr->getFontDataCount();
			if (fontDataCount == 0)
				return false;

			Microsoft::WRL::ComPtr<FontFileLoader> newFileLoader;
			newFileLoader.Attach(new FontFileLoader());
			if (FAILED(dwriteFactory->RegisterFontFileLoader(newFileLoader.Get())))
				return false;

			std::vector<std::string> keys;
			for (uint32_t i = 0; i < fontDataCount; ++i) {
				auto* data = mgr->getFontData(i);
				if (!data)
					continue;
				std::string key = cacheKey + ":" + std::to_string(i);
				newFileLoader->preload(key, data);
				keys.push_back(std::move(key));
			}
			if (keys.empty())
				return false;

			Microsoft::WRL::ComPtr<FontCollectionLoader> newColLoader;
			newColLoader.Attach(new FontCollectionLoader());
			newColLoader->init(dwriteFactory.Get(), newFileLoader.Get(), keys);
			if (FAILED(dwriteFactory->RegisterFontCollectionLoader(newColLoader.Get())))
				return false;

			Microsoft::WRL::ComPtr<IDWriteFontCollection> newCollection;
			HRESULT hr = dwriteFactory->CreateCustomFontCollection(
				newColLoader.Get(), cacheKey.data(), (UINT32)cacheKey.size(), &newCollection);
			if (FAILED(hr))
				return false;

			FontCollectionCacheEntry entry;
			entry.collection = newCollection;
			entry.fileLoader = newFileLoader;
			entry.colLoader = newColLoader;
			cache.emplace(std::move(cacheKey), std::move(entry));

			fontCollection = newCollection;
			return true;
		}

	public:
		void tearDownFontLoaders() {
			fontCollection.Reset();
			textFormat.Reset();
			textLayout.Reset();
			glyphGeometryCache.clear();
		}

		bool changeFontFromFile(std::string_view fontFilePath) {
			tearDownFontLoaders();
			if (!loadFileFontCollection(fontFilePath))
				return false;
			if (!detectFontFamily())
				return false;
			return createTextFormat();
		}

		bool changeFontFromSystem(std::string_view familyNameUtf8) {
			tearDownFontLoaders();
			fontFamily = utf8::to_wstring(familyNameUtf8);
			return createTextFormat();
		}

		bool changeFontFromPool(std::string_view resName) {
			tearDownFontLoaders();
			if (!loadPoolFontCollection(resName))
				return false;
			if (!detectFontFamily())
				return false;
			return createTextFormat();
		}

	private:
		bool detectFontFamily() {
			if (!fontCollection)
				return false;
			Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
			if (FAILED(fontCollection->GetFontFamily(0, &family)))
				return false;
			Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
			if (FAILED(family->GetFamilyNames(&names)))
				return false;
			UINT32 len = 0;
			names->GetStringLength(0, &len);
			fontFamily.resize((size_t)len + 1);
			names->GetString(0, fontFamily.data(), len + 1);
			if (!fontFamily.empty() && fontFamily.back() == L'\0')
				fontFamily.pop_back();
			return !fontFamily.empty();
		}

		bool createTextFormat() {
			HRESULT hr = dwriteFactory->CreateTextFormat(
				fontFamily.c_str(),
				fontCollection.Get(),
				DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL, fontSize, L"",
				&textFormat);
			return SUCCEEDED(hr);
		}

		void computePadding(float& padLeft, float& padTop, float& padRight, float& padBottom, float fontSizePx) const {
			padLeft = outlineWidth + std::max(0.f, -shadowOffsetX) + shadowBlur * 2.f + 4.f;
			padTop = outlineWidth + std::max(0.f, -shadowOffsetY) + shadowBlur * 2.f + 4.f;
			padRight = outlineWidth + std::max(0.f, shadowOffsetX) + shadowBlur * 2.f + 4.f;
			padBottom = outlineWidth + std::max(0.f, shadowOffsetY) + shadowBlur * 2.f + 4.f;
			if (!parsed.rubies.empty())
				padTop += fontSizePx * 0.5f + 3.f;
		}

		bool rebuildLayout() {
			if (!textFormat || parsed.plainText.empty())
				return false;

			glyphGeometryCache.clear();

			float const fontSizePx = std::max(1.f, fontSize / unitPerPixel);

			Microsoft::WRL::ComPtr<IDWriteTextFormat> pixelFmt;
			if (FAILED(dwriteFactory->CreateTextFormat(
					fontFamily.c_str(), fontCollection.Get(),
					DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
					DWRITE_FONT_STRETCH_NORMAL, fontSizePx, L"", &pixelFmt)))
				return false;

			float padL, padT, padR, padB;
			computePadding(padL, padT, padR, padB, fontSizePx);
			float layoutWidthPx = layoutWidth > 0.f ? layoutWidth / unitPerPixel : 0.f;
			float layoutHeightPx = layoutHeight > 0.f ? layoutHeight / unitPerPixel : 0.f;
			float maxFitWidthPx = maxFitWidth > 0.f ? maxFitWidth / unitPerPixel : 0.f;
			float maxFitHeightPx = maxFitHeight > 0.f ? maxFitHeight / unitPerPixel : 0.f;
			float maxW = layoutWidthPx > 0.f ? std::max(1.f, layoutWidthPx - padL - padR) : 100000.f;
			float maxH = layoutHeightPx > 0.f ? std::max(1.f, layoutHeightPx - padT - padB) : 100000.f;

			DWRITE_TEXT_RANGE const wholeRange{ 0, (UINT32)parsed.plainText.size() };
			float sizeScale = 1.f;

			auto applyRunSizes = [&](IDWriteTextLayout* layout, float scale) {
				if (scale != 1.f)
					layout->SetFontSize(std::max(1.f, fontSizePx * scale), wholeRange);
				for (auto const& run : parsed.runs)
					if (run.style.size.has_value())
						layout->SetFontSize(run.style.size.value() / unitPerPixel * scale, { run.start, run.length });
			};

			if (maxFitWidthPx > 0.f) {
				Microsoft::WRL::ComPtr<IDWriteTextLayout> measureLayout;
				if (SUCCEEDED(dwriteFactory->CreateTextLayout(
						parsed.plainText.c_str(), (UINT32)parsed.plainText.size(),
						pixelFmt.Get(), 100000.f, 100000.f, &measureLayout))) {
					applyRunSizes(measureLayout.Get(), sizeScale);
					DWRITE_TEXT_METRICS m{};
					measureLayout->GetMetrics(&m);
					float totalW = m.width + padL + padR;
					if (totalW > maxFitWidthPx && m.width > 0.f)
						sizeScale = std::max((maxFitWidthPx - padL - padR) / m.width, 1.f / fontSizePx);
				}
			}

			if (maxFitHeightPx > 0.f) {
				Microsoft::WRL::ComPtr<IDWriteTextLayout> hMeasureLayout;
				if (SUCCEEDED(dwriteFactory->CreateTextLayout(
						parsed.plainText.c_str(), (UINT32)parsed.plainText.size(),
						pixelFmt.Get(), maxW, 100000.f, &hMeasureLayout))) {
					applyRunSizes(hMeasureLayout.Get(), sizeScale);
					DWRITE_TEXT_METRICS m{};
					hMeasureLayout->GetMetrics(&m);
					float totalH = m.height + padT + padB;
					if (totalH > maxFitHeightPx && m.height > 0.f) {
						float newScale = std::max(sizeScale * std::max(1.f, maxFitHeightPx - padT - padB) / m.height, 1.f / fontSizePx);
						if (newScale < sizeScale)
							sizeScale = newScale;
					}
				}
			}

			if (layoutWidthPx <= 0.f) {
				Microsoft::WRL::ComPtr<IDWriteTextLayout> naturalLayout;
				if (SUCCEEDED(dwriteFactory->CreateTextLayout(
						parsed.plainText.c_str(), (UINT32)parsed.plainText.size(),
						pixelFmt.Get(), 100000.f, 100000.f, &naturalLayout))) {
					applyRunSizes(naturalLayout.Get(), sizeScale);
					DWRITE_TEXT_METRICS naturalMetrics{};
					naturalLayout->GetMetrics(&naturalMetrics);
					if (naturalMetrics.width > 0.f)
						maxW = naturalMetrics.width;
				}
			}

			HRESULT hr = dwriteFactory->CreateTextLayout(
				parsed.plainText.c_str(), (UINT32)parsed.plainText.size(),
				pixelFmt.Get(), maxW, maxH, &textLayout);
			if (FAILED(hr))
				return false;

			applyRunSizes(textLayout.Get(), sizeScale);

			textLayout->SetTextAlignment(hAlign);
			textLayout->SetParagraphAlignment(layoutHeightPx > 0.f ? vAlign : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

			for (auto const& run : parsed.runs) {
				DWRITE_TEXT_RANGE range{ run.start, run.length };
				if (run.style.bold)
					textLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
				if (run.style.italic)
					textLayout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
				if (run.style.underline)
					textLayout->SetUnderline(TRUE, range);
				if (run.style.strikethrough)
					textLayout->SetStrikethrough(TRUE, range);
				if (fontCollection)
					textLayout->SetFontCollection(fontCollection.Get(), range);

				if (run.style.color.has_value() || run.style.wave || run.style.shake || run.style.gradient) {
					auto* de = new DrawingEffect();
					if (run.style.color.has_value()) {
						de->fillColor = run.style.color.value();
						de->hasFillColor = true;
					}
					de->wave = run.style.wave;
					de->waveAmp = run.style.waveAmp;
					de->waveSpeed = run.style.waveSpeed;
					de->shake = run.style.shake;
					de->shakeIntensity = run.style.shakeIntensity;
					de->gradient = run.style.gradient;
					de->gradientVertical = run.style.gradientVertical;
					de->gradientColors = run.style.gradientColors;
					textLayout->SetDrawingEffect(de, range);
					if (de->gradient) {
						UINT32 hitCount = 0;
						textLayout->HitTestTextRange(run.start, run.length, 0.f, 0.f, nullptr, 0, &hitCount);
						if (hitCount > 0) {
							std::vector<DWRITE_HIT_TEST_METRICS> htMetrics(hitCount);
							textLayout->HitTestTextRange(run.start, run.length, 0.f, 0.f, htMetrics.data(), hitCount, &hitCount);
							float spanLeft = htMetrics[0].left;
							float spanRight = htMetrics[0].left + htMetrics[0].width;
							float spanTop = htMetrics[0].top;
							float spanBottom = htMetrics[0].top + htMetrics[0].height;
							for (UINT32 k = 1; k < hitCount; ++k) {
								spanLeft = std::min(spanLeft, htMetrics[k].left);
								spanRight = std::max(spanRight, htMetrics[k].left + htMetrics[k].width);
								spanTop = std::min(spanTop, htMetrics[k].top);
								spanBottom = std::max(spanBottom, htMetrics[k].top + htMetrics[k].height);
							}
							de->gradientSpanStartX = spanLeft;
							de->gradientSpanWidth = std::max(1.f, spanRight - spanLeft);
							de->gradientSpanStartY = spanTop;
							de->gradientSpanHeight = std::max(1.f, spanBottom - spanTop);
						}
					}
					de->Release();
				}
			}

			layoutDirty = false;
			effectiveSizeScale = sizeScale;
			renderDirty = true;
			return true;
		}

		bool renderToTarget() {
			if (!textLayout)
				return false;

			auto* d2d1Ctx = reinterpret_cast<ID2D1DeviceContext*>(
				LAPP.GetAppModel()->getDevice()->getNativeRendererHandle());
			if (!d2d1Ctx)
				return false;

			DWRITE_TEXT_METRICS metrics{};
			textLayout->GetMetrics(&metrics);

			float padLeft, padTop, padRight, padBottom;
			computePadding(padLeft, padTop, padRight, padBottom, fontSize / unitPerPixel);

			uint32_t newW = std::max(1u, layoutWidth > 0.f
				? (uint32_t)std::ceil(layoutWidth / unitPerPixel)
				: (uint32_t)std::ceil(metrics.width + padLeft + padRight));
			uint32_t newH = std::max(1u, layoutHeight > 0.f
				? (uint32_t)std::ceil(layoutHeight / unitPerPixel)
				: (uint32_t)std::ceil(metrics.height + padTop + padBottom));

			if (!renderTarget || newW != texWidth || newH != texHeight) {
				texWidth = newW;
				texHeight = newH;
				core::Graphics::IRenderTarget* rt = nullptr;
				if (!LAPP.GetAppModel()->getDevice()->createRenderTarget({ texWidth, texHeight }, &rt))
					return false;
				renderTarget.attach(rt);
				renderTarget->getTexture()->setPremultipliedAlpha(true);

				core::Graphics::ISprite* sp = nullptr;
				if (!core::Graphics::ISprite::create(
					    LAPP.GetAppModel()->getRenderer(),
					    renderTarget->getTexture(), &sp))
					return false;
				sprite.attach(sp);
				sprite->setTextureRect(core::RectF(0.f, 0.f, (float)texWidth, (float)texHeight));
				sprite->setTextureCenter(core::Vector2F((float)texWidth * 0.5f, (float)texHeight * 0.5f));
				sprite->setUnitsPerPixel(unitPerPixel);
				sprite->setColor(core::Color4B(255, 255, 255, 255));
				sprite->setZ(0.5f);

				core::Graphics::ISpriteRenderer* sr = nullptr;
				if (!core::Graphics::ISpriteRenderer::create(&sr))
					return false;
				spriteRenderer.attach(sr);
				spriteRenderer->setSprite(sprite.get());
				spriteRenderer->setZ(0.5f);
				spriteRenderer->setColor(core::Color4B(255, 255, 255, 255));
				spriteRenderer->setLegacyBlendState(
					core::Graphics::IRenderer::VertexColorBlendState::Mul,
					core::Graphics::IRenderer::BlendState::Alpha);
			}

			auto* d2d1Bitmap = reinterpret_cast<ID2D1Bitmap1*>(
				renderTarget->getNativeBitmapHandle());
			if (!d2d1Bitmap)
				return false;

			if (parsed.hasAnimation) {
				for (auto const& run : parsed.runs) {
					if (!run.style.wave && !run.style.shake)
						continue;
					DWRITE_TEXT_RANGE range{ run.start, run.length };
					Microsoft::WRL::ComPtr<IUnknown> eff;
					textLayout->GetDrawingEffect(run.start, &eff, nullptr);
					if (eff) {
						auto* de = static_cast<DrawingEffect*>(eff.Get());
						de->time = time;
					}
				}
			}

			Microsoft::WRL::ComPtr<ID2D1Factory> d2d1Factory;
			d2d1Ctx->GetFactory(&d2d1Factory);
			FLOAT savedDpiX = 96.f, savedDpiY = 96.f;
			d2d1Ctx->GetDpi(&savedDpiX, &savedDpiY);
			d2d1Ctx->SetDpi(96.f, 96.f);

			Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> sharedBrush;
			d2d1Ctx->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0), &sharedBrush);

			d2d1Ctx->BeginDraw();
			d2d1Ctx->SetTarget(d2d1Bitmap);
			d2d1Ctx->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
			d2d1Ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

			float originX = padLeft;
			float originY = padTop;
			(void)padRight;
			(void)padBottom;

			if (hasShadow) {
				if (shadowBlur > 0.001f) {
					Microsoft::WRL::ComPtr<ID2D1Bitmap1> tmpBitmap;
					D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
						D2D1_BITMAP_OPTIONS_TARGET,
						D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
					d2d1Ctx->CreateBitmap(D2D1::SizeU(texWidth, texHeight), nullptr, 0, bmpProps, &tmpBitmap);
					if (tmpBitmap) {
						d2d1Ctx->SetTarget(tmpBitmap.Get());
						d2d1Ctx->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
						GlyphRendererContext gctx{};
						gctx.defaultFillColor = shadowColor;
						gctx.renderOutlinePass = false;
						gctx.reusableBrush = sharedBrush.Get();
						gctx.geometryCache = &glyphGeometryCache;
						drawLayout(d2d1Factory.Get(), (ID2D1RenderTarget*)d2d1Ctx, gctx, originX, originY);

						d2d1Ctx->SetTarget(d2d1Bitmap);
						Microsoft::WRL::ComPtr<ID2D1Effect> shadowFx;
						if (SUCCEEDED(d2d1Ctx->CreateEffect(CLSID_D2D1Shadow, &shadowFx))) {
							shadowFx->SetInput(0, tmpBitmap.Get());
							shadowFx->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, shadowBlur / 3.f);
							shadowFx->SetValue(D2D1_SHADOW_PROP_COLOR, toD2D1Color(shadowColor));
							d2d1Ctx->DrawImage(shadowFx.Get(),
								D2D1::Point2F(shadowOffsetX, shadowOffsetY));
						}
					}
				} else {
					GlyphRendererContext gctx{};
					gctx.defaultFillColor = shadowColor;
					gctx.renderOutlinePass = false;
					gctx.reusableBrush = sharedBrush.Get();
					gctx.geometryCache = &glyphGeometryCache;
					drawLayout(d2d1Factory.Get(), (ID2D1RenderTarget*)d2d1Ctx, gctx, originX + shadowOffsetX, originY + shadowOffsetY);
				}
			}

			if (outlineWidth > 0.001f) {
				GlyphRendererContext gctx{};
				gctx.defaultFillColor = fillColor;
				gctx.outlineWidth = outlineWidth * 2.f;
				gctx.outlineColor = outlineColor;
				gctx.renderOutlinePass = true;
				gctx.reusableBrush = sharedBrush.Get();
				gctx.geometryCache = &glyphGeometryCache;
				drawLayout(d2d1Factory.Get(), (ID2D1RenderTarget*)d2d1Ctx, gctx, originX, originY);
			}

			{
				GlyphRendererContext gctx{};
				gctx.defaultFillColor = fillColor;
				gctx.renderOutlinePass = false;
				gctx.reusableBrush = sharedBrush.Get();
				gctx.geometryCache = &glyphGeometryCache;
				drawLayout(d2d1Factory.Get(), (ID2D1RenderTarget*)d2d1Ctx, gctx, originX, originY);
			}

			if (!parsed.rubies.empty()) {
				drawRubyAnnotations(d2d1Ctx, d2d1Factory.Get(), sharedBrush.Get(), originX, originY);
			}

			d2d1Ctx->SetTarget(nullptr);
			HRESULT hr = d2d1Ctx->EndDraw();

			d2d1Ctx->SetDpi(savedDpiX, savedDpiY);

			if (FAILED(hr)) {
				renderDirty = true;
				return false;
			}

			renderDirty = false;
			return true;
		}

		void drawLayout(ID2D1Factory* factory, ID2D1RenderTarget* rt, GlyphRendererContext& gctx, float x, float y) {
			gctx.layoutOriginX = x;
			gctx.layoutOriginY = y;
			RichTextGlyphRenderer renderer;
			renderer.init(factory, rt, &gctx);
			textLayout->Draw(nullptr, &renderer, x, y);
		}

		void drawRubyAnnotations(ID2D1DeviceContext* ctx, ID2D1Factory* factory, ID2D1SolidColorBrush* sharedBrush, float originX, float originY) {
			float rubySize = (fontSize / unitPerPixel) * effectiveSizeScale * 0.5f;

			Microsoft::WRL::ComPtr<IDWriteTextFormat> rubyFormat;
			dwriteFactory->CreateTextFormat(
				fontFamily.c_str(), fontCollection.Get(),
				DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL, rubySize, L"", &rubyFormat);
			if (!rubyFormat)
				return;

			for (auto const& ann : parsed.rubies) {
				if (ann.rubyText.empty())
					continue;

				UINT32 hitCount = 0;
				textLayout->HitTestTextRange(ann.start, ann.length, 0.f, 0.f, nullptr, 0, &hitCount);
				if (hitCount == 0)
					continue;
				std::vector<DWRITE_HIT_TEST_METRICS> hitMetrics(hitCount);
				textLayout->HitTestTextRange(ann.start, ann.length, 0.f, 0.f,
					hitMetrics.data(), hitCount, &hitCount);
				if (hitMetrics.empty())
					continue;

				float baseLeft = hitMetrics[0].left;
				float baseTop = hitMetrics[0].top;
				float baseWidth = 0.f;
				for (auto const& hm : hitMetrics)
					baseWidth += hm.width;

				Microsoft::WRL::ComPtr<IDWriteTextLayout> rubyLayout;
				dwriteFactory->CreateTextLayout(
					ann.rubyText.c_str(), (UINT32)ann.rubyText.size(),
					rubyFormat.Get(), 100000.f, rubySize * 2.f, &rubyLayout);
				if (!rubyLayout)
					continue;

				DWRITE_TEXT_METRICS rm{};
				rubyLayout->GetMetrics(&rm);

				float rx = originX + baseLeft + (baseWidth - rm.width) * 0.5f;
				float ry = originY + baseTop - rm.height - 1.f;

				GlyphRendererContext gctx{};
				gctx.defaultFillColor = fillColor;
				gctx.renderOutlinePass = false;
				gctx.reusableBrush = sharedBrush;

				RichTextGlyphRenderer renderer;
				renderer.init(factory, (ID2D1RenderTarget*)ctx, &gctx);

				if (outlineWidth > 0.001f) {
					GlyphRendererContext ogctx{};
					ogctx.defaultFillColor = fillColor;
					ogctx.outlineWidth = outlineWidth;
					ogctx.outlineColor = outlineColor;
					ogctx.renderOutlinePass = true;
					ogctx.reusableBrush = sharedBrush;
					RichTextGlyphRenderer oRenderer;
					oRenderer.init(factory, (ID2D1RenderTarget*)ctx, &ogctx);
					rubyLayout->Draw(nullptr, &oRenderer, rx, ry);
				}
				rubyLayout->Draw(nullptr, &renderer, rx, ry);
			}
		}
	};

}

namespace luastg::binding {

	std::string_view const RichText::class_name{ "lstg.RichText" };

	struct RichTextBinding : RichText {
		static int __gc(lua_State* vm) {
			auto* self = as(vm, 1);
			if (self->data) {
				delete self->data;
				self->data = nullptr;
			}
			return 0;
		}
		static int destroy(lua_State* vm) {
			auto* self = as(vm, 1);
			if (self->data) {
				delete self->data;
				self->data = nullptr;
			}
			return 0;
		}
		static int __concat(lua_State* vm) {
			bool selfIsFirst = is(vm, 1);
			auto* self = selfIsFirst ? as(vm, 1) : as(vm, 2);
			int strIdx = selfIsFirst ? 2 : 1;
			size_t slen = 0;
			const char* s = lua_tolstring(vm, strIdx, &slen);
			if (!s)
				return luaL_error(vm, "RichText: concat requires a string operand");
			std::string newText = selfIsFirst
				? std::string(self->data->getText()) + std::string(s, slen)
				: std::string(s, slen) + std::string(self->data->getText());
			self->data->setText(newText);
			lua_pushvalue(vm, selfIsFirst ? 1 : 2);
			return 1;
		}
		static int __tostring(lua_State* vm) {
			lua::stack_t ctx(vm);
			ctx.push_value(class_name);
			return 1;
		}
		static int __eq(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* a = as(vm, 1);
			if (is(vm, 2)) {
				auto* b = as(vm, 2);
				ctx.push_value(a->data == b->data);
			} else {
				ctx.push_value(false);
			}
			return 1;
		}
		static int __len(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			ctx.push_value((lua_Integer)(self->data ? self->data->parsed.plainText.size() : 0));
			return 1;
		}
		static int setText(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			auto text = ctx.get_value<std::string_view>(2);
			self->data->setText(text);
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setState(lua_State* vm) {
			auto* self = as(vm, 1);
			self->data->blendMode = luastg::TranslateBlendMode(vm, 2);
			self->data->vertexColor = *Color::Cast(vm, 3);
			lua::stack_t(vm).push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setFillColor(lua_State* vm) {
			auto* self = as(vm, 1);
			auto* color = Color::Cast(vm, 2);
			self->data->fillColor = *color;
			self->data->renderDirty = true;
			lua::stack_t(vm).push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setOutline(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			float width = ctx.get_value<float>(2);
			auto* color = Color::Cast(vm, 3);
			self->data->outlineWidth = width;
			self->data->outlineColor = *color;
			if (self->data->layoutWidth > 0.f || self->data->layoutHeight > 0.f)
				self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setShadow(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			float ox = ctx.get_value<float>(2);
			float oy = ctx.get_value<float>(3);
			auto* color = Color::Cast(vm, 4);
			float blur = ctx.get_value<float>(5, 0.f);
			self->data->shadowOffsetX = ox;
			self->data->shadowOffsetY = oy;
			self->data->shadowColor = *color;
			self->data->shadowBlur = blur;
			self->data->hasShadow = true;
			if (self->data->layoutWidth > 0.f || self->data->layoutHeight > 0.f)
				self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int clearShadow(lua_State* vm) {
			auto* self = as(vm, 1);
			self->data->hasShadow = false;
			if (self->data->layoutWidth > 0.f || self->data->layoutHeight > 0.f)
				self->data->layoutDirty = true;
			self->data->renderDirty = true;
			lua::stack_t(vm).push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setFontSize(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			float size = ctx.get_value<float>(2);
			if (size <= 0.f)
				return luaL_error(vm, "font size must be > 0");
			self->data->fontSize = size;
			self->data->textLayout.Reset();
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setFont(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			if (!self->data)
				return luaL_error(vm, "setFont: RichText not initialized");
			auto path = ctx.get_value<std::string_view>(2);
			if (!lua_isnoneornil(vm, 3)) {
				float size = ctx.get_value<float>(3);
				if (size <= 0.f)
					return luaL_error(vm, "font size must be > 0");
				self->data->fontSize = size;
			}
			if (!self->data->changeFontFromFile(path))
				return luaL_error(vm, "setFont: failed to load font '%s'", std::string(path).c_str());
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setFontFromSystem(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			if (!self->data)
				return luaL_error(vm, "setFontFromSystem: RichText not initialized");
			auto family = ctx.get_value<std::string_view>(2);
			if (!lua_isnoneornil(vm, 3)) {
				float size = ctx.get_value<float>(3);
				if (size <= 0.f)
					return luaL_error(vm, "font size must be > 0");
				self->data->fontSize = size;
			}
			if (!self->data->changeFontFromSystem(family))
				return luaL_error(vm, "setFontFromSystem: failed to set font '%s'", std::string(family).c_str());
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setFontFromPool(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			if (!self->data)
				return luaL_error(vm, "setFontFromPool: RichText not initialized");
			auto resName = ctx.get_value<std::string_view>(2);
			if (!lua_isnoneornil(vm, 3)) {
				float size = ctx.get_value<float>(3);
				if (size <= 0.f)
					return luaL_error(vm, "font size must be > 0");
				self->data->fontSize = size;
			}
			if (!self->data->changeFontFromPool(resName))
				return luaL_error(vm, "setFontFromPool: failed to use resource '%s'", std::string(resName).c_str());
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setTextWrap(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			self->data->layoutWidth = ctx.get_value<float>(2, 0.f);
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setMaxWidth(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			self->data->maxFitWidth = std::max(0.f, ctx.get_value<float>(2, 0.f));
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setMaxHeight(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			self->data->maxFitHeight = std::max(0.f, ctx.get_value<float>(2, 0.f));
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setHAlign(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			auto align = ctx.get_value<std::string_view>(2);
			if (align == "center")
				self->data->hAlign = DWRITE_TEXT_ALIGNMENT_CENTER;
			else if (align == "right")
				self->data->hAlign = DWRITE_TEXT_ALIGNMENT_TRAILING;
			else
				self->data->hAlign = DWRITE_TEXT_ALIGNMENT_LEADING;
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setAlignment(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			bool dirty = false;
			if (!lua_isnoneornil(vm, 2)) {
				auto h = ctx.get_value<std::string_view>(2);
				if (h == "center")
					self->data->hAlign = DWRITE_TEXT_ALIGNMENT_CENTER;
				else if (h == "right")
					self->data->hAlign = DWRITE_TEXT_ALIGNMENT_TRAILING;
				else
					self->data->hAlign = DWRITE_TEXT_ALIGNMENT_LEADING;
				dirty = true;
			}
			if (!lua_isnoneornil(vm, 3)) {
				auto v = ctx.get_value<std::string_view>(3);
				if (v == "middle")
					self->data->vAlign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
				else if (v == "bottom")
					self->data->vAlign = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
				else
					self->data->vAlign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
				dirty = true;
			}
			if (dirty) {
				self->data->layoutDirty = true;
				self->data->renderDirty = true;
			}
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setUnitPerPixel(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			float v = ctx.get_value<float>(2, 1.f);
			if (v <= 0.f)
				return luaL_error(vm, "unitPerPixel must be > 0");
			self->data->autoScale = false;
			self->data->unitPerPixel = v;
			if (self->data->sprite)
				self->data->sprite->setUnitsPerPixel(v);
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int getUnitPerPixel(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			ctx.push_value(self->data->unitPerPixel);
			return 1;
		}
		static int setAutoScale(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			self->data->autoScale = ctx.get_value<bool>(2, true);
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int setVAlign(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			auto align = ctx.get_value<std::string_view>(2);
			if (align == "middle")
				self->data->vAlign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
			else if (align == "bottom")
				self->data->vAlign = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
			else
				self->data->vAlign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
			self->data->layoutDirty = true;
			self->data->renderDirty = true;
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int update(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			self->data->update();
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int hasAnimation(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			ctx.push_value(self->data->hasAnimation());
			return 1;
		}
		static int measureText(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			float w = 0.f, h = 0.f;
			if (!self->data->measure(w, h))
				return luaL_error(vm, "measure failed");
			ctx.push_value(w);
			ctx.push_value(h);
			return 2;
		}
		static int draw(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto* self = as(vm, 1);
			float x = ctx.get_value<float>(2);
			float y = ctx.get_value<float>(3);
			float sx = ctx.get_value<float>(4, 1.f);
			float sy = ctx.get_value<float>(5, sx);
			float rot = ctx.get_value<float>(6, 0.f);
			if (!self->data->drawAt(x, y, sx, sy, rot))
				return luaL_error(vm, "RichText:draw failed");
			ctx.push_value(lua::stack_index_t(1));
			return 1;
		}
		static int createFromFile(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto path = ctx.get_value<std::string_view>(1);
			float size = ctx.get_value<float>(2, 24.f);
			if (size <= 0.f)
				return luaL_error(vm, "font size must be > 0");
			auto* self = RichText::create(vm);
			self->data = new Impl();
			if (!self->data->initFromFile(path, size)) {
				delete self->data;
				self->data = nullptr;
				return luaL_error(vm, "RichText.create: failed to load font '%s'", std::string(path).c_str());
			}
			return 1;
		}
		static int createFromSystem(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto family = ctx.get_value<std::string_view>(1);
			float size  = ctx.get_value<float>(2, 24.f);
			if (size <= 0.f)
				return luaL_error(vm, "font size must be > 0");
			auto* self = RichText::create(vm);
			self->data = new Impl();
			if (!self->data->initFromSystem(family, size)) {
				delete self->data;
				self->data = nullptr;
				return luaL_error(vm, "RichText.createFromSystem: failed to create font '%s'", std::string(family).c_str());
			}
			return 1;
		}
		static int createFromPool(lua_State* vm) {
			lua::stack_t ctx(vm);
			auto resName = ctx.get_value<std::string_view>(1);
			float size = ctx.get_value<float>(2, 24.f);
			if (size <= 0.f)
				return luaL_error(vm, "font size must be > 0");
			auto* self = RichText::create(vm);
			self->data = new Impl();
			if (!self->data->initFromPool(resName, size)) {
				delete self->data;
				self->data = nullptr;
				return luaL_error(vm, "RichText.createFromPool: failed to use resource '%s'", std::string(resName).c_str());
			}
			return 1;
		}
	};

	bool RichText::is(lua_State* vm, int const index) {
		lua::stack_t ctx(vm);
		return ctx.is_metatable(index, class_name);
	}
	RichText* RichText::as(lua_State* vm, int const index) {
		lua::stack_t ctx(vm);
		return ctx.as_userdata<RichText>(index);
	}
	RichText* RichText::create(lua_State* vm) {
		lua::stack_t ctx(vm);
		auto* self = ctx.create_userdata<RichText>();
		ctx.set_metatable(ctx.index_of_top(), class_name);
		self->data = nullptr;
		return self;
	}
	void RichText::registerClass(lua_State* vm) {
		lua::stack_balancer_t sb(vm); //Somehow crashes if I don't put that. For some fucking reason..?
		lua::stack_t ctx(vm);

		auto methods = ctx.create_module(class_name);
		ctx.set_map_value(methods, "setState", &RichTextBinding::setState);
		ctx.set_map_value(methods, "setText", &RichTextBinding::setText);
		ctx.set_map_value(methods, "setFillColor", &RichTextBinding::setFillColor);
		ctx.set_map_value(methods, "setOutline", &RichTextBinding::setOutline);
		ctx.set_map_value(methods, "setShadow", &RichTextBinding::setShadow);
		ctx.set_map_value(methods, "clearShadow", &RichTextBinding::clearShadow);
		ctx.set_map_value(methods, "setFontSize", &RichTextBinding::setFontSize);
		ctx.set_map_value(methods, "setFont", &RichTextBinding::setFont);
		ctx.set_map_value(methods, "setFontFromSystem", &RichTextBinding::setFontFromSystem);
		ctx.set_map_value(methods, "setFontFromPool", &RichTextBinding::setFontFromPool);
		ctx.set_map_value(methods, "setTextWrap", &RichTextBinding::setTextWrap);
		ctx.set_map_value(methods, "setMaxWidth", &RichTextBinding::setMaxWidth);
		ctx.set_map_value(methods, "setMaxHeight", &RichTextBinding::setMaxHeight);
		ctx.set_map_value(methods, "setHAlign", &RichTextBinding::setHAlign);
		ctx.set_map_value(methods, "setVAlign", &RichTextBinding::setVAlign);
		ctx.set_map_value(methods, "setAlignment", &RichTextBinding::setAlignment);
		ctx.set_map_value(methods, "setUnitPerPixel", &RichTextBinding::setUnitPerPixel);
		ctx.set_map_value(methods, "getUnitPerPixel", &RichTextBinding::getUnitPerPixel);
		ctx.set_map_value(methods, "setAutoScale", &RichTextBinding::setAutoScale);
		ctx.set_map_value(methods, "update", &RichTextBinding::update);
		ctx.set_map_value(methods, "hasAnimation", &RichTextBinding::hasAnimation);
		ctx.set_map_value(methods, "measure", &RichTextBinding::measureText);
		ctx.set_map_value(methods, "render", &RichTextBinding::draw);
		ctx.set_map_value(methods, "create", &RichTextBinding::createFromFile);
		ctx.set_map_value(methods, "createFromSystem",&RichTextBinding::createFromSystem);
		ctx.set_map_value(methods, "createFromPool", &RichTextBinding::createFromPool);
		ctx.set_map_value(methods, "destroy", &RichTextBinding::destroy);

		auto meta = ctx.create_metatable(class_name);
		ctx.set_map_value(meta, "__gc", &RichTextBinding::__gc);
		ctx.set_map_value(meta, "__tostring", &RichTextBinding::__tostring);
		ctx.set_map_value(meta, "__eq", &RichTextBinding::__eq);
		ctx.set_map_value(meta, "__len", &RichTextBinding::__len);
		ctx.set_map_value(meta, "__concat", &RichTextBinding::__concat);
		ctx.set_map_value(meta, "__index", methods);
	}
}