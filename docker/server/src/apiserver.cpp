// Author: Wes Kendall
// Copyright 2011 www.mpitutorial.com
// This code is provided freely with the tutorials on mpitutorial.com. Feel
// free to modify it for your own use. Any distribution of the code must
// either provide a link to www.mpitutorial.com or keep this header in tact.
//
// An intro MPI hello world program that uses MPI_Init, MPI_Comm_size,
// MPI_Comm_rank, MPI_Finalize, and MPI_Get_processor_name.
//
#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
using namespace std;
int main(int argc, char** argv) {
  // Initialize the MPI environment. The two arguments to MPI Init are not
  // currently used by MPI implementations, but are there in case future
  // implementations might need the arguments.
  string etcd_ip=argv[1];
  vector<string> ips;
  
  ifstream fin;
  fin.open(argv[2]);
  string str;
  while(getline(fin,str))
  {
    ips.push_back(str);
  }
  fin.close();
  fin.clear();

  MPI_Init(NULL, NULL);

  // Get the number of processes
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Get the rank of the process
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  // Get the name of the processor
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  
  char cmd[1000];
  //launch apiserver
  sprintf(cmd,"docker run  -d -p 8000:8000 --name apiserver qicongc/kubernetes ./kubernetes/apiserver --address=0.0.0.0 --port=8000 --etcd_servers=http://%s:4001 --machines=",etcd_ip.c_str());
  sprintf(cmd,"%s%s",cmd,ips[0].c_str());
  for(int i=1;i<ips.size();i++)
  {
    sprintf(cmd,"%s,%s",cmd,ips[i].c_str());
  }
  sprintf(cmd,"%s --logtostderr=true",cmd);
  system(cmd);
  //launch controller-manager
  sprintf(cmd,"docker run -d --name controller-manager  qicongc/kubernetes ./kubernetes/controller-manager --master=http://%s:8000 --logtostderr=true",etcd_ip.c_str());
  system(cmd);
  //launch scheduler
  sprintf(cmd,"docker run -d --name scheduler qicongc/kubernetes ./kubernetes/scheduler -address=0.0.0.0 -port=10251 -master=http://%s:8000 -logtostderr=true",etcd_ip.c_str());
  system(cmd);

  
  // Finalize the MPI environment. No more MPI calls can be made after this
  MPI_Finalize();
}
