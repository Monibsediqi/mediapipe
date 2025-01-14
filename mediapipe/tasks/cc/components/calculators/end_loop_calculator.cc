/* Copyright 2022 The MediaPipe Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "mediapipe/calculators/core/end_loop_calculator.h"

#include <vector>

#include "mediapipe/tasks/cc/components/containers/classifications.pb.h"

// Specialized EndLoopCalculator for Tasks specific types.
namespace mediapipe::tasks {

typedef EndLoopCalculator<std::vector<ClassificationResult>>
    EndLoopClassificationResultCalculator;
REGISTER_CALCULATOR(::mediapipe::tasks::EndLoopClassificationResultCalculator);

}  // namespace mediapipe::tasks
