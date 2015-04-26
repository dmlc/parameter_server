# Parameter Server Cloud

Parameter Server Cloud runs workers and servers on cluster as docker containers managed by docker swarm, letting you develop, debug and scale up your machine learning applications in an easier way.

## Requirements
- A computer with [docker](https://www.docker.com/) installed. If it is Linux system, please ensure you can [run docker without sudo](http://askubuntu.com/questions/477551/how-can-i-use-docker-without-sudo) 
- A [docker account](https://hub.docker.com/account/signup/) to enable push/pull containers to/from your dockerhub. 
- Account to cloud providers (Currently support amazonec2)

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

Parameter Server Cloud can run on both mac and linux system. This example shows how to run on **mac**. Before the next steps please check again your computer **has docker installed and started**. You can check it by running.
```bash
docker version
```

Currently we only support cloud provider amazonec2(and spot instances). This example will go through the running process on ec2 spot instances for a cheaper test. If you want to save time, just change "spot" to "regular" later when running fire_amazonec2.sh.

```bash
cd docker
./fire_amazonec2.sh 3 c4.xlarge spot 0.08 <amazonec2-access-key> <amazonec2-secret-key> us-west-1 vpc-3c3de859 default b  
```
This command will first clone docker-machine if you haven't it installed. Then it will use docker-machine to launch a docker-swarm master with 3 c4.xlarge Ubuntu 14.04 LTS spot instances bidding at $0.08, one as master, two as slaves. This fire_amazonec2.sh is just a template. To change the AMI please run "docker-machine create --help" and change the aws related parameters in fire_amazonec2.sh. For further info about docker-machine please look at [docker machine repository](https://github.com/docker/machine)

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

Then your local config files given at 1st argument will be uploaded to the s3 paths given at 2nd argument and automatically granted public-read permission. Config file is different from data file, usually you want to change it frequently, so you may feel annoyed to use console update it and make it public again and again. This script will do all this for you. 

Till now things about amazonec2 cloud are done. Let's launch our application.

### Build your container
```bash
vi ../make/config.mk
# change the USE_S3 flag from 0 to 1
# save and quit
# this will enable you to activate cloud function of parameter server
./build.sh swarm-master <your docker account>/parameter_server
```
This command will copy the current source file and Makefile into a container on your swarm manager, compile the parameter server inside this container, and update it to your dockerhub. **For contributors** this is where you can compile your source code, build your container with no compiler/library dependencies. For further information please refer to the Dockerfile. Also you can change swarm-master to any name of machine in your "docker-machine ls".

### Train your model
```bash
./cloud.sh amazonec2 swarm-master <your docker account>/parameter_server 2 4 s3://qicongc-dev-bucket-ps/config/online_l1lr.conf -logtostderr 
```
This will tell Parameter Server Cloud your cloud provider is amazonec2 and your swarm manager is called "swarm-master". And you want to launch 2 servers 4 workers from the docker image \<your docker account\>/parameter_server. And the config file is at the s3 path. You can append any parameter server arguments at the end of this command such as -logtostderr...

Then every node of your cluster will pull the newest version of your image, and run your application distributively. The docker swarm scheduler will ensure no port conflict will happen.

We have a special name n0 for scheduler. You can see the log of scheduler by running:

```bash
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
docker logs n0
[0426 15:57:49.845 manager.cc:33] Staring system. Logging into /tmp/linear.log.*
[0426 15:57:49.919 model_evaluation.h:32] find 2 model files
[0426 15:57:50.442 model_evaluation.h:61] load 303554 model entries
[0426 15:57:50.479 model_evaluation.h:66] find 4 data files
  load 31275 examples                        
[0426 15:57:52.012 model_evaluation.h:104] auc: 0.651429
[0426 15:57:52.015 model_evaluation.h:105] accuracy: 0.608441
[0426 15:57:52.015 model_evaluation.h:106] logloss: 0.657620
```

##Debugging FAQ:
**1. Why in the scheduler of the log it says "failed to parse conf" or "find 0 data files"?**

The reason might be you forget to make your data and config public. This can be done by either make it public on the s3 console or upload it with our upload_s3.sh script. And we strongly recommend you to revoke the permission right after you finish your experiments. We will support signature soon.

**2. Why I cannot see my model, my config file on s3?**

The reason might be you forget to open the list and upload permission of your parameter server s3 bucket. This can be done by go to the root directory of your bucket, click Properties->Permissions, tick the List and Upload permission to everyone. We strongly recommend you to revoke the permission right after you finish your experiments.

**3. Why my parameter server application get stuck?**

The reason might be your workers and servers scheduled on different machines can not talk to each other. You should check whether you have enabled traffic within your security group.

**4. Why I wait for so long but cannot get all instances from amazon?**

If you are using spot instances, you should go to aws console to see what's the problem with your spot requests. Your price might be too low or there are not enough machines.


