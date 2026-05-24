import networkx as nx


def _build_graph(node_count):
    graph = nx.Graph()
    graph.add_node("hub")
    for index in range(int(node_count)):
        node = f"n{index}"
        graph.add_edge("hub", node, weight=index + 1)
        if index > 0:
            graph.add_edge(f"n{index - 1}", node, weight=((index * 7) % 13) + 1)
    return graph


def graph_checksum(node_count):
    graph = _build_graph(node_count)
    total = (graph.number_of_nodes() * 17) + (graph.number_of_edges() * 19)
    for left, right, data in graph.edges(data=True):
        total += (len(left) * 3) + (len(right) * 5) + int(data.get("weight", 1))
    return int(total)


def graph_report(node_count):
    graph = _build_graph(node_count)
    return f"nodes={graph.number_of_nodes()} edges={graph.number_of_edges()} checksum={graph_checksum(node_count)}"
