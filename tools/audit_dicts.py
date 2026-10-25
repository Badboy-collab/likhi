import sys
sys.stdout.reconfigure(encoding='utf-8')

appdata = r"C:\Users\Pervez\AppData\Roaming\PC-Bangla-Typing-App"

def inspect_file(fname):
    print(f"=== {fname} ===")
    with open(f"{appdata}\\{fname}", 'r', encoding='utf-8') as f:
        for idx, line in enumerate(f, 1):
            parts = line.strip().split('\t')
            if len(parts) >= 2:
                roman, bn = parts[0], parts[1]
                print(f"{idx:3d}: {roman:15s} -> {bn}")

inspect_file("personal_dict.txt")
inspect_file("user_dict.txt")
