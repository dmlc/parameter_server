import os
import sys
import json
import time
def getMinionNum():
        out=os.popen('../bin/kubecfg list minions').read()
        return len(out.splitlines()[2:-1])
def killAll():
        '''
        Kill all pods and controller managers on kubernetes
        '''
        #kill replicationControllers
        out=os.popen('../bin/kubecfg list replicationControllers').read()
        for line in out.splitlines()[2:]:
              line=line.strip().split()
              if len(line)>0:
                      os.system('../bin/kubecfg stop '+line[0])
                      os.system('../bin/kubecfg rm '+line[0])

        #kill scheduler
        os.system("../bin/kubecfg delete pods/scheduler")


def upScheduler(num_servers,num_workers,algorithm,data_dir,output_dir):
	'''
	Bring up scheduler as a pod -- scheduler
	'''
	scheduler=dict()
	scheduler['id']='scheduler'
	scheduler['kind']='Pod'
	scheduler['apiVersion']='v1beta1'

	###set container###
	container=dict()
	container['name']='scheduler'
	#image
	container['image']='qicongc/pserver'
	port=dict()
	#port mapping
	port['containerPort']=8000
	port['hostPort']=11000
	container['ports']=[port]
	#environment variables
	env=dict()
	env['name']='LD_LIBRARY_PATH'
	env['value']='/home/parameter_server/third_party/lib/'
	container['env']=[env]
	#working directory when container starts running
	container['workingDir']='/home/parameter_server/script'
	#command
	cmd='../build/ps -num_servers '+str(num_servers)+' -num_workers '+str(num_workers)+' -num_threads '+str(num_servers+num_workers)+' -app_file ../config/'+algorithm+'_l1lr.conf -print_van  -bind_to 8000 '
	cmd+="-scheduler \"role:SCHEDULER,hostname:\'`cat /tmp/docker/host/host`\',port:11000,id:\'H\'\" "
	cmd+="-my_node \"role:SCHEDULER,hostname:\'`cat /tmp/docker/host/host`\',port:11000,id:\'H\'\""
	command=['sh','-c']
	command.append(cmd)
	container['command']=command
	#volume mapping -- container side
	#path for container to identify current minion
	volumeMount=dict()
	volumeMount['name']='host'
	volumeMount['mountPath']='/tmp/docker/host'
	container['volumeMounts']=[volumeMount]
	#path to van print
	volumeMount=dict()
	volumeMount['name']='log'
	volumeMount['mountPath']='/home/parameter_server/script'
	container['volumeMounts'].append(volumeMount)
	#path to config file
	volumeMount=dict()
	volumeMount['name']='config'
	volumeMount['mountPath']='/home/parameter_server/config'
	container['volumeMounts'].append(volumeMount)
	#path to data
	volumeMount=dict()
	volumeMount['name']='data'
	volumeMount['mountPath']='/home/parameter_server/data'
	container['volumeMounts'].append(volumeMount)
	#path to output
	volumeMount=dict()
	volumeMount['name']='output'
	volumeMount['mountPath']='/home/parameter_server/output'
	container['volumeMounts'].append(volumeMount)
	###set container done###

	#volume mapping -- minion side
	#path for container to identify current minion
	volume=dict()
	volume['name']='host'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/host'
	volumes=[volume]
	#path to van print
	volume=dict()
	volume['name']='log'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/log/scheduler'
	volumes.append(volume)
	#path to config file
	volume=dict()
	volume['name']='config'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=os.path.split(os.path.abspath("../config/config.conf"))[0]
	volumes.append(volume)
	#path to data
	volume=dict()
	volume['name']='data'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=data_dir
	volumes.append(volume)
	#path to output
	volume=dict()
	volume['name']='output'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=output_dir
	volumes.append(volume)

	manifest=dict()
	manifest['version']='v1beta1'
	manifest['id']='scheduler'
	manifest['containers']=[container]
	manifest['volumes']=volumes

	#set restart policy as never
	manifest['restartPolicy']=dict()
	manifest['restartPolicy']['never']=dict()

	scheduler['desiredState']=dict()
	scheduler['desiredState']['manifest']=manifest

	scheduler['labels']=dict()
	scheduler['labels']['name']='scheduler'

	#convert scheduler to json
	fpath='../json/pods/'+scheduler['id']+'.json'
	with open(fpath, 'w') as outfile:
	  json.dump(scheduler, outfile)
	#submit json to kubernetes
	cmd='../bin/kubecfg -c '+fpath+' create pods'
	print cmd
	os.system(cmd)

	#wait until scheduler container is running, return the minion 
	while True:
		print "waiting for scheduler to up... "
		time.sleep(3)
		out=os.popen('../bin/kubecfg list pods').read()
		for line in out.splitlines():
			line=line.strip().split()
			if len(line)>0 and line[0]=='scheduler':
				print line
				if len(line)>4  and line[4]=='Running':
					return line[2].strip('/')

def upWorker(index,num_replicas,scheduler_host,num_servers,num_workers,algorithm,data_dir,output_dir):
        '''
        Bring up worker as a pod -- worker
        '''
        worker=dict()
        worker['id']='worker'+str(index)
        worker['kind']='ReplicationController'
        worker['apiVersion']='v1beta1'

        ###set container###
        container=dict()
        container['name']='worker'+str(index)
        #image
        container['image']='qicongc/pserver'
        port=dict()
        #port mapping
        port['containerPort']=8001
        hostPort=11401+index
        port['hostPort']=hostPort
        container['ports']=[port]
        #environment variables
        env=dict()
        env['name']='LD_LIBRARY_PATH'
        env['value']='/home/parameter_server/third_party/lib/'
        container['env']=[env]
        #working directory when container starts running
        container['workingDir']='/home/parameter_server/script'
        #command
        cmd='../build/ps -num_servers '+str(num_servers)+' -num_workers '+str(num_workers)+' -num_threads '+str(num_servers+num_workers)+' -app_file ../config/'+algorithm+'_l1lr.conf -print_van -bind_to 8001 '
        cmd+="-scheduler \"role:SCHEDULER,hostname:\'"+scheduler_host+"\',port:11000,id:\'H\'\" "
        cmd+="-my_node \"role:WORKER,hostname:\'`cat /tmp/docker/host/host`\',port:"+str(hostPort)+",id:\'W_$(hostname)\'\""
        command=['sh','-c']
	command.append(cmd)
        container['command']=command
        #volume mapping -- container side
        #path for container to identify current minion
        volumeMount=dict()
        volumeMount['name']='host'
        volumeMount['mountPath']='/tmp/docker/host'
        container['volumeMounts']=[volumeMount]
        #path to van print
        volumeMount=dict()
        volumeMount['name']='log'
        volumeMount['mountPath']='/home/parameter_server/script'
        container['volumeMounts'].append(volumeMount)
        #path to config file
        volumeMount=dict()
        volumeMount['name']='config'
        volumeMount['mountPath']='/home/parameter_server/config'
        container['volumeMounts'].append(volumeMount)
        #path to data
        volumeMount=dict()
        volumeMount['name']='data'
        volumeMount['mountPath']='/home/parameter_server/data'
        container['volumeMounts'].append(volumeMount)
        #path to output
        volumeMount=dict()
        volumeMount['name']='output'
        volumeMount['mountPath']='/home/parameter_server/output'
        container['volumeMounts'].append(volumeMount)
        ###set container done###

        #volume mapping -- minion side
        #path for container to identify current minion
        volume=dict()
        volume['name']='host'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']='/tmp/docker/host'
        volumes=[volume]
        #path to van print
        volume=dict()
        volume['name']='log'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']='/tmp/docker/log/worker'
        volumes.append(volume)
        #path to config file
        volume=dict()
        volume['name']='config'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']=os.path.split(os.path.abspath("../config/config.conf"))[0]
        volumes.append(volume)
        #path to data
        volume=dict()
	volume['name']='data'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']=data_dir
        volumes.append(volume)
        #path to output
        volume=dict()
        volume['name']='output'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']=output_dir
        volumes.append(volume)


        manifest=dict()
	manifest['version']='v1beta1'
	manifest['id']='worker'+str(index)
	manifest['containers']=[container]
	manifest['volumes']=volumes

	desiredState=dict()
	desiredState['manifest']=manifest

	podTemplate=dict()
	podTemplate['desiredState']=desiredState
	podTemplate['labels']=dict()
	podTemplate['labels']['name']='worker'+str(index)

	desiredState=dict()
	desiredState['podTemplate']=podTemplate
	desiredState['replicas']=num_replicas
	desiredState['replicaSelector']=dict()
	desiredState['replicaSelector']['name']='worker'+str(index)

	worker['desiredState']=desiredState
	worker['labels']=dict()
	worker['labels']['name']='worker'+str(index)

	#convert scheduler to json
	fpath='../json/replicationControllers/'+worker['id']+'.json'
	with open(fpath, 'w') as outfile:
	  json.dump(worker, outfile)
	#submit json to kubernetes
	cmd='../bin/kubecfg -c '+fpath+' create replicationControllers'
	print cmd
	os.system(cmd)

def upServer(index,num_replicas,scheduler_host,num_servers,num_workers,algorithm,data_dir,output_dir):
        '''
        Bring up worker as a pod -- worker
        '''
        server=dict()
        server['id']='server'+str(index)
        server['kind']='ReplicationController'
        server['apiVersion']='v1beta1'

        ###set container###
        container=dict()
        container['name']='server'+str(index)
	#image
        container['image']='qicongc/pserver'
        port=dict()
        #port mapping
        port['containerPort']=8002
        hostPort=11101+index
        port['hostPort']=hostPort
        container['ports']=[port]
        #environment variables
        env=dict()
        env['name']='LD_LIBRARY_PATH'
        env['value']='/home/parameter_server/third_party/lib/'
        container['env']=[env]
        #working directory when container starts running
        container['workingDir']='/home/parameter_server/script'
        #command
        cmd='../build/ps -num_servers '+str(num_servers)+' -num_workers '+str(num_workers)+' -num_threads '+str(num_servers+num_workers)+' -app_file ../config/'+algorithm+'_l1lr.conf -print_van  -bind_to 8002 '
        cmd+="-scheduler \"role:SCHEDULER,hostname:\'"+scheduler_host+"\',port:11000,id:\'H\'\" "
        cmd+="-my_node \"role:SERVER,hostname:\'`cat /tmp/docker/host/host`\',port:"+str(hostPort)+",id:\'S_$(hostname)\'\""
        command=['sh','-c']
        command.append(cmd)
        container['command']=command
        #volume mapping -- container side
        #path for container to identify current minion
        volumeMount=dict()
        volumeMount['name']='host'
        volumeMount['mountPath']='/tmp/docker/host'
        container['volumeMounts']=[volumeMount]
        #path to van print
        volumeMount=dict()
        volumeMount['name']='log'
        volumeMount['mountPath']='/home/parameter_server/script'
        container['volumeMounts'].append(volumeMount)
        #path to config file
        volumeMount=dict()
        volumeMount['name']='config'
        volumeMount['mountPath']='/home/parameter_server/config'
        container['volumeMounts'].append(volumeMount)
        #path to data
        volumeMount=dict()
        volumeMount['name']='data'
        volumeMount['mountPath']='/home/parameter_server/data'
        container['volumeMounts'].append(volumeMount)
        #path to output
        volumeMount=dict()
        volumeMount['name']='output'
        volumeMount['mountPath']='/home/parameter_server/output'
        container['volumeMounts'].append(volumeMount)
        ###set container done###

        #volume mapping -- minion side
        #path for container to identify current minion
        volume=dict()
        volume['name']='host'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/host'
        volumes=[volume]
        #path to van print
        volume=dict()
        volume['name']='log'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']='/tmp/docker/log/server'
        volumes.append(volume)
        #path to config file
        volume=dict()
        volume['name']='config'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']=os.path.split(os.path.abspath("../config/config.conf"))[0]
        volumes.append(volume)
        #path to data
        volume=dict()
        volume['name']='data'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']=data_dir
        volumes.append(volume)
        #path to output
        volume=dict()
        volume['name']='output'
        volume['source']=dict()
        volume['source']['hostDir']=dict()
        volume['source']['hostDir']['path']=output_dir
        volumes.append(volume)

        manifest=dict()
	manifest['version']='v1beta1'
	manifest['id']='server'+str(index)
	manifest['containers']=[container]
	manifest['volumes']=volumes

	desiredState=dict()
	desiredState['manifest']=manifest

	podTemplate=dict()
	podTemplate['desiredState']=desiredState
	podTemplate['labels']=dict()
	podTemplate['labels']['name']='server'+str(index)

	desiredState=dict()
	desiredState['podTemplate']=podTemplate
	desiredState['replicas']=num_replicas
	desiredState['replicaSelector']=dict()
	desiredState['replicaSelector']['name']='server'+str(index)

	server['desiredState']=desiredState
	server['labels']=dict()
	server['labels']['name']='server'+str(index)

	#convert scheduler to json
	fpath='../json/replicationControllers/'+server['id']+'.json'
	with open(fpath, 'w') as outfile:
	  json.dump(server, outfile)
	#submit json to kubernetes
	cmd='../bin/kubecfg -c '+fpath+' create replicationControllers'
	print cmd
	os.system(cmd)


if __name__=='__main__':
        program=sys.argv[1]
        if program=="clear":
                killAll()
        elif program=="submit":
		if len(sys.argv)<6:
			print 'usage : python submit.py submit num_servers num_workers algorithm data_dir output_dir'
			exit(1)
		if sys.argv[4]!='batch' and sys.argv[4]!='online':
			print 'algorithm currently has online | batch'
			exit(1)
                num_minions=getMinionNum()
                num_servers=int(sys.argv[2])
                num_workers=int(sys.argv[3])
		algorithm=sys.argv[4]
		data_dir=os.path.abspath(sys.argv[5])
		output_dir=os.path.abspath(sys.argv[6])
		print data_dir
		print output_dir
                scheduler_host=upScheduler(num_servers,num_workers,algorithm,data_dir,output_dir)
		print 'detect scheduler at '+scheduler_host
		#launch servers
		remained=num_servers
		index=0
		while remained>num_minions:
			upServer(index,num_minions,scheduler_host,num_servers,num_workers,algorithm,data_dir,output_dir)
			remained-=num_minions
			index+=1
		upServer(index,remained,scheduler_host,num_servers,num_workers,algorithm,data_dir,output_dir)
		#launch workers
		remained=num_workers
		index=0
		while remained>num_minions:
			upWorker(index,num_minions,scheduler_host,num_servers,num_workers,algorithm,data_dir,output_dir)
			remained-=num_minions
			index+=1
		upWorker(index,remained,scheduler_host,num_servers,num_workers,algorithm,data_dir,output_dir)


                
