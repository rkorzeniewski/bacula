import bacula

def EndJob(j):
    jobid = bacula.get(j, "JobId")
    client = bacula.get(j, "Client") 
    bacula.set(jcr=j, JobReport="Python EndJob output: JobId=%d Client=%s.\n" % (jobid, client))
    if (jobid < 5) :
       startid = bacula.run(j, "run kernsave")
       print "Python started new Job: jobid=", startid

    return 1
