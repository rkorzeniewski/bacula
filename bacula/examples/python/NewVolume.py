import bacula

def NewVolume(jcr):
    jobid = jcr.get("JobId")
    print "JobId=", jobid
    client = jcr.get("Client") 
    print "Client=" + client
    numvol = jcr.get("NumVols");
    print "NumVols=", numvol
    jcr.set(JobReport="Python New Volume set for Job.\n") 
    jcr.set(VolumeName="TestA-001")
    return 1
