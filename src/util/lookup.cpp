#include "lookup.h"

namespace YomitanMPV {
    static const nlohmann::json* member(const nlohmann::json& j, const char* key) {
        if (!j.is_object()) return nullptr;

        auto it = j.find(key);
        return it == j.end() ? nullptr : &(*it);
    }

    static std::string stringField(const nlohmann::json& j, const char* key) {
        const nlohmann::json* v = member(j, key);

        return v && v->is_string()
            ? v->get<std::string>()
            : "";
    }

    static std::string dataContent(const nlohmann::json& j) {
        const nlohmann::json* data = member(j, "data");
        return data ? stringField(*data, "content") : "";
    }

    template<typename F>
    static void walkContent(const nlohmann::json& node, const F& callback) {
        if (node.is_array()) {
            for (const auto& child : node)
                walkContent(child, callback);
            return;
        }

        if (!node.is_object()) return;

        callback(node);

        if (const nlohmann::json* content = member(node, "content"))
            walkContent(*content, callback);
    }

    static std::string plainText(const nlohmann::json& node) {
        if (node.is_string())
            return node.get<std::string>();

        if (node.is_array()) {
            std::string result;
            for (const auto& child : node)
                result += plainText(child);
            return result;
        }

        if (node.is_object()) {
            if (stringField(node, "tag") == "rt" ||
                dataContent(node) == "attribution-footnote")
                return "";

            if (const nlohmann::json* content = member(node, "content"))
                return plainText(*content);
        }

        return "";
    }

    static bool isSense(const nlohmann::json& node) {
        const nlohmann::json* data = member(node, "data");
        if (!data) return false;

        return member(*data, "sense-number") != nullptr ||
               dataContent(node) == "sense";
    }


    void parseSentenceFragments(
        const nlohmann::json& node,
        std::vector<LookupResult::WordFragment>& output,
        bool keyword
    ) {
        if (node.is_string()) {
            output.push_back({
                .fragment = node.get<std::string>(),
                .furigana = "",
                .key_word = keyword
            });
            return;
        }

        if (node.is_array()) {
            for (const auto& child : node)
                parseSentenceFragments(child, output, keyword);
            return;
        }

        if (!node.is_object()) return;

        const std::string tag = stringField(node, "tag");

        if (tag == "rt") return;

        keyword = keyword ||
                  dataContent(node) == "example-keyword";

        const nlohmann::json* content = member(node, "content");
        if (!content) return;

        if (tag == "ruby") {
            std::string base;
            std::string reading;

            if (content->is_array()) {
                for (const auto& child : *content) {
                    if (stringField(child, "tag") == "rt") {
                        if (const nlohmann::json* rt = member(child, "content"))
                            reading += plainText(*rt);
                    } else {
                        base += plainText(child);
                    }
                }
            } else {
                base = plainText(*content);
            }

            if (!base.empty()) {
                output.push_back({
                    .fragment = base,
                    .furigana = reading,
                    .key_word = keyword
                });
            }

            return;
        }

        parseSentenceFragments(*content, output, keyword);
    }

    LookupResult parseLookupResult(const nlohmann::json& json) {
        LookupResult result;

        const nlohmann::json* dictionaryEntries =
            member(json, "dictionaryEntries");

        if (!dictionaryEntries || !dictionaryEntries->is_array())
            return result;

        for (const auto& jsonEntry : *dictionaryEntries) {
            LookupResult::Entry entry;

            const nlohmann::json* headwords = member(jsonEntry, "headwords");

            if (headwords && headwords->is_array() &&
                !headwords->empty()) {

                const nlohmann::json& headword = (*headwords)[0];

                entry.word.push_back({
                    .fragment = stringField(headword, "term"),
                    .furigana = stringField(headword, "reading"),
                    .key_word = false
                });
                }

            const nlohmann::json* definitions =
                member(jsonEntry, "definitions");

            if (definitions && definitions->is_array()) {
                for (const auto& definition : *definitions) {
                    const nlohmann::json* entries =
                        member(definition, "entries");

                    if (!entries || !entries->is_array())
                        continue;

                    for (const auto& rawEntry : *entries) {
                        if (stringField(rawEntry, "type") !=
                            "structured-content")
                            continue;

                        const nlohmann::json* content =
                            member(rawEntry, "content");

                        if (!content) continue;

                        walkContent(*content, [&](const nlohmann::json& sense) {
                            if (!isSense(sense)) return;

                            LookupResult::DefinitionEntry defEntry;

                            walkContent(sense, [&](const nlohmann::json& node) {
                                const std::string type =
                                    dataContent(node);

                                if (type == "glossary") {
                                    const nlohmann::json* glosses =
                                        member(node, "content");

                                    if (!glosses) return;

                                    if (glosses->is_array()) {
                                        for (const auto& gloss : *glosses) {
                                            std::string text =
                                                plainText(gloss);

                                            if (!text.empty())
                                                defEntry.definitions.push_back(text);
                                        }
                                    } else {
                                        const std::string text =
                                            plainText(*glosses);

                                        if (!text.empty())
                                            defEntry.definitions.push_back(text);
                                    }
                                }

                                else if (type == "example-sentence") {
                                    LookupResult::DefinitionEntry::Example example;

                                    walkContent(node, [&](const nlohmann::json& part) {
                                        const std::string partType =
                                            dataContent(part);

                                        if (partType == "example-sentence-a") {
                                            if (const nlohmann::json* sentence =
                                                    member(part, "content")) {
                                                parseSentenceFragments(
                                                    *sentence,
                                                    example.sentence
                                                );
                                            }
                                        }

                                        else if (partType == "example-sentence-b") {
                                            if (const nlohmann::json* meaning =
                                                    member(part, "content")) {
                                                example.meaning =
                                                    plainText(*meaning);
                                            }
                                        }
                                    });

                                    if (!example.sentence.empty() ||
                                        !example.meaning.empty()) {
                                        defEntry.examples.push_back(example);
                                    }
                                }
                            });

                            if (!defEntry.definitions.empty() ||
                                !defEntry.examples.empty()) {
                                entry.definition_entries.push_back(defEntry);
                            }
                        });
                    }
                }
            }

            result.entries.push_back(entry);
        }

        return result;
    }
}