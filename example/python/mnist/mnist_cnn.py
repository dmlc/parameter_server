import sys
import time
import argparse
import numpy as np
import mnist_io
import owl
import owl.elewise as ele
import owl.conv as conv

# PS
import ps
#bulk = True

lazy_cycle = 4

class MNISTCNNModel:
    def __init__(self):
        self.convs = [
            conv.Convolver(0, 0, 1, 1),
            conv.Convolver(2, 2, 1, 1),
        ];
        self.poolings = [
            conv.Pooler(2, 2, 2, 2, 0, 0, conv.pool_op.max),
            conv.Pooler(3, 3, 3, 3, 0, 0, conv.pool_op.max)
        ];

    def init_random(self):
        # PS: pull from the server
        #self.weights = [
        #    owl.randn([5, 5, 1, 16], 0.0, 0.1),
        #    owl.randn([5, 5, 16, 32], 0.0, 0.1),
        #    owl.randn([10, 512], 0.0, 0.1)
        #];
        #self.weightdelta = [
        #    owl.zeros([5, 5, 1, 16]),
        #    owl.zeros([5, 5, 16, 32]),
        #    owl.zeros([10, 512])
        #];
        #self.bias = [
        #    owl.zeros([16]),
        #    owl.zeros([32]),
        #    owl.zeros([10, 1])
        #];
        #self.biasdelta = [
        #    owl.zeros([16]),
        #    owl.zeros([32]),
        #    owl.zeros([10, 1])
        #];
        self.weights = [
            ps.pull_weight(owl.zeros([5, 5, 1, 16]), 'w_0'),
            ps.pull_weight(owl.zeros([5, 5, 16, 32]), 'w_1'),
            ps.pull_weight(owl.zeros([10, 512]), 'w_2'),
        ]
        self.weightdelta = [
            ps.pull_weight(owl.zeros([5, 5, 1, 16]), 'wd_0'),
            ps.pull_weight(owl.zeros([5, 5, 16, 32]), 'wd_1'),
            ps.pull_weight(owl.zeros([10, 512]), 'wd_2'),
        ]
        self.bias = [
            ps.pull_weight(owl.zeros([16]), 'b_0'),
            ps.pull_weight(owl.zeros([32]), 'b_1'),
            ps.pull_weight(owl.zeros([10, 1]), 'b_2'),
        ]
        self.biasdelta = [
            ps.pull_weight(owl.zeros([16]), 'bd_0'),
            ps.pull_weight(owl.zeros([32]), 'bd_1'),
            ps.pull_weight(owl.zeros([10, 1]), 'bd_2'),
        ]

def print_training_accuracy(o, t, mbsize, prefix):
    predict = o.reshape([10, mbsize]).argmax(0)
    ground_truth = t.reshape([10, mbsize]).argmax(0)
    correct = (predict - ground_truth).count_zero()
    print prefix, 'error: {}'.format((mbsize - correct) * 1.0 / mbsize)

def bpprop(model, samples, label):
    num_layers = 6
    num_samples = samples.shape[-1]
    fc_shape = [512, num_samples]

    acts = [None] * num_layers
    errs = [None] * num_layers
    weightgrad = [None] * len(model.weights)
    biasgrad = [None] * len(model.bias)

    acts[0] = samples
    acts[1] = ele.relu(model.convs[0].ff(acts[0], model.weights[0], model.bias[0]))
    acts[2] = model.poolings[0].ff(acts[1])
    acts[3] = ele.relu(model.convs[1].ff(acts[2], model.weights[1], model.bias[1]))
    acts[4] = model.poolings[1].ff(acts[3])
    acts[5] = model.weights[2] * acts[4].reshape(fc_shape) + model.bias[2]

    out = conv.softmax(acts[5], conv.soft_op.instance)

    errs[5] = out - label
    errs[4] = (model.weights[2].trans() * errs[5]).reshape(acts[4].shape)
    errs[3] = ele.relu_back(model.poolings[1].bp(errs[4], acts[4], acts[3]), acts[3])
    errs[2] = model.convs[1].bp(errs[3], acts[2], model.weights[1])
    errs[1] = ele.relu_back(model.poolings[0].bp(errs[2], acts[2], acts[1]), acts[1])

    weightgrad[2] = errs[5] * acts[4].reshape(fc_shape).trans()
    biasgrad[2] = errs[5].sum(1)
    weightgrad[1] = model.convs[1].weight_grad(errs[3], acts[2], model.weights[1])
    biasgrad[1] = model.convs[1].bias_grad(errs[3])
    weightgrad[0] = model.convs[0].weight_grad(errs[1], acts[0], model.weights[0])
    biasgrad[0] = model.convs[0].bias_grad(errs[1])
    return (out, weightgrad, biasgrad)

# PS: "mom" is used by the server
#def train_network(model, num_epochs=100, minibatch_size=256, lr=0.01, mom=0.75, wd=5e-4):
def train_network(model, num_epochs=100, minibatch_size=256, lr=0.01, wd=5e-4):
    # load data
    (train_data, test_data) = mnist_io.load_mb_from_mat('mnist_all.mat', minibatch_size / len(gpu))
    num_test_samples = test_data[0].shape[0]
    test_samples = owl.from_numpy(test_data[0]).reshape([28, 28, 1, num_test_samples])
    test_labels = owl.from_numpy(test_data[1])
    for i in xrange(num_epochs):
        print "---Epoch #", i
        last = time.time()
        count = 0
        weightgrads = [None] * len(gpu)
        biasgrads = [None] * len(gpu)
        for idx, (mb_samples, mb_labels) in enumerate(train_data):
            if idx % ps.rank_size != ps.my_rank: continue

            count += 1
            current_gpu = count % len(gpu)
            owl.set_device(gpu[current_gpu])
            num_samples = mb_samples.shape[0]
            data = owl.from_numpy(mb_samples).reshape([28, 28, 1, num_samples])
            label = owl.from_numpy(mb_labels)
            out, weightgrads[current_gpu], biasgrads[current_gpu] = bpprop(model, data, label)
            # PS: XXX: where is start_eval()?
            #out.start_eval()
            if current_gpu == 0:
                for k in range(len(model.weights)):
                    # PS: use the server for updates
                    #model.weightdelta[k] = mom * model.weightdelta[k] - lr / num_samples / len(gpu) * multi_gpu_merge(weightgrads, 0, k) - lr * wd * model.weights[k]
                    #model.biasdelta[k] = mom * model.biasdelta[k] - lr / num_samples / len(gpu) * multi_gpu_merge(biasgrads, 0, k)
                    #model.weights[k] += model.weightdelta[k]
                    #model.bias[k] += model.biasdelta[k]
                    model.weightdelta[k] = ps.push_grad_and_pull_weight(lr / num_samples / len(gpu) * multi_gpu_merge(weightgrads, 0, k) + lr * wd * model.weights[k], model.weightdelta[k], "wd_%d" % k)
                    model.biasdelta[k] = ps.push_grad_and_pull_weight(lr / num_samples / len(gpu) * multi_gpu_merge(biasgrads, 0, k), model.biasdelta[k], "bd_%d" % k)
                    model.weights[k] = ps.push_grad_and_pull_weight(model.weightdelta[k], model.weights[k], "w_%d" % k)
                    model.bias[k] = ps.push_grad_and_pull_weight(model.biasdelta[k], model.bias[k], "b_%d" % k)
                if count % (len(gpu) * lazy_cycle) == 0:
                    print_training_accuracy(out, label, num_samples, 'Training')
        print '---End of Epoch #', i, 'time:', time.time() - last
        # do test
        out, _, _  = bpprop(model, test_samples, test_labels)
        print_training_accuracy(out, test_labels, num_test_samples, 'Testing')

def multi_gpu_merge(l, base, layer):
    if len(l) == 1:
        return l[0][layer]
    left = multi_gpu_merge(l[:len(l) / 2], base, layer)
    right = multi_gpu_merge(l[len(l) / 2:], base + len(l) / 2, layer)
    owl.set_device(base)
    return left + right


class MnistServer:
    def __init__(self, mom=0.75):
        print('Server; NodeID: %s, Rank: %d, RankSize: %d' % (ps.my_node_id, ps.my_rank, ps.rank_size))
        self.cpu = owl.create_cpu_device()
        self.mom = mom

    def init_layer(self, name, weight):
        type_, _, i = name.partition('_')
        i = int(i)

        weights = [
            owl.randn([5, 5, 1, 16], 0.0, 0.1),
            owl.randn([5, 5, 16, 32], 0.0, 0.1),
            owl.randn([10, 512], 0.0, 0.1)
        ];
        weightdelta = [
            owl.zeros([5, 5, 1, 16]),
            owl.zeros([5, 5, 16, 32]),
            owl.zeros([10, 512])
        ];
        bias = [
            owl.zeros([16]),
            owl.zeros([32]),
            owl.zeros([10, 1])
        ];
        biasdelta = [
            owl.zeros([16]),
            owl.zeros([32]),
            owl.zeros([10, 1])
        ];

        if type_ == 'w':
            np.copyto(weight, weights[i].to_numpy().flatten())
        elif type_ == 'wd':
            np.copyto(weight, weightdelta[i].to_numpy().flatten())
        elif type_ == 'b':
            np.copyto(weight, bias[i].to_numpy().flatten())
        elif type_ == 'bd':
            np.copyto(weight, biasdelta[i].to_numpy().flatten())
        else:
            assert False

    def update_layer(self, name, weight, gradient):
        type_, _, _ = name.partition('_')
        if type_ == 'wd' or type_ == 'bd':
            # weight must be updated in place
            weight *= self.mom
            weight -= gradient
        elif type_ == 'w' or type_ == 'b':
            weight += gradient
        else:
            assert False


# PS: server
server = None
def server_node_init():
    global server
    owl.initialize(sys.argv + ['-no_init_glog'])
    server = MnistServer()

def server_init_layer(name, weight):
    server.init_layer(name, weight)

def server_update_layer(name, weight, gradient):
    server.update_layer(name, weight, gradient)

# PS: worker
worker = None
def worker_node_init():
    global gpu
    parser = argparse.ArgumentParser(description='MNIST CNN')
    parser.add_argument('-n', '--num', dest='num', help='number of GPUs to use', action='store', type=int, default=1)
    (args, remain) = parser.parse_known_args()
    owl.initialize([sys.argv[0]] + remain + ['-no_init_glog'])
    assert(1 <= args.num)
    print 'Using %d GPU(s)' % args.num
    # PS: for local test
    #gpu = [owl.create_gpu_device(i) for i in range(args.num)]
    gpu = [owl.create_gpu_device((i + ps.my_rank * args.num) % owl.get_gpu_device_count()) for i in range(args.num)]
    owl.set_device(gpu[0])

def worker_node_main():
    model = MNISTCNNModel()
    model.init_random()
    train_network(model)

if __name__ == '__main__':
    pass

