ts_array = [0.33 * i for i in range(90)]
layer1_bw = []
layer2_bw = []
layer3_bw = []
with open("svc.trace", "r") as fin:
    for line in fin:
        words = line.split(" ")
        if int(words[1]) == 0:
            layer1_bw.append( int(words[2]) / 0.033 * 8 / 1024 / 1024 )
        if int(words[1]) == 1:
            layer2_bw.append( int(words[2]) / 0.033 * 8 / 1024 / 1024 )
        if int(words[1]) == 2:
            layer3_bw.append( int(words[2]) / 0.033 * 8 / 1024 / 1024 )
for i, bw3 in enumerate(layer3_bw):
    layer3_bw[i] = bw3 + layer1_bw[i] + layer2_bw[2]
for i, bw2 in enumerate(layer2_bw):
    layer2_bw[i] = bw2 + layer1_bw[i]


