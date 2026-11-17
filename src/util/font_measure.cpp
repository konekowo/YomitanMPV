#include "font_measure.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <type_traits>

#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H
#include <hb-ft.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite.h>
#else
#include <fontconfig/fontconfig.h>
#endif

// libass's default renderer obtains metrics at this size, then scales each glyph
// to the requested ASS font size.
constexpr double FONT_METRIC_SIZE = 256.0;

namespace YomitanMPV {
    struct FontFile {
        std::filesystem::path path;
        FT_Long index = 0;
        std::string postscript_name;
    };

#if defined(__APPLE__)
    template<typename T>
    struct CFDeleter {
        void operator()(T *value) const {
            CFRelease(value);
        }
    };

    template<typename T>
    using CFPtr = std::unique_ptr<std::remove_pointer_t<T>, CFDeleter<std::remove_pointer_t<T>>>;

    static std::string cf_string(const CFStringRef value) {
        if (!value)
            return {};

        const auto capacity = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
        std::vector<char> bytes(capacity);

        if (!CFStringGetCString(value, bytes.data(), capacity, kCFStringEncodingUTF8))
            return {};

        return bytes.data();
    }

    static CFPtr<CTFontRef> regular_fallback_font(const CTFontRef fallback, const uint32_t codepoint) {
        CFPtr<CFStringRef> family(CTFontCopyFamilyName(fallback));
        const void *key = kCTFontFamilyNameAttribute;
        const void *value = family.get();
        CFPtr<CFDictionaryRef> attributes(CFDictionaryCreate(
            nullptr, &key, &value, 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
        ));
        CFPtr<CTFontDescriptorRef> descriptor(CTFontDescriptorCreateWithAttributes(attributes.get()));
        CFPtr<CFSetRef> mandatory(CFSetCreate(nullptr, &key, 1, &kCFTypeSetCallBacks));
        CFPtr<CFArrayRef> matches(CTFontDescriptorCreateMatchingFontDescriptors(descriptor.get(), mandatory.get()));
        CFPtr<CTFontRef> best(static_cast<CTFontRef>(CFRetain(fallback)));
        int best_score = std::numeric_limits<int>::max();

        // libass selects weight 400 from the fallback family. Core Text's default
        // face can be weight 300 (e.g. Hiragino Sans W3), with different metrics.
        for (CFIndex i = 0; matches && i < CFArrayGetCount(matches.get()); ++i) {
            auto candidate_descriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matches.get(), i));
            CFPtr<CTFontRef> candidate(CTFontCreateWithFontDescriptor(candidate_descriptor, 0, nullptr));
            if (!candidate)
                continue;

            CFPtr<CFCharacterSetRef> characters(CTFontCopyCharacterSet(candidate.get()));
            if (!characters || !CFCharacterSetIsLongCharacterMember(characters.get(), codepoint))
                continue;

            CFPtr<CFDataRef> os2(CTFontCopyTable(candidate.get(), kCTFontTableOS2, kCTFontTableOptionNoOptions));
            if (!os2 || CFDataGetLength(os2.get()) < 8)
                continue;

            const auto *bytes = CFDataGetBytePtr(os2.get());
            const int weight = (bytes[4] << 8) | bytes[5];
            const int width = (bytes[6] << 8) | bytes[7];
            const int score = std::abs(weight - 400) + 1000 * std::abs(width - 5) +
                              ((CTFontGetSymbolicTraits(candidate.get()) & kCTFontTraitItalic) ? 10000 : 0);

            if (score < best_score) {
                best_score = score;
                best = std::move(candidate);
            }
        }

        return best;
    }
#elif defined(_WIN32)
    template<typename T>
    struct ComPtr {
        T *value = nullptr;

        ComPtr() = default;
        ComPtr(const ComPtr &) = delete;
        ComPtr &operator=(const ComPtr &) = delete;

        ~ComPtr() {
            if (value)
                value->Release();
        }

        T *operator->() const {
            return value;
        }
    };

    // DirectWrite's text layout performs the same fallback lookup used by libass.
    // Capture its font face without drawing any pixels.
    class FallbackRenderer final : public IDWriteTextRenderer {
    public:
        ComPtr<IDWriteFontFace> face;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **result) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;
        HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *disabled) override;
        HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *, DWRITE_MATRIX *matrix) override;
        HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *pixels) override;
        HRESULT STDMETHODCALLTYPE DrawGlyphRun(
            void *, FLOAT, FLOAT, DWRITE_MEASURING_MODE,
            const DWRITE_GLYPH_RUN *run, const DWRITE_GLYPH_RUN_DESCRIPTION *, IUnknown *
        ) override;
        HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *, IUnknown *) override;
        HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *, IUnknown *) override;
        HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL, BOOL, IUnknown *) override;
    private:
        ULONG references = 1;
    };

    HRESULT STDMETHODCALLTYPE FallbackRenderer::QueryInterface(REFIID iid, void **result) {
        if (!result)
            return E_POINTER;

        *result = nullptr;

        if (iid != __uuidof(IUnknown) && iid != __uuidof(IDWritePixelSnapping) && iid != __uuidof(IDWriteTextRenderer))
            return E_NOINTERFACE;

        *result = static_cast<IDWriteTextRenderer *>(this);
        AddRef();

        return S_OK;
    }

    ULONG STDMETHODCALLTYPE FallbackRenderer::AddRef() {
        return ++references;
    }

    ULONG STDMETHODCALLTYPE FallbackRenderer::Release() {
        const auto remaining = --references;

        if (!remaining)
            delete this;

        return remaining;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::IsPixelSnappingDisabled(void *, BOOL *disabled) {
        *disabled = TRUE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::GetCurrentTransform(void *, DWRITE_MATRIX *matrix) {
        *matrix = {1, 0, 0, 1, 0, 0};
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::GetPixelsPerDip(void *, FLOAT *pixels) {
        *pixels = 1;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::DrawGlyphRun(
        void *, FLOAT, FLOAT, DWRITE_MEASURING_MODE,
        const DWRITE_GLYPH_RUN *run, const DWRITE_GLYPH_RUN_DESCRIPTION *, IUnknown *
    ) {
        if (!face.value) {
            face.value = run->fontFace;
            face->AddRef();
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *, IUnknown *) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *, IUnknown *) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FallbackRenderer::DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL, BOOL, IUnknown *) {
        return S_OK;
    }
#endif

    static std::optional<FontFile> find_font_file(const std::string &family, const uint32_t codepoint = 0) {
#if defined(__APPLE__)
        const char *name = family.c_str();

        if (family == "sans-serif")
            name = "Helvetica";
        else if (family == "serif")
            name = "Times";
        else if (family == "monospace")
            name = "Courier";

        CFPtr<CFStringRef> font_name(CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8));
        if (!font_name)
            return std::nullopt;

        CFPtr<CTFontRef> font(CTFontCreateWithName(font_name.get(), 12, nullptr));
        if (!font)
            return std::nullopt;

        if (codepoint) {
            UniChar characters[2];
            CFIndex length = 1;

            if (codepoint <= 0xffff) {
                characters[0] = static_cast<UniChar>(codepoint);
            } else {
                characters[0] = static_cast<UniChar>(0xd800 + ((codepoint - 0x10000) >> 10));
                characters[1] = static_cast<UniChar>(0xdc00 + ((codepoint - 0x10000) & 0x3ff));
                length = 2;
            }

            CFPtr<CFStringRef> text(CFStringCreateWithCharacters(nullptr, characters, length));
            CFPtr<CTFontRef> fallback(CTFontCreateForString(font.get(), text.get(), CFRangeMake(0, length)));
            if (!fallback)
                return std::nullopt;

            font = regular_fallback_font(fallback.get(), codepoint);
        }

        CFPtr<CFTypeRef> attribute(CTFontCopyAttribute(font.get(), kCTFontURLAttribute));
        if (!attribute || CFGetTypeID(attribute.get()) != CFURLGetTypeID())
            return std::nullopt;

        CFPtr<CFStringRef> path(CFURLCopyFileSystemPath(static_cast<CFURLRef>(attribute.get()), kCFURLPOSIXPathStyle));
        CFPtr<CFStringRef> postscript_name(CTFontCopyPostScriptName(font.get()));
        const auto filename = cf_string(path.get());
        if (filename.empty())
            return std::nullopt;

        // Core Text identifies collection members by PostScript name, not FT face index.
        return FontFile{
            .path = filename,
            .index = 0,
            .postscript_name = cf_string(postscript_name.get())
        };
#elif defined(_WIN32)
        ComPtr<IDWriteFactory> factory;
        if (FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown **>(&factory.value)
        )))
            return std::nullopt;

        ComPtr<IDWriteFontCollection> collection;
        if (FAILED(factory->GetSystemFontCollection(&collection.value)))
            return std::nullopt;

        const char *name = family.c_str();

        if (family == "sans-serif")
            name = "Arial";
        else if (family == "serif")
            name = "Times New Roman";
        else if (family == "monospace")
            name = "Courier New";

        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0);
        if (length == 0)
            return std::nullopt;

        std::vector<wchar_t> wide_name(length);
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide_name.data(), length))
            return std::nullopt;

        UINT32 index = 0;
        BOOL exists = FALSE;
        ComPtr<IDWriteFontFamily> font_family;
        ComPtr<IDWriteFont> font;
        ComPtr<IDWriteFontFace> face;

        if (codepoint) {
            ComPtr<IDWriteTextFormat> format;
            ComPtr<IDWriteTextLayout> layout;
            ComPtr<FallbackRenderer> capture;
            capture.value = new FallbackRenderer;

            wchar_t characters[2];
            UINT32 count = 1;

            if (codepoint <= 0xffff) {
                characters[0] = static_cast<wchar_t>(codepoint);
            } else {
                characters[0] = static_cast<wchar_t>(0xd800 + ((codepoint - 0x10000) >> 10));
                characters[1] = static_cast<wchar_t>(0xdc00 + ((codepoint - 0x10000) & 0x3ff));
                count = 2;
            }

            ComPtr<IDWriteFont> fallback;
            if (FAILED(factory->CreateTextFormat(
                    L"Arial", nullptr, DWRITE_FONT_WEIGHT_MEDIUM,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 1, L"", &format.value
                )) ||
                FAILED(factory->CreateTextLayout(characters, count, format.value, 0, 0, &layout.value)) ||
                FAILED(layout->Draw(nullptr, capture.value, 0, 0)) || !capture->face.value ||
                FAILED(collection->GetFontFromFontFace(capture->face.value, &fallback.value)) ||
                FAILED(fallback->GetFontFamily(&font_family.value)))
                return std::nullopt;
        } else if (FAILED(collection->FindFamilyName(wide_name.data(), &index, &exists)) || !exists ||
                   FAILED(collection->GetFontFamily(index, &font_family.value))) {
            return std::nullopt;
        }

        if (FAILED(font_family->GetFirstMatchingFont(
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, &font.value
            )) ||
            FAILED(font->CreateFontFace(&face.value)))
            return std::nullopt;

        UINT32 count = 0;
        if (FAILED(face->GetFiles(&count, nullptr)) || count != 1)
            return std::nullopt;

        ComPtr<IDWriteFontFile> file;
        ComPtr<IDWriteFontFileLoader> loader;
        ComPtr<IDWriteLocalFontFileLoader> local_loader;
        const void *key = nullptr;
        UINT32 key_size = 0;
        if (FAILED(face->GetFiles(&count, &file.value)) ||
            FAILED(file->GetReferenceKey(&key, &key_size)) ||
            FAILED(file->GetLoader(&loader.value)) ||
            FAILED(loader->QueryInterface(
                __uuidof(IDWriteLocalFontFileLoader), reinterpret_cast<void **>(&local_loader.value)
            )))
            return std::nullopt;

        UINT32 path_length = 0;
        if (FAILED(local_loader->GetFilePathLengthFromKey(key, key_size, &path_length)))
            return std::nullopt;

        std::vector<wchar_t> path(path_length + 1);
        if (FAILED(local_loader->GetFilePathFromKey(key, key_size, path.data(), path_length + 1)))
            return std::nullopt;

        return FontFile{
            .path = std::filesystem::path(path.data()),
            .index = static_cast<FT_Long>(face->GetIndex()),
            .postscript_name = ""
        };
#else
        std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)> config(FcInitLoadConfigAndFonts(), FcConfigDestroy);
        std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(FcPatternCreate(), FcPatternDestroy);
        if (!config || !pattern)
            return std::nullopt;

        FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8 *>(family.c_str()));
        FcPatternAddInteger(pattern.get(), FC_WEIGHT, FC_WEIGHT_REGULAR);
        FcPatternAddInteger(pattern.get(), FC_SLANT, FC_SLANT_ROMAN);

        if (codepoint) {
            std::unique_ptr<FcCharSet, decltype(&FcCharSetDestroy)> characters(FcCharSetCreate(), FcCharSetDestroy);
            FcCharSetAddChar(characters.get(), codepoint);
            FcPatternAddCharSet(pattern.get(), FC_CHARSET, characters.get());
        }

        FcConfigSubstitute(config.get(), pattern.get(), FcMatchPattern);
        FcDefaultSubstitute(pattern.get());

        FcResult result;
        std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> match(
            FcFontMatch(config.get(), pattern.get(), &result), FcPatternDestroy
        );
        FcChar8 *path = nullptr;
        int index = 0;

        if (!match || FcPatternGetString(match.get(), FC_FILE, 0, &path) != FcResultMatch)
            return std::nullopt;

        FcPatternGetInteger(match.get(), FC_INDEX, 0, &index);

        return FontFile{
            .path = reinterpret_cast<const char *>(path),
            .index = index,
            .postscript_name = ""
        };
#endif
    }

    struct FontMeasure::Font {
        std::vector<FT_Byte> bytes;
        FT_Face face = nullptr;
        hb_font_t *shaper = nullptr;
        std::string render_name;

        ~Font();
    };

    FontMeasure::Font::~Font() {
        if (shaper)
            hb_font_destroy(shaper);

        if (face)
            FT_Done_Face(face);
    }

    FontMeasure::FontMeasure(const std::shared_ptr<spdlog::logger> &logger) {
        this->logger = logger;

        if (FT_Init_FreeType(&library))
            logger->error("[FontMeasure] Could not initialize FreeType");
    }

    FontMeasure::~FontMeasure() {
        fonts.clear();

        if (library)
            FT_Done_FreeType(library);
    }

    FontMeasure::Font *FontMeasure::load_font(const std::string &family, const uint32_t codepoint) {
        const auto [request, new_request] = font_requests.try_emplace({family, codepoint}, nullptr);

        if (!new_request || !library)
            return request->second;

        auto file = find_font_file(family, codepoint);
        if (!file && !codepoint && family != "sans-serif")
            file = find_font_file("sans-serif");

        if (!file)
            return nullptr;

        const auto [entry, inserted] = fonts.try_emplace(FontKey{file->path, file->index, file->postscript_name});
        if (!inserted) {
            request->second = entry->second.get();
            return request->second;
        }

        auto font = std::make_unique<Font>();

        // Reading through filesystem::path also supports non-ASCII Windows paths.
        std::ifstream stream(file->path, std::ios::binary | std::ios::ate);
        const auto length = stream.tellg();
        if (length > 0 && length <= std::numeric_limits<FT_Long>::max()) {
            font->bytes.resize(static_cast<size_t>(length));
            stream.seekg(0);
            if (stream.read(reinterpret_cast<char *>(font->bytes.data()), length)) {
                FT_New_Memory_Face(library, font->bytes.data(), static_cast<FT_Long>(length), file->index, &font->face);
            }
        }

        if (font->face && !file->postscript_name.empty() && font->face->num_faces > 1) {
            const auto count = font->face->num_faces;
            for (FT_Long index = 0; index < count; ++index) {
                const char *name = FT_Get_Postscript_Name(font->face);
                if (name && file->postscript_name == name)
                    break;

                FT_Done_Face(font->face);
                font->face = nullptr;
                if (index + 1 == count || FT_New_Memory_Face(
                    library, font->bytes.data(), static_cast<FT_Long>(font->bytes.size()), index + 1, &font->face
                ))
                    break;
            }
        }

        if (!font->face || !FT_IS_SCALABLE(font->face)) {
            logger->error("[FontMeasure] Could not load scalable font '{}'", family);
            return nullptr;
        }

        FT_Select_Charmap(font->face, FT_ENCODING_UNICODE);

        // Match libass's ASS metrics, including its Windows ascent/descent
        // preference and real-dimension (rather than em-square) sizing.
        const auto *os2 = static_cast<const TT_OS2 *>(FT_Get_Sfnt_Table(font->face, ft_sfnt_os2));
        if (os2 && static_cast<FT_Short>(os2->usWinAscent) + static_cast<FT_Short>(os2->usWinDescent) > 0) {
            font->face->ascender = static_cast<FT_Short>(os2->usWinAscent);
            font->face->descender = -static_cast<FT_Short>(os2->usWinDescent);
            font->face->height = font->face->ascender - font->face->descender;
        }

        if (font->face->ascender == font->face->descender || !font->face->height) {
            if (os2 && os2->sTypoAscender != os2->sTypoDescender) {
                font->face->ascender = os2->sTypoAscender;
                font->face->descender = os2->sTypoDescender;
            } else {
                font->face->ascender = static_cast<FT_Short>(font->face->bbox.yMax);
                font->face->descender = static_cast<FT_Short>(font->face->bbox.yMin);
            }

            font->face->height = font->face->ascender - font->face->descender;
        }

        FT_Size_RequestRec size{};
        size.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
        size.height = static_cast<FT_Long>(FONT_METRIC_SIZE * 64);
        if (FT_Request_Size(font->face, &size)) {
            logger->error("[FontMeasure] Could not set font size for '{}'", family);
            return nullptr;
        }

        font->shaper = hb_ft_font_create_referenced(font->face);
        hb_ft_font_set_load_flags(font->shaper, FT_LOAD_DEFAULT | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH | FT_LOAD_IGNORE_TRANSFORM);
        font->render_name = family;
#if defined(__APPLE__)
        // libass matches Microsoft SFNT full names, falling back to the
        // family name for fonts without those records (e.g. Apple's Times).
        if (font->face->family_name)
            font->render_name = font->face->family_name;

        for (FT_UInt i = 0; i < FT_Get_Sfnt_Name_Count(font->face); ++i) {
            FT_SfntName name{};
            if (FT_Get_Sfnt_Name(font->face, i, &name) || name.platform_id != 3 || name.name_id != 4)
                continue;

            CFPtr<CFStringRef> full_name(CFStringCreateWithBytes(
                nullptr, name.string, name.string_len, kCFStringEncodingUTF16BE, false
            ));
            const auto decoded_name = cf_string(full_name.get());
            if (!decoded_name.empty())
                font->render_name = decoded_name;

            if (name.language_id == 0x409 && !decoded_name.empty())
                break;
        }
#endif
        entry->second = std::move(font);
        request->second = entry->second.get();

        return request->second;
    }

    FontMeasure::Font *FontMeasure::font_for_character(const std::string &family, Font *base, const uint32_t character) {
        if (FT_Get_Char_Index(base->face, character))
            return base;

        auto &fallbacks = fallback_fonts[family];
        for (auto *font : fallbacks) {
            if (FT_Get_Char_Index(font->face, character))
                return font;
        }
#if defined(__APPLE__)
        auto *font = load_font(base->render_name, character);
#else
        auto *font = load_font(family, character);
#endif
        if (font && FT_Get_Char_Index(font->face, character)) {
            fallbacks.push_back(font);
            return font;
        }

        return base;
    }

    Clay_Dimensions FontMeasure::measure(
        const Clay_StringSlice &text,
        const Clay_TextElementConfig &config,
        const std::string &family
    ) {
        if (config.fontSize == 0)
            return {};

        auto *base = load_font(family);
        if (!base && family != "sans-serif")
            base = load_font("sans-serif");

        if (!base || !text.chars || text.length <= 0)
            return {
                .width = 0,
                .height = static_cast<float>(config.fontSize)
            };

        struct Character {
            uint32_t codepoint;
            uint32_t byte_offset;
            Font *font;
            hb_script_t script;
        };

        std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer(hb_buffer_create(), hb_buffer_destroy);
        hb_buffer_add_utf8(buffer.get(), text.chars, text.length, 0, text.length);

        unsigned int count = 0;
        const auto *decoded = hb_buffer_get_glyph_infos(buffer.get(), &count);
        auto *unicode = hb_unicode_funcs_get_default();
        std::vector<Character> characters;
        characters.reserve(count);
        hb_script_t script = HB_SCRIPT_UNKNOWN;
        Font *previous_font = base;

        for (unsigned int i = 0; i < count; ++i) {
            const auto codepoint = decoded[i].codepoint;
            auto current_script = hb_unicode_script(unicode, codepoint);
            if (current_script == HB_SCRIPT_COMMON || current_script == HB_SCRIPT_INHERITED)
                current_script = script;
            else
                script = current_script;

            // Formatting controls and variation selectors stay with their base
            // glyph; they must not trigger a fallback to a missing-glyph box.
            const bool invisible =
                hb_unicode_general_category(unicode, codepoint) == HB_UNICODE_GENERAL_CATEGORY_FORMAT ||
                (codepoint >= 0xfe00 && codepoint <= 0xfe0f) ||
                (codepoint >= 0xe0100 && codepoint <= 0xe01ef);
            Font *font = base;

            if (codepoint != '\n') {
                font = invisible ? previous_font : font_for_character(family, base, codepoint);
            }

            characters.push_back({
                .codepoint = codepoint,
                .byte_offset = decoded[i].cluster,
                .font = font,
                .script = current_script
            });
            previous_font = font;

            if (codepoint == '\n') {
                script = HB_SCRIPT_UNKNOWN;
                previous_font = base;
            }
        }

        // Resolve leading common/inherited characters from following context.
        script = HB_SCRIPT_UNKNOWN;
        for (auto it = characters.rbegin(); it != characters.rend(); ++it) {
            if (it->codepoint == '\n')
                script = HB_SCRIPT_UNKNOWN;
            else if (it->script == HB_SCRIPT_UNKNOWN)
                it->script = script;
            else
                script = it->script;
        }

        const double scale = config.fontSize / FONT_METRIC_SIZE;
        float width = 0;
        float height = 0;
        float line_width = 0;
        float ascender = 0;
        float descender = 0;

        auto finish_line = [&] {
            width = std::max(width, line_width);
            height += ascender + descender > 0 ? ascender + descender : config.fontSize;
            line_width = 0;
            ascender = 0;
            descender = 0;
        };

        for (size_t start = 0; start < characters.size();) {
            if (characters[start].codepoint == '\n') {
                finish_line();
                ++start;
                continue;
            }

            const auto &first = characters[start];
            size_t end = start + 1;
            while (end < characters.size() && characters[end].codepoint != '\n' &&
                   characters[end].font == first.font && characters[end].script == first.script)
                ++end;

            const auto byte_end = end == characters.size() ? text.length : characters[end].byte_offset;
            hb_buffer_reset(buffer.get());
            hb_buffer_add_utf8(buffer.get(), text.chars, text.length, first.byte_offset, byte_end - first.byte_offset);
            hb_buffer_set_script(buffer.get(), first.script);
            hb_buffer_guess_segment_properties(buffer.get());

            const hb_feature_t features[] = {
                {HB_TAG('k', 'e', 'r', 'n'), 1, 0, HB_FEATURE_GLOBAL_END},
                {HB_TAG('l', 'i', 'g', 'a'), config.letterSpacing ? 0u : 1u, 0, HB_FEATURE_GLOBAL_END},
                {HB_TAG('c', 'l', 'i', 'g'), config.letterSpacing ? 0u : 1u, 0, HB_FEATURE_GLOBAL_END},
                {HB_TAG('v', 'e', 'r', 't'), 0, 0, HB_FEATURE_GLOBAL_END},
                {HB_TAG('v', 'k', 'n', 'a'), 0, 0, HB_FEATURE_GLOBAL_END}
            };
            hb_shape(first.font->shaper, buffer.get(), features, 5);

            const auto *positions = hb_buffer_get_glyph_positions(buffer.get(), &count);
            const auto *info = hb_buffer_get_glyph_infos(buffer.get(), nullptr);
            double advance = 0;
            unsigned int clusters = 0;

            for (unsigned int i = 0; i < count; ++i) {
                // libass scales and rounds each glyph separately to 26.6 units.
                advance += std::nearbyint(positions[i].x_advance * scale);
                if (i == 0 || info[i].cluster != info[i - 1].cluster)
                    ++clusters;
            }

            // Clay subtracts final letter spacing. Combining marks share a cluster.
            line_width += static_cast<float>(std::abs(advance) / 64.0 + static_cast<double>(clusters) * config.letterSpacing);
            const auto *face = first.font->face;
            ascender = std::max(ascender, static_cast<float>(FT_MulFix(face->ascender, face->size->metrics.y_scale) * scale / 64));
            descender = std::max(descender, static_cast<float>(FT_MulFix(-face->descender, face->size->metrics.y_scale) * scale / 64));
            start = end;
        }

        finish_line();

        return {
            .width = width,
            .height = height
        };
    }

    std::string FontMeasure::render_font_name(const std::string &family) {
#if defined(__APPLE__)
        // Core Text accepts family aliases that libass does not. Use the exact
        // selected face for drawing as well as measurement.
        if (auto *font = load_font(family); font && !font->render_name.empty())
            return font->render_name;
#endif
        return family;
    }

    void FontMeasure::clear_cache() {
        font_requests.clear();
        fallback_fonts.clear();
        fonts.clear();
    }
}
