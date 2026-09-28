#ifndef BANGLA_MORPHOLOGY_H
#define BANGLA_MORPHOLOGY_H

#include <string>
#include <vector>
#include <utility>

namespace bangla {

struct MorphologyCandidate {
    std::string bengali_text;
    std::string roman_stem;
    std::string roman_suffix;
    float confidence = 0.95f;
};

class BanglaMorphology {
public:
    // Decompose a hyphenated Banglish token into stem and suffix.
    // Examples:
    //   "Google-er" -> stem: "Google", suffix: "er"
    //   "pele-o"    -> stem: "pele",   suffix: "o"
    //   "Likhi-ke"  -> stem: "Likhi",  suffix: "ke"
    //   "office-e"  -> stem: "office", suffix: "e"
    //   "desh-ke"   -> stem: "desh",   suffix: "ke"
    //   "manush-er" -> stem: "manush", suffix: "er"
    static bool DecomposeHyphenatedToken(const std::string& token,
                                         std::string& out_stem,
                                         std::string& out_suffix);

    // Decompose compound Banglish tokens without hyphens (e.g. "geleo", "korleo", "peleo").
    static bool DecomposeCompoundToken(const std::string& token,
                                       std::string& out_stem,
                                       std::string& out_suffix);

    // Attaches a roman suffix to a Bengali stem using the phonetic and
    // grammatical rules of Bengali morphology.
    //
    // Rules:
    //   -er / -r (Genitive):
    //       If stem ends in consonant -> appends "-ের" (গুগলের, মানুষের, দেশের, অফিসের)
    //       If stem ends in vowel-kar -> appends "-র" (ঢাকার, লিখির, পেলের)
    //       If stem ends in independent vowel -> appends "-য়ের" (বইয়ের)
    //   -o (Emphatic / Inclusive):
    //       Appends "-ও" (পেলেও, গেলেও, করলেও, আমিও, আজও)
    //   -ke / -re (Accusative / Dative):
    //       Appends "-কে" / "-রে" (লিখিকে, দেশকে, গুগলকে, মানুষকে)
    //   -e (Locative):
    //       If stem ends in consonant -> appends "-ে" (অফিসে, দেশে, ঘরে, স্কুলে)
    //       If stem ends in vowel-kar/vowel -> appends "-য়" / "-তে" (ঢাকায়, গাড়িতে)
    //   -te (Locative / Instrumental):
    //       Appends "-তে" (গাড়িতে, বাড়িতে, লিখিতে)
    //   -ta / -ti (Definiteness / Classifier):
    //       Appends "-টা" / "-টি" (বইটা, গুগলটা, লিখিটি)
    //   -ra / -der (Plural):
    //       Appends "-রা" / "-দের" (মানুষদের, বন্ধুরা, বন্ধুদের)
    //   -gulo / -guli (Plural):
    //       Appends "-গুলো" / "-গুলি" (দেশগুলো, বইগুলো, দেশগুলি)
    static std::string AttachSuffix(const std::string& bengali_stem,
                                    const std::string& roman_suffix);

    // Checks if the given roman suffix is a recognized Bengali inflection/particle.
    static bool IsRecognizedSuffix(const std::string& roman_suffix);

    // Helper: checks whether a UTF-8 Bengali word ends in a vowel-sign (kar)
    static bool EndsInKar(const std::string& bengali_word);

    // Helper: checks whether a UTF-8 Bengali word ends in an independent vowel
    static bool EndsInIndependentVowel(const std::string& bengali_word);
};

} // namespace bangla

#endif // BANGLA_MORPHOLOGY_H
