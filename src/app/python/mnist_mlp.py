import sys,os
import math
import owl
import owl.elewise as ele
import owl.conv as co
import numpy as np
import mnist_io

# PS
import ps

class MnistTrainer:
    def __init__(self, data_file='mnist_all.mat', num_epochs=100, mb_size=256, eps_w=0.01, eps_b=0.01):
        self.cpu = owl.create_cpu_device()
        self.gpu = owl.create_gpu_device(0)
        self.data_file = data_file
        self.num_epochs=num_epochs
        self.mb_size=mb_size
        self.eps_w=eps_w
        self.eps_b=eps_b
        # init weight
        l1 = 784; l2 = 256; l3 = 10
        # PS: do not initialize weights on workers
        # self.l1 = l1; self.l2 = l2; self.l3 = l3
        # self.w1 = owl.randn([l2, l1], 0.0, math.sqrt(4.0 / (l1 + l2)))
        # self.w2 = owl.randn([l3, l2], 0.0, math.sqrt(4.0 / (l2 + l3)))
        # self.b1 = owl.zeros([l2, 1])
        # self.b2 = owl.zeros([l3, 1])
        # PS: instead, pull weights from servers
        t_w1 = owl.zeros([l2, l1]).to_numpy()
        t_w2 = owl.zeros([l3, l2]).to_numpy()
        t_b1 = owl.zeros([l2, 1]).to_numpy()
        t_b2 = owl.zeros([l3, 1]).to_numpy()
        ps.PullWeight(t_w1, 'w1')
        ps.PullWeight(t_w2, 'w2')
        ps.PullWeight(t_b1, 'b1')
        ps.PullWeight(t_b2, 'b2')
        self.w1 = owl.from_numpy(t_w1)
        self.w2 = owl.from_numpy(t_w2)
        self.b1 = owl.from_numpy(t_b1)
        self.b2 = owl.from_numpy(t_b2)

    def run(self):
        (train_data, test_data) = mnist_io.load_mb_from_mat(self.data_file, self.mb_size)
        np.set_printoptions(linewidth=200)
        num_test_samples = test_data[0].shape[0]
        (test_samples, test_labels) = map(lambda npdata : owl.from_numpy(npdata), test_data)
        count = 1
        owl.set_device(self.gpu)
        for epoch in range(self.num_epochs):
            print '---Start epoch #%d' % epoch
            # train
            for (mb_samples, mb_labels) in train_data:
                num_samples = mb_samples.shape[0]

                a1 = owl.from_numpy(mb_samples)
                target = owl.from_numpy(mb_labels)

                # ff
                a2 = ele.relu(self.w1 * a1 + self.b1)
                a3 = self.w2 * a2 + self.b2
                # softmax & error
                out = co.softmax(a3)
                s3 = out - target
                # bp
                s2 = self.w2.trans() * s3
                s2 = ele.relu_back(s2, a2)
                # grad
                gw1 = s2 * a1.trans() / num_samples
                gb1 = s2.sum(1) / num_samples
                gw2 = s3 * a2.trans() / num_samples
                gb2 = s3.sum(1) / num_samples
                # update
                # PS: do not update weights locally
                #self.w1 -= self.eps_w * gw1
                #self.w2 -= self.eps_w * gw2
                #self.b1 -= self.eps_b * gb1
                #self.b2 -= self.eps_b * gb2
                # PS: instead, push gradients and pull weights from servers
                t_w1 = self.w1.to_numpy()
                t_w2 = self.w2.to_numpy()
                t_b1 = self.b1.to_numpy()
                t_b2 = self.b2.to_numpy()
                ps.PushGradAndPullWeight(gw1.to_numpy(), t_w1, 'w1')
                ps.PushGradAndPullWeight(gw2.to_numpy(), t_w2, 'w2')
                ps.PushGradAndPullWeight(gb1.to_numpy(), t_b1, 'b1')
                ps.PushGradAndPullWeight(gb2.to_numpy(), t_b2, 'b2')
                self.w1 = owl.from_numpy(t_w1)
                self.w2 = owl.from_numpy(t_w2)
                self.b1 = owl.from_numpy(t_b1)
                self.b2 = owl.from_numpy(t_b2)

                if (count % 40 == 0):
                    correct = out.argmax(0) - target.argmax(0)
                    val = correct.to_numpy()
                    print 'Training error:', float(np.count_nonzero(val)) / num_samples
                count = count + 1

            # test
            a1 = test_samples
            a2 = ele.relu(self.w1 * a1 + self.b1)
            a3 = self.w2 * a2 + self.b2
            correct = a3.argmax(0) - test_labels.argmax(0)
            val = correct.to_numpy()
            #print val
            print 'Testing error:', float(np.count_nonzero(val)) / num_test_samples
            print '---Finish epoch #%d' % epoch

# PS - server
g_server_cpu = None

def init_layer(name, weight):
  # weight initialization

  # we must use the CPU on the server to run Minerva
  global g_server_cpu
  if g_server_cpu is None:
    g_server_cpu = owl.create_cpu_device()

  print(ps.myNodeID(), ", this is server ", ps.myRank())

  l1 = 784; l2 = 256; l3 = 10

  w1 = owl.randn([l2, l1], 0.0, math.sqrt(4.0 / (l1 + l2))).to_numpy()
  w2 = owl.randn([l3, l2], 0.0, math.sqrt(4.0 / (l2 + l3))).to_numpy()
  b1 = owl.zeros([l2, 1]).to_numpy()
  b2 = owl.zeros([l3, 1]).to_numpy()

  if name == 'w1':
    np.copyto(weight, w1.flatten())
  elif name == 'w2':
    np.copyto(weight, w2.flatten())
  elif name == 'b1':
    np.copyto(weight, b1.flatten())
  elif name == 'b2':
    np.copyto(weight, b2.flatten())
  else:
    assert False
  print('init_layer done')

def update_layer(name, weight, gradient):
  eps_w = 0.01
  eps_b = 0.01

  if name[0] == 'w':
    weight -= eps_w * gradient
  elif name[0] == 'b':
    weight -= eps_b * gradient
  else:
    assert False

# PS - worker
def worker_node_main():
    trainer = MnistTrainer(num_epochs = 10)
    trainer.run()

if __name__ == '__main__':
    owl.initialize(sys.argv)

