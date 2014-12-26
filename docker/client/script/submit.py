import os
import sys
import json
import time

def extractDatPath(path):
	'''
	Get the absolute dir of data
	'''
	fread=open(path,'r')
	fwrite=open("../config/config.conf",'w')
	flag=0
	dirname=""
	filename=""
	for line in fread:
		items=line.strip().split()
		if len(items)>0:
			if items[0]=="training_data":
				flag=1
			elif items[0]=="file:" and flag==1:
				flag=2
				dirname,filename=os.path.split(os.path.abspath(items[1][1:-1]))
				fwrite.write("file: \"/home/parameter_server/dat/%s\"\n" % filename)
				continue
		fwrite.write("%s" % line)
	fread.close()
	return dirname

def killAll():
	'''
	Kill all pods and controller managers on kubernetes
	'''
	os.system('../bin/kubecfg stop worker')
	os.system('../bin/kubecfg stop server')
	os.system('../bin/kubecfg rm worker')
	os.system('../bin/kubecfg rm server')
	
	out=os.popen('../bin/kubecfg list pods').read()
	for line in out.splitlines()[2:]:
		line=line.strip().split()
		if len(line)>0:
			os.system('../bin/kubecfg delete pods/'+line[0])

def upScheduler(num_servers,num_workers,data_dir):
	'''
	Bring up scheduler as a pod -- scheduler
	'''
	scheduler=dict()
	scheduler['id']='scheduler'
	scheduler['kind']='Pod'
	scheduler['apiVersion']='v1beta1'

	container=dict()
	container['name']='scheduler'
	container['image']='qicongc/ps'
	port=dict()
	port['containerPort']=8000
	port['hostPort']=11000
	container['ports']=[port]
	env=dict()
	env['name']='LD_LIBRARY_PATH'
	env['value']='/home/parameter_server/third_party/lib/'
	container['env']=[env]
	container['workingDir']='/home/parameter_server/script'

	#parse the path
	cmd='../bin/ps -num_servers '+str(num_servers)+' -num_workers '+str(num_workers)+' -num_threads '+str(num_servers+num_workers)+' -app ../cfg/config.conf -print_van  -bind_to 8000 '
	cmd+="-scheduler \"role:SCHEDULER,hostname:\'`cat /tmp/docker/host/host`\',port:11000,id:\'H\'\" "
	cmd+="-my_node \"role:SCHEDULER,hostname:\'`cat /tmp/docker/host/host`\',port:11000,id:\'H\'\""
	command=['sh','-c']
	command.append(cmd)
	container['command']=command
	volumeMount=dict()
	volumeMount['name']='host'
	volumeMount['mountPath']='/tmp/docker/host'
	container['volumeMounts']=[volumeMount]
	volumeMount=dict()
	volumeMount['name']='log'
	volumeMount['mountPath']='/home/parameter_server/script'
	container['volumeMounts'].append(volumeMount)
	volumeMount=dict()
	volumeMount['name']='cfg'
	volumeMount['mountPath']='/home/parameter_server/cfg'
	container['volumeMounts'].append(volumeMount)
	volumeMount=dict()
	volumeMount['name']='dat'
	volumeMount['mountPath']='/home/parameter_server/dat'
	container['volumeMounts'].append(volumeMount)

	volume=dict()
	volume['name']='host'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/host'
	volumes=[volume]
	volume=dict()
	volume['name']='log'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/log/scheduler'
	volumes.append(volume)
	volume=dict()
	volume['name']='cfg'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=os.path.split(os.path.abspath("../config/config.conf"))[0]
	volumes.append(volume)
	volume=dict()
	volume['name']='dat'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=data_dir
	volumes.append(volume)


	manifest=dict()
	manifest['version']='v1beta1'
	manifest['id']='scheduler'
	manifest['containers']=[container]
	manifest['restartPolicy']=dict()
	manifest['restartPolicy']['never']=dict()
	manifest['volumes']=volumes

	scheduler['desiredState']=dict()
	scheduler['desiredState']['manifest']=manifest

	scheduler['labels']=dict()
	scheduler['labels']['name']='scheduler'

	fpath='../json/pods/'+scheduler['id']+'.json'
	with open(fpath, 'w') as outfile:
	  json.dump(scheduler, outfile)
	cmd='../bin/kubecfg -c '+fpath+' create pods'
	print cmd
	os.system(cmd)

	while True:
		print "waiting for scheduler to up... "
		time.sleep(10)
		out=os.popen('../bin/kubecfg list pods').read()
		for line in out.splitlines():
			line=line.strip().split()
			if len(line)>0 and line[0]=='scheduler':
				print line
				if len(line)>4  and line[4]=='Running':
					return line[2].strip('/')
	


def upWorker(scheduler_host,num_servers,num_workers,data_dir):
	'''
	Bring up workers as pods managed by a replication controller -- worker
	'''
	worker=dict()
	worker['id']='worker'
	worker['kind']='ReplicationController'
	worker['apiVersion']='v1beta1'

	container=dict()
	container['name']='worker'
	container['image']='qicongc/ps'
	port=dict()
	port['containerPort']=8001
	port['hostPort']=11001
	container['ports']=[port]
	env=dict()
	env['name']='LD_LIBRARY_PATH'
	env['value']='/home/parameter_server/third_party/lib/'
	container['env']=[env]
	container['workingDir']='/home/parameter_server/script'
	#parse the path
	cmd='../bin/ps -num_servers '+str(num_servers)+' -num_workers '+str(num_workers)+' -num_threads '+str(num_servers+num_workers)+' -app ../cfg/config.conf -print_van -bind_to 8001 '
	cmd+="-scheduler \"role:SCHEDULER,hostname:\'"+scheduler_host+"\',port:11000,id:\'H\'\" "
	cmd+="-my_node \"role:WORKER,hostname:\'`cat /tmp/docker/host/host`\',port:11001,id:\'W_$(hostname)\'\""
	command=['sh','-c']
	command.append(cmd)
	print cmd
	container['command']=command
	volumeMount=dict()
	volumeMount['name']='host'
	volumeMount['mountPath']='/tmp/docker/host'
	container['volumeMounts']=[volumeMount]
	volumeMount=dict()
	volumeMount['name']='log'
	volumeMount['mountPath']='/home/parameter_server/script'
	container['volumeMounts'].append(volumeMount)
	volumeMount=dict()
	volumeMount['name']='cfg'
	volumeMount['mountPath']='/home/parameter_server/cfg'
	container['volumeMounts'].append(volumeMount)
	volumeMount=dict()
	volumeMount['name']='dat'
	volumeMount['mountPath']='/home/parameter_server/dat'
	container['volumeMounts'].append(volumeMount)

	volume=dict()
	volume['name']='host'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/host'
	volumes=[volume]
	volume=dict()
	volume['name']='log'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/log/worker'
	volumes.append(volume)
	volume=dict()
	volume['name']='cfg'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=os.path.split(os.path.abspath("../config/config.conf"))[0]
	volumes.append(volume)
	volume=dict()
	volume['name']='dat'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=data_dir
	volumes.append(volume)

	manifest=dict()
	manifest['version']='v1beta1'
	manifest['id']='worker'
	manifest['containers']=[container]
	manifest['volumes']=volumes

	desiredState=dict()
	desiredState['manifest']=manifest

	podTemplate=dict()
	podTemplate['desiredState']=desiredState
	podTemplate['labels']=dict()
	podTemplate['labels']['name']='worker'

	desiredState=dict()
	desiredState['podTemplate']=podTemplate
	desiredState['replicas']=num_workers
	desiredState['replicaSelector']=dict()
	desiredState['replicaSelector']['name']='worker'

	worker['desiredState']=desiredState
	worker['labels']=dict()
	worker['labels']['name']='worker'


	fpath='../json/replicationControllers/'+worker['id']+'.json'
	with open(fpath, 'w') as outfile:
	  json.dump(worker, outfile)
	cmd='../bin/kubecfg -c '+fpath+' create replicationControllers'
	print cmd
	os.system(cmd)

def upServer(scheduler_host,num_servers,num_workers,data_dir):
	'''
	Bring up servers as pods managed by a replication controller -- server
	'''
	server=dict()
	server['id']='server'
	server['kind']='ReplicationController'
	server['apiVersion']='v1beta1'

	container=dict()
	container['name']='server'
	container['image']='qicongc/ps'
	port=dict()
	port['containerPort']=8002
	port['hostPort']=11002
	container['ports']=[port]
	env=dict()
	env['name']='LD_LIBRARY_PATH'
	env['value']='/home/parameter_server/third_party/lib/'
	container['env']=[env]
	container['workingDir']='/home/parameter_server/script'
	#parse the path
	cmd='../bin/ps -num_servers '+str(num_servers)+' -num_workers '+str(num_workers)+' -num_threads '+str(num_servers+num_workers)+' -app ../cfg/config.conf -print_van  -bind_to 8002 '
	cmd+="-scheduler \"role:SCHEDULER,hostname:\'"+scheduler_host+"\',port:11000,id:\'H\'\" "
	cmd+="-my_node \"role:SERVER,hostname:\'`cat /tmp/docker/host/host`\',port:11002,id:\'S_$(hostname)\'\""
	command=['sh','-c']
	command.append(cmd)
	print cmd
	container['command']=command
	volumeMount=dict()
	volumeMount['name']='host'
	volumeMount['mountPath']='/tmp/docker/host'
	container['volumeMounts']=[volumeMount]
	volumeMount=dict()
	volumeMount['name']='log'
	volumeMount['mountPath']='/home/parameter_server/script'
	container['volumeMounts'].append(volumeMount)
	volumeMount=dict()
	volumeMount['name']='cfg'
	volumeMount['mountPath']='/home/parameter_server/cfg'
	container['volumeMounts'].append(volumeMount)
	volumeMount=dict()
	volumeMount['name']='dat'
	volumeMount['mountPath']='/home/parameter_server/dat'
	container['volumeMounts'].append(volumeMount)

	volume=dict()
	volume['name']='host'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/host'
	volumes=[volume]
	volume=dict()
	volume['name']='log'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']='/tmp/docker/log/server'
	volumes.append(volume)
	volume=dict()
	volume['name']='cfg'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=os.path.split(os.path.abspath("../config/config.conf"))[0]
	volumes.append(volume)
	volume=dict()
	volume['name']='dat'
	volume['source']=dict()
	volume['source']['hostDir']=dict()
	volume['source']['hostDir']['path']=data_dir
	volumes.append(volume)

	manifest=dict()
	manifest['version']='v1beta1'
	manifest['id']='server'
	manifest['containers']=[container]
	manifest['volumes']=volumes

	desiredState=dict()
	desiredState['manifest']=manifest

	podTemplate=dict()
	podTemplate['desiredState']=desiredState
	podTemplate['labels']=dict()
	podTemplate['labels']['name']='server'

	desiredState=dict()
	desiredState['podTemplate']=podTemplate
	desiredState['replicas']=num_servers
	desiredState['replicaSelector']=dict()
	desiredState['replicaSelector']['name']='server'

	server['desiredState']=desiredState
	server['labels']=dict()
	server['labels']['name']='server'


	fpath='../json/replicationControllers/'+server['id']+'.json'
	with open(fpath, 'w') as outfile:
	  json.dump(server, outfile)
	cmd='../bin/kubecfg -c '+fpath+' create replicationControllers'
	print cmd
	os.system(cmd)

if __name__=='__main__':
	program=sys.argv[1]
	if program=="clear":
		killAll()
	elif program=="submit":
		num_servers=int(sys.argv[2])
		num_workers=int(sys.argv[3])
		config_path=os.path.abspath(sys.argv[4])
		data_dir=extractDatPath(config_path)
		scheduler_host=upScheduler(num_servers,num_workers,data_dir)
		print "Detect scheduler at "+scheduler_host
		print "Waiting for seconds, to ensure scheduler successfully binded."
		time.sleep(20)
		upServer(scheduler_host,num_servers,num_workers,data_dir)
		upWorker(scheduler_host,num_servers,num_workers,data_dir)
 




