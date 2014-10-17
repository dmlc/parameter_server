#pragma once

namespace PS {

// solve argmin_y x*y + .5*lambda_2*y^2 + lambda_1*|y|
template <typename V>
V softThresholding(V x, V lambda_1, V lambda_2) {
  if (x > 0) {
    return x > lambda_1 ? (x - lambda_1) / lambda_2 : 0;
  } else {
    return x < - lambda_1 ? (x + lambda_1) / lambda_2 : 0;
  }
}


} // namespace PS
