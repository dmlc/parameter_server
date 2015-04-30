# Parameter Server Cloud 

Parameter Server Cloud runs workers and servers on cluster as [docker](https://github.com/docker/docker) containers managed by [docker machine](https://github.com/docker/machine) and [docker swarm](https://github.com/docker/swarm), letting you develop, debug and scale up your machine learning applications in an easier way without copying your data to each machine in your cluster, writing mpi/ssh, or compiling and running with strong OS and library dependencies.

## Requirements
- A computer with [docker](https://www.docker.com/) installed. If you are using MAC, we recommend to install the command line docker [boot2docker](https://github.com/boot2docker/osx-installer/releases/tag/v1.6.0). If it is Linux system, please ensure you can [run docker without sudo](http://askubuntu.com/questions/477551/how-can-i-use-docker-without-sudo) 
- A [docker account](https://hub.docker.com/account/signup/) to enable push/pull containers to/from your dockerhub. 
- Account to cloud providers (Currently support [amazonec2](http://aws.amazon.com/ec2/))

Other than parameter server itself, this readme will also guild you step by step on the cloud providers in case you are not familiar with them.

### Set up on EC2
- Enter AWS Console, Services->IAM->Users->Create New Users->Enter User Names->Create. Save the **Access Key ID** and **Secret Access Key**;
- Go back to Users-><Choose the user you just created>->Attach Policy->AmazonEC2FullAccess&AmazonS3FullAccess->Attach Policy;
- Go to Services->EC2, at the top right corner, choose a region that you want your instances to launch from, and save the **vpc id** under the default-VPC and **region** at the input of browser, we will use them later;

![vpc](https://cloud.githubusercontent.com/assets/6102328/7334742/bb3185b6-eb69-11e4-901f-ccbdb592ab22.png)

- Press the bottom left Network & Security->Security Groups->Select default security group->Inbound. You should make sure that all tcp inbound traffic is allowed within this security group so that later your workers and servers can communicate to each other.

![sginbound](https://cloud.githubusercontent.com/assets/6102328/7334745/dbf9132c-eb69-11e4-8891-8028821780b0.png)

- At last, go to EC2 Dashboard to see what zones (one of a b c d e) are available in this region, and choose a **zone** that you want to launch instances from.

![zone](https://cloud.githubusercontent.com/assets/6102328/7334737/8add81bc-eb69-11e4-9fc8-45f6a6cae0af.png)

## Bring up a docker swarm cluster

Before the next steps please check again your computer **has docker installed and started**.

You can start docker by:
```bash
boot2docker start
#you should see something like:
#To connect the Docker client to the Docker daemon, please set:
#    export DOCKER_TLS_VERIFY=1
#    export DOCKER_HOST=tcp://192.168.59.103:2376
#    export DOCKER_CERT_PATH=/Users/yuchenluo/.boot2docker/certs/boot2docker-vm

#please copy and run those three export commands, then run
docker version
#if no error, you succeed to start docker!
```

Currently we only support cloud provider amazonec2(and spot instances). This example will go through the running process on ec2 spot instances for a cheaper test. If you want to save time, just change "spot" to "regular" later when running fire_amazonec2.sh.

```bash
cd docker
./fire_amazonec2.sh 3 c4.xlarge spot 0.08 <amazonec2-access-key> <amazonec2-secret-key> us-west-1 vpc-3c3de859 default b  
```
This command will first clone and install right version of docker-machine according to your OS if you haven't it installed. Then it will use docker-machine to launch a docker-swarm cluster with 3 c4.xlarge Ubuntu 14.04 LTS spot instances bidding at $0.08, one as master, two as slaves. This fire_amazonec2.sh is just a template. To change the AMI please run "docker-machine create --help" and change the aws related parameters in fire_amazonec2.sh. For further info about docker-machine please look at [docker machine repository](https://github.com/docker/machine)

Please wait for 5 minutes for the spot request fullfillment. If you want to save time, just change "spot" to "regular".

```bash
docker-machine ls
#You should see something like
#NAME          ACTIVE    DRIVER      STATE     URL                         SWARM
#swarm-master            amazonec2   Running   tcp://50.18.210.123:2376    swarm-master (master)
#swarm-node-0            amazonec2   Running   tcp://52.8.7.32:2376        swarm-master
#swarm-node-1   *        amazonec2   Running   tcp://52.8.79.252:2376      swarm-master
```
This command will list the ec2 instances that you have launched just now. 

## Bring up a parameter server application
### Upload data and config to S3
Since currently we do not support aws signature, you should change your S3's List and Upload permission to everyone to enable the parameter server to list file and save model, then **save** the permission. 

![bucket](https://cloud.githubusercontent.com/assets/6102328/7334808/b3c83cb8-eb6c-11e4-9799-b7bf42438021.png)

We recommend two ways to upload your data to s3:

####Use aws s3 console to upload large input data into your s3 folder. 
After uploading your data into a folder, also make this folder public to enable the parameter server to read from it.

![data](https://cloud.githubusercontent.com/assets/6102328/7337985/2c9e6560-ec0b-11e4-96a0-2257d088075c.png)

####Use ./upload_s3.sh we provided to upload your small application config file to s3. 

The config file is almost the same as under our example/ except you have to change all the local path to s3 path(s3://bucket-n ame/path/to/your/file):
```bash
#example/linear/ctr/online_l1lr.conf
training_data {
format: TEXT
text: SPARSE_BINARY
file: "s3://qicongc-dev-bucket-ps/data/ctr/train/part.*"
}

model_output {
format: TEXT
file: "s3://qicongc-dev-bucket-ps/model/ctr_online"
}
#...

#example/linear/ctr/eval_online.conf
validation_data {
format: TEXT
text: SPARSE_BINARY
file: "s3://qicongc-dev-bucket-ps/data/ctr/test/part.*"
}

model_input {
format: TEXT
file: "s3://qicongc-dev-bucket-ps/model/ctr_online.*"
}
#...
```

Don't worry Parameter Server Cloud will create the model path for you even if it does not exist at this moment, as long as you have openned the upload&list permission of your parameter server bucket to everyone. After changing this config file run:

```bash
 ./upload_s3.sh ../example/linear/ctr/online_l1lr.conf s3://qicongc-dev-bucket-ps/config/online_l1lr.conf
 ./upload_s3.sh ../example/linear/ctr/eval_online.conf s3://qicongc-dev-bucket-ps/config/eval_online.conf
```

Then your local config files given at 1st argument will be uploaded to the s3 paths given at 2nd argument and automatically granted public-read permission. Config file is different from data file that usually you want to change it frequently. So you may feel annoyed to use aws console to update it and make it public again and again. This script will do all these things for you. 

Till now things about amazonec2 cloud are done. Let's launch our application.

### Build your container
```bash
vi ../make/config.mk
# change the USE_S3 flag from 0 to 1
# save and quit
# this will enable you to activate cloud function of parameter server
./build.sh swarm-master <your docker account>/parameter_server
```
This command will copy the current source files and Makefiles into a container on your swarm manager, compile the parameter server inside this container, and update it to your dockerhub. **For contributors** this is where you can compile your source code, build your container with no compiler/library dependencies. For further information please refer to the Dockerfile. Also you can change swarm-master to any name of machine in your "docker-machine ls".

### Train your model
```bash
./cloud.sh amazonec2 swarm-master <your docker account>/parameter_server 2 4 s3://qicongc-dev-bucket-ps/config/online_l1lr.conf -logtostderr 
```
This will tell Parameter Server Cloud your cloud provider is amazonec2 and your swarm manager is called "swarm-master". And you want to launch 2 servers 4 workers from the docker image \<your docker account\>/parameter_server. And the config file is at the s3 path. You can append any parameter server arguments at the end of this command such as -logtostderr...

Then every node of your cluster will pull the newest version of your image, and run your application distributively. The docker swarm scheduler will ensure no port conflict will happen.

We have a special name n0 for scheduler. You can see the log of scheduler by running:

```bash
#point docker client to your swarm manager
eval "$(docker-machine env --swarm swarm-master)"
#see the log of scheduler
docker logs n0
#you will see training iterations like
#...
#[0426 02:09:26.418 sgd.cc:75]  sec  examples    loss      auc   accuracy   |w|_0  updt ratio
#[0426 02:09:26.418 sgd.cc:85]    8  1.00e+04  6.932e-01  0.5148  0.5039  0.00e+00  inf
#[0426 02:09:27.502 sgd.cc:85]    9  5.00e+04  6.891e-01  0.5851  0.5454  8.17e+02  4.39e-01
#[0426 02:09:28.578 sgd.cc:85]   10  9.59e+04  5.353e-01  0.5961  0.5628  1.90e+03  2.27e-01
#[0426 02:09:29.652 sgd.cc:85]   11  1.22e+05  6.814e-01  0.5999  0.5671  5.06e+03  1.36e-01
#[0426 02:09:30.721 sgd.cc:85]   12  1.70e+05  5.338e-01  0.6235  0.5871  8.42e+03  1.19e-01
#...
```

You can see your model at the s3 address you provide in your config file:

![model](https://cloud.githubusercontent.com/assets/6102328/7335713/60a0e4f4-eb9e-11e4-8188-733987a80b76.png)

### Evaluate your model
```bash
./cloud.sh amazonec2 swarm-master <your docker account>/parameter_server 1 1 s3://qicongc-dev-bucket-ps/config/eval_online.conf
```

Still you can see the evaluation result from the log of scheduler:

```bash
eval "$(docker-machine env --swarm swarm-master)"
docker logs n0
#[0426 15:57:49.845 manager.cc:33] Staring system. Logging into /tmp/linear.log.*
#[0426 15:57:49.919 model_evaluation.h:32] find 2 model files
#[0426 15:57:50.442 model_evaluation.h:61] load 303554 model entries
#[0426 15:57:50.479 model_evaluation.h:66] find 4 data files
#  load 31275 examples                        
#[0426 15:57:52.012 model_evaluation.h:104] auc: 0.651429
#[0426 15:57:52.015 model_evaluation.h:105] accuracy: 0.608441
#[0426 15:57:52.015 model_evaluation.h:106] logloss: 0.657620
```

### Using cache
If you are running batch algorithms, you can cache your data on the machines of your swarm cluster. You should modify paths of training data and model output to s3 path, while leaving local cache as local path:
```bash
training_data {
format: TEXT
text: SPARSE_BINARY
file: "s3://qicongc-dev-bucket-ps/data/ctr/train/part.*"
}

model_output {
format: TEXT
file: "s3://qicongc-dev-bucket-ps/model/ctr_batch"
}

...

local_cache {
format: BIN
file: "data/cache/ctr_train_"
}

...

```
Parameter Server Cloud currently only supports the cache under data/cache/, which means you should fix your cache path as data/cache/\<prefix\> as above. We do this to free users from dealing with docker volume mapping themselves. Advanced docker users can try to modify the cache volume mapping in cloud.sh. After also changing the paths of eval_batch.conf, you can upload these config files and train/test your model:
```bash
./upload_s3.sh ../example/linear/ctr/batch_l1lr.conf s3://qicongc-dev-bucket-ps/config/batch_l1lr.conf
./upload_s3.sh ../example/linear/ctr/eval_batch.conf s3://qicongc-dev-bucket-ps/config/eval_batch.conf
./cloud.sh amazonec2 swarm-master <your docker account>/parameter_server 2 4 s3://qicongc-dev-bucket-ps/config/batch_l1lr.conf
./cloud.sh amazonec2 swarm-master <your docker account>/parameter_server 1 1 s3://qicongc-dev-bucket-ps/config/eval_batch.conf
eval "$(docker-machine env --swarm swarm-master)"
docker logs n0
#[0426 23:29:36.590 manager.cc:33] Staring system. Logging into /tmp/linear.log.*
#[0426 23:29:36.650 model_evaluation.h:32] find 2 model files
#[0426 23:29:36.715 model_evaluation.h:61] load 829 model entries
#[0426 23:29:36.737 model_evaluation.h:66] find 4 data files
#  load 31275 examples                        
#[0426 23:29:38.020 model_evaluation.h:104] auc: 0.666663
#[0426 23:29:38.022 model_evaluation.h:105] accuracy: 0.622542
#[0426 23:29:38.022 model_evaluation.h:106] logloss: 0.649341
```
To see your cached data:
```bash
docker-machine ssh swarm-node-0
cd /tmp/parameter_server/data/cache
```
The data will be cached in /tmp/parameter_server/data/cache of each machine on your swarm cluster.



### Safely shut down your 3-nodes swarm cluster
```bash
./shut.sh 3
```


##Debugging FAQ:
**1. Why in the scheduler of the log it says "failed to parse conf" or "find 0 data files"?**

The reason might be you forget to make your data and config public. This can be done by either making it public on the s3 console or uploading it with our upload_s3.sh script. And we strongly recommend you to revoke the permission right after you finish your experiments. We will support signature soon.

**2. Why I cannot see my model, my config file on s3?**

The reason might be you forget to open the list and upload permission of your parameter server s3 bucket. This can be done by going to the root directory of your bucket, clicking Properties->Permissions, ticking the List and Upload permission to everyone. We strongly recommend you to revoke the permission right after you finish your experiments.

**3. Why my parameter server application get stuck?**

The reason might be your workers and servers scheduled on different machines can not talk to each other. You should check whether you have enabled traffic within your security group.

**4. Why parameter server get stuck when compiling in container after running build.sh?**

You may request too poor machines such as t2.micro to build the container.

**5. Why I wait for so long but cannot get all instances from amazon?**

If you are using spot instances, you should go to aws console to see what's the problem with your spot requests. Your price might be too low or there are not enough machines.

**6. Why I get the message "please point your docker client to the right docker server!" when running fire_amazonec2.sh?**

You haven't started docker or haven't pointed to the right machine with docker installed. To check this, run "docker version". If you are mac boot2docker user and get an error from "docker version", you can quickly reconnect to your docker daemon by running "boot2docker start" and copy&run those three export commands again.

**7. Why after running fire_amazonec2.sh I get the message:**
```bash
ERRO[0000] Error creating machine: Machine swarm-node-1 already exists 
WARN[0000] You will want to check the provider to make sure the machine and associated resources were properly removed. 
FATA[0000] Error creating machine                       
ERRO[0000] Error creating machine: Machine swarm-master already exists 
WARN[0000] You will want to check the provider to make sure the machine and associated resources were properly removed. 
FATA[0000] Error creating machine                       
ERRO[0000] Error creating machine: Machine swarm-node-0 already exists 
WARN[0000] You will want to check the provider to make sure the machine and associated resources were properly removed. 
FATA[0000] Error creating machine       
```
It is because last time you didn't use our script to safely shut down your ec2 machines, or you interrrupt the fire_amazonec2.sh launching process by ctrl+c/ctrl+z. You can recover in two steps:
1)Running "./shut.sh 3"
2)Go to aws console, in your target region manually cancel the related spot requests and instances, and delete related **key pairs**.  



