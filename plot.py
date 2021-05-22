import matplotlib.pyplot as plt
import numpy as np
import matplotlib.ticker as ticker

arr = [(1,50,54,58),(10,1731,3240,1328),(20,1793,5685,1312),(30,1355,7546,1040),(40,1840,9390,1081),(50,1799,11850,1154),(60,1788,12648,1073),(70,1796,15283,964),(80,1844,17596,990),(90,1834,17434,1049),(100,1829,24327,992),(110,1803,21004,1072),(120,1821,22977,861),(130,1845,27103,920),(140,1819,26420,803),(150,1752,27041,1028),(160,1849,20235,830),(170,1844,27156,699),(180,1809,31543,953),(190,1767,40503,829),(200,1829,33453,875),]

x = [i[0] for i in arr]

y = [i[1] for i in arr]

k = [i[2] for i in arr]

z = [i[3] for i in arr]

plt.plot(x, k, label="TTAS")
plt.plot(x, y, label="mutex")
plt.plot(x, z, label="best spin")
plt.xlabel('thread_count')
plt.ylabel('ms')
plt.legend()
plt.show()

#run: python3 plot.py
