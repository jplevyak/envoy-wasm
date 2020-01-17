load("@rules_proto//proto:defs.bzl", "proto_library")

filegroup(
    name = "test_cpp_srcs",
    srcs = [
        "test_cpp.cc",
    ],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "test_cpp.wasm",
    srcs = [
        "test_cpp.cc",
    ],
    additional_linker_inputs = ["@envoy_wasm_api//:jslib"],
    linkopts = [
        "--js-library",
        "external/envoy_wasm_api/proxy_wasm_intrinsics.js",
    ],
    deps = [
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
    ],
    visibility = ["//visibility:public"],
)
