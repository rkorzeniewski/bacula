import bacula

def StartJob(jcr):
    jobid = jcr.get("JobId")
    client = jcr.get("Client")
    numvols = jcr.get("NumVols") 
    jcr.set(JobReport="Python StartJob: JobId=%d Client=%s NumVols=%d\n" % (jobid,client,numvols))
    return 1
