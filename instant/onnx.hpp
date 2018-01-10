#ifndef INSTANT_LOAD_ONNX_HPP
#define INSTANT_LOAD_ONNX_HPP

#include <algorithm>
#include <exception>
#include <fstream>
#include <functional>
#include <numeric>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <instant/array.hpp>
#include <instant/dtype.hpp>
#include <instant/graph.hpp>
#include <instant/onnx.pb.h>

namespace instant {

    class onnx_load_error : public std::runtime_error {
    public:
        onnx_load_error(std::string const& message) : runtime_error(message) {}
    };

    inline auto load_onnx_model(std::string const& filename) {
        namespace gpio = ::google::protobuf::io;

        std::ifstream ifs(filename);
        gpio::IstreamInputStream iis(&ifs);
        gpio::CodedInputStream cis(&iis);
        cis.SetTotalBytesLimit(std::numeric_limits<int>::max(),
                               std::numeric_limits<int>::max());
        onnx::ModelProto onnx_model;
        if(!onnx_model.ParseFromCodedStream(&cis)) {
            throw onnx_load_error("ONNX parse error");
        }
        return onnx_model;
    }

    inline auto
    tensor_proto_data_type_to_dtype_t(onnx::TensorProto_DataType tpdt) {
        if(tpdt == onnx::TensorProto_DataType_FLOAT) {
            return dtype_t::float_;
        }
        throw std::runtime_error("Not implemented data type: " +
                                 std::to_string(tpdt));
    }

    inline auto extract_parameter_name_set(onnx::GraphProto const& graph) {
        std::set<std::string> parameter_name_set;
        for(int i = 0; i < graph.initializer_size(); ++i) {
            auto& tensor = graph.initializer(i);
            parameter_name_set.insert(tensor.name());
        }
        return parameter_name_set;
    }

    inline auto make_parameter_table_from_onnx_graph(
      onnx::GraphProto& graph,
      std::set<std::string> const& needed_parameter_name_set) {
        std::unordered_map<std::string, instant::array> parameter_table;
        for(int i = 0; i < graph.initializer_size(); ++i) {
            auto& tensor = *graph.mutable_initializer(i);
            if(needed_parameter_name_set.find(tensor.name()) ==
               needed_parameter_name_set.end()) {
                continue;
            }
            assert(tensor.has_data_type());
            dtype_t d = tensor_proto_data_type_to_dtype_t(tensor.data_type());

            std::vector<int> dims(tensor.dims().begin(), tensor.dims().end());
            auto total_size = std::accumulate(dims.begin(), dims.end(), 1,
                                              std::multiplies<int>());

            std::shared_ptr<void> data;
            if(d == instant::dtype_t::float_) {
                using float_t =
                  instant::dtype_to_type_t<instant::dtype_t::float_>;
                data = std::unique_ptr<float_t[]>(new float_t[total_size]);
                // TODO other format: float_data
                assert(tensor.has_raw_data());
                assert(tensor.raw_data().length() == total_size * 4);
                std::copy(tensor.raw_data().begin(), tensor.raw_data().end(),
                          static_cast<char*>(data.get()));
                delete tensor.release_raw_data();
            } else {
                throw onnx_load_error("Not implemented");
            }
            parameter_table.insert(
              {tensor.name(),
               instant::array(d, std::move(dims), std::move(data))});
        }
        return parameter_table;
    }

    inline auto
    extract_node_list_from_onnx_graph(onnx::GraphProto const& graph) {
        std::vector<node> node_list;
        for(auto const& onnx_node : graph.node()) {
            std::unordered_map<std::string, attribute> attribute_table;
            for(auto const& attr : onnx_node.attribute()) {
                if(attr.has_i()) {
                    attribute_table.insert(
                      {attr.name(), static_cast<int>(attr.i())}); // TODO int64
                } else if(attr.has_f()) {
                    attribute_table.insert({attr.name(), attr.f()});
                } else if(attr.ints_size()) {
                    attribute_table.insert(
                      {attr.name(), std::vector<int>(attr.ints().begin(),
                                                     attr.ints().end())});
                } else if(attr.floats_size()) {
                    attribute_table.insert(
                      {attr.name(), std::vector<float>(attr.floats().begin(),
                                                       attr.floats().end())});
                } else {
                    throw std::runtime_error(
                      "Not implemented type: " +
                      std::to_string(static_cast<int>(attr.type())) +
                      " to load for: " + attr.name());
                }
            }
            instant::node n(string_to_op_type(onnx_node.op_type()),
                            std::vector<std::string>(onnx_node.input().begin(),
                                                     onnx_node.input().end()),
                            std::vector<std::string>(onnx_node.output().begin(),
                                                     onnx_node.output().end()),
                            attribute_table);
            node_list.push_back(n);
        }
        return node_list;
    }

    inline auto
    load_onnx(std::string const& filename,
              std::set<std::string> const& required_output_name_set) {
        auto onnx_model = load_onnx_model(filename);
        auto node_list = extract_node_list_from_onnx_graph(onnx_model.graph());
        reconstruct_node_list(node_list, trim_dropout);
        reconstruct_node_list(node_list, trim_reshape);
        auto parameter_name_set =
          extract_parameter_name_set(onnx_model.graph());
        auto needed_node_list =
          extract_needed_node_list(node_list, required_output_name_set);
        auto needed_input_name_set =
          extract_needed_input_name_set(needed_node_list, parameter_name_set);
        auto needed_parameter_name_set = extract_needed_parameter_name_set(
          needed_node_list, needed_input_name_set);
        auto graph = make_graph(needed_node_list, needed_input_name_set,
                                needed_parameter_name_set);
        auto parameter_table = make_parameter_table_from_onnx_graph(
          *onnx_model.mutable_graph(), needed_parameter_name_set);
        return std::make_tuple(graph, parameter_table, needed_input_name_set);
    }

} // namespace instant

#endif // INSTANT_LOAD_ONNX_HPP
