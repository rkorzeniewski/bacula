import bacula

def EndJob(j):
    jobid = bacula.get(j, "JobId")
    client = bacula.get(j, "Client") 
    bacula.set(jcr=j, JobReport="Python JndJob output: JobId=%d Client=%s.\n" % (jobid, client))
    if (jobid < 5) :
       startid = bacula.run(j, "run kernsave")
       print "Python started jobid=", startid

    return 1
