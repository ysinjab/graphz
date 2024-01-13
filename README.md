Redis Graphz Module
===

This module provides a simple way to store graph edges and perform DFS on the graph.

Quick start guide
---

1. Build a Redis server with support for modules.
2. Build the graphz module: `make`

Loading the extension
---

To run this extension start Redis server with:

    redis-server --loadmodule /path/to/graphz.so

Alternatively add the following in your `redis.conf` file:

    loadmodule /path/to/graphz.so

Commands
---

### `graphz.addedge n1 n2`
This add undirected edge between nodes n1 and n2. If not exists it will create two hashsets one for each node where key is node name and field is the adjacent node.

### `graphz.dfs n1`
Perform a DFS on node n and returns all visited nodes with its degrees.
**Returns:** 
```
1) "n1"
2) "1"
3) "n2"
3) "1"
```


Notes
---

* The module supports only undirected graphs.

* The module doesn't support storing node data.

* The module doesn't support deleting edges.

Contributing
---

Issue reports, pull and feature requests are welcome.

License
---

MIT License - see [LICENSE](LICENSE)