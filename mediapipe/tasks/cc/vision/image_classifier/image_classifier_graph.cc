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

#include <limits>
#include <type_traits>

#include "absl/status/statusor.h"
#include "mediapipe/framework/api2/builder.h"
#include "mediapipe/framework/api2/port.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/tasks/cc/components/classification_postprocessing.h"
#include "mediapipe/tasks/cc/components/classification_postprocessing_options.pb.h"
#include "mediapipe/tasks/cc/components/containers/classifications.pb.h"
#include "mediapipe/tasks/cc/components/image_preprocessing.h"
#include "mediapipe/tasks/cc/components/image_preprocessing_options.pb.h"
#include "mediapipe/tasks/cc/core/model_resources.h"
#include "mediapipe/tasks/cc/core/model_task_graph.h"
#include "mediapipe/tasks/cc/vision/image_classifier/proto/image_classifier_graph_options.pb.h"

namespace mediapipe {
namespace tasks {
namespace vision {
namespace image_classifier {

namespace {

using ::mediapipe::api2::Input;
using ::mediapipe::api2::Output;
using ::mediapipe::api2::builder::GenericNode;
using ::mediapipe::api2::builder::Graph;
using ::mediapipe::api2::builder::Source;

constexpr float kDefaultScoreThreshold = std::numeric_limits<float>::lowest();

constexpr char kClassificationResultTag[] = "CLASSIFICATION_RESULT";
constexpr char kImageTag[] = "IMAGE";
constexpr char kNormRectTag[] = "NORM_RECT";
constexpr char kTensorsTag[] = "TENSORS";

// Struct holding the different output streams produced by the image classifier
// subgraph.
struct ImageClassifierOutputStreams {
  Source<ClassificationResult> classification_result;
  Source<Image> image;
};

}  // namespace

// An "ImageClassifierGraph" performs image classification.
// - Accepts CPU input images and outputs classifications on CPU.
//
// Inputs:
//   IMAGE - Image
//     Image to perform classification on.
//   NORM_RECT - NormalizedRect @Optional
//     Describes region of image to perform classification on.
//     @Optional: rect covering the whole image is used if not specified.
// Outputs:
//   CLASSIFICATION_RESULT - ClassificationResult
//     The aggregated classification result object has two dimensions:
//     (classification head, classification category)
//   IMAGE - Image
//     The image that object detection runs on.
//
// Example:
// node {
//   calculator: "mediapipe.tasks.vision.image_classifier.ImageClassifierGraph"
//   input_stream: "IMAGE:image_in"
//   output_stream: "CLASSIFICATION_RESULT:classification_result_out"
//   output_stream: "IMAGE:image_out"
//   options {
//     [mediapipe.tasks.vision.image_classifier.proto.ImageClassifierGraphOptions.ext]
//     {
//       base_options {
//         model_asset {
//           file_name: "/path/to/model.tflite"
//         }
//       }
//       max_results: 3
//       score_threshold: 0.5
//       category_allowlist: "foo"
//       category_allowlist: "bar"
//     }
//   }
// }

class ImageClassifierGraph : public core::ModelTaskGraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      SubgraphContext* sc) override {
    ASSIGN_OR_RETURN(
        const auto* model_resources,
        CreateModelResources<proto::ImageClassifierGraphOptions>(sc));
    Graph graph;
    ASSIGN_OR_RETURN(
        auto output_streams,
        BuildImageClassificationTask(
            sc->Options<proto::ImageClassifierGraphOptions>(), *model_resources,
            graph[Input<Image>(kImageTag)],
            graph[Input<NormalizedRect>::Optional(kNormRectTag)], graph));
    output_streams.classification_result >>
        graph[Output<ClassificationResult>(kClassificationResultTag)];
    output_streams.image >> graph[Output<Image>(kImageTag)];
    return graph.GetConfig();
  }

 private:
  // Adds a mediapipe image classification task graph into the provided
  // builder::Graph instance. The image classification task takes images
  // (mediapipe::Image) as input and returns one classification result per input
  // image.
  //
  // task_options: the mediapipe tasks ImageClassifierGraphOptions.
  // model_resources: the ModelSources object initialized from an image
  // classification model file with model metadata.
  // image_in: (mediapipe::Image) stream to run classification on.
  // graph: the mediapipe builder::Graph instance to be updated.
  absl::StatusOr<ImageClassifierOutputStreams> BuildImageClassificationTask(
      const proto::ImageClassifierGraphOptions& task_options,
      const core::ModelResources& model_resources, Source<Image> image_in,
      Source<NormalizedRect> norm_rect_in, Graph& graph) {
    // Adds preprocessing calculators and connects them to the graph input image
    // stream.
    auto& preprocessing =
        graph.AddNode("mediapipe.tasks.components.ImagePreprocessingSubgraph");
    MP_RETURN_IF_ERROR(ConfigureImagePreprocessing(
        model_resources,
        &preprocessing
             .GetOptions<tasks::components::ImagePreprocessingOptions>()));
    image_in >> preprocessing.In(kImageTag);
    norm_rect_in >> preprocessing.In(kNormRectTag);

    // Adds inference subgraph and connects its input stream to the outoput
    // tensors produced by the ImageToTensorCalculator.
    auto& inference = AddInference(
        model_resources, task_options.base_options().acceleration(), graph);
    preprocessing.Out(kTensorsTag) >> inference.In(kTensorsTag);

    // Adds postprocessing calculators and connects them to the graph output.
    auto& postprocessing = graph.AddNode(
        "mediapipe.tasks.components.ClassificationPostprocessingSubgraph");
    MP_RETURN_IF_ERROR(ConfigureClassificationPostprocessing(
        model_resources, task_options.classifier_options(),
        &postprocessing.GetOptions<
            tasks::components::ClassificationPostprocessingOptions>()));
    inference.Out(kTensorsTag) >> postprocessing.In(kTensorsTag);

    // Outputs the aggregated classification result as the subgraph output
    // stream.
    return ImageClassifierOutputStreams{
        /*classification_result=*/postprocessing[Output<ClassificationResult>(
            kClassificationResultTag)],
        /*image=*/preprocessing[Output<Image>(kImageTag)]};
  }
};
REGISTER_MEDIAPIPE_GRAPH(
    ::mediapipe::tasks::vision::image_classifier::ImageClassifierGraph);

}  // namespace image_classifier
}  // namespace vision
}  // namespace tasks
}  // namespace mediapipe
