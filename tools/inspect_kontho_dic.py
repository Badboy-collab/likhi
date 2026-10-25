import pickle
import sys

sys.stdout.reconfigure(encoding='utf-8')

with open(r'C:\Users\Pervez\OneDrive\Documents\Kontho\Dictionaries\Langs\Bangla\Dics\Bangla.pkl', 'rb') as f:
    data = pickle.load(f)

val_dic = data.get('valueDic', {})
print('valueDic count:', len(val_dic))
filter_dic = data.get('filterDic', {})
print('filterDic count:', len(filter_dic))

for test_w in ['topbar', 'tumi', 'kemon', 'acho', 'likhi', 'bangla', 'bhalo', 'korcho']:
    print(test_w, '->', val_dic.get(test_w))
    print('  filter:', filter_dic.get(test_w))
