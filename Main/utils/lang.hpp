#pragma once

enum class Language
{
    English = 0,
    Chinese = 1,
};

namespace Lang
{
    inline Language currentLanguage = Language::English;

    inline void SetLanguage(Language lang)
    {
        currentLanguage = lang;
    }

    inline Language GetLanguage()
    {
        return currentLanguage;
    }

    struct TrPair
    {
        const char* en;
        const char* zh;
    };

    inline const char* Tr(const TrPair& pair)
    {
        switch (currentLanguage)
        {
        case Language::Chinese:
            return pair.zh;
        case Language::English:
        default:
            return pair.en;
        }
    }
}

#define TR(enStr, zhStr) Lang::Tr({ enStr, zhStr })
