package main

import (
	"bufio"
	"log"
	"os"
	"strings"

	"github.com/go-redis/redis"
)

type Node struct {
	Value     string
	Degree    string
	Component map[string]bool // node ids only
}

func buildGraph() map[string]Node {
	f, _ := os.Open("graph.nodes.txt")
	defer f.Close()

	scanner := bufio.NewScanner(f)
	scanner.Split(bufio.ScanLines)

	g := make(map[string]Node)
	for scanner.Scan() {
		l := strings.Split(scanner.Text(), ",")
		node := Node{Value: l[0], Degree: l[1]}
		if node.Component == nil {
			node.Component = make(map[string]bool, 0)
		}
		for _, v := range l[2:] {
			if v == "" {
				continue
			}
			node.Component[v] = true
		}
		g[l[0]] = node
	}
	return g
}

func addEdgesInRedis() error {
	rdb := redis.NewClient(&redis.Options{
		Addr:     "localhost:6382",
		Password: "", // no password set
		DB:       0,  // use default DB
	})
	defer rdb.Close()

	f, _ := os.Open("graph.edgelist.txt")
	defer f.Close()

	scanner := bufio.NewScanner(f)
	scanner.Split(bufio.ScanLines)

	for scanner.Scan() {
		edge := strings.Split(scanner.Text(), ",")
		_, err := rdb.Do("GRAPHZ.ADDEDGE", edge[0], edge[1]).Result()
		if err != nil {
			return err
		}
	}
	return nil
}

// dfs return a subgraph of the graph g
func dfs(n Node, g map[string]Node) (map[string]Node, error) {
	rdb := redis.NewClient(&redis.Options{
		Addr:     "localhost:6382",
		Password: "", // no password set
		DB:       0,  // use default DB
	})
	defer rdb.Close()

	result, err := rdb.Do("GRAPHZ.DFS", n.Value).Result()
	if err != nil {
		return nil, err
	}
	subGraph := make(map[string]Node)

	dfs := result.([]interface{})
	for i := 0; i < len(dfs); i = i + 2 {
		node := dfs[i].(string)
		degree := dfs[i+1].(string)
		subGraph[node] = Node{Value: node, Degree: degree}
	}

	return subGraph, nil
}

// assert that dfs graph has the same nodes as the original graph with the same degree
func assertDFSGraph(g map[string]Node, dfsGraph map[string]Node) {
	for k, v := range dfsGraph {
		if _, ok := g[k]; !ok {
			log.Fatalf("node %s is not in graph", k)
		}
		if v.Degree != g[k].Degree {
			log.Fatalf("degree of node %s in dfs graph %s is not equal to degree %s in the original graph", v.Value, v.Degree, g[k].Degree)
		}
	}
}

func assertNodeComponent(n Node, dfsGraph map[string]Node) {
	if len(n.Component) == 0 {
		return
	}
	for k, _ := range dfsGraph {
		if _, ok := n.Component[k]; !ok {
			log.Fatalf("node %s is not in component of node %s", k, n.Value)
		}
	}
	if len(dfsGraph) != len(n.Component) {
		log.Fatalf("component of node %s has %d nodes but dfs graph has %d nodes", n.Value, len(n.Component), len(dfsGraph))
	}
}

func main() {
	g := buildGraph()
	err := addEdgesInRedis()
	if err != nil {
		log.Fatal(err)
	}
	dfsGraph, err := dfs(g["42"], g)
	if err != nil {
		log.Fatal(err)
	}
	assertDFSGraph(g, dfsGraph)
	assertNodeComponent(g["42"], dfsGraph)

	dfsGraph, err = dfs(g["25"], g)
	if err != nil {
		log.Fatal(err)
	}
	assertDFSGraph(g, dfsGraph)
	assertNodeComponent(g["25"], dfsGraph)

	dfsGraph, err = dfs(g["5"], g)
	if err != nil {
		log.Fatal(err)
	}
	assertDFSGraph(g, dfsGraph)
	assertNodeComponent(g["5"], dfsGraph)
}
