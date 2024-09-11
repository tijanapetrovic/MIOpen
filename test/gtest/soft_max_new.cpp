/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2024 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include "test.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <miopen/convolution.hpp>
#include <miopen/miopen.h>
#include <miopen/softmax.hpp>
#include <miopen/tensor.hpp>
#include <utility>
#include <algorithm>

#include "driver.hpp"
#include "get_handle.hpp"
#include "tensor_holder.hpp"
#include "verify.hpp"

#include <cstdint>
#include <tuple>
#include <gtest/gtest.h>

struct InputDimension
{
    size_t N;
    size_t C;
    size_t H;
    size_t W;

    friend std::ostream& operator<<(std::ostream& os, const InputDimension& input)
    {
        return os << "(N: " << input.N << " C:" << input.C << " H:" << input.H << " W:" << input.W
                  << ")";
    }
};
// this is just try
std::set<std::vector<int>> GetInputDimensions()
{
    return {
        {1, 2, 3, 4},
        {4, 2, 3, 4},
        {8, 2, 3, 4},
        {16, 2, 3, 4},
    };
}
std::set<std::vector<int>> GetOutputDimensions()
{
    return {
        {1, 2, 3, 4},
        {4, 3, 2, 4},

    };
}

struct Scales
{
    float alpha;
    float beta;

    friend std::ostream& operator<<(std::ostream& os, const Scales& ab)
    {
        return os << "(alpha: " << ab.alpha << " beta:" << ab.beta << ")";
    }
};

template <typename DataType>
class SoftMaxTest
    : public ::testing::TestWithParam<
          std::tuple<InputDimension, miopenSoftmaxAlgorithm_t, miopenSoftmaxMode_t, Scales>>
{
public:
    static std::vector<InputDimension> convertDim(const std::set<std::vector<int>>& input)
    {

        std::vector<InputDimension> result;
        result.reserve(input.size());
        std::transform(input.begin(),
                       input.end(),
                       std::back_inserter(result),
                       [](const std::vector<int>& inp) {
                           return InputDimension{static_cast<size_t>(inp[0]),
                                                 static_cast<size_t>(inp[1]),
                                                 static_cast<size_t>(inp[2]),
                                                 static_cast<size_t>(inp[3])};
                       });
        return result;
    }

protected:
    void SetUp() override
    {
        std::tie(input, algo, mode, scales) = GetParam();

        auto&& handle = get_handle();
/* ovo mozda ne valja
        input_dim = tensor<DataType>{input.N, input.C, input.H, input.W};  
        std::generate(input_dim.begin(), input_dim.end(), []() { return static_cast<DataType>(rand()) / RAND_MAX; });
*/
    auto input_dims = GetInputDimensions();
    
    //prve dimenzije iz skupa
    auto selected_input_dim = *input_dims.begin(); // Možeš koristiti i druge dimenzije

    // Kreiraj tensor koristeći dimenzije iz selected_input_dim
    input_dim = tensor<DataType>{static_cast<size_t>(selected_input_dim[0]),
                                 static_cast<size_t>(selected_input_dim[1]),
                                 static_cast<size_t>(selected_input_dim[2]),
                                 static_cast<size_t>(selected_input_dim[3])};

    // Popuni input_dim tensor
    std::generate(input_dim.begin(), input_dim.end(), []() { return static_cast<DataType>(rand()) / RAND_MAX; });

        output = tensor<DataType>{input_dim};
        std::fill(output.begin(), output.end(), std::numeric_limits<DataType>::quiet_NaN());

        auto output_dev = handle.Write(output.data);
        
        ref_out = tensor<DataType>{input_dim};
        std::fill(ref_out.begin(), ref_out.end(), std::numeric_limits<DataType>::quiet_NaN());
        auto ref_out_dev = handle.Write(ref_out.data);

        //backward pass
        bw_input = tensor<DataType>{input_dim};  
        std::fill(bw_input.begin(), bw_input.end(), std::numeric_limits<DataType>::quiet_NaN());
        bw_output = tensor<DataType>{input_dim};
        std::fill(bw_output.begin(), bw_output.end(), std::numeric_limits<DataType>::quiet_NaN());

        //backward pass na GPU
        auto bw_input_dev = handle.Write(bw_input.data);
        auto bw_output_dev = handle.Write(bw_output.data);

        auto din_dev = handle.Write(input_dim.data);

    }
    void Print()
    {
        // print input test parameters
        std::cout << "Run test:" << std::endl;
        std::cout << "Input Dimensions: " << input << std::endl;
        std::cout << "Softmax Algorithm: " << algo << std::endl;
        std::cout << "Softmax Mode: " << mode << std::endl;
        std::cout << "Scales: " << scales << std::endl;
    }

    void TearDown() override
    {
        // probably deallocate anything if required
    }

private:
    // keep all the input\output intermediate data here
    InputDimension input;
    tensor<DataType> output;
    tensor<DataType> input_dim;
    tensor<DataType> ref_out;
    
    tensor<DataType> bw_input;  // Backward input tensor
    tensor<DataType> bw_output; // Backward output tensor

    miopenSoftmaxAlgorithm_t algo;
    miopenSoftmaxMode_t mode;
    Scales scales;

    // std::vector<miopen::Allocator::ManageDataPtr> inputs_dev;
    // miopen::Allocator::ManageDataPtr output_dev;
};

using GPU_SoftMax_Fwd_FP32 = SoftMaxTest<float>;

TEST_P(GPU_SoftMax_Fwd_FP32, Test) { Print(); };

INSTANTIATE_TEST_SUITE_P(
    Smoke,
    GPU_SoftMax_Fwd_FP32,
    testing::Combine(
        testing::ValuesIn(GPU_SoftMax_Fwd_FP32::convertDim(GetInputDimensions())),
        testing::Values(MIOPEN_SOFTMAX_FAST, MIOPEN_SOFTMAX_ACCURATE, MIOPEN_SOFTMAX_LOG),
        testing::Values(MIOPEN_SOFTMAX_MODE_INSTANCE, MIOPEN_SOFTMAX_MODE_CHANNEL),
        testing::Values(Scales{1.0f, 0.0f}, Scales{0.5f, 0.5f})));