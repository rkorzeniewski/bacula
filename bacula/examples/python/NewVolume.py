import bacula

def NewVolume(j):
    jobid = bacula.get(j, "JobId")
    print "JobId=", jobid
    client = bacula.get(j, "Client") 
    print "Client=" + client
    numvol = bacula.get(j, "NumVols");
    print "NumVols=", numvol
    bacula.set(jcr=j, JobReport="Python New Volume set for Job.\n") 
    bacula.set(jcr=j, VolumeName="TestA-001")
    return 1
