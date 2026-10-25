import os
import sys

sys.stdout.reconfigure(encoding='utf-8')

appdata = os.path.expandvars(r"%APPDATA%\PC-Bangla-Typing-App")

# Curated correct spellings for common words
CORRECTIONS = {
    'topbar': 'টপবার',
    'bhalobashi': 'ভালোবাসি',
    'porishromi': 'পরিশ্রমী',
    'porishrom': 'পরিশ্রম',
    'jamon': 'যেমন',
    'jomuna': 'যমুনা',
    'pora': 'পড়া',
    'dorkar': 'দরকার',
    'jai': 'যাই',
    'dao': 'দাও',
    'likhi': 'লিখি',
    'unnoti': 'উন্নতি',
    'koro': 'করো',
    'kora': 'করা',
    'kori': 'করি',
    'korchi': 'করছি',
    'korche': 'করছে',
    'korcho': 'করছো',
    'korben': 'করবেন',
    'korbo': 'করবো',
    'korte': 'করতে',
    'kore': 'করে',
    'korechi': 'করেছি',
    'koreche': 'করেছে',
    'korecho': 'করেছো',
    'korechen': 'করেছেন',
    'porer': 'পরের',
    'app': 'অ্যাপ',
    'apps': 'অ্যাপস',
    'appsa': 'অ্যাপস',
    'chot': 'ছোট',
    'dosa': 'দশা',
    'brista': 'বৃষ্টি',
    'egentke': 'এজেন্টকে',
    'laibery': 'লাইব্রেরি',
    'ofline': 'অফলাইনে',
    'jenaret': 'জেনারেট',
    'dhoroner': 'ধরনের',
    'silect': 'সিলেক্ট',
    'promot': 'প্রমোট',
    'chandra': 'চন্দ্র',
    'othe': 'ওঠে',
    'keno': 'কেন',
    'shona': 'শোনা',
    'jotokhon': 'যতক্ষণ',
    'totokhon': 'ততক্ষণ',
    'jotodin': 'যতদিন',
    'totodin': 'ততদিন',
    'shobar': 'সবার',
    'sundor': 'সুন্দর',
    'shundor': 'সুন্দর',
    'bhalo': 'ভালো',
    'valo': 'ভালো',
    'shasthyo': 'স্বাস্থ্য',
    'shringkhola': 'শৃঙ্খলা',
    'antorjatik': 'আন্তর্জাতিক',
    'brohmoputro': 'ব্রহ্মপুত্র',
    'utkrishto': 'উৎকৃষ্ট',
    'smriti': 'স্মৃতি',
    'akangkha': 'আকাঙ্ক্ষা',
    'dondwo': 'দ্বন্দ্ব',
    'tottwo': 'তত্ত্ব',
    'ujjwol': 'উজ্জ্বল',
    'pobitro': 'পবিত্র',
    'sohojboddo': 'সহজবোধ্য',
    'kivabe': 'কীভাবে',
    'chalak': 'চালাক',
    'chotur': 'চতুর',
    'shanti': 'শান্তি',
    'mon': 'মন',
    'naam': 'নাম',
    'amar': 'আমার',
    'tumi': 'তুমি',
    'tomake': 'তোমাকে',
    'amader': 'আমাদের',
    'amra': 'আমরা',
    'desh': 'দেশ',
    'bangla': 'বাংলা',
    'shikhok': 'শিক্ষক',
    'shikkhok': 'শিক্ষক',
    'manush': 'মানুষ',
    'manusher': 'মানুষের',
    'kaj': 'কাজ',
    'kotha': 'কথা',
    'priyo': 'প্রিয়',
    'nodi': 'নদী',
    'khobor': 'খবর',
    'somoy': 'সময়',
    'dekha': 'দেখা',
    'dekhano': 'দেখানো',
    'bujhte': 'বুঝতে',
    'bujhi': 'বুঝি',
    'shob': 'সব',
    'kichu': 'কিছু',
    'ekhon': 'এখন',
    'ajke': 'আজকে',
    'shokal': 'সকাল',
    'shonibar': 'শনিবার',
    'brishti': 'বৃষ্টি',
    'pani': 'পানি',
    'khabar': 'খাবার',
    'basha': 'বাসা',
    'bari': 'বাড়ি',
    'chithi': 'চিঠি',
    'notun': 'নতুন',
    'purano': 'পুরনো',
    'prothom': 'প্রথম',
    'shesh': 'শেষ',
    'shuru': 'শুরু',
}

# Entries to completely discard (junk/corrupt)
DROP_KEYS = {
    'by', 'cs', 'l', 'mo', 'c', 'si', 'es', 'ta', 'o', 'na', 'noy',
    'karjjokor', 'voice', 'stil', 'ami ajke brishti', 'gay',
}

def clean_dict_file(path):
    if not os.path.exists(path):
        return
    
    clean_lines = []
    seen = set()
    
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line_str = line.strip()
            if not line_str or line_str.startswith('#'):
                continue
            parts = line_str.split('\t')
            if len(parts) < 2:
                continue
            roman = parts[0].strip().lower()
            bn = parts[1].strip()
            
            # Skip junk
            if roman in DROP_KEYS or len(roman) <= 1 or ' ' in roman:
                continue
            
            # Apply corrections
            if roman in CORRECTIONS:
                bn = CORRECTIONS[roman]
            
            # Avoid duplicate roman keys
            if roman in seen:
                continue
            seen.add(roman)
            
            # Reconstruct tab-separated line
            parts[0] = roman
            parts[1] = bn
            # If parts has count, keep high priority
            if len(parts) >= 3:
                try:
                    count = int(parts[2])
                    if count < 10:
                        parts[2] = '25'
                except:
                    pass
            clean_lines.append('\t'.join(parts))
            
    # Add any missing essential words
    for k, v in CORRECTIONS.items():
        if k not in seen:
            clean_lines.append(f"{k}\t{v}\t50\t1790061204\t1")
            seen.add(k)
            
    # Sort alphabetically by roman key
    clean_lines.sort()
    
    # Save back
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        for l in clean_lines:
            f.write(l + '\n')
            
    print(f"Cleaned {path}: {len(clean_lines)} pristine words.")

clean_dict_file(os.path.join(appdata, "personal_dict.txt"))
clean_dict_file(os.path.join(appdata, "user_dict.txt"))
