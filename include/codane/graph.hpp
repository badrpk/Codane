#pragma once
#include "codane/agent.hpp"
#include "codane/journal.hpp"
namespace codane { struct Node {NodeId id;std::string type,prompt;std::vector<NodeId> depends;std::vector<std::string> command;NodeState state=NodeState::Pending;int attempts=0;int retry_pending=0;int loop_iteration=0;bool verified=false,accepted=false;std::vector<std::string> artifacts;std::vector<ArtifactInput> artifact_inputs;std::optional<Json> verifier;std::string artifact_dir;}; struct Graph {std::string name;Limits limits;RetryPolicy retry;bool fail_fast=true;std::map<NodeId,Node> nodes; void validate()const; std::vector<NodeId> ready()const; void mutate(const std::vector<Node>&);}; Graph load_graph(const std::filesystem::path&); Json graph_json(const Graph&); Graph graph_from_json(const Json&); std::filesystem::path artifact_path(const std::filesystem::path&,const NodeId&,const std::string&);
void populate_artifacts(AgentRequest&,const Node&,const std::filesystem::path&); }
