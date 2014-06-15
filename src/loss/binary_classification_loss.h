#pragma once
#include "loss/scalar_loss.h"

namespace PS {

// label = 1 or -1
template <typename T>
class BinaryClassificationLoss : public ScalarLoss<T> {

};

} // namespace PS
