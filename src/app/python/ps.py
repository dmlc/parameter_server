import _ps

try:
    import owl
    _owl_loaded = True
except ImportError:
    owl = object()
    owl.NArray = None
    owl.from_numpy = lambda x: None
    _owl_loaded = False


__all__ = ['my_node_id', 'my_rank', 'rank_size', 'pull_weight', 'push_grad_and_pull_weight']


my_node_id = _ps.myNodeID()

my_rank = _ps.myRank()

rank_size = _ps.rankSize()


def pull_weight(weight, name):
    is_weight_narray = _owl_loaded and isinstance(weight, owl.NArray)

    if is_weight_narray:
        weight = weight.to_numpy()

    _ps.PullWeight(weight, name)

    if is_weight_narray:
        return owl.from_numpy(weight)
    else:
        return weight


def push_grad_and_pull_weight(grad, weight, name):
    is_weight_narray = _owl_loaded and isinstance(weight, owl.NArray)

    if is_weight_narray:
        weight = weight.to_numpy()

    if _owl_loaded and isinstance(grad, owl.NArray):
        grad = grad.to_numpy()

    _ps.PushGradAndPullWeight(grad, weight, name)

    if is_weight_narray:
        return owl.from_numpy(weight)
    else:
        return weight

