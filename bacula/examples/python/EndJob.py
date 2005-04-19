import bacula

def EndJob(jcr):
     jobid = jcr.get("JobId")
     client = jcr.get("Client") 
     jcr.set(JobReport="Python EndJob output: JobId=%d Client=%s.\n" % (jobid, client))
     if (jobid < 2) :
        startid = bacula.run("run kernsave")
        print "Python started new Job: jobid=", startid

     return 1
