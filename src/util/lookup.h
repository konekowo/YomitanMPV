#pragma once
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace YomitanMPV {
    struct LookupResult {
        struct WordFragment {
            std::string fragment;
            std::string furigana;
            bool key_word;
        };

        struct DefinitionEntry {
            std::vector<std::string> definitions;

            struct Example {
                std::vector<WordFragment> sentence;
                std::string meaning;
            };

            std::vector<Example> examples;
        };

        struct Entry {
            std::vector<WordFragment> word;
            std::vector<DefinitionEntry> definition_entries;
        };

        std::vector<Entry> entries;
    };

    void parseSentenceFragments(const nlohmann::json& node, std::vector<LookupResult::WordFragment>& output, bool keyword = false);
    LookupResult parseLookupResult(const nlohmann::json& json);
}
