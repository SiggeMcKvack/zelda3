// text_decode.c - Multi-language text decoding for ALTTP dialogue extraction
// Matches Python's text_compression.py decode_strings_generic functionality
#include "text_decode.h"
#include "restool_util.h"
#include "../logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ============================================================================
// Language-specific alphabet and dictionary tables
// ============================================================================

// US alphabet (95 characters)
static const char *kTextAlphabet_US[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]",
  "\"", "[Up]", "[Down]", "[Left]", "[Right]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]",
  "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]",
};

// PT-CBT alphabet (98 characters) - the US alphabet with unused symbol glyphs
// redrawn as accented letters, plus 0x60-0x61
static const char *kTextAlphabet_PT_CBT[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "ê", "ã", "é", "ú", "ó", "[LinkL]", "[LinkR]",
  "\"", "[Up]", "[Down]", "[Left]", "[Right]", "í", "[1HeartL]", "[1HeartR]", "[2HeartL]",
  "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]",
  "[5F]", "ç", "á",
};

// IT alphabet (98 characters) - the US alphabet plus 0x5F-0x61. The Italian
// text uses 0x61, a "!" centered in an 8 pixel wide cell, for most exclamations
static const char *kTextAlphabet_IT[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]",
  "\"", "[Up]", "[Down]", "[Left]", "[Right]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]",
  "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]",
  "[5F]", "[60]", "[!]",
};

// GL alphabet (95 characters) - the US alphabet with letters Galician does not
// need (j w ' and some symbols) redrawn as accented letters, ñ and a note
static const char *kTextAlphabet_GL[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "í", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "ú", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "Á", ">", "é", "ó", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "ñ", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]", "[Note]", "[4HeartR]", " ", "<", "á", "É", "Ó", "Ú",
};

// CA alphabet (99 characters) - the US alphabet with symbol glyphs redrawn as
// accented letters, plus 0x61 (·) and 0x62 (É). 'l' is written with the
// capital I glyph, so 0x08 decodes as l and 0x25 is ç
static const char *kTextAlphabet_CA[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "l", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "ç", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "à", ">", "(", ")", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]", "ü", "[Up]", "[Down]", "í",
  "è", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]", "é", "[4HeartR]", " ", "<", "ï", "ò", "ó", "ú", "[5F]",
  "[60]", "·", "É",
};

// GA alphabet (103 characters) - the US alphabet, with 0x17 (X, unused) as é
// and the Irish letters that the Irish ROM draws over W w [Waves] [Snake] '
// moved to 0x62-0x66, so that the untranslated English messages keep their
// letters (see FixIrishFont in main.c)
static const char *kTextAlphabet_GA[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "é", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]", "[5F]",
  "[60]", "[61]", "Ó", "ó", "ú", "á", "í",
};

// The Irish ROM's own alphabet, used to decode the messages it translates
static const char *kTextAlphabet_GA_ROM[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "Ó", "é", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "ó", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]", "ú", "á", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "í", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]",
};

// The Irish ROM translates only these messages; the rest are the US text
static const uint16_t kIrishMessages[] = {
  3, 13, 14, 15, 16, 17, 18, 19, 20, 21, 31, 32, 81, 82, 95, 96, 108, 123, 178, 179,
  274, 275, 276, 277, 390,
};

// Messages the Irish ROM overwrote with requests for translation help, or
// emptied; they get the US text back
static const struct { uint16_t id; const char *text; } kIrishRestored[] = {
  { 28, "Thank you, [Name].  I had a[2]feeling you were getting close." },
  { 36, "All right,  let's get out of here[2]before the wizard notices.  I[3]know a secret path, but first[Waitkey][Scroll]we have to go to the[Scroll]first floor.  Let's  go!" },
  { 37, "[Name], listen carefully.  The[2]wizard is magically controlling[3]all the soldiers in the castle.[Waitkey][Scroll]I fear the worst for[Scroll]my father[...][Scroll]The wizard is an inhuman fiend[Waitkey][Scroll]with strong magical powers![Scroll][...]   [...]   [...][Waitkey][Scroll]Do you understand?[Scroll]    > Yes[Scroll]       Not at all[Choose]" },
  { 58, "I will give 100 Rupees to the[2]man who finds the descendants[3]of the wise men.   THE KING" },
  { 393, "I see.  Then I give up.  Save[2]some Rupees and come back." },
  { 394, "OK!  Time's up, game over.[2]Come back again.  Good bye[...]" },
  { 395, "Come back again![2]I will be waiting for you." },
  { 396, "I can't tell you details, but[2]it's not a convenient time for[3]me now.  Come back here again.[Waitkey][Scroll]Sorry." },
};

// DE/EN alphabet (112 characters - includes extended chars)
static const char *kTextAlphabet_DE[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]",
  "\"", "[UpL]", "[UpR]", "[LeftL]", "[LeftR]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]",
  "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "ö", "[A]", "[B]", "[X]", "[Y]", "ü",
  "ß", ":", "[DownL]", "[DownR]", "[RightL]", "[RightR]",
  "è", "é", "ê", "à", "ù", "ç", "Ä", "Ö", "Ü", "ä"
};

// FR alphabet (112 characters)
static const char *kTextAlphabet_FR[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]", "[Waves]", "[Snake]", "[LinkL]", "[LinkR]",
  "\"", "[UpL]", "[UpR]", "[LeftL]", "[LeftR]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]",
  "[3HeartL]", "[3HeartR]", "[4HeartL]", "[4HeartR]", " ", "ö", "[A]", "[B]", "[X]", "[Y]", "ü",
  "ô", ":", "[DownL]", "[DownR]", "[RightL]", "[RightR]",
  "è", "é", "ê", "à", "ù", "ç", "â", "û", "î", "ä"
};

// US dictionary (87 entries)
static const char *kTextDictionary_US[] = {
  "    ", "   ", "  ", "'s ", "and ",
  "are ", "all ", "ain", "and", "at ",
  "ast", "an", "at", "ble", "ba",
  "be", "bo", "can ", "che", "com",
  "ck", "des", "di", "do", "en ",
  "er ", "ear", "ent", "ed ", "en",
  "er", "ev", "for", "fro", "give ",
  "get", "go", "have", "has", "her",
  "hi", "ha", "ight ", "ing ", "in",
  "is", "it", "just", "know", "ly ",
  "la", "lo", "man", "ma", "me",
  "mu", "n't ", "non", "not", "open",
  "ound", "out ", "of", "on", "or",
  "per", "ple", "pow", "pro", "re ",
  "re", "some", "se", "sh", "so",
  "st", "ter ", "thin", "ter", "tha",
  "the", "thi", "to", "tr", "up",
  "ver", "with", "wa", "we", "wh",
  "wi", "you", "Her", "Tha", "The",
  "Thi", "You",
};

// PT-CBT dictionary - the US dictionary bytes read with the CBT alphabet (' is í)
static const char *kTextDictionary_PT_CBT[] = {
  "    ", "   ", "  ", "ís ", "and ",
  "are ", "all ", "ain", "and", "at ",
  "ast", "an", "at", "ble", "ba",
  "be", "bo", "can ", "che", "com",
  "ck", "des", "di", "do", "en ",
  "er ", "ear", "ent", "ed ", "en",
  "er", "ev", "for", "fro", "give ",
  "get", "go", "have", "has", "her",
  "hi", "ha", "ight ", "ing ", "in",
  "is", "it", "just", "know", "ly ",
  "la", "lo", "man", "ma", "me",
  "mu", "nít ", "non", "not", "open",
  "ound", "out ", "of", "on", "or",
  "per", "ple", "pow", "pro", "re ",
  "re", "some", "se", "sh", "so",
  "st", "ter ", "thin", "ter", "tha",
  "the", "thi", "to", "tr", "up",
  "ver", "with", "wa", "we", "wh",
  "wi", "you", "Her", "Tha", "The",
  "Thi", "You",
};

// GL dictionary (97 entries) - from the Galician ROM
static const char *kTextDictionary_GL[] = {
  "     ", "razas", "Alfa", "Caba", "Donc", "Dour", "Hyli", "leir",
  "máis", "Non ", "orza", "unha", "que ", "    ", "Rupi", "Sabi",
  "Sete", "Trif", "Xaro", "   ", "...", "ada", "ado", "ame",
  "ar ", "ara", "as ", "che", "da ", "de ", "der", "des",
  "do ", "dos", "ent", "er ", "ero", "es ", "est", "la ",
  "lo ", "ndo", "non", "nte", "ode", "on ", "or ", "ora",
  "os ", "par", "pod", "por", "que", "ra ", "res", "ro ",
  "te ", "tra", "un ", "a ", "ad", "al", "an", "ar",
  "as", "ca", "co", "e ", "ei", "en", "er", "es",
  "ha", "he", "la", "lo", "me", "n ", "no", "nt",
  "o ", "od", "or", "os", "pa", "po", "ra", "re",
  "ro", "se", "st", "ta", "te", "to", "tr", "ue",
  "un",
};

// CA dictionary (97 entries) - from the Catalan ROM
static const char *kTextDictionary_CA[] = {
  "    ", "   ", "  ", "...", " la ", " el ", " del", "eu ",
  "res", " se", " co", "an", "at", "ent", "ba", "be",
  "bo", " per", " a ", " de", "ck", " no", "di", "do",
  "ies", "que", "ra ", "ar ", "ort", "en", "er", "ev",
  "est", " to", " que ", "es ", "go", " de ", "ant", " l'",
  "hi", "ha", " de l", "... ", "in", "is", "it", " un ",
  "ques", " ha", "la", "lo", " ca", "ma", "me", "mu",
  " en ", " d'", "Per", "una ", "els ", " us ", "of", "on",
  "or", " al", "ada", " és", "ell", "pas", "re", " amb",
  "se", "sh", "so", "st", "pode", "les ", "i, ", "ing",
  " po", "tre", "to", "tr", "up", "cor", " més", "wa",
  "we", "wh", "wi", " ma", "par", " ll", "per", "ona",
  "vos",
};

// DE dictionary (112 entries) - must match Python kTextDictionary_DE exactly
static const char *kTextDictionary_DE[] = {
  "    ", "   ", "                                          ", "-Knopf", " ich ",
  " Sch", " Ver", " zu ", " es ", "aber",
  "alle", "auch", "ang", "aus", "auf",
  "an", "bist", "bin", "bei", "der ",
  "die ", "das ", "den ", "dem ", "daß",
  "der", "die", "das", "den", "da",
  "etwas", "ein ", "ein", "en ", "er ",
  "es ", "en", "er", "es", "ei",
  "für", "fe", "habe", "hier", "hast",
  "her", "ich ", "icht", "ich", "ist",
  "ie ", "im", "ie", "kannst ", "kannst",
  "kommen", "kann ", "ll", "mich", "mein",
  "mit", "mal", "mir", "nicht ", "nicht",
  "nen", "nn", "och ", "och", "or",
  "schon", "sich", "sein", "sch", "sie",
  "st", "tte", "te ", "te", "und ",
  "und", "ung", "um", "von", "ver",
  "vor", "wird", "zu ", "Amulett", "Aber",
  "Deine", "Dich ", "Dir ", "Dir", "Der",
  "Die", "Das", "Du ", "Du", "Da",
  "Ein", "Hyrule", "Hier", "Ich ", "Master-Schwert",
  "Mach", "Rubine", "Sch", "Sie", "Ver",
  "Weisen", "Zelda",
};

// FR dictionary (99 entries) - must match Python kTextDictionary_FR exactly
static const char *kTextDictionary_FR[] = {
  "                                          ", " de ", " la ", " le ", " ! ",
  " d", " p", " t", " !", ", c'est moi, Sahasrahla",
  ", ", "ais ", "as ", "an", "ai",
  "a ", "che", "ce", "ch", "dans ",
  "des ", "de ", "de", "est ", "ent",
  "en ", "er ", "es ", "en", "es",
  "et", "eu", "e,", "e ", "ique",
  "ien", "is ", "ie", "in", "ir",
  "is", "i ", "les ", "la ", "le ",
  "le", "ll", "maintenant", "magique", "ment",
  "mon", "mai", "me", "ne ", "onne",
  "oir", "our", "ouv", "oi", "on",
  "ou", "or", "pouvoir", "pour", "peux",
  "pas", "que ", "qu", "rubis", "re ",
  "ra", "re", "r ", "sorcier", "s l",
  "s d", "se", "so", "s ", "tro",
  "te ", "tu ", "te", "t ", "un",
  "ur", "u ", "ver", "Ah ! Ah ! Ah !", "C'est",
  "Ganon", "Maintenant", "Merci", "Monde", "Perle de Lune",
  "Tu as trouvé ", "Ténèbres", "Tu peux", "Tu ",
};

// SV alphabet (99 characters) - Swedish with Å, Ä, Ö
static const char *kTextAlphabet_SV[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "Ö", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "å", ".", ",", "ä", ">", "(", ")", "ö",
  "Å", "Ä", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]",
  "[4HeartL]", "[4HeartR]", " ", "<", "[Ankh]", "[Waves]", "[Snake]", "-", "[I]",
  "[i]", "…", " ",
};

// SV dictionary (97 entries) - Swedish
static const char *kTextDictionary_SV[] = {
  "    ", "   ", "  ", "Du ", "till", "vill", "bara", "det", "den", "och",
  "en ", "r ", "n ", "ett", "en", " d", "a ", "Hjäl", "har", "ter",
  "t ", "var", " s", "de", "kan", "med", "som", "för", "att", "ar",
  " h", "er", "jag", "dig", "öppna", "mig", "är", "inte", "hit", "på ",
  "an", "e ", "rupie", "0kej", " m", "et", ", ", "gång", "måst", "ten",
  " f", "u ", "men", "te", "tt", "ka", "vara", "ken", "0m ", "från",
  "myck", "någo", "in", " k", " i", "vil", "bar", "ond", "För", "Jag",
  "ra", "tack", "ll", "g ", "ta", "om", "anna", "alla", "en,", "ber",
  "hem", "han", "st", "ig", " t", "tro", "kraf", "ör", " v", "ag",
  "… ", "får", "sin", "mme", "mma", "en ", "tat",
};

// PL alphabet (99 characters) - Polish with ą, ć, ę, ł, ń, ó, ś, ź, ż
static const char *kTextAlphabet_PL[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "ć", "[Right]", "(", ")", "[Ankh]",
  "[Waves]", "[Snake]", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "ę",
  "ł", "ń", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]",
  "ą", "[4HeartR]", " ", "[Left]", "ó", "ś", "ż", "ź", "Ł",
  "Ś", "Ż", "Ź",
};

// PL dictionary (97 entries) - Polish
static const char *kTextDictionary_PL[] = {
  "Trój", "...", "ść", "Nie", " nie", " się", "może", " że", "and", "at ",
  " ty", "an", "at", "kus", "ba", "be", "bo", "chce", "che", "ki ",
  "za", "des", "di", "do", "en ", "er ", "sz ", "ent", "ed ", "en",
  "er", " w", "moc", "zię", "przez", "ale", "go", "dzie", "has", "rze",
  "hi", "ha", "który", "aby ", "in", "is", "it", "twoj", "Może", "łeś",
  "la", "lo", "czn", "ma", "me", "mu", "szcz", "ska", "śli", "przy",
  "znaj", "iecz", "of", "on", "or", "   ", "ple", "pow", "pro", "re ",
  "re", "mnie", "se", " z", "so", "st", "któr", " jak", "ksz", "sze",
  "coś", " je", "to", "tr", "up", "kie", "praw", "wa", "we", "mi",
  "wi", "szy", "chc", "pra", "cie", " i ", "esz",
};

// PT alphabet (121 characters) - Portuguese with accented characters
static const char *kTextAlphabet_PT[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]",
  "[Waves]", "[Snake]", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]",
  "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]", "[I]",
  "¡", "[!]", "Á", "À", "Â", "Ã", "É", "Ê", "Í", "Ó", "Ô", "Õ", "Ú", "á", "à", "â",
  "ã", "é", "ê", "í", "ó", "ô", "õ", "ú", "ç",
};

// PT dictionary (97 entries) - Portuguese
static const char *kTextDictionary_PT[] = {
  "     ", "    ", "   ", "                                          ", "o ", "a ", "e ", "..", "de", "ar",
  "s ", "ra", " d", "es", "ocê ", "do", " a", " p", "er", " e",
  "que", "r ", "os", "te", ", ", "as", "or", "m ", "en", " o",
  "nt", "re", " s", "co", "da", "se", "st", " c", " m", "em",
  "ma", "ta", " n", "ad", "on", "al", "ro", "an", "u ", "nd",
  " um", "pa", "ca", "el", " f", "to", "in", " t", "ou", "ei",
  "ss", "ir", "no", "ri", "tr", "me", "la", "ia", "le", "ve",
  "is", "sa", "eu", "pe", "a.", "na", "so", "mo", "ga", "o.",
  "á ", "lo", "ha", "pr", "ua", " l", "! ", "ui", "am", "ti",
  "io", "gu", "i ", "di", "nh", " i", "id",
};

// ES alphabet (99 characters) - Spanish with ñ, á, é, í, ó, ú
static const char *kTextAlphabet_ES[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "é", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "ó", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "[Waves]", ".", ",", "[...]", ">", "(", ")",
  "ñ", "ú", "á", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "í", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]",
  "[Ankh]", "[4HeartR]", " ", "[Snake]", "[A]", "[B]", "[X]", "[Y]", "[I]",
  "¡", "¿", "Ñ",
};

// ES dictionary (97 entries) - Spanish
static const char *kTextDictionary_ES[] = {
  "    ", "   ", "  ", " en", " la ", " el ", " de ", "ien", "tra", " de",
  "te ", "ar", "a ", "ada", "es", "as", "o ", " con", "ero", "ado",
  "e ", "que", "en", "al", "os ", "ora", "nte", " al", "lo ", "or",
  "os", "er", "aci", "res", " que ", " es", "el", "los ", "tar", " se",
  ", ", "ro", " de l", " est", "re", "on", "an", "pued", " del", "ás ",
  "la", "ti", "la ", "Es", "to", "ta", "para", "uer", "ier", " un ",
  " por", "oder", "da", "in", "cu", " ha", "per", "ano", " ve", "cer",
  "lo", " no ", "ic", "ra", "ab", "ir", " una", "undo", "es ", "as ",
  "con", "a, ", "te", " m", "gu", " tu", "ando", " p", "de", "le",
  "ol", "o, ", "ten", "lle", " a ", "aba", "com",
};

// NL alphabet (94 characters) - Dutch (same as US but without [Y])
static const char *kTextAlphabet_NL[] = {
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f",
  "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
  "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?",
  "-", ".", ",", "[...]", ">", "(", ")", "[Ankh]",
  "[Waves]", "[Snake]", "[LinkL]", "[LinkR]", "\"", "[Up]", "[Down]", "[Left]",
  "[Right]", "'", "[1HeartL]", "[1HeartR]", "[2HeartL]", "[3HeartL]", "[3HeartR]",
  "[4HeartL]", "[4HeartR]", " ", "<", "[A]", "[B]", "[X]", "[Y]",
};

// NL dictionary (97 entries) - Dutch (same as US)
static const char *kTextDictionary_NL[] = {
  "    ", "   ", "  ", "'s ", "and ", "are ", "all ", "ain", "and", "at ",
  "ast", "an", "at", "ble", "ba", "be", "bo", "can ", "che", "com",
  "ck", "des", "di", "do", "en ", "er ", "ear", "ent", "ed ", "en",
  "er", "ev", "for", "fro", "give ", "get", "go", "have", "has", "her",
  "hi", "ha", "ight ", "ing ", "in", "is", "it", "just", "know", "ly ",
  "la", "lo", "man", "ma", "me", "mu", "n't ", "non", "not", "open",
  "ound", "out ", "of", "on", "or", "per", "ple", "pow", "pro", "re ",
  "re", "some", "se", "sh", "so", "st", "ter ", "thin", "ter", "tha",
  "the", "thi", "to", "tr", "up", "ver", "with", "wa", "we", "wh",
  "wi", "you", "Her", "Tha", "The", "Thi", "You",
};

// US command names and lengths
static const char *kText_CommandNames_US[] = {
  "NextPic", "Choose", "Item", "Name", "Window", "Number",
  "Position", "ScrollSpd", "Selchg", "Unused_Crash", "Choose3",
  "Choose2", "Scroll", "1", "2", "3", "Color",
  "Wait", "Sound", "Speed", "Unused_Mark", "Unused_Mark2", "Unused_Clear",
  "Waitkey", "EndMessage"
};

static const uint8_t kText_CommandLengths_US[] = {
  1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1
};

// EU command names and lengths
static const char *kText_CommandNames_EU[] = {
  "Selchg", "Choose3", "Choose2", "Scroll", "1", "2", "3",
  "Color", "Wait", "Sound", "Speed", "Mark", "Mark2",
  "Clear", "Waitkey", "EndMessage", "NextPic", "Choose",
  "Item", "Name", "Window", "Number", "Position", "ScrollSpd"
};

static const uint8_t kText_CommandLengths_EU[] = {
  1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2
};

// ============================================================================
// Language configurations
// ============================================================================

static const LanguageConfig kLangUS = {
  .alphabet = kTextAlphabet_US,
  .alphabet_size = sizeof(kTextAlphabet_US) / sizeof(kTextAlphabet_US[0]),
  .dictionary = kTextDictionary_US,
  .dictionary_size = sizeof(kTextDictionary_US) / sizeof(kTextDictionary_US[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangEN = {
  .alphabet = kTextAlphabet_DE,
  .alphabet_size = sizeof(kTextAlphabet_DE) / sizeof(kTextAlphabet_DE[0]),
  .dictionary = kTextDictionary_US,
  .dictionary_size = sizeof(kTextDictionary_US) / sizeof(kTextDictionary_US[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf60, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangDE = {
  .alphabet = kTextAlphabet_DE,
  .alphabet_size = sizeof(kTextAlphabet_DE) / sizeof(kTextAlphabet_DE[0]),
  .dictionary = kTextDictionary_DE,
  .dictionary_size = sizeof(kTextDictionary_DE) / sizeof(kTextDictionary_DE[0]),
  .command_lengths = kText_CommandLengths_EU,
  .command_names = kText_CommandNames_EU,
  .command_count = sizeof(kText_CommandNames_EU) / sizeof(kText_CommandNames_EU[0]),
  .rom_addrs = {0x9c8000, 0x8CEB00, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x70,
  .SWITCH_BANK = 0x88,
  .FINISH = 0x8f,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x90,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = true
};

static const LanguageConfig kLangFR = {
  .alphabet = kTextAlphabet_FR,
  .alphabet_size = sizeof(kTextAlphabet_FR) / sizeof(kTextAlphabet_FR[0]),
  .dictionary = kTextDictionary_FR,
  .dictionary_size = sizeof(kTextDictionary_FR) / sizeof(kTextDictionary_FR[0]),
  .command_lengths = kText_CommandLengths_EU,
  .command_names = kText_CommandNames_EU,
  .command_count = sizeof(kText_CommandNames_EU) / sizeof(kText_CommandNames_EU[0]),
  .rom_addrs = {0x9c8000, 0x8CE800, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x70,
  .SWITCH_BANK = 0x88,
  .FINISH = 0x8f,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x90,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = true
};

static const LanguageConfig kLangFR_C = {
  .alphabet = kTextAlphabet_FR,
  .alphabet_size = sizeof(kTextAlphabet_FR) / sizeof(kTextAlphabet_FR[0]),
  .dictionary = kTextDictionary_FR,
  .dictionary_size = sizeof(kTextDictionary_FR) / sizeof(kTextDictionary_FR[0]),
  .command_lengths = kText_CommandLengths_EU,
  .command_names = kText_CommandNames_EU,
  .command_count = sizeof(kText_CommandNames_EU) / sizeof(kText_CommandNames_EU[0]),
  .rom_addrs = {0x9c8000, 0x8CF150, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x70,
  .SWITCH_BANK = 0x88,
  .FINISH = 0x8f,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x90,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = true
};

static const LanguageConfig kLangSV = {
  .alphabet = kTextAlphabet_SV,
  .alphabet_size = sizeof(kTextAlphabet_SV) / sizeof(kTextAlphabet_SV[0]),
  .dictionary = kTextDictionary_SV,
  .dictionary_size = sizeof(kTextDictionary_SV) / sizeof(kTextDictionary_SV[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangPL = {
  .alphabet = kTextAlphabet_PL,
  .alphabet_size = sizeof(kTextAlphabet_PL) / sizeof(kTextAlphabet_PL[0]),
  .dictionary = kTextDictionary_PL,
  .dictionary_size = sizeof(kTextDictionary_PL) / sizeof(kTextDictionary_PL[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangPT = {
  .alphabet = kTextAlphabet_PT,
  .alphabet_size = sizeof(kTextAlphabet_PT) / sizeof(kTextAlphabet_PT[0]),
  .dictionary = kTextDictionary_PT,
  .dictionary_size = sizeof(kTextDictionary_PT) / sizeof(kTextDictionary_PT[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0x62,
  .has_escape = true,
  .uses_new_format = true  // PT uses "new" EU encoder
};

static const LanguageConfig kLangPT_CBT = {
  .alphabet = kTextAlphabet_PT_CBT,
  .alphabet_size = sizeof(kTextAlphabet_PT_CBT) / sizeof(kTextAlphabet_PT_CBT[0]),
  .dictionary = kTextDictionary_PT_CBT,
  .dictionary_size = sizeof(kTextDictionary_PT_CBT) / sizeof(kTextDictionary_PT_CBT[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangIT = {
  .alphabet = kTextAlphabet_IT,
  .alphabet_size = sizeof(kTextAlphabet_IT) / sizeof(kTextAlphabet_IT[0]),
  .dictionary = kTextDictionary_US,
  .dictionary_size = sizeof(kTextDictionary_US) / sizeof(kTextDictionary_US[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangGL = {
  .alphabet = kTextAlphabet_GL,
  .alphabet_size = sizeof(kTextAlphabet_GL) / sizeof(kTextAlphabet_GL[0]),
  .dictionary = kTextDictionary_GL,
  .dictionary_size = sizeof(kTextDictionary_GL) / sizeof(kTextDictionary_GL[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangCA = {
  .alphabet = kTextAlphabet_CA,
  .alphabet_size = sizeof(kTextAlphabet_CA) / sizeof(kTextAlphabet_CA[0]),
  .dictionary = kTextDictionary_CA,
  .dictionary_size = sizeof(kTextDictionary_CA) / sizeof(kTextDictionary_CA[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangGA = {
  .alphabet = kTextAlphabet_GA,
  .alphabet_size = sizeof(kTextAlphabet_GA) / sizeof(kTextAlphabet_GA[0]),
  .dictionary = kTextDictionary_US,
  .dictionary_size = sizeof(kTextDictionary_US) / sizeof(kTextDictionary_US[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangES = {
  .alphabet = kTextAlphabet_ES,
  .alphabet_size = sizeof(kTextAlphabet_ES) / sizeof(kTextAlphabet_ES[0]),
  .dictionary = kTextDictionary_ES,
  .dictionary_size = sizeof(kTextDictionary_ES) / sizeof(kTextDictionary_ES[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

static const LanguageConfig kLangNL = {
  .alphabet = kTextAlphabet_NL,
  .alphabet_size = sizeof(kTextAlphabet_NL) / sizeof(kTextAlphabet_NL[0]),
  .dictionary = kTextDictionary_NL,
  .dictionary_size = sizeof(kTextDictionary_NL) / sizeof(kTextDictionary_NL[0]),
  .command_lengths = kText_CommandLengths_US,
  .command_names = kText_CommandNames_US,
  .command_count = sizeof(kText_CommandNames_US) / sizeof(kText_CommandNames_US[0]),
  .rom_addrs = {0x9c8000, 0x8edf40, 0},
  .rom_addr_count = 2,
  .COMMAND_START = 0x67,
  .SWITCH_BANK = 0x80,
  .FINISH = 0xff,
  .DICT_BASE_ENC = 0x88,
  .DICT_BASE_DEC = 0x88,
  .ESCAPE_CHARACTER = 0,
  .has_escape = false,
  .uses_new_format = false
};

// ============================================================================
// Public API
// ============================================================================

const LanguageConfig* TextDecode_GetLanguageConfig(const char *lang_code) {
  if (!lang_code) return NULL;

  if (strcmp(lang_code, "us") == 0) return &kLangUS;
  if (strcmp(lang_code, "en") == 0) return &kLangEN;
  if (strcmp(lang_code, "de") == 0) return &kLangDE;
  if (strcmp(lang_code, "fr") == 0) return &kLangFR;
  if (strcmp(lang_code, "fr-c") == 0) return &kLangFR_C;
  if (strcmp(lang_code, "sv") == 0) return &kLangSV;
  if (strcmp(lang_code, "pl") == 0) return &kLangPL;
  if (strcmp(lang_code, "pt") == 0) return &kLangPT;
  if (strcmp(lang_code, "pt-cbt") == 0) return &kLangPT_CBT;
  if (strcmp(lang_code, "it") == 0) return &kLangIT;
  if (strcmp(lang_code, "gl") == 0) return &kLangGL;
  if (strcmp(lang_code, "ca") == 0) return &kLangCA;
  if (strcmp(lang_code, "ga") == 0) return &kLangGA;
  if (strcmp(lang_code, "es") == 0) return &kLangES;
  if (strcmp(lang_code, "nl") == 0) return &kLangNL;
  if (strcmp(lang_code, "en-redux") == 0) return &kLangUS;  // Redux uses US config
  if (strcmp(lang_code, "en-kal") == 0) return &kLangUS;  // Kaleidoscope uses US config
  if (strcmp(lang_code, "en-gn") == 0) return &kLangUS;  // Gender neutral uses US config

  return NULL;
}

const char* TextDecode_GetLanguageCode(RomLanguage lang) {
  switch (lang) {
    case ROM_LANG_US: return "us";
    case ROM_LANG_EN: return "en";
    case ROM_LANG_DE: return "de";
    case ROM_LANG_FR: return "fr";
    case ROM_LANG_FR_C: return "fr-c";
    case ROM_LANG_ES: return "es";
    case ROM_LANG_PL: return "pl";
    case ROM_LANG_PT: return "pt";
    case ROM_LANG_REDUX: return "en-redux";
    case ROM_LANG_NL: return "nl";
    case ROM_LANG_SV: return "sv";
    case ROM_LANG_RETRANS_KAL: return "en-kal";
    case ROM_LANG_EN_GN: return "en-gn";
    case ROM_LANG_PT_CBT: return "pt-cbt";
    case ROM_LANG_IT: return "it";
    case ROM_LANG_GL: return "gl";
    case ROM_LANG_CA: return "ca";
    case ROM_LANG_GA: return "ga";
    default: return NULL;
  }
}

// Helper: append string to dynamic buffer
static void AppendString(char **buf, size_t *len, size_t *cap, const char *str) {
  size_t str_len = strlen(str);
  if (*len + str_len + 1 > *cap) {
    *cap = (*cap + str_len + 1) * 2;
    *buf = realloc(*buf, *cap);
  }
  memcpy(*buf + *len, str, str_len);
  *len += str_len;
  (*buf)[*len] = '\0';
}

// Helper: append formatted string
static void AppendFormatted(char **buf, size_t *len, size_t *cap, const char *fmt, ...) {
  char temp[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(temp, sizeof(temp), fmt, args);
  va_end(args);
  AppendString(buf, len, cap, temp);
}

static bool IsIrishMessage(size_t id) {
  for (size_t i = 0; i < sizeof(kIrishMessages) / sizeof(kIrishMessages[0]); i++) {
    if (kIrishMessages[i] == id) return true;
  }
  return false;
}

static void RestoreIrishMessages(DecodedStringsArray *result) {
  for (size_t i = 0; i < sizeof(kIrishRestored) / sizeof(kIrishRestored[0]); i++) {
    if (kIrishRestored[i].id < result->count) {
      DecodedString *s = &result->strings[kIrishRestored[i].id];
      free(s->text);
      s->text = strdup(kIrishRestored[i].text);
    }
  }
}

DecodedStringsArray* TextDecode_DecodeStrings(Rom *rom, const char *lang_code) {
  const LanguageConfig *config = TextDecode_GetLanguageConfig(lang_code);
  if (!config) {
    LogError("Unsupported language: %s", lang_code);
    return NULL;
  }

  DecodedStringsArray *result = calloc(1, sizeof(DecodedStringsArray));
  if (!result) return NULL;

  result->capacity = 512;
  result->strings = calloc(result->capacity, sizeof(DecodedString));
  if (!result->strings) {
    free(result);
    return NULL;
  }

  uint32_t p = config->rom_addrs[0];
  size_t rom_idx = 1;

  while (1) {
    // Irish: translated messages use the ROM's alphabet, the others the US one
    const char **alphabet = config->alphabet;
    size_t alphabet_size = config->alphabet_size;
    if (config == &kLangGA && IsIrishMessage(result->count)) {
      alphabet = kTextAlphabet_GA_ROM;
      alphabet_size = sizeof(kTextAlphabet_GA_ROM) / sizeof(kTextAlphabet_GA_ROM[0]);
    }

    // Allocate text buffer
    size_t text_cap = 1024;
    size_t text_len = 0;
    char *text = malloc(text_cap);
    text[0] = '\0';

    // Allocate raw bytes buffer
    size_t raw_cap = 512;
    size_t raw_len = 0;
    uint8_t *raw = malloc(raw_cap);

    while (1) {
      uint8_t c = Rom_ReadByte(rom, p);

      // Store raw byte
      if (raw_len >= raw_cap) {
        raw_cap *= 2;
        raw = realloc(raw, raw_cap);
      }
      raw[raw_len++] = c;

      // Calculate command length
      uint8_t cmd_len = 1;
      if (c >= config->COMMAND_START && c < config->SWITCH_BANK) {
        size_t cmd_idx = c - config->COMMAND_START;
        if (cmd_idx < config->command_count) {
          cmd_len = config->command_lengths[cmd_idx];
        }
      }

      p += cmd_len;

      // End of message
      if (c == 0x7f) {
        break;
      }

      // Process character/command
      if (c < config->COMMAND_START) {
        // Handle escape character
        if (config->has_escape && c == config->ESCAPE_CHARACTER) {
          c = Rom_ReadByte(rom, p);
          p++;
          if (raw_len >= raw_cap) {
            raw_cap *= 2;
            raw = realloc(raw, raw_cap);
          }
          raw[raw_len++] = c;
        }

        // Alphabet character
        if (c < alphabet_size) {
          AppendString(&text, &text_len, &text_cap, alphabet[c]);
        }
      } else if (c < config->SWITCH_BANK) {
        // Command
        size_t cmd_idx = c - config->COMMAND_START;
        if (cmd_idx < config->command_count) {
          if (cmd_len == 2) {
            uint8_t param = Rom_ReadByte(rom, p - 1);
            if (raw_len >= raw_cap) {
              raw_cap *= 2;
              raw = realloc(raw, raw_cap);
            }
            raw[raw_len++] = param;
            AppendFormatted(&text, &text_len, &text_cap, "[%s %.2d]",
                           config->command_names[cmd_idx], param);
          } else {
            AppendFormatted(&text, &text_len, &text_cap, "[%s]",
                           config->command_names[cmd_idx]);
          }
        }
      } else if (c == config->FINISH) {
        // Done with all strings
        free(text);
        free(raw);
        if (config == &kLangGA)
          RestoreIrishMessages(result);
        return result;
      } else if (c == config->SWITCH_BANK) {
        // Switch to next ROM bank
        if (rom_idx < config->rom_addr_count) {
          p = config->rom_addrs[rom_idx];
          rom_idx++;
        }
        // Reset current string
        text_len = 0;
        text[0] = '\0';
        raw_len = 0;
        continue;
      } else if (c >= config->DICT_BASE_DEC) {
        // Dictionary entry
        size_t dict_idx = c - config->DICT_BASE_DEC;
        if (dict_idx < config->dictionary_size) {
          AppendString(&text, &text_len, &text_cap, config->dictionary[dict_idx]);
        }
      }
    }

    // Store decoded string
    if (result->count >= result->capacity) {
      result->capacity *= 2;
      result->strings = realloc(result->strings, result->capacity * sizeof(DecodedString));
    }
    result->strings[result->count].text = text;
    result->strings[result->count].raw_bytes = raw;
    result->strings[result->count].raw_len = raw_len;
    result->count++;

    // Workaround: Portuguese ROM doesn't have proper FINISH byte, limit to 397 strings
    // (matches Python: if len(result) >= 397 and lang == 'pt': return result)
    if (result->count >= 397 && strcmp(lang_code, "pt") == 0) {
      return result;
    }
  }

  return result;
}

void TextDecode_FreeStrings(DecodedStringsArray *strings) {
  if (!strings) return;

  for (size_t i = 0; i < strings->count; i++) {
    free(strings->strings[i].text);
    free(strings->strings[i].raw_bytes);
  }
  free(strings->strings);
  free(strings);
}

char* TextDecode_GetDialogueFilename(const char *lang_code) {
  char *filename = malloc(64);
  if (strcmp(lang_code, "us") == 0) {
    strcpy(filename, "dialogue.txt");
  } else {
    // Replace '-' with '_' in language code
    char safe_lang[16];
    strncpy(safe_lang, lang_code, sizeof(safe_lang) - 1);
    safe_lang[sizeof(safe_lang) - 1] = '\0';
    for (char *p = safe_lang; *p; p++) {
      if (*p == '-') *p = '_';
    }
    snprintf(filename, 64, "dialogue_%s.txt", safe_lang);
  }
  return filename;
}

bool TextDecode_WriteDialogueFile(const DecodedStringsArray *strings, const char *lang_code, const char *output_dir) {
  char *filename = TextDecode_GetDialogueFilename(lang_code);

  char filepath[512];
  if (output_dir && strlen(output_dir) > 0) {
    snprintf(filepath, sizeof(filepath), "%s/%s", output_dir, filename);
  } else {
    snprintf(filepath, sizeof(filepath), "assets/%s", filename);
  }
  free(filename);

  FILE *f = fopen(filepath, "w");
  if (!f) {
    LogError("Failed to open %s for writing", filepath);
    return false;
  }

  // Python adds an extra string at index 4 for PAL ROMs that have 396 strings
  // We need to match this behavior
  bool needs_extra_string = (strings->count == 396);
  const char *extra_str = "[Speed 00]0- [Number 00]. 1- [Number 01][2]2- [Number 02]. 3- [Number 03]";

  size_t output_idx = 1;
  for (size_t i = 0; i < strings->count; i++) {
    // Insert extra string at position 5 (after first 4 strings)
    if (needs_extra_string && i == 4) {
      fprintf(f, "%zu: %s\n", output_idx++, extra_str);
    }
    fprintf(f, "%zu: %s\n", output_idx++, strings->strings[i].text);
  }

  fclose(f);
  printf("Wrote dialogue to %s (%zu strings)\n", filepath, strings->count + (needs_extra_string ? 1 : 0));
  return true;
}
