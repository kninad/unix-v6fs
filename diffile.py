
# Test script to see the difference between two files

def read_binf(fpath):
    with open(fpath, 'rb') as rf:
        data = rf.read()
    return data


f1 = './h1.txt'
f2 = './h2.txt'

d1 = read_binf(f1)
d2 = read_binf(f2)

print(d1)

print(d2[:50])


