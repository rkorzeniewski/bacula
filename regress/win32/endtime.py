from time import time as now 
 
t = now()
fn = open('time.out', 'r')
s = fn.readline()     
fn.close()
diff = t - float(s)
h = int(diff / 3600)
m = int((diff - h * 3600) / 60)
sec = diff - h * 3600 - m * 60
print 'Total time = %d:%02d:%02d or %d secs' % (h, m, sec, t - float(s))