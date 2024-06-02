import matplotlib.pyplot as plt
import numpy as np
from copy import copy

with open("constdist.txt") as f:
    data = f.readlines()
    data = [x.split() for x in data]
    data = [[int(x[0]), int(x[1])] for x in data]

data.sort(key = lambda x: x[1])
top16 = data[-16:]
data = [x[1] for x in data]
old_data = copy(data)
for i in range(1, len(data)):
    data[i] += data[i - 1]
sum = data[-1]
ratio = 0.999
thres = sum * (1-ratio)
pos = 0
for i in range(len(data)):
    if data[i] >= thres:
        pos = i
        break
print(pos, old_data[pos])
X = np.linspace(0, 1, len(data))
plt.plot(X, data)
plt.savefig("constdist.png")

for k, v in top16:
    print(k, v)
