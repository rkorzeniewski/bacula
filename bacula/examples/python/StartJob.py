import bacula

def StartJob(j):
    jobid = bacula.get(j, "JobId")
    client = bacula.get(j, "Client")
    numvols = bacula.get(j, "NumVols") 
    bacula.set(jcr=j, JobReport="Python StartJob: JobId=%d Client=%s NumVols=%d\n" % (jobid,client,numvols))
    return 1
