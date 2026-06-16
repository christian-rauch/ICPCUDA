#ifndef INTERNAL_HPP_
#define INTERNAL_HPP_

#include "containers/device_array.hpp"
#include <Eigen/Eigen>

#define MAX_THREADS 1024

static inline int divUp(int total, int grain) {
  return (total + grain - 1) / grain;
}

struct Intr {
  float fx, fy, cx, cy;
  Intr() : fx(0), fy(0), cx(0), cy(0) {}
  Intr(float fx_, float fy_, float cx_, float cy_)
      : fx(fx_), fy(fy_), cx(cx_), cy(cy_) {}

  Intr operator()(int level_index) const {
    int div = 1 << level_index;
    return (Intr(fx / div, fy / div, cx / div, cy / div));
  }
};

void estimateStep(
    const Eigen::Matrix<float, 3, 3, Eigen::DontAlign> &R_prev_curr,
    const Eigen::Matrix<float, 3, 1, Eigen::DontAlign> &t_prev_curr,
    const DeviceArray2D<float> &vmap_curr,
    const DeviceArray2D<float> &nmap_curr, const Intr &intr,
    const DeviceArray2D<float> &vmap_prev,
    const DeviceArray2D<float> &nmap_prev, float dist_thresh,
    float angle_thresh,
    DeviceArray<Eigen::Matrix<float, 29, 1, Eigen::DontAlign>> &sum,
    DeviceArray<Eigen::Matrix<float, 29, 1, Eigen::DontAlign>> &out,
    float *matrixA_host, float *vectorB_host, float *residual_inliers,
    int threads, int blocks);

void pyrDown(const DeviceArray2D<unsigned short> &src,
             DeviceArray2D<unsigned short> &dst);

void createVMap(const Intr &intr, const DeviceArray2D<unsigned short> &depth,
                DeviceArray2D<float> &vmap, const float depthCutoff);
void createNMap(const DeviceArray2D<float> &vmap, DeviceArray2D<float> &nmap);

#endif /* INTERNAL_HPP_ */
