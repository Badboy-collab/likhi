import pickle
import os
import sys

sys.stdout.reconfigure(encoding='utf-8')

kontho_path = r"C:\Users\Pervez\OneDrive\Documents\Kontho\Dictionaries\Langs\Bangla\Dics\Bangla.pkl"
if not os.path.exists(kontho_path):
    print("Kontho dict not found")
    sys.exit(0)

with open(kontho_path, 'rb') as f:
    data = pickle.load(f)

val_dic = data.get('valueDic', {})
print(f"Kontho valueDic count: {len(val_dic)}")

# Read existing overrides
overrides_path = "engine/data/roman_overrides.txt"
existing = set()
if os.path.exists(overrides_path):
    with open(overrides_path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().split('\t')
            if parts and not line.startswith('#'):
                existing.add((parts[0].lower(), parts[1] if len(parts) > 1 else ''))

# Priority words to add
priority = [
    ("topbar", "টপবার"),
    ("top", "টপ"),
    ("jamon", "যেমন"),
    ("jemor", "যেমন"),
    ("koro", "করো"),
    ("kora", "করা"),
    ("kori", "করি"),
    ("korchi", "করছি"),
    ("korche", "করছে"),
    ("korcho", "করছো"),
    ("korben", "করবেন"),
    ("korbo", "করবো"),
    ("korte", "করতে"),
    ("kore", "করে"),
    ("korechi", "করেছি"),
    ("koreche", "করেছে"),
    ("korecho", "করেছো"),
    ("korechen", "করেছেন"),
    ("porishromi", "পরিশ্রমী"),
    ("porishrom", "পরিশ্রম"),
    ("bhalobashi", "ভালোবাসি"),
    ("bhalobasho", "ভালোবাসো"),
    ("bhalobasha", "ভালোবাসা"),
    ("dorkar", "দরকার"),
    ("jomuna", "যমুনা"),
    ("pora", "পড়া"),
    ("porashona", "পড়াশোনা"),
    ("unnoti", "উন্নতি"),
    ("likhi", "লিখি"),
    ("lekh", "লেখ"),
    ("lekha", "লেখা"),
    ("lekhi", "লেখি"),
    ("likhe", "লিখে"),
    ("likhechi", "লিখেছি"),
    ("likheche", "লিখেছে"),
    ("jai", "যাই"),
    ("jabo", "যাব"),
    ("dao", "দাও"),
    ("dekhano", "দেখানো"),
    ("dekho", "দেখো"),
    ("dekhun", "দেখুন"),
    ("app", "অ্যাপ"),
    ("apps", "অ্যাপস"),
]

added_count = 0
new_pairs = []

for r, b in priority:
    if (r, b) not in existing:
        new_pairs.append(f"{r}\t{b}\n")
        existing.add((r, b))
        added_count += 1

# Also check Kontho val_dic for words of length >= 2
for k, vlist in val_dic.items():
    k_lower = k.lower()
    if len(k_lower) < 2 or ' ' in k_lower or not k_lower.isalpha():
        continue
    if vlist and len(vlist) > 0:
        b = list(vlist)[0]
        # Only add valid Bengali words (Unicode range 0x0980 - 0x09FF)
        if any('\u0980' <= c <= '\u09FF' for c in b):
            if (k_lower, b) not in existing:
                new_pairs.append(f"{k_lower}\t{b}\n")
                existing.add((k_lower, b))
                added_count += 1

print(f"Adding {added_count} high-quality mappings to roman_overrides.txt")

with open(overrides_path, 'a', encoding='utf-8', newline='\n') as f:
    for p in new_pairs:
        f.write(p)

print("Enrichment complete.")
