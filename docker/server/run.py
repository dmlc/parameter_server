import os
import sys



def readMaster(path):
	master=""
	fhandler=open(path,'r')
	for line in fhandler:
		master=line.strip()
	return master

def readMinions(path):
	minionList=list()
	fhandler=open(path,'r')
	for line in fhandler:
		minionList.append(line.strip())
	return minionList

if __name__ == "__main__":
    program=sys.argv[1]
    master_file=sys.argv[2]
    minions_file=sys.argv[3]
    master=readMaster(master_file)
    minionsList=readMinions(minions_file)
    num_minions=len(minionsList)
    if program=="up":
        #launch etcd
        cmd="mpirun -np 1 -hostfile "+master_file+" ./etcd "+master
        print cmd
        os.system(cmd)

        #launch kubelet and proxy
        cmd="mpirun -np "+str(num_minions)+" -hostfile "+minions_file+" ./kubelet "+master+" "+minions_file
        print cmd
        os.system(cmd)

        #launch apiserver, controller manager and scheduler
        cmd="mpirun -np 1 -hostfile "+master_file+" ./apiserver "+master+" "+minions_file
        print cmd
        os.system(cmd)

        #config kubecfg
        cmd="export KUBERNETES_MASTER=http://"+master+":8000"
        print cmd
        os.system(cmd)

    elif program=="down":
        cmd="cat "+master_file+" "+minions_file+" >host/all_file"
	print cmd
        os.system(cmd)
        cmd="mpirun -np "+str(num_minions+1)+" -hostfile host/all_file ./clean "
	print cmd
	os.system(cmd)
	






