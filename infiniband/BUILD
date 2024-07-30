load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")
load("@bazel_gazelle//:def.bzl", "gazelle")

# Gazelle directive to update BUILD files automatically
# gazelle:prefix github.com/koshieguchi/sakura-exporter

gazelle(name = "gazelle")

go_library(
    name = "sakura-exporter_lib",
    srcs = ["infiniband-exporter.go"],
    importpath = "github.com/koshieguchi/sakura-exporter",
    visibility = ["//visibility:private"],
    deps = [
        "@com_github_prometheus_client_golang//prometheus",
        "@com_github_prometheus_client_golang//prometheus/promhttp",
        "@com_github_prometheus_procfs//sysfs",
    ],
)

go_binary(
    name = "infiniband-exporter",
    embed = [":sakura-exporter_lib"],
    out = "infiniband-exporter.out",
    visibility = ["//visibility:public"],
)
