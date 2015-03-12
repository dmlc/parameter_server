import ps

#kServerGroup = 'all_servers'

def init_layer(name, weight):
  print(ps.myNodeID(), ", this is server ", ps.myRank())
  pass

def update_layer(name, weight, gradient):
  pass

def worker_node_main():
  print(ps.myNodeID(), ", this is worker ", ps.myRank())

  print(1)
  import owl
  print(2)
  import sys
  print(3)
  owl.initialize(sys.argv)
  print(4)
  cpu = owl.create_cpu_device()
  print(5)
  gpu = [owl.create_gpu_device(i) for i in range(owl.get_gpu_device_count())]
  print(6)
  print '''
       __   __   _   __   _   _____   ____    _    _   ___
      /  | /  | | | |  \\ | | |  ___| |  _ \\  | |  / / /   |
     /   |/   | | | |   \\| | | |__   | |_| | | | / / / /| |
    / /|   /| | | | |      | |  __|  |    /  | |/ / / /_| |
   / / |  / | | | | | |\\   | | |___  | |\\ \\  |   / / ___  |
  /_/  |_/  |_| |_| |_| \\__| |_____| |_| \\_\\ |__/ /_/   |_|
  '''
  print '[INFO] You have %d GPU devices' % len(gpu)
  print '[INFO] Set device to gpu[0]'
  print(7)


if __name__ == '__main__':
  pass

